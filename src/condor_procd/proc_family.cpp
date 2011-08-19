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
#include "proc_family.h"
#include "proc_family_monitor.h"
#include "procd_common.h"

#if !defined(WIN32)
#include "glexec_kill.unix.h"
#endif

#if defined(HAVE_EXT_LIBCGROUP)
#include "libcgroup.h"
#define MEMORY_CONTROLLER "memory"
#define CPUACCT_CONTROLLER "cpuacct"
#define FREEZE_CONTROLLER "freezer"
#define BLOCK_CONTROLLER "blkio"

#define FROZEN "FROZEN"
#define THAWED "THAWED"

#define BLOCK_STATS_LINE_MAX 64

bool ProcFamily::m_cgroup_initialized = false;
bool ProcFamily::m_cgroup_freezer_mounted = false;
bool ProcFamily::m_cgroup_cpuacct_mounted = false;
bool ProcFamily::m_cgroup_memory_mounted = false;
bool ProcFamily::m_cgroup_block_mounted = false;
#include <unistd.h>
long ProcFamily::clock_tick = sysconf( _SC_CLK_TCK );
#endif

ProcFamily::ProcFamily(ProcFamilyMonitor* monitor,
                       pid_t              root_pid,
                       birthday_t         root_birthday,
                       pid_t              watcher_pid,
                       int                max_snapshot_interval) :
	m_monitor(monitor),
	m_root_pid(root_pid),
	m_root_birthday(root_birthday),
	m_watcher_pid(watcher_pid),
	m_max_snapshot_interval(max_snapshot_interval),
	m_exited_user_cpu_time(0),
	m_exited_sys_cpu_time(0),
	m_max_image_size(0),
	m_member_list(NULL)
{
#if !defined(WIN32)
	m_proxy = NULL;
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	m_cgroup_string = NULL;
	m_cgroup = NULL;
	m_created_cgroup = false;
#endif
}

ProcFamily::~ProcFamily()
{
	// delete our member list
	//
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		ProcFamilyMember* next_member = member->m_next;
		//dprintf(D_ALWAYS,
		//        "PROCINFO DEALLOCATION: %p for pid %u\n",
		//        member->m_proc_info,
		//        member->m_proc_info->pid);
		delete member->m_proc_info;
		delete member;
		member = next_member;
	}

#if !defined(WIN32)
	// delete the proxy if we've been given one
	//
	if (m_proxy != NULL) {
		free(m_proxy);
	}
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	if (m_cgroup && m_created_cgroup) {
		delete_cgroup(m_cgroup_string);
	}
	if (m_cgroup != NULL) {
		cgroup_free(&m_cgroup);
	}
	if (m_cgroup_string != NULL) {
		free(m_cgroup_string);
	}
#endif
}

#if defined(HAVE_EXT_LIBCGROUP)

void
ProcFamily::delete_cgroup(const char * cg_string)
{
	int err;

	struct cgroup* dcg = cgroup_new_cgroup(m_cgroup_string);
	ASSERT (dcg != NULL);
	if ((err = cgroup_get_cgroup(dcg))) {
		dprintf(D_PROCFAMILY,
			"Unable to read cgroup %s for deletion (ProcFamily %u): %u %s\n",
			cg_string, m_root_pid, err, cgroup_strerror(err));
	}
	else if ((err = cgroup_delete_cgroup(dcg, CGFLAG_DELETE_RECURSIVE | CGFLAG_DELETE_IGNORE_MIGRATION))) {
		dprintf(D_ALWAYS,
			"Unable to completely remove cgroup %s for ProcFamily %u. %u %s\n",
			cg_string, m_root_pid, err, cgroup_strerror(err)
			);
	} else {
		dprintf(D_PROCFAMILY,
			"Deleted cgroup %s for ProcFamily %u\n",
			cg_string, m_root_pid);
	}
	cgroup_free(&dcg);
}

