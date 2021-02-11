/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "proc_family_monitor.h"
#include "pid_tracker.h"
#include "login_tracker.h"
#include "environment_tracker.h"
#include "parent_tracker.h"

#if !defined(WIN32)
#include "glexec_kill.unix.h"
#endif

#if defined(LINUX)
#include "group_tracker.linux.h"
#endif

#if defined(HAVE_EXT_LIBCGROUP)
#include "cgroup_tracker.linux.h"
#endif

ProcFamilyMonitor::ProcFamilyMonitor(pid_t pid,
                                     birthday_t birthday,
                                     int snapshot_interval,
									 bool except_if_pid_dies) :
	m_everybody_else(NULL),
	m_family_table(pidHashFunc),
	m_member_table(pidHashFunc),
	m_except_if_pid_dies(except_if_pid_dies)
{
	// the snapshot interval must either be non-negative or -1, which
	// means infinite (higher layers should enforce this)
	//
	ASSERT(snapshot_interval >= -1);

	// create our "tracker" objects that provide the various methods
	// for tracking process families
	//
	m_pid_tracker = new PIDTracker(this);
	ASSERT(m_pid_tracker != NULL);
#if defined(LINUX)
	m_group_tracker = NULL;
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	m_cgroup_tracker = NULL;
#endif
	m_login_tracker = new LoginTracker(this);
	ASSERT(m_login_tracker != NULL);
	m_environment_tracker = new EnvironmentTracker(this);
	ASSERT(m_environment_tracker != NULL);
	m_parent_tracker = new ParentTracker(this);
	ASSERT(m_parent_tracker != NULL);

	// create the "family" that will serve as a container for all processes
	// that are not in the family that we are tracking
	//
	// TODO: we currently use the ProcFamily object for this, but we
	// don't need much of what's in there. we should pull out the logic
	// that we need for this object into some kind of "ProcGroup" base
	// class
	//
	m_everybody_else = new ProcFamily(this);
	ASSERT(m_everybody_else != NULL);

	// create the "root" family; if we'd like to EXCEPT if the root family
	// dies, we make the watcher pid the same as the root pid.  Otherwise, we
	// set it to zero which means that "garbage collection" won't be enabled
	// for this family and we won't EXCEPT if it dies.
	//
	ProcFamily* family = new ProcFamily(this,
	                                    pid,
	                                    birthday,
	                                    except_if_pid_dies ? pid : 0,
	                                    snapshot_interval);
	ASSERT(family != NULL);

	// make sure we're tracking this family by its root pid/birthday
	//
	m_pid_tracker->add_mapping(family, pid, birthday);

	// make this family the root-level tree node
	//
	m_tree = new Tree<ProcFamily*>(family);
	ASSERT(m_tree != NULL);

	// and insert the tree into our hash table for families
	//
	int ret = m_family_table.insert(pid, m_tree);
	ASSERT(ret != -1);

	// take an initial snapshot
	//
	snapshot();
}

ProcFamilyMonitor::~ProcFamilyMonitor()
{
	delete_all_families(m_tree);
	delete m_tree;
	delete m_everybody_else;
	delete m_parent_tracker;
	delete m_environment_tracker;
	delete m_login_tracker;
#if defined(LINUX)
	if (m_group_tracker != NULL) {
		delete m_group_tracker;
	}
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	if (m_cgroup_tracker != NULL) {
		delete m_cgroup_tracker;
	}
#endif
	delete m_pid_tracker;
}

#if defined(LINUX)
void
ProcFamilyMonitor::enable_group_tracking(gid_t min_tracking_gid,
                                         gid_t max_tracking_gid,
										 bool allocating)
{
	ASSERT(m_group_tracker == NULL);
	m_group_tracker = new GroupTracker(this,
	                                   min_tracking_gid,
	                                   max_tracking_gid,
									   allocating);
	ASSERT(m_group_tracker != NULL);
}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
void
ProcFamilyMonitor::enable_cgroup_tracking()
{
	ASSERT(m_cgroup_tracker == NULL);
	m_cgroup_tracker = new CGroupTracker(this);
	ASSERT(m_cgroup_tracker != NULL);
}
#endif

