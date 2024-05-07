/***************************************************************
 *
 * Copyright (C) 1990-2022, Condor Team, Computer Sciences Department,
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


#ifndef _PROC_FAMILY_DIRECT_CGROUP_V2_H
#define _PROC_FAMILY_DIRECT_CGROUP_V2_H

#include "proc_family_interface.h"

// Ths class manages sets of Linux processes with cgroups.
// This is efficient, so we do it in the caller's process,
// not via the procd.  
//
// This requires that any sub-families of this family
// are managed by cgroups and those cgroups are children
// of this one, so that we can measure the family tree
// in one cgroup call, as well as signal them all.

// ProcAPI -- a retro design doc
// The root class is ProcFamilyInterface.  It has a static factory
// method, ::create, which is called from daemon core::Create_Process
// to decide which of two concrete classes to instantiate, either
// ProcFamilyDirect or ProcFamilyProxy, the latter of which talks
// to the ProcD daemon.  This decision is done early, mainly
// looking at the USE_PROCD knob.
//
// Once DaemonCore has a ProcFamily[Direct|Proxy] instance
// it keeps it for the lifetime of the process.
//
// When a DaemonCore client wants to tracke a family of
// processes it creates, it passes in a familyInfo to 
// DC::Create_Process, and daemonCore calls first
// register_subfamily on the instance, then 
// track_family_via_[env|login|cgroup|cgroup].
//
// Later calls to get usage are keyed by pid, which is
// a bit of a problem.

class ProcFamilyDirectCgroupV2 : public ProcFamilyInterface {

public:

	ProcFamilyDirectCgroupV2() = default;
	virtual ~ProcFamilyDirectCgroupV2() = default;
	
	bool register_subfamily(pid_t pid, pid_t /*ppid*/, int /*snapshot_interval*/) {
		family_root_pid = pid;
		start_time = time(nullptr);
		return true;
	}

	// Tell DaemonCore to call register_subfamily
	// from the parent. Otherwise the state passed in is lost
	// to the parent by being set in the forked child.
	bool register_from_child() { return true; }

	// This is the way.  The only way.

	// As we don't get the requested cgroup name in register, this method
	// actually makes the cgroup, if need be.
	bool track_family_via_cgroup(pid_t pid, FamilyInfo *fi);
	void assign_cgroup_for_pid(pid_t pid, const std::string &cgroup_name);

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

	// Returns true if cgroup v2 is mounted and we aren't
	// in "unified mode"
	static bool has_cgroup_v2();
	
	// Returns true if cgroup v2 is mounted and we can write to it
	static bool can_create_cgroup_v2();
private:

	bool cgroupify_process(const std::string &cgroup_name, pid_t pid);

	time_t start_time;
	pid_t family_root_pid;
	uint64_t cgroup_memory_limit;
	uint64_t cgroup_memory_limit_low;
	uint64_t cgroup_memory_and_swap_limit;
	int cgroup_cpu_shares;
};

#endif