int
ProcFamily::migrate_to_cgroup(pid_t pid)
{
	// Attempt to migrate a given process to a cgroup.
	// This can be done without regards to whether the
	// process is already in the cgroup

	// We want to make sure task migration is turned on for the
	// associated memory controller.  So, we get to look up the original cgroup.
	//
	// If there is no memory controller present, we skip all this and just attempt a migrate
	int err;
	u_int64_t orig_migrate;
	bool changed_orig = false;
	char * orig_cgroup_string = NULL;
	struct cgroup * orig_cgroup;
	struct cgroup_controller * memory_controller;
	if (m_cgroup_memory_mounted && (err = cgroup_get_current_controller_path(pid, MEMORY_CONTROLLER, &orig_cgroup_string))) {
		dprintf(D_PROCFAMILY,
			"Unable to determine current memory cgroup for PID %u (ProcFamily %u): %u %s\n",
			pid, m_root_pid, err, cgroup_strerror(err));
		return 1;
	}
	// We will migrate the PID to the new cgroup even if it is in the proper memory controller cgroup
	// It is possible for the task to be in multiple cgroups.
	if (m_cgroup_memory_mounted && (orig_cgroup_string != NULL) && (strcmp(m_cgroup_string, orig_cgroup_string))) {
		// Yes, there are race conditions here - can't really avoid this.
		// Throughout this block, we can assume memory controller exists.
		// Get original value of migrate.
		orig_cgroup = cgroup_new_cgroup(orig_cgroup_string);
		ASSERT (orig_cgroup != NULL);
		if ((err = cgroup_get_cgroup(orig_cgroup))) {
			dprintf(D_PROCFAMILY,
				"Unable to read original cgroup %s (ProcFamily %u): %u %s\n",
				orig_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			goto after_migrate;
		}
		if ((memory_controller = cgroup_get_controller(orig_cgroup, MEMORY_CONTROLLER)) == NULL) {
			cgroup_free(&orig_cgroup);
			goto after_migrate;
		}
		if ((err = cgroup_get_value_uint64(memory_controller, "memory.move_charge_at_immigrate", &orig_migrate))) {
			if (err == ECGROUPVALUENOTEXIST) {
				// Older kernels don't have the ability to migrate memory accounting to the new cgroup.
				dprintf(D_PROCFAMILY,
					"This kernel does not support memory usage migration; cgroup %s memory statistics"
					" will be slightly incorrect (ProcFamily %u)\n",
					m_cgroup_string, m_root_pid);
			} else {
				dprintf(D_PROCFAMILY,
					"Unable to read cgroup %s memory controller settings for "
					"migration (ProcFamily %u): %u %s\n",
					orig_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			}
			cgroup_free(&orig_cgroup);
			goto after_migrate;
		}
		if (orig_migrate != 3) {
			orig_cgroup = cgroup_new_cgroup(orig_cgroup_string);
			memory_controller = cgroup_add_controller(orig_cgroup, MEMORY_CONTROLLER);
			ASSERT (memory_controller != NULL); // Memory controller must already exist
			cgroup_add_value_uint64(memory_controller, "memory.move_charge_at_immigrate", 3);
			if ((err = cgroup_modify_cgroup(orig_cgroup))) {
				// Not allowed to change settings
				dprintf(D_ALWAYS,
					"Unable to change cgroup %s memory controller settings for migration. "
					"Some memory accounting will be inaccurate (ProcFamily %u): %u %s\n",
					orig_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			} else {
				changed_orig = true;
			}
		}
		cgroup_free(&orig_cgroup);
	}

after_migrate:

	orig_cgroup = NULL;
	ASSERT (m_cgroup_string != NULL)
	err = cgroup_attach_task_pid(m_cgroup, pid);
	if (err) {
		dprintf(D_PROCFAMILY,
			"Cannot attach pid %u to cgroup %s for ProcFamily %u: %u %s\n",
			pid, m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
	}

	if (changed_orig) {
		if ((orig_cgroup = cgroup_new_cgroup(orig_cgroup_string))) {
			goto after_restore;
		}
		if (((memory_controller = cgroup_add_controller(orig_cgroup, MEMORY_CONTROLLER)) != NULL) &&
			(!cgroup_add_value_uint64(memory_controller, "memory.move_charge_at_immigrate", orig_migrate))) {
			cgroup_modify_cgroup(orig_cgroup);
		}
		cgroup_free(&orig_cgroup);
	}

after_restore:
	if (orig_cgroup_string != NULL) {
		free(orig_cgroup_string);
	}
	return err;
}

int
ProcFamily::set_cgroup(const char * cgroup)
{
	int err;
	bool has_cgroup = true, changed_cgroup = false;

	if (!strcmp(cgroup, "/")) {
		dprintf(D_ALWAYS,
			"Cowardly refusing to monitor the root cgroup out "
			"of security concerns.\n");
		return 1;
	}

	// Initialize library and data structures
	if (m_cgroup_initialized == false) {
		dprintf(D_PROCFAMILY, "Initializing cgroup library.\n");
		cgroup_init();
		void *handle;
		controller_data info;
		int ret = cgroup_get_all_controller_begin(&handle, &info);
		while (ret == 0) {
			if (strcmp(info.name, MEMORY_CONTROLLER) == 0) {
				m_cgroup_memory_mounted = (info.hierarchy != 0);
			} else if (strcmp(info.name, CPUACCT_CONTROLLER) == 0) {
				m_cgroup_cpuacct_mounted = (info.hierarchy != 0);
			} else if (strcmp(info.name, FREEZE_CONTROLLER) == 0) {
				m_cgroup_freezer_mounted = (info.hierarchy != 0);
			} else if (strcmp(info.name, BLOCK_CONTROLLER) == 0) {
				m_cgroup_block_mounted = (info.hierarchy != 0);
			}
			ret = cgroup_get_all_controller_next(&handle, &info);
		}
		cgroup_get_all_controller_end(&handle);
		if (!m_cgroup_block_mounted) {
			dprintf(D_ALWAYS, "Cgroup controller for I/O statistics is not mounted; accounting will be inaccurate.\n");
		}
		if (!m_cgroup_freezer_mounted) {
			dprintf(D_ALWAYS, "Cgroup controller for process management is not mounted; process termination will be inaccurate.\n");
		}
		if (!m_cgroup_cpuacct_mounted) {
			dprintf(D_ALWAYS, "Cgroup controller for CPU accounting is not mounted; cpu accounting will be inaccurate.\n");
		}
		if (!m_cgroup_memory_mounted) {
			dprintf(D_ALWAYS, "Cgroup controller for memory accounting is not mounted; memory accounting will be inaccurate.\n");
		}
		if (ret != ECGEOF) {
			dprintf(D_ALWAYS, "Error iterating through cgroups mount information: %s\n", cgroup_strerror(ret));
		}

	}

	// Ignore this command if we've done this before.
	if ((m_cgroup_string != NULL) && !strcmp(m_cgroup_string, cgroup)) {
		return 0;
	}

	dprintf(D_PROCFAMILY, "Setting cgroup to %s for ProcFamily %u.\n",
		cgroup, m_root_pid);

	if (m_cgroup_string) {
		if (m_created_cgroup)
			delete_cgroup(m_cgroup_string);
		m_created_cgroup = false;
		free(m_cgroup_string);
	}
	if (m_cgroup)
		cgroup_free(&m_cgroup);

	size_t cgroup_len = strlen(cgroup);
	m_cgroup_string = (char *)malloc(sizeof(char)*(cgroup_len+1));
	strcpy(m_cgroup_string, cgroup);
	m_cgroup = cgroup_new_cgroup(m_cgroup_string);
	ASSERT(m_cgroup != NULL);

	if (ECGROUPNOTEXIST == cgroup_get_cgroup(m_cgroup)) {
		has_cgroup = false;
	}

	// Record if we don't have a particular subsystem, but it's not fatal.
	// Add the controller if the cgroup didn't exist or it is not yet present.
	if (m_cgroup_cpuacct_mounted && ((has_cgroup == false) || (cgroup_get_controller(m_cgroup, CPUACCT_CONTROLLER) == NULL))) {
		changed_cgroup = true;
		if (cgroup_add_controller(m_cgroup, CPUACCT_CONTROLLER) == NULL) {
			dprintf(D_PROCFAMILY,
				"Unable to initialize cgroup CPU accounting subsystem"
				" for %s.\n",
				m_cgroup_string);
		}
	}
	if (m_cgroup_memory_mounted && ((has_cgroup == false) || (cgroup_get_controller(m_cgroup, MEMORY_CONTROLLER) == NULL))) {
		changed_cgroup = true;
		if (cgroup_add_controller(m_cgroup, MEMORY_CONTROLLER) == NULL) {
			dprintf(D_PROCFAMILY,
				"Unable to initialize cgroup memory accounting subsystem"
				" for %s.\n",
				cgroup);
		}
	}
	if (m_cgroup_freezer_mounted && ((has_cgroup == false) || (cgroup_get_controller(m_cgroup, FREEZE_CONTROLLER) == NULL))) {
		changed_cgroup = true;
		if (cgroup_add_controller(m_cgroup, FREEZE_CONTROLLER) == NULL) {
			dprintf(D_PROCFAMILY,
				"Unable to initialize cgroup subsystem for killing "
				"processes for %s.\n",
				m_cgroup_string);
		}
	}
	if (m_cgroup_block_mounted && ((has_cgroup == false) || (cgroup_get_controller(m_cgroup, BLOCK_CONTROLLER) == NULL))) {
		changed_cgroup = true;
		if (cgroup_add_controller(m_cgroup, BLOCK_CONTROLLER) == NULL) {
			dprintf(D_PROCFAMILY,
			"Unable to initialize cgroup subsystem for bloock "
			"statistics for %s.\n",
			m_cgroup_string);
		}
	}
	// We don't consider failures fatal, as anything is better than nothing.
	if (has_cgroup == false) {
		if ((err = cgroup_create_cgroup(m_cgroup, 0))) {
			dprintf(D_PROCFAMILY,
				"Unable to create cgroup %s for ProcFamily %u."
				"  Cgroup functionality will not work: %s\n",
				m_cgroup_string, m_root_pid, cgroup_strerror(err));
		} else {
			m_created_cgroup = true;
		}
	} else if ((has_cgroup == true) && (changed_cgroup == true) && (err = cgroup_modify_cgroup(m_cgroup))) {
		dprintf(D_PROCFAMILY,
			"Unable to modify cgroup %s for ProcFamily %u."
			"  Some cgroup functionality may not work: %u %s\n",
			m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
	}

	// Make sure hierarchical memory accounting is turned on.
	struct cgroup_controller * mem_controller = cgroup_get_controller(m_cgroup, MEMORY_CONTROLLER);
	if (m_cgroup_memory_mounted && m_created_cgroup && (mem_controller != NULL)) {
		if ((err = cgroup_add_value_bool(mem_controller, "memory.use_hierarchy", true))) {
			dprintf(D_PROCFAMILY,
				"Unable to set hierarchical memory settings for %s (ProcFamily) %u: %u %s\n",
				m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
		} else {
			if ((err = cgroup_modify_cgroup(m_cgroup))) {
				dprintf(D_PROCFAMILY,
					"Unable to enable hierarchical memory accounting for %s "
					"(ProcFamily %u): %u %s\n",
					m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			}
		}
	}

	// Now that we have a cgroup, let's move all the existing processes to it
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		migrate_to_cgroup(member->get_proc_info()->pid);
		member = member->m_next;
	}

	return 0;
}

int
ProcFamily::freezer_cgroup(const char * state)
{
	// According to kernel docs, freezer will either succeed
	// or return EBUSY in the errno.
	//
	// This function either returns 0 (success), a positive value (fatal error)
	// or -EBUSY.
	int err = 0;
	struct cgroup_controller* freezer;
	struct cgroup *cgroup = cgroup_new_cgroup(m_cgroup_string);
	ASSERT (cgroup != NULL);

	if (!m_cgroup_freezer_mounted) {
		err = 1;
		goto ret;
	}

	freezer = cgroup_add_controller(cgroup, FREEZE_CONTROLLER);
	if (NULL == freezer) {
		dprintf(D_ALWAYS,
			"Unable to access the freezer subsystem for ProcFamily %u "
			"for cgroup %s\n",
			m_root_pid, m_cgroup_string);
		err = 2;
		goto ret;
	}

	if ((err = cgroup_add_value_string(freezer, "freezer.state", state))) {
		dprintf(D_ALWAYS,
			"Unable to write %s to freezer for cgroup %s (ProcFamily %u). %u %s\n",
			state, m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
		err = 3;
		goto ret;
	}
	if ((err = cgroup_modify_cgroup(cgroup))) {
		if (ECGROUPVALUENOTEXIST == err) {
			dprintf(D_ALWAYS,
				"Does not appear condor_procd is allowed to freeze"
				" cgroup %s (ProcFamily %u).\n",
				m_cgroup_string, m_root_pid);
		} else if ((ECGOTHER == err) && (EBUSY == cgroup_get_last_errno())) {
			dprintf(D_ALWAYS, "Kernel was unable to freeze cgroup %s "
				"(ProcFamily %u) due to process state; signal delivery "
				"won't be atomic\n", m_cgroup_string, m_root_pid);
			err = -EBUSY;
		} else {
			dprintf(D_ALWAYS,
				"Unable to commit freezer change %s for cgroup %s (ProcFamily %u). %u %s\n",
				state, m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
		}
		err = 4;
		goto ret;
	}

	ret:
	cgroup_free(&cgroup);
	return err;
}

int
ProcFamily::spree_cgroup(int sig)
{
	// The general idea here is we freeze the cgroup, give the signal,
	// then thaw everything out.  This way, signals are given in an atomic manner.
	//
	// Note that if the FREEZE call could be attempted, but not 100% completed, we
	// proceed anyway.

	int err = freezer_cgroup(FROZEN);
	if ((err != 0) && (err != -EBUSY)) {
		return err;
	}

	ASSERT (m_cgroup != NULL);
	cgroup_get_cgroup(m_cgroup);

	void **handle = (void **)malloc(sizeof(void*));
	ASSERT (handle != NULL);
	pid_t pid;
	ASSERT (m_cgroup_string != NULL);
	err = cgroup_get_task_begin(m_cgroup_string, FREEZE_CONTROLLER, handle, &pid);
	if ((err > 0) && (err != ECGEOF))
		handle = NULL;
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_ALWAYS,
				"Unable to iterate through cgroup %s (ProcFamily %u): %u %s\n",
				m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			goto release;
		}
		send_signal(pid, sig);
		err = cgroup_get_task_next(handle, &pid);
	}
	err = 0;

	release:
	if (handle != NULL) {
		cgroup_get_task_end(handle);
		free(handle);
	}

	freezer_cgroup(THAWED);

	return err;
}

int
ProcFamily::count_tasks_cgroup()
{
	if (!m_cgroup_cpuacct_mounted)
		return -1;

	int tasks = 0, err = 0;
	pid_t pid;
	void **handle = (void **)malloc(sizeof(void*));
	ASSERT (handle != NULL)
	*handle = NULL;

	ASSERT (m_cgroup_string != NULL);
	err = cgroup_get_task_begin(m_cgroup_string, CPUACCT_CONTROLLER, handle, &pid);
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_PROCFAMILY,
				"Unable to read cgroup %s memory stats (ProcFamily %u): %u %s.\n",
				m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			break;
		}
		tasks ++;
		err = cgroup_get_task_next(handle, &pid);
	}
	// Reset err to 0
	if (err == ECGEOF) {
		err = 0;
	}
	if (*handle) {
		cgroup_get_task_end(handle);
	}
	if (handle) {
		free(handle);
	}
	if (err) {
		return -err;
	}
	return tasks;

}

bool
inline
_check_stat_uint64(const struct cgroup_stat stats, const char* name, u_int64_t* result){
	u_int64_t tmp;
	if (0 == strcmp(name, stats.name)) {
		errno = 0;
		tmp = (u_int64_t)strtoll(stats.value, NULL, 0);
		if (errno == 0) {
			*result = tmp;
			return true;
		} else {
			dprintf(D_PROCFAMILY,
				"Invalid cgroup stat %s for %s.\n",
				stats.value, name);
			return false;
		}
	}
	return false;
}

void
ProcFamily::update_max_image_size_cgroup()
{
	if (!m_cgroup_memory_mounted) {
		return;
	}

	int err;
	u_int64_t max_image;
	struct cgroup *memcg;
	struct cgroup_controller *memct;
	if ((memcg = cgroup_new_cgroup(m_cgroup_string)) == NULL) {
		dprintf(D_PROCFAMILY,
			"Unable to allocate cgroup %s (ProcFamily %u).\n",
			m_cgroup_string, m_root_pid);
		return;
	}
	if ((err = cgroup_get_cgroup(memcg))) {
		dprintf(D_PROCFAMILY,
			"Unable to load cgroup %s (ProcFamily %u).\n",
			m_cgroup_string, m_root_pid);
		cgroup_free(&memcg);
		return;
	}
	if ((memct = cgroup_get_controller(memcg, MEMORY_CONTROLLER)) == NULL) {
		dprintf(D_PROCFAMILY,
			"Unable to load memory controller for cgroup %s (ProcFamily %u).\n",
			m_cgroup_string, m_root_pid);
		cgroup_free(&memcg);
		return;
	}
	if ((err = cgroup_get_value_uint64(memct, "memory.memsw.max_usage_in_bytes", &max_image))) {
		dprintf(D_PROCFAMILY,
			"Unable to load max memory usage for cgroup %s (ProcFamily %u): %u %s\n",
			m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
		cgroup_free(&memcg);
		return;
	}
	m_max_image_size = max_image/1024;
	cgroup_free(&memcg);
}

int
ProcFamily::aggregate_usage_cgroup_blockio(ProcFamilyUsage* usage)
{

	if (!m_cgroup_block_mounted)
		return 1;

	int ret;
	void *handle;
	char line_contents[BLOCK_STATS_LINE_MAX], sep[]=" ", *tok_handle, *word, *info[3];
	char blkio_stats_name[] = "blkio.io_service_bytes";
	short ctr;
	long int read_bytes=0, write_bytes=0;
	ret = cgroup_read_value_begin(BLOCK_CONTROLLER, m_cgroup_string,
	                              blkio_stats_name, &handle, line_contents, BLOCK_STATS_LINE_MAX);
	while (ret == 0) {
		ctr = 0;
		word = strtok_r(line_contents, sep, &tok_handle);
		while (word && ctr < 3) {
			info[ctr++] = word;
			word = strtok_r(NULL, sep, &tok_handle);
		}
		if (ctr == 3) {
			errno = 0;
			long ctrval = strtol(info[2], NULL, 10);
			if (errno) {
				dprintf(D_FULLDEBUG, "Error parsing kernel value to a long: %s; %s\n",
					 info[2], strerror(errno));
				break;
			}
			if (strcmp(info[1], "Read") == 0) {
				read_bytes += ctrval;
			} else if (strcmp(info[1], "Write") == 0) {
				write_bytes += ctrval;
			}
		}
		ret = cgroup_read_value_next(&handle, line_contents, BLOCK_STATS_LINE_MAX);
	}
	if (ret != ECGEOF) {
		dprintf(D_ALWAYS, "Internal cgroup error when retrieving block statistics: %s\n", cgroup_strerror(ret));
		return 1;
	}

	usage->block_read_bytes = read_bytes;
	usage->block_write_bytes = write_bytes;

	return 0;
}

int
ProcFamily::aggregate_usage_cgroup(ProcFamilyUsage* usage)
{
	if (!m_cgroup_memory_mounted || !m_cgroup_cpuacct_mounted) {
		return -1;
	}

	int err;
	struct cgroup_stat stats;
	void **handle;
	u_int64_t tmp = 0, image = 0;
	bool found_rss;

	// Update memory
	handle = (void **)malloc(sizeof(void*));
	ASSERT (handle != NULL);
	*handle = NULL;

	err = cgroup_read_stats_begin(MEMORY_CONTROLLER, m_cgroup_string, handle, &stats);
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_PROCFAMILY,
				"Unable to read cgroup %s memory stats (ProcFamily %u): %u %s.\n",
				m_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			break;
		}
		if (_check_stat_uint64(stats, "total_rss", &tmp)) {
			image += tmp;
			usage->total_resident_set_size = tmp/1024;
			found_rss = true;
		} else if (_check_stat_uint64(stats, "total_mapped_file", &tmp)) {
			image += tmp;
		} else if (_check_stat_uint64(stats, "total_swap", &tmp)) {
			image += tmp;
		}
		err = cgroup_read_stats_next(handle, &stats);
	}
	if (*handle != NULL) {
		cgroup_read_stats_end(handle);
	}
	if (found_rss) {
		usage->total_image_size = image/1024;
	} else {
		dprintf(D_PROCFAMILY,
			"Unable to find all necesary memory structures for cgroup %s"
			" (ProcFamily %u)\n",
			m_cgroup_string, m_root_pid);
	}
	// The poor man's way of updating the max image size.
	if (image > m_max_image_size) {
		m_max_image_size = image/1024;
	}
	// Try updating the max size using cgroups
	update_max_image_size_cgroup();

	// Update CPU
	*handle = NULL;
	err = cgroup_read_stats_begin(CPUACCT_CONTROLLER, m_cgroup_string, handle, &stats);
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_PROCFAMILY,
				"Unable to read cgroup %s cpuacct stats (ProcFamily %u): %s.\n",
				m_cgroup_string, m_root_pid, cgroup_strerror(err));
			break;
		}
		if (_check_stat_uint64(stats, "user", &tmp)) {
			usage->user_cpu_time = tmp/clock_tick;
		} else if (_check_stat_uint64(stats, "system", &tmp)) {
			usage->sys_cpu_time = tmp/clock_tick;
		}
		err = cgroup_read_stats_next(handle, &stats);
	}
	if (*handle != NULL) {
		cgroup_read_stats_end(handle);
	}
	free(handle);

	aggregate_usage_cgroup_blockio(usage);

	// Finally, update the list of tasks
	if ((err = count_tasks_cgroup()) < 0) {
		return -err;
	} else {
		usage->num_procs = err;
	}
	return 0;
}
#endif