proc_family_error_t
ProcFamilyMonitor::register_subfamily(pid_t root_pid,
                                      pid_t watcher_pid,
                                      int max_snapshot_interval)
{
	int ret;

	// root pid must be positive
	//
	if (root_pid <= 0) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure: bad root pid: %u\n",
		        root_pid);
		return PROC_FAMILY_ERROR_BAD_ROOT_PID;
	}

	// watcher pid must be non-negative; zero indicates the subfamily
	// is unwatched (which just means there will be no automatic
	// garbage collection for this family)
	//
	if (watcher_pid < 0) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure; bad watcher pid: %u\n",
		        watcher_pid);
		return PROC_FAMILY_ERROR_BAD_WATCHER_PID;
	}

	// max_snapshot interval must either be non-negative, or -1
	// for infinite
	//
	if (max_snapshot_interval < -1) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure: bad max snapshot interval: %d\n",
		        max_snapshot_interval);
		return PROC_FAMILY_ERROR_BAD_SNAPSHOT_INTERVAL;
	}

	// get our family tree state as up to date as possible
	//
	snapshot();

	// find the root process of the (potential) new subfamily
	// in our snapshot. we require that the process of any newly
	// created subfamily is in a family we are already tracking
	//
	ProcFamilyMember* member;
	ret = m_member_table.lookup(root_pid, member);
	if ((ret == -1) || (member->get_proc_family() == m_everybody_else)) {
		dprintf(D_ALWAYS,
		        "register_subfamily failure: pid %u not in process tree\n",
		        root_pid);
		return PROC_FAMILY_ERROR_PROCESS_NOT_FAMILY;
	}

	// make sure this process isn't already the root of a family
	//
	Tree<ProcFamily*>* tree = lookup_family(root_pid);
	if (tree != NULL) {
		dprintf(D_ALWAYS,
		        "register_subfamily: pid %u already registered\n",
		        root_pid);
		return PROC_FAMILY_ERROR_ALREADY_REGISTERED;
	}

	// finally create the new family
	//
	ProcFamily* family = new ProcFamily(this,
	                                    root_pid,
	                                    member->get_proc_info()->birthday,
	                                    watcher_pid,
	                                    max_snapshot_interval);

	// register this family with the PID-based tracker object
	//
	m_pid_tracker->add_mapping(family,
	                           root_pid,
	                           member->get_proc_info()->birthday);

	// find the family that will be this new subfamily's parent and create
	// the parent-child link
	//
	pid_t parent_root = member->get_proc_family()->get_root_pid();
	Tree<ProcFamily*>* parent_tree_node = lookup_family(parent_root);
	ASSERT(parent_tree_node != NULL);
	Tree<ProcFamily*>* child_tree_node =
		parent_tree_node->add_child(family);
	ASSERT(child_tree_node != NULL);

	// move the new family's root process into the correct family
	//
	dprintf(D_ALWAYS,
	        "moving process %u into new subfamily %u\n",
	        root_pid,
	        root_pid);
	member->move_to_subfamily(family);

	// and insert the tree into our hash table for families
	//
	ret = m_family_table.insert(root_pid, child_tree_node);
	ASSERT(ret != -1);

	dprintf(D_ALWAYS,
	        "new subfamily registered: root = %u, watcher = %u\n",
	        root_pid,
	        watcher_pid);

	return PROC_FAMILY_ERROR_SUCCESS;
}

Tree<ProcFamily*>*
ProcFamilyMonitor::lookup_family(pid_t pid, bool zero_means_root)
{
	if (zero_means_root && (pid == 0)) {
		return m_tree;
	}
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	if (ret == -1) {
		return NULL;
	}
	return tree;
}

