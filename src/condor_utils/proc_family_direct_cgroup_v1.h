/***************************************************************
 *
 * Copyright (C) 1990-2023, Condor Team, Computer Sciences Department,
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


#ifndef _PROC_FAMILY_DIRECT_CGROUP_V1_H
#define _PROC_FAMILY_DIRECT_CGROUP_V1_H

#include <string>
#include "proc_family_interface.h"

// Ths class manages sets of Linux processes with cgroups.
// This is efficient, so we do it in the caller's process,
// not via the procd.  
//
// Originally, cgroup v1 was managed in the procd using
// the libcgroup library.  This library was abandoned,
// and only available on platforms that support cgroup v1
// natively.  Sometimes, we run on platforms like el9
// that don't support cgroup v1, and thus don't have a
// libcgroup avaible, but we are running inside a container,
// and the *host's* cgroup version is what matters.  In 
// this case we need to manipulate cgroupv1.
//
// So, we do it ourselves now, via raw writes to /sys/fs/cgropu
// in the same way we did via the procd.  See the 
// proc_family_direct_cgroup_v1 header file for more info
// about the hierarchy

class ProcFamilyDirectCgroupV1 : public ProcFamilyInterface {

public:

	ProcFamilyDirectCgroupV1() = default;
	virtual ~ProcFamilyDirectCgroupV1() = default;
	
	bool register_subfamily(pid_t pid, pid_t /*ppid*/, int /*snapshot_interval*/) {
		family_root_pid = pid;
		start_time = time(nullptr);
		return true;
	}

	// Tell DaemonCore to call register_subfamily
	// from the child. 
	bool register_from_child() { return true; }
	void assign_cgroup_for_pid(pid_t pid, const std::string &cgroup_name);

	bool register_subfamily_before_fork(FamilyInfo *fi);

	// This is the way.  The only way.

	// As we don't get the requested cgroup name in register, this method
	// actually makes the cgroup, if need be.
	bool track_family_via_cgroup(pid_t pid, FamilyInfo *fi);

	bool get_usage(pid_t, ProcFamilyUsage&, bool);

	// Despite the name, send the signal to everyone in the family
	bool signal_process(pid_t, int);

	bool suspend_family(pid_t);

	bool continue_family(pid_t);

	bool kill_family(pid_t);
	
	// Note this isn't called in the starter, as DaemonCore calls
	// it after calling the Reaper, and the starter exits in the
	// job reaper
	// Also, it should be called unregister_subfamily
	//
	bool unregister_family(pid_t);

	bool quit(void(*)(void*, int, int),void*) { return false; }; // nothing to do here, only needed when there is a procd

	// Have we seen an oom kill event in this cgroup;
	bool has_been_oom_killed(pid_t pid);

	// We don't need these, cgroups just works
	bool track_family_via_environment(pid_t, PidEnvID&) {return true;}
	bool track_family_via_login(pid_t, const char*) {return true;}
	bool track_family_via_allocated_supplementary_group(pid_t, gid_t&) { return true; }

	// Returns true if cgroup v1 is mounted and we aren't
	// in "unified mode"
	static bool has_cgroup_v1();
	
	// Returns true if cgroup v1 is mounted and we can write to it
	static bool can_create_cgroup_v1(std::string &cgroup);
private:

	bool cgroupify_myself(const std::string &cgroup_name);

	time_t start_time;
	pid_t family_root_pid;
	uint64_t cgroup_memory_limit;
	int cgroup_cpu_shares;
	std::vector<dev_t> cgroup_hide_devices;
};

#endif