unsigned long
ProcFamily::update_max_image_size(unsigned long children_imgsize)
{
	// add image sizes from our processes to the total image size from
	// our child families
	//
	unsigned long imgsize = children_imgsize;
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
#if defined(WIN32)
		// comment copied from the older process tracking logic
		// (before the ProcD):
		//    On Win32, the imgsize from ProcInfo returns exactly
		//    what it says.... this means we get all the bytes mapped
		//    into the process image, incl all the DLLs. This means
		//    a little program returns at least 15+ megs. The ProcInfo
		//    rssize is much closer to what the TaskManager reports,
		//    which makes more sense for now.
		imgsize += member->m_proc_info->rssize;
#else
		imgsize += member->m_proc_info->imgsize;
#endif
		member = member->m_next;
	}

	// update m_max_image_size if we have a new max
	//
	if (imgsize > m_max_image_size) {
		m_max_image_size = imgsize;
	}

	// finally, return our _current_ total image size
	//
	return imgsize;
}

void
ProcFamily::aggregate_usage(ProcFamilyUsage* usage)
{
	ASSERT(usage != NULL);

	// factor in usage from processes that are still alive
	//
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {

		// cpu
		//
		usage->user_cpu_time += member->m_proc_info->user_time;
		usage->sys_cpu_time += member->m_proc_info->sys_time;
		usage->percent_cpu += member->m_proc_info->cpuusage;

		// current total image size
		//
		usage->total_image_size += member->m_proc_info->imgsize;
		usage->total_resident_set_size += member->m_proc_info->rssize;
#if HAVE_PSS
		if( member->m_proc_info->pssize_available ) {
			usage->total_proportional_set_size_available = true;
			usage->total_proportional_set_size += member->m_proc_info->pssize;
		}
#endif

		// number of alive processes
		//
		usage->num_procs++;

		member = member->m_next;
	}

	// factor in CPU usage from processes that have exited
	//
	usage->user_cpu_time += m_exited_user_cpu_time;
	usage->sys_cpu_time += m_exited_sys_cpu_time;

#if defined(HAVE_EXT_LIBCGROUP)
        if (m_cgroup != NULL) {
		aggregate_usage_cgroup(usage);
	}
#endif

}