proc_family_error_t
ProcFamilyMonitor::track_family_via_environment(pid_t pid, PidEnvID* penvid)
{
	// lookup the family
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "track_family_via_environment failure: "
				    "family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	// add this association to the tracker
	//
	m_environment_tracker->add_mapping(tree->get_data(), penvid);
	return PROC_FAMILY_ERROR_SUCCESS;
}

proc_family_error_t
ProcFamilyMonitor::track_family_via_login(pid_t pid, char* login)
{
	// lookup the family
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "track_family_via_login failure: "
				    "family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	// add this association to the tracker
	//
	m_login_tracker->add_mapping(tree->get_data(), login);
	return PROC_FAMILY_ERROR_SUCCESS;
}

#if defined(LINUX)
proc_family_error_t
ProcFamilyMonitor::track_family_via_supplementary_group(pid_t pid, gid_t& gid)
{
	// check if group tracking is enabled
	//
	if (m_group_tracker == NULL) {
		return PROC_FAMILY_ERROR_NO_GROUP_ID_AVAILABLE;
	}

	// lookup the family
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "track_family_via_supplementary_group failure: "
				    "family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	bool ok = m_group_tracker->add_mapping(tree->get_data(), gid);
	if (!ok) {
		return PROC_FAMILY_ERROR_NO_GROUP_ID_AVAILABLE;
	}

	return PROC_FAMILY_ERROR_SUCCESS;
}
#endif

#if defined(HAVE_EXT_LIBCGROUP)
proc_family_error_t
ProcFamilyMonitor::track_family_via_cgroup(pid_t pid, const char * cgroup)
{
	if (m_cgroup_tracker == NULL) {
		return PROC_FAMILY_ERROR_NO_CGROUP_ID_AVAILABLE;
	}

	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
			"track_family_via_cgroup failure: "
				"family with root %u not found\n",
			pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	bool ok = m_cgroup_tracker->add_mapping(tree->get_data(), cgroup);
	if (!ok) {
		return PROC_FAMILY_ERROR_NO_CGROUP_ID_AVAILABLE;
	}

	return PROC_FAMILY_ERROR_SUCCESS;
}
#endif

proc_family_error_t
ProcFamilyMonitor::unregister_subfamily(pid_t pid)
{
	// lookup the family
	//
	Tree<ProcFamily*>* tree = lookup_family(pid);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "unregister_subfamily failure: family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	// make sure its not the "root family" (which can't be unregistered)
	//
	if (tree->get_parent() == NULL) {
		dprintf(D_ALWAYS,
		        "unregister_subfamily failure: cannot unregister root family\n");
		return PROC_FAMILY_ERROR_UNREGISTER_ROOT;
	}

	// do the deed
	//
	dprintf(D_ALWAYS,
	        "unregistering family with root pid %u\n",
	        pid);
	unregister_subfamily(tree);

	return PROC_FAMILY_ERROR_SUCCESS;
}

