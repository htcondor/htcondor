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

#if defined(HAVE_EXT_LIBCGROUP)
#include "libcgroup.h"

#define FROZEN "FROZEN"
#define THAWED "THAWED"
#define BLOCK_STATS_LINE_MAX 64

#include <unistd.h>
long ProcFamily::clock_tick = sysconf( _SC_CLK_TCK );

// Swap accounting is sometimes turned off.  We use this variable so we
// warn about that situation only once.
bool ProcFamily::have_warned_about_memsw = false;
#endif

ProcFamily::ProcFamily(ProcFamilyMonitor* monitor,
                       pid_t              root_pid,
                       birthday_t         root_birthday,
                       pid_t              watcher_pid,
                       int                max_snapshot_interval) :
	m_monitor(monitor)
	, m_root_pid(root_pid)
	, m_root_birthday(root_birthday)
	, m_watcher_pid(watcher_pid)
	, m_max_snapshot_interval(max_snapshot_interval)
	, m_exited_user_cpu_time(0)
	, m_exited_sys_cpu_time(0)
	, m_max_image_size(0)
	, m_member_list(NULL)
#ifdef LINUX
	, m_perf_counter(root_pid)
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	, m_cgroup_string("")
	, m_cm(CgroupManager::getInstance())
	, m_initial_user_cpu(0)
	, m_initial_sys_cpu(0)
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	, m_last_signal_was_sigstop(false)
#endif
{
#ifdef LINUX
	m_perf_counter.start();
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
}

#if defined(HAVE_EXT_LIBCGROUP)