void
ProcFamily::signal_root(int sig)
{
#if !defined(WIN32)
	if (m_proxy != NULL) {
		glexec_kill(m_proxy, m_root_pid, sig);
		return;
	}
#endif
	send_signal(m_root_pid, sig);
}

void
ProcFamily::spree(int sig)
{
#if defined(HAVE_EXT_LIBCGROUP)
	if ((NULL != m_cgroup_string) && (0 == spree_cgroup(sig))) {
		return;
	}
#endif

	ProcFamilyMember* member;
	for (member = m_member_list; member != NULL; member = member->m_next) {
#if !defined(WIN32)
		if (m_proxy != NULL) {
			glexec_kill(m_proxy,
			            member->m_proc_info->pid,
			            sig);
			continue;
		}
#endif
		send_signal(member->m_proc_info->pid, sig);
	}
}

void
ProcFamily::add_member(procInfo* pi)
{
	ProcFamilyMember* member = new ProcFamilyMember;
	member->m_family = this;
	member->m_proc_info = pi;
	member->m_still_alive = false;

	// add it to our list
	//
	member->m_prev = NULL;
	member->m_next = m_member_list;
	if (m_member_list != NULL) {
		m_member_list->m_prev = member;
	}
	m_member_list = member;

#if defined(HAVE_EXT_LIBCGROUP)
	// Add to the associated cgroup
	if (m_cgroup_string != NULL) {
		migrate_to_cgroup(pi->pid);
	}
#endif

	// keep our monitor's hash table up to date!
	m_monitor->add_member(member);
}