#if !defined(WIN32)
proc_family_error_t
ProcFamilyMonitor::use_glexec_for_family(pid_t pid, char* proxy)
{
	// only allow this if the glexec_kill module has been
	// initialized
	//
	if (!glexec_kill_check()) {
		dprintf(D_ALWAYS,
		        "use_glexec_for_family failure: "
		            "glexec_kill not initialized\n");
		return PROC_FAMILY_ERROR_NO_GLEXEC;
	}

	// lookup the family
	//
	Tree<ProcFamily*>* tree;
	int ret = m_family_table.lookup(pid, tree);
	if (ret == -1) {
		dprintf(D_ALWAYS,
		        "use_glexec_for_family failure: "
		            "family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	// associate the proxy with the family
	//
	tree->get_data()->set_proxy(proxy);

	return PROC_FAMILY_ERROR_SUCCESS;
}
#endif

int
ProcFamilyMonitor::get_snapshot_interval()
{
	return get_snapshot_interval(m_tree);
}

proc_family_error_t
ProcFamilyMonitor::signal_process(pid_t pid, int sig)
{
	// make sure signals are only sent to subtree roots
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "signal_process failure: family with root %u not found\n",
				pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	dprintf(D_ALWAYS, "sending signal %d to process %u\n", sig, pid);
	tree->get_data()->signal_root(sig);

	return PROC_FAMILY_ERROR_SUCCESS;
}

proc_family_error_t
ProcFamilyMonitor::signal_family(pid_t pid, int sig)
{
	// get as up to date as possible
	//
	snapshot();

	// find the family
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "signal_family error: family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}
	
	// now send the signal and return
	//
	dprintf(D_ALWAYS, "sending signal %d to family with root %u\n", sig, pid);
	signal_family(tree, sig);

	return PROC_FAMILY_ERROR_SUCCESS;
}

proc_family_error_t
ProcFamilyMonitor::get_family_usage(pid_t pid, ProcFamilyUsage* usage)
{
	// find the family
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "get_family_usage failure: family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	// get usage from the requested family and all subfamilies
	//
	ASSERT(usage != NULL);
	usage->user_cpu_time = 0;
	usage->sys_cpu_time = 0;
	usage->percent_cpu = 0.0;
	usage->total_image_size = 0;
	usage->total_resident_set_size = 0;
#if HAVE_PSS
    usage->total_proportional_set_size = 0;
    usage->total_proportional_set_size_available = false;
#endif
	// We initialize these to -1 to distinguish the "cannot record" and "no I/O" cases.
	usage->block_read_bytes = -1;
	usage->block_write_bytes = -1;
    usage->block_reads = -1;
    usage->block_writes = -1;
    usage->io_wait = -1;
	usage->num_procs = 0;
	get_family_usage(tree, usage);

	// max image size is handled separately; we request it directly from
	// the top-level family
	//
	usage->max_image_size = tree->get_data()->get_max_image_size();

	return PROC_FAMILY_ERROR_SUCCESS;
}

void
ProcFamilyMonitor::snapshot()
{
	dprintf(D_ALWAYS, "taking a snapshot...\n");

	// get a snapshot of all processes on the system
	// TODO: should we do something here if ProcAPI returns a NULL result?
	// (the algorithm below will handle it just fine, but its probably an
	// indication that something is wrong)
	//
	procInfo* pi_list = ProcAPI::getProcInfoList();

	// print info about all procInfo allocations
	//
	//procInfo* pi = pi_list;
	//while (pi != NULL) {
	//	dprintf(D_ALWAYS,
	//	        "PROCINFO ALLOCATION: %p for pid %u\n",
	//	        pi,
	//	        pi->pid);
	//	pi = pi->next;
	//}

	// scan through the latest snapshot looking for processes
	// that we've seen before in previous calls to snapshot();
	// for each such process we find:
	//   - call its ProcFamilyMember's still_alive method, which
	//     will mark it as such and update its procInfo
	//   - remove its procInfo struct from pi_list, since this
	//     process's family membership was already determined in
	//     a previous snapshot
	// we also hackishly get rid of any procInfo's with a PID of
	// zero. windows tends to return two of these (they are system
	// processes of some sort), and the code below gets confused
	// when multiple processes with a single PID are in our list
	//
	procInfo** prev_ptr = &pi_list;
	procInfo* curr = pi_list;
	while (curr != NULL) {

		if (curr->pid == 0) {
			*prev_ptr = curr->next;
			delete curr;
			curr = *prev_ptr;
			continue;
		}

		ProcFamilyMember* pm;
		int ret = m_member_table.lookup(curr->pid, pm);
		if (ret != -1 &&
		    pm->get_proc_info()->birthday == curr->birthday)
		{
			// we've seen this process before; update it with
			// its newer procInfo struct (this call will result
			// in the old procInfo struct being freed)
			//
			*prev_ptr = curr->next;
			pm->still_alive(curr);
		}
		else {
			// this process is not in any of our families, so it stays
			// on the list
			//
			prev_ptr = &curr->next;
		}

		curr = curr->next;
	}

	// now tell all our ProcFamily objects to get rid of the family members
	// that are no longer on the system (i.e. those that did not get the
	// still_alive method of ProcFamilyMember called in the loop above)
	//
	remove_exited_processes(m_tree);
	m_everybody_else->remove_exited_processes();

	// we've now handled all processes that we've seen
	// in previous calls to snapshot(). now we have to handle the
	// rest by determining whether they belong in any of the families we're
	// monitoring. for this, we rely on our set of "tracker" objects.
	//
	// NOTE: it is important that we use m_parent_tracker last, since its
	//       results depend on the results of the other trackers (for example,
	//       a process whose parent is pid 1 (init) that can be found via
	//       m_environment_tracker might have a direct child that can't since
	//       it has cleared its environment. if we run m_parent_tracker before
	//       m_environment_tracker in this case, the child will not be found)
	//
	m_pid_tracker->find_processes(pi_list);
#if defined(LINUX)
	if (m_group_tracker != NULL) {
		m_group_tracker->find_processes(pi_list);
	}
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	if (m_cgroup_tracker != NULL) {
		m_cgroup_tracker->find_processes(pi_list);
	}
#endif
	m_login_tracker->find_processes(pi_list);
	m_environment_tracker->find_processes(pi_list);
	m_parent_tracker->find_processes(pi_list);

	// at this point, any procInfo structures in pi_list that aren't in
	// m_member_table are processes that (a) we haven't seen before, and
	// (b) don't belong in the family tree. we'll now add all such processes
	// to m_everybody_else
	//
	curr = pi_list;
	while (curr != NULL) {
		ProcFamilyMember* pfm;
		int ret = m_member_table.lookup(curr->pid, pfm);
		if (ret == -1) {
#if defined(CHATTY_PROC_LOG)
			dprintf(D_ALWAYS,
			        "no methods have determined process %u to be in a monitored family\n",
			        curr->pid);
#endif /* defined(CHATTY_PROC_LOG) */
			m_everybody_else->add_member(curr);
		}
		curr = curr->next;
	}

	// cleanup any families whose "watchers" have exited
	//
	delete_unwatched_families(m_tree);

	// now walk the tree and update the families' maximum image size
	// bookkeeping
	//
	update_max_image_sizes(m_tree);

	dprintf(D_ALWAYS, "...snapshot complete\n");
}

void
ProcFamilyMonitor::add_member(ProcFamilyMember* member)
{
	int ret = m_member_table.insert(member->get_proc_info()->pid, member);
	ASSERT(ret != -1);
}

void
ProcFamilyMonitor::remove_member(ProcFamilyMember* member)
{
	int ret = m_member_table.remove(member->get_proc_info()->pid);
	ASSERT(ret != -1);
}

ProcFamilyMember*
ProcFamilyMonitor::lookup_member(pid_t pid)
{
	ProcFamilyMember* pm;
	int ret = m_member_table.lookup(pid, pm);
	return (ret != -1) ? pm : NULL;
}

bool
ProcFamilyMonitor::add_member_to_family(ProcFamily* pf,
                                        procInfo* pi,
                                        const char* method_str)
{
	// see if this process has already been inserted into a family
	//
	ProcFamilyMember* pfm;
	int ret = m_member_table.lookup(pi->pid, pfm);
	if (ret == -1) {

		// this process is not already associated with a family;
		// just go ahead and insert it
		//
#if defined(CHATTY_PROC_LOG)
		dprintf(D_ALWAYS,
		        "method %s: found family %u for process %u\n",
		        method_str,
		        pf->get_root_pid(),
		        pi->pid);
#endif /* defined(CHATTY_PROC_LOG) */
		pf->add_member(pi);
		return true;
	}
	else if (pf != pfm->get_proc_family()) {

		// this process is already associated with a family; if
		// pf is a subfamily of the family that the process is
		// already assigned to (i.e. pf is a "more specific"
		// match), then move the process
		//
		Tree<ProcFamily*>* node = lookup_family(pf->get_root_pid());
		ASSERT(node != NULL);
		while(node->get_parent() != NULL) {
			node = node->get_parent();
			if (node->get_data() == pfm->get_proc_family()) {
				dprintf(D_ALWAYS,
				        "method %s: found family %u for process %u "
				        	"(more specific than current family %u)\n",
				        method_str,
				        pf->get_root_pid(),
				        pi->pid,
				        pfm->get_proc_family()->get_root_pid());
				pfm->move_to_subfamily(pf);
				return true;
			}
		}

		dprintf(D_ALWAYS,
		        "method %s: found family %u for process %u "
		        	"(less specific - ignoring)\n",
		        method_str,
		        pf->get_root_pid(),
		        pi->pid);
		return false;
	}
	else {

		// this process is already in pf; there's nothing to do
		//
		dprintf(D_ALWAYS,
		        "method %s: found family %u for process %u "
		        	"(already determined)\n",
		        method_str,
		        pf->get_root_pid(),
		        pi->pid);
		return false;
	}
}

int
ProcFamilyMonitor::get_snapshot_interval(Tree<ProcFamily*>* tree)
{
	// start with the value from the current tree node
	//
	int ret_value = tree->get_data()->get_max_snapshot_interval();
	
	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		int child_value = get_snapshot_interval(child);
		if (ret_value == -1) {
			ret_value = child_value;
		}
		else if (child_value < ret_value) {
			ret_value = child_value;
		}
		child = child->get_sibling();
	}

	return ret_value;
}

unsigned long
ProcFamilyMonitor::update_max_image_sizes(Tree<ProcFamily*>* tree)
{
	// recurse on children, keeping sum of their total image
	// size as we go
	//
	unsigned long sum = 0;
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		sum += update_max_image_sizes(child);
		child = child->get_sibling();
	}

	// now call update_image_size on the current node, passing
	// it the sum of our children's image sizes, and return our
	// own total image size
	//
	return tree->get_data()->update_max_image_size(sum);
}