int
ProcFamily::migrate_to_cgroup(pid_t pid)
{
	// Attempt to migrate a given process to a cgroup.
	// This can be done without regards to whether the
	// process is already in the cgroup
	if (!m_cgroup.isValid()) {
		return 1;
	}

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
	if (m_cm.isMounted(CgroupManager::MEMORY_CONTROLLER) && (err = cgroup_get_current_controller_path(pid, MEMORY_CONTROLLER_STR, &orig_cgroup_string))) {
		dprintf(D_PROCFAMILY,
			"Unable to determine current memory cgroup for PID %u (ProcFamily %u): %u %s\n",
			pid, m_root_pid, err, cgroup_strerror(err));
		return 1;
	}
	// We will migrate the PID to the new cgroup even if it is in the proper memory controller cgroup
	// It is possible for the task to be in multiple cgroups.
	if (m_cm.isMounted(CgroupManager::MEMORY_CONTROLLER) && (orig_cgroup_string != NULL) && (strcmp(m_cgroup_string.c_str(), orig_cgroup_string))) {
		// Yes, there are race conditions here - can't really avoid this.
		// Throughout this block, we can assume memory controller exists.
		// Get original value of migrate.
		orig_cgroup = cgroup_new_cgroup(orig_cgroup_string);
		ASSERT (orig_cgroup != NULL);
		if ((err = cgroup_get_cgroup(orig_cgroup))) {
			dprintf(D_PROCFAMILY,
				"Unable to read original cgroup %s (ProcFamily %u): %u %s\n",
				orig_cgroup_string, m_root_pid, err, cgroup_strerror(err));
			cgroup_free(&orig_cgroup);
			goto after_migrate;
		}
		if ((memory_controller = cgroup_get_controller(orig_cgroup, MEMORY_CONTROLLER_STR)) == NULL) {
			cgroup_free(&orig_cgroup);
			goto after_migrate;
		}
		if ((err = cgroup_get_value_uint64(memory_controller, "memory.move_charge_at_immigrate", &orig_migrate))) {
			if (err == ECGROUPVALUENOTEXIST) {
				// Older kernels don't have the ability to migrate memory accounting to the new cgroup.
				dprintf(D_PROCFAMILY,
					"This kernel does not support memory usage migration; cgroup %s memory statistics"
					" will be slightly incorrect (ProcFamily %u)\n",
					m_cgroup_string.c_str(), m_root_pid);
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
			cgroup_free(&orig_cgroup);
			orig_cgroup = cgroup_new_cgroup(orig_cgroup_string);
			memory_controller = cgroup_add_controller(orig_cgroup, MEMORY_CONTROLLER_STR);
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
	err = cgroup_attach_task_pid(& const_cast<struct cgroup &>(m_cgroup.getCgroup()), pid);
	if (err) {
		dprintf(D_PROCFAMILY,
			"Cannot attach pid %u to cgroup %s for ProcFamily %u: %u %s\n",
			pid, m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
	}

	if (changed_orig) {
		if ((orig_cgroup = cgroup_new_cgroup(orig_cgroup_string)) == NULL) {
			goto after_restore;
		}
		if (((memory_controller = cgroup_add_controller(orig_cgroup, MEMORY_CONTROLLER_STR)) != NULL) &&
			(!cgroup_add_value_uint64(memory_controller, "memory.move_charge_at_immigrate", orig_migrate))) {
				if ((err = cgroup_modify_cgroup(orig_cgroup))) {
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


after_restore:
	if (orig_cgroup_string != NULL) {
		free(orig_cgroup_string);
	}
	return err;
}

int
ProcFamily::set_cgroup(const std::string &cgroup_string)
{
	if (cgroup_string == "/") {
		dprintf(D_ALWAYS,
			"Cowardly refusing to monitor the root cgroup out "
			"of security concerns.\n");
		return 1;
	}

	// Ignore this command if we've done this before.
	if (m_cgroup.isValid()) {
		if (cgroup_string == m_cgroup.getCgroupString()) {
			return 0;
		} else {
			m_cgroup.destroy();
		}
	}

	dprintf(D_PROCFAMILY, "Setting cgroup to %s for ProcFamily %u.\n",
		cgroup_string.c_str(), m_root_pid);

	m_cm.create(cgroup_string, m_cgroup, CgroupManager::ALL_CONTROLLERS, CgroupManager::NO_CONTROLLERS);
	m_cgroup_string = m_cgroup.getCgroupString();

	if (!m_cgroup.isValid()) {
		return 1;
	}

	// Now that we have a cgroup, let's move all the existing processes to it
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		migrate_to_cgroup(member->get_proc_info()->pid);
		member = member->m_next;
	}

	// Record the amount of pre-existing CPU usage here.
	m_initial_user_cpu = 0;
	m_initial_sys_cpu = 0;
	get_cpu_usage_cgroup(m_initial_user_cpu, m_initial_sys_cpu);

	// Reset block IO controller
	if (m_cm.isMounted(CgroupManager::BLOCK_CONTROLLER)) {
		struct cgroup *tmp_cgroup = cgroup_new_cgroup(m_cgroup_string.c_str());
		struct cgroup_controller *blkio_controller = cgroup_add_controller(tmp_cgroup, BLOCK_CONTROLLER_STR);
		ASSERT (blkio_controller != NULL); // Block IO controller should already exist.
		cgroup_add_value_uint64(blkio_controller, "blkio.reset_stats", 0);
		int err;
		if ((err = cgroup_modify_cgroup(tmp_cgroup))) {
			// Not allowed to reset stats?
			dprintf(D_ALWAYS,
				"Unable to reset cgroup %s block IO statistics. "
				"Some block IO accounting will be inaccurate (ProcFamily %u): %u %s\n",
				m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
		}
		cgroup_free(&tmp_cgroup);
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
	struct cgroup *cgroup = cgroup_new_cgroup(m_cgroup_string.c_str());
	ASSERT (cgroup != NULL);

	if (!m_cm.isMounted(CgroupManager::FREEZE_CONTROLLER)) {
		err = 1;
		goto ret;
	}

	freezer = cgroup_add_controller(cgroup, FREEZE_CONTROLLER_STR);
	if (NULL == freezer) {
		dprintf(D_ALWAYS,
			"Unable to access the freezer subsystem for ProcFamily %u "
			"for cgroup %s\n",
			m_root_pid, m_cgroup_string.c_str());
		err = 2;
		goto ret;
	}

	if ((err = cgroup_add_value_string(freezer, "freezer.state", state))) {
		dprintf(D_ALWAYS,
			"Unable to write %s to freezer for cgroup %s (ProcFamily %u). %u %s\n",
			state, m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
		err = 3;
		goto ret;
	}
	if ((err = cgroup_modify_cgroup(cgroup))) {
		if (ECGROUPVALUENOTEXIST == err) {
			dprintf(D_ALWAYS,
				"Does not appear condor_procd is allowed to freeze"
				" cgroup %s (ProcFamily %u).\n",
				m_cgroup_string.c_str(), m_root_pid);
		} else if ((ECGOTHER == err) && (EBUSY == cgroup_get_last_errno())) {
			dprintf(D_ALWAYS, "Kernel was unable to freeze cgroup %s "
				"(ProcFamily %u) due to process state; signal delivery "
				"won't be atomic\n", m_cgroup_string.c_str(), m_root_pid);
			err = -EBUSY;
			goto ret;
		} else {
			dprintf(D_ALWAYS,
				"Unable to commit freezer change %s for cgroup %s (ProcFamily %u). %u %s\n",
				state, m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
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

	bool use_freezer = !m_last_signal_was_sigstop;
	m_last_signal_was_sigstop = sig == SIGSTOP ? true : false;
	if (!use_freezer) {
		dprintf(D_ALWAYS,
			"Not using freezer controller to send signal; last "
			"signal was SIGSTOP.\n");
	} else {
		dprintf(D_FULLDEBUG,
			"Using freezer controller to send signal to process family.\n");
	}

	int err = use_freezer ? freezer_cgroup(FROZEN) : 0;
	if ((err != 0) && (err != -EBUSY)) {
		return err;
	}

	ASSERT (m_cgroup.isValid());
	cgroup_get_cgroup(&const_cast<struct cgroup&>(m_cgroup.getCgroup()));

	void **handle = (void **)malloc(sizeof(void*));
	ASSERT (handle != NULL);
	pid_t pid;
	err = cgroup_get_task_begin(m_cgroup_string.c_str(), FREEZE_CONTROLLER_STR, handle, &pid);
	if ((err > 0) && (err != ECGEOF))
		handle = NULL;
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_ALWAYS,
				"Unable to iterate through cgroup %s (ProcFamily %u): %u %s\n",
				m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
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

	if (use_freezer) freezer_cgroup(THAWED);

	return err;
}

int
ProcFamily::count_tasks_cgroup()
{
	if (!m_cm.isMounted(CgroupManager::CPUACCT_CONTROLLER) || !m_cgroup.isValid()) {
		return -1;
	}

	int tasks = 0, err = 0;
	pid_t pid;
	void **handle = (void **)malloc(sizeof(void*));
	ASSERT (handle != NULL)
	*handle = NULL;

	err = cgroup_get_task_begin(m_cgroup_string.c_str(), CPUACCT_CONTROLLER_STR, handle, &pid);
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_PROCFAMILY,
				"Unable to read cgroup %s memory stats (ProcFamily %u): %u %s.\n",
				m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
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

	free(handle);

	if (err) {
		return -err;
	}
	return tasks;

}

bool
_check_stat_uint64(const struct cgroup_stat &stats, const char* name, u_int64_t* result){
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

int
ProcFamily::aggregate_usage_cgroup_blockio(ProcFamilyUsage* usage)
{

	if (!m_cm.isMounted(CgroupManager::BLOCK_CONTROLLER) || !m_cgroup.isValid())
		return 1;

	int ret;
	void *handle;
	char line_contents[BLOCK_STATS_LINE_MAX], sep[]=" ", *tok_handle, *word, *info[3];
	char blkio_stats_name[] = "blkio.throttle.io_service_bytes";
	short ctr;
	int64_t read_bytes=0, write_bytes=0;
	ret = cgroup_read_value_begin(BLOCK_CONTROLLER_STR, m_cgroup_string.c_str(),
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
			int64_t ctrval = strtoll(info[2], NULL, 10);
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
	if (handle != NULL) {
		cgroup_read_value_end(&handle);
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
ProcFamily::aggregate_usage_cgroup_blockio_io_serviced(ProcFamilyUsage* usage)
{

	if (!m_cm.isMounted(CgroupManager::BLOCK_CONTROLLER) || !m_cgroup.isValid())
		return 1;

	int ret;
	void *handle;
	char line_contents[BLOCK_STATS_LINE_MAX], sep[]=" ", *tok_handle, *word, *info[3];
	char blkio_stats_name[] = "blkio.throttle.io_serviced";
	short ctr;
	int64_t reads=0, writes=0;
	ret = cgroup_read_value_begin(BLOCK_CONTROLLER_STR, m_cgroup_string.c_str(),
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
			int64_t ctrval = strtoll(info[2], NULL, 10);
			if (errno) {
				dprintf(D_FULLDEBUG, "Error parsing kernel value to a long: %s; %s\n",
					 info[2], strerror(errno));
				break;
			}
			if (strcmp(info[1], "Read") == 0) {
				reads += ctrval;
			} else if (strcmp(info[1], "Write") == 0) {
				writes += ctrval;
			}
		}
		ret = cgroup_read_value_next(&handle, line_contents, BLOCK_STATS_LINE_MAX);
	}
	if (handle != NULL) {
		cgroup_read_value_end(&handle);
	}

	if (ret != ECGEOF) {
		dprintf(D_ALWAYS, "Internal cgroup error when retrieving block statistics: %s\n", cgroup_strerror(ret));
		return 1;
	}

	usage->block_reads = reads;
	usage->block_writes = writes;

	return 0;
}

int
ProcFamily::aggregate_usage_cgroup_io_wait(ProcFamilyUsage* usage) {
	if (!m_cm.isMounted(CgroupManager::BLOCK_CONTROLLER) || !m_cgroup.isValid())
		return 1;

	int ret;
	void *handle;
	char line_contents[BLOCK_STATS_LINE_MAX], sep[]=" ", *tok_handle, *word, *info[3];
	char blkio_stats_name[] = "blkio.io_wait_time";
	short ctr;
	int64_t total = 0;
	ret = cgroup_read_value_begin(BLOCK_CONTROLLER_STR, m_cgroup_string.c_str(),
	                              blkio_stats_name, &handle, line_contents, BLOCK_STATS_LINE_MAX);
	while (ret == 0) {
		ctr = 0;
		word = strtok_r(line_contents, sep, &tok_handle);
		while (word && ctr < 3) {
			info[ctr++] = word;
			word = strtok_r(NULL, sep, &tok_handle);
		}
		if (ctr == 2) {
			errno = 0;
			int64_t ctrval = strtoll(info[1], NULL, 10);
			if (errno) {
				dprintf(D_FULLDEBUG, "Error parsing kernel value to a long: %s; %s\n",
					 info[1], strerror(errno));
				break;
			}
			if (strcmp(info[0], "Total") == 0) {
				total = ctrval;
			} 
		}
		ret = cgroup_read_value_next(&handle, line_contents, BLOCK_STATS_LINE_MAX);
	}
	if (handle != NULL) {
		cgroup_read_value_end(&handle);
	}

	// kernels with BFQ enabled don't have io_wait_time, don't spam logs if it isn't here
	if ((ret != ECGEOF) && (ret != ENOENT)) {
		dprintf(D_ALWAYS, "Internal cgroup error when retrieving iowait statistics: %s\n", cgroup_strerror(ret));
		return 1;
	}

	usage->io_wait = total / 1.e9;

	return 0;
}

int ProcFamily::get_cpu_usage_cgroup(long &user_time, long &sys_time) {

	if (!m_cm.isMounted(CgroupManager::CPUACCT_CONTROLLER)) {
		return 1;
	}

	void * handle = NULL;
	u_int64_t tmp = 0;
	struct cgroup_stat stats;
	int err = cgroup_read_stats_begin(CPUACCT_CONTROLLER_STR, m_cgroup_string.c_str(), &handle, &stats);
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_PROCFAMILY,
				"Unable to read cgroup %s cpuacct stats (ProcFamily %u): %s.\n",
				m_cgroup_string.c_str(), m_root_pid, cgroup_strerror(err));
			break;
		}
		if (_check_stat_uint64(stats, "user", &tmp)) {
			user_time = tmp/clock_tick-m_initial_user_cpu;
		} else if (_check_stat_uint64(stats, "system", &tmp)) {
			sys_time = tmp/clock_tick-m_initial_sys_cpu;
		}
			err = cgroup_read_stats_next(&handle, &stats);
	}
	if (handle != NULL) {
		cgroup_read_stats_end(&handle);
	}
	if (err != ECGEOF) {
		dprintf(D_ALWAYS, "Internal cgroup error when retrieving CPU statistics: %s\n", cgroup_strerror(err));
		return 1;
	}
	return 0;
}

int
ProcFamily::aggregate_usage_cgroup(ProcFamilyUsage* usage)
{
	if (!m_cm.isMounted(CgroupManager::MEMORY_CONTROLLER) || !m_cm.isMounted(CgroupManager::CPUACCT_CONTROLLER) 
			|| !m_cgroup.isValid()) {
		return -1;
	}

	int err;
	struct cgroup_stat stats;
	void *handle = NULL;
	u_int64_t tmp = 0, image = 0;
	bool found_rss = false;

	// Update memory

	err = cgroup_read_stats_begin(MEMORY_CONTROLLER_STR, m_cgroup_string.c_str(), &handle, &stats);
	while (err != ECGEOF) {
		if (err > 0) {
			dprintf(D_PROCFAMILY,
				"Unable to read cgroup %s memory stats (ProcFamily %u): %u %s.\n",
				m_cgroup_string.c_str(), m_root_pid, err, cgroup_strerror(err));
			break;
		}
		if (_check_stat_uint64(stats, "total_rss", &tmp)) {
			image += tmp;
			usage->total_resident_set_size = tmp/1024;
			found_rss = true;
		} else if (_check_stat_uint64(stats, "total_mapped_file", &tmp)) {
			image += tmp;
		} else if (_check_stat_uint64(stats, "total_shmem", &tmp)) {
			image += tmp;
		} 
		err = cgroup_read_stats_next(&handle, &stats);
	}
	if (handle != NULL) {
		cgroup_read_stats_end(&handle);
	}
	if (found_rss) {
		usage->total_image_size = image/1024;
	} else {
		dprintf(D_PROCFAMILY,
			"Unable to find all necesary memory structures for cgroup %s"
			" (ProcFamily %u)\n",
			m_cgroup_string.c_str(), m_root_pid);
	}
	// The poor man's way of updating the max image size.
	if (image/1024 > m_max_image_size) {
		m_max_image_size = image/1024;
	}

	// Update CPU
	get_cpu_usage_cgroup(usage->user_cpu_time, usage->sys_cpu_time);

	aggregate_usage_cgroup_blockio(usage);
	aggregate_usage_cgroup_blockio_io_serviced(usage);
	aggregate_usage_cgroup_io_wait(usage);

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

		// PSS is special: it's expensive to calculate for every process,
		// so we calculate it on demand
		int status; // Is ignored
		int rc = ProcAPI::getPSSInfo(member->m_proc_info->pid, *(member->m_proc_info), status);
		if( (rc == PROCAPI_SUCCESS) && (member->m_proc_info->pssize_available) ) {
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

	// the perf counter itself measures for the whole tree, so we don't need to
#ifdef LINUX
	usage->m_instructions = m_perf_counter.getInsns();
#endif

#if defined(HAVE_EXT_LIBCGROUP)
	aggregate_usage_cgroup(usage);
#endif

}

void
ProcFamily::signal_root(int sig)
{
	send_signal(m_root_pid, sig);
}

void
ProcFamily::spree(int sig)
{
#if defined(HAVE_EXT_LIBCGROUP)
	if ((m_cgroup.isValid()) && (0 == spree_cgroup(sig))) {
		return;
	}
#endif

	ProcFamilyMember* member;
	for (member = m_member_list; member != NULL; member = member->m_next) {
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
	migrate_to_cgroup(pi->pid);
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
#if defined(CHATTY_PROC_LOG)
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
#endif /* defined(CHATTY_PROC_LOG) */
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