void
ProcFamily::remove_exited_processes()
{
	// our monitor will have marked the m_still_alive field of all
	// processes that are still alive on the system, so all remaining
	// processes have exited and need to be removed
	//
	ProcFamilyMember* member = m_member_list;
	ProcFamilyMember* prev = NULL;
	ProcFamilyMember** prev_ptr = &m_member_list;
	while (member != NULL) {
	
		ProcFamilyMember* next_member = member->m_next;

		if (!member->m_still_alive) {

			// HACK for logging: if our root pid is 0, we
			// know this to mean that we are actually the
			// family that holds all processes that aren't
			// in the monitored family; this hack should go
			// away when we pull out a separate ProcGroup
			// class
			//
			if (m_root_pid != 0) {
				dprintf(D_ALWAYS,
				        "process %u (of family %u) has exited\n",
				        member->m_proc_info->pid,
				        m_root_pid);
			}
			else {
				dprintf(D_ALWAYS,
				        "process %u (not in monitored family) has exited\n",
				        member->m_proc_info->pid);
			}

			// save CPU usage from this process
			//
			m_exited_user_cpu_time +=
				member->m_proc_info->user_time;
			m_exited_sys_cpu_time +=
				member->m_proc_info->sys_time;

			// keep our monitor's hash table up to date!
			//
			m_monitor->remove_member(member);

			// remove this member from our list and free up our data
			// structures
			//
			*prev_ptr = next_member;
			if (next_member != NULL) {
				next_member->m_prev = prev;
			}
			//dprintf(D_ALWAYS,
			//        "PROCINFO DEALLOCATION: %p for pid %u\n",
			//        member->m_proc_info,
			//        member->m_proc_info->pid);
			delete member->m_proc_info;
			delete member;
		}
		else {

			// clear still_alive bit for next time around
			//
			member->m_still_alive = false;

			// update our "previous" list data to point to
			// the current member
			//
			prev = member;
			prev_ptr = &member->m_next;
		}	

		// advance to the next member in the list
		//
		member = next_member;
	}
}