void
ProcFamilyMonitor::get_family_usage(Tree<ProcFamily*>* tree, ProcFamilyUsage* usage)
{
	// get usage from current tree node
	//
	tree->get_data()->aggregate_usage(usage);

	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		get_family_usage(child, usage);
		child = child->get_sibling();
	}
}

void
ProcFamilyMonitor::signal_family(Tree<ProcFamily*>* tree, int sig)
{
	// signal current tree node
	//
	tree->get_data()->spree(sig);

	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		signal_family(child, sig);
		child = child->get_sibling();
	}
}

void
ProcFamilyMonitor::remove_exited_processes(Tree<ProcFamily*>* tree)
{
	// remove exited processes from current tree node
	//
	tree->get_data()->remove_exited_processes();

	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		remove_exited_processes(child);
		child = child->get_sibling();
	}
}

void
ProcFamilyMonitor::delete_unwatched_families(Tree<ProcFamily*>* tree)
{
	// recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {

		// save the next child in line, since recursing may result
		// in the current child begin removed from the tree
		//
		Tree<ProcFamily*>* next_child = child->get_sibling();
		delete_unwatched_families(child);
		child = next_child;
	}

	// check to see if the current tree node's watcher has exited
	// (a watcher PID of 0 means the family is not watched: automatic
	// unregistering will not happen)
	pid_t watcher_pid = tree->get_data()->get_watcher_pid();
	if (watcher_pid == 0) {
		return;
	}

	ProcFamilyMember* member;
	int ret = m_member_table.lookup(watcher_pid, member);
	if (ret != -1) {
		if (member->get_proc_info()->birthday <=
	            tree->get_data()->get_root_birthday())
		{
			// this family's watcher is still around; do nothing
			//
			return;
		}
		dprintf(D_ALWAYS,
		        "watcher %u found with later birthdate ("
		            PROCAPI_BIRTHDAY_FORMAT
		            ") than watched process %u ("
		            PROCAPI_BIRTHDAY_FORMAT
		            ")\n",
		        watcher_pid,
		        member->get_proc_info()->birthday,
		        tree->get_data()->get_root_pid(),
		        tree->get_data()->get_root_birthday());
	}

	// it looks like the watcher has exited. if this family is the
	// "root family" and we should except if it dies, then this means 
	// the Master has gone away and we should die. 
	// otherwise, simply unregister the unwatched family.
	//
	if (m_except_if_pid_dies && tree->get_parent() == NULL) {
		EXCEPT("Watcher pid %lu exited so procd is exiting. Bye.",
			(unsigned long)watcher_pid);
	}
	ASSERT(tree->get_parent() != NULL);
	pid_t root_pid = tree->get_data()->get_root_pid();
	unregister_subfamily(tree);

	dprintf(D_ALWAYS,
	        "watcher %u of family with root %u has died; family removed\n",
	        watcher_pid,
	        root_pid);
}

