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


#ifndef _PROC_FAMILY_H
#define _PROC_FAMILY_H

#include "../condor_procapi/procapi.h"
#include "proc_family_member.h"
#include "proc_family_io.h"

#if defined(HAVE_EXT_LIBCGROUP)
#include "../condor_starter.V6.1/cgroup.linux.h"
#endif

#ifdef LINUX
#include "../condor_utils/perf_counter.linux.h"
#endif

class ProcFamilyMonitor;

class ProcFamily {

friend class ProcFamilyMember;

public:
	// create a ProcFamily that's rooted at the process which has
	// the given ppid/birthday, has been registered by the given
	// ppid, requires the given maximum interval in between snapshots,
	// and that we'll optionally track via environment variables or
	// user login
	//
	ProcFamily(ProcFamilyMonitor* monitor,
	           pid_t              root_pid = 0,
	           birthday_t         root_birthday = 0,
	           pid_t              watcher_pid = 0,
	           int                max_snapshot_interval = -1);

	// cleanup time!
	//
	~ProcFamily();

	// accessor for the "root" process PID
	//
	pid_t get_root_pid() const { return m_root_pid; };

	// accessor for the "root" process birthday
	//
	birthday_t get_root_birthday() const { return m_root_birthday; }

	// accessor for the "watcher" process PID
	//
	pid_t get_watcher_pid() const { return m_watcher_pid; };

	// accessor for the requested maximum snapshot interval
	//
	int get_max_snapshot_interval() const { return m_max_snapshot_interval; }

	// since we maintain the tree of process families in
	// ProcFamilyMonitor, not here, we need help in maintaining the
	// maximum image size seen for our family of processes (in order
	// to take into account processes that are in child families).
	// this method will be called from ProcFamilyMonitor after each
	// snapshot. the parameter will contain the total image size from
	// processes in our child families. we'll update m_max_image_size
	// if needed and then return our total image size
	//
	unsigned long update_max_image_size(unsigned long children_imgsize);

	// return the maximum image size
	//
	unsigned long get_max_image_size() const { return m_max_image_size; }

	// fill in usage information about this family
	//
	void aggregate_usage(ProcFamilyUsage*);

	// send a signal to the root process in the family
	//
	void signal_root(int sig);

	// send a signal to all processes in the family
	//
	void spree(int sig);

	// add a new family member
	//
	void add_member(procInfo*);

	// deal with processes that are no longer on the system: account for
	// their resource usage, remove them from our bookkeeping data, and
	// free up their data structures
	//
	void remove_exited_processes();

	// our monitor is about to delete us, so we need to offload any
	// members we have in our list to our parent (passed in)
	//
	void fold_into_parent(ProcFamily*);

#if defined(HAVE_EXT_LIBCGROUP)
	// Set the cgroup to use for this family
	int set_cgroup(const std::string&); 
#endif

	// dump info about all processes in this family
	//
	void dump(ProcFamilyDump& fam);

private:
	// we need a pointer to the monitor that's tracking us since we
	// help maintain its hash table
	//
	ProcFamilyMonitor* m_monitor;

	// information about our "root" process
	//
	pid_t      m_root_pid;
	birthday_t m_root_birthday;

	// the pid of the process that has registered us (should be the parent
	// of our root process)
	//
	pid_t m_watcher_pid;

	// the maximum time in between snapshots. this will be honored by
	// higher layers in the procd.
	//
	int m_max_snapshot_interval;

	// CPU usage from exited processes
	//
	long m_exited_user_cpu_time;
	long m_exited_sys_cpu_time;

	// max total image size of all our family's (and child familes')
	// processes
	//
	unsigned long m_max_image_size;

	// lists of member processes; at the beginning of a snapshot (i.e.
	// when takesnapshot() in our containing ProcFamilyMonitor begins
	// executing, all out family members will be cotained in
	// m_member_list. the snapshot algorithm will call the still_alive()
	// method on any Member whose process is still alive on the system,
	// which will move the Member object over to m_alive_member_list.
	// finally, when remove_exited_processes() is called at the end of
	// takesnapshot(), any processes still in m_member_list will be
	// consdered to have exited
	//
	ProcFamilyMember* m_member_list;

#ifdef LINUX
	PerfCounter m_perf_counter;
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	Cgroup m_cgroup;
	std::string m_cgroup_string;
	CgroupManager &m_cm;
	static long clock_tick;
	static bool have_warned_about_memsw;
	// Sometimes Condor doesn't successfully clear out the cgroup from the
	// previous run.  Hence, we subtract off any CPU usage found at the
	// start of the job.
	long m_initial_user_cpu;
	long m_initial_sys_cpu;

	// See #3847; if the last signal we sent to the cgroup was SIGSTOP,
	// this flag will be true.  This avoids a Linux kernel panic.
	bool m_last_signal_was_sigstop;

	int count_tasks_cgroup();
	int aggregate_usage_cgroup_blockio(ProcFamilyUsage*);
	int aggregate_usage_cgroup_blockio_io_serviced(ProcFamilyUsage*);
	int aggregate_usage_cgroup_io_wait(ProcFamilyUsage*);
	int aggregate_usage_cgroup(ProcFamilyUsage*);
	int freezer_cgroup(const char *);
	int spree_cgroup(int);
	int migrate_to_cgroup(pid_t);
	int get_cpu_usage_cgroup(long &user_cpu, long &sys_cpu);
#endif
};

#endif