void
ProcFamily::fold_into_parent(ProcFamily* parent)
{
	// fold in CPU usage info from our dead processes
	//
	parent->m_exited_user_cpu_time += m_exited_user_cpu_time;
	parent->m_exited_sys_cpu_time += m_exited_sys_cpu_time;

	// nothing left to do if our member list is empty
	//
	if (m_member_list == NULL) {
		return;
	}

	// traverse our list, pointing all members at their
	// new family
	//
	ProcFamilyMember* member = m_member_list;
	while (member->m_next != NULL) {
		member->m_family = parent;
		member = member->m_next;
	}
	member->m_family = parent;

	// attach the end of our list to the beginning of
	// the parent's
	//
	member->m_next = parent->m_member_list;
	if (member->m_next != NULL) {
		member->m_next->m_prev = member;
	}

	// make our beginning the beginning of the parent's
	// list, and then make our list empty
	//
	parent->m_member_list = m_member_list;
	m_member_list = NULL;
}

#if !defined(WIN32)
void
ProcFamily::set_proxy(char* proxy)
{
	if (m_proxy != NULL) {
		free(m_proxy);
	}
	m_proxy = strdup(proxy);
	ASSERT(m_proxy != NULL);
}
#endif

void
ProcFamily::dump(ProcFamilyDump& fam)
{
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		ProcFamilyProcessDump proc;
		proc.pid = member->m_proc_info->pid;
		proc.ppid = member->m_proc_info->ppid;
		proc.birthday = member->m_proc_info->birthday;
		proc.user_time = member->m_proc_info->user_time;
		proc.sys_time = member->m_proc_info->sys_time;
		fam.procs.push_back(proc);
		member = member->m_next;
	}
}