void
ProcFamilyMonitor::unregister_subfamily(Tree<ProcFamily*>* tree)
{
	// remove any information that these families might have registered with
	// our tracker objects
	//
	m_pid_tracker->remove_mapping(tree->get_data());
#if defined(LINUX)
	if (m_group_tracker != NULL) {
		m_group_tracker->remove_mapping(tree->get_data());
	}
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	if (m_cgroup_tracker != NULL) {
		m_cgroup_tracker->remove_mapping(tree->get_data());
	}
#endif
	m_login_tracker->remove_mapping(tree->get_data());
	m_environment_tracker->remove_mapping(tree->get_data());

	// get rid of the hash table entry for this family
	//
	int ret = m_family_table.remove(tree->get_data()->get_root_pid());
	ASSERT(ret != -1);

	// this will move all family members of the current
	// family up into its parent family
	//
	ASSERT(tree->get_parent() != NULL);
	tree->get_data()->fold_into_parent(tree->get_parent()->get_data());

	// remove the current node from the tree, reparenting all
	// children up to our parent
	//
	tree->remove();

	// clean up the current node now that its relationships
	// with other ProcFamilys and ProcFamilyMembers have
	// been removed
	//
	delete tree->get_data();
	delete tree;
}

void
ProcFamilyMonitor::delete_all_families(Tree<ProcFamily*>* tree)
{
	// recurse on chlidren
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		delete_all_families(child);
		child = child->get_sibling();
	}

	// free up the family object for the current node
	//
	delete tree->get_data();
}

proc_family_error_t
ProcFamilyMonitor::dump(pid_t pid, std::vector<ProcFamilyDump>& vec)
{
	// lookup the family. if the lookup fails, send "false" to the
	// client and return. otherwise, send "true" and keep going
	//
	Tree<ProcFamily*>* tree = lookup_family(pid, true);
	if (tree == NULL) {
		dprintf(D_ALWAYS,
		        "output failure: family with root %u not found\n",
		        pid);
		return PROC_FAMILY_ERROR_FAMILY_NOT_FOUND;
	}

	// clear the passed-in vector and fill it using our helper method
	//
	vec.clear();
	dump(tree, vec);

	return PROC_FAMILY_ERROR_SUCCESS;
}

void
ProcFamilyMonitor::dump(Tree<ProcFamily*>* tree,
                        std::vector<ProcFamilyDump>& vec)
{
	// do the current family first
	//
	ProcFamilyDump fam;
	fam.parent_root = 0;
	if (tree->get_parent() != NULL) {
		fam.parent_root = tree->get_parent()->get_data()->get_root_pid();
	}
	fam.root_pid = tree->get_data()->get_root_pid();
	fam.watcher_pid = tree->get_data()->get_watcher_pid();
	tree->get_data()->dump(fam);
	vec.push_back(fam);

	// now recurse on children
	//
	Tree<ProcFamily*>* child = tree->get_child();
	while (child != NULL) {
		dump(child, vec);
		child = child->get_sibling();
	}
}
