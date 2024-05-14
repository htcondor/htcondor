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


#ifndef _PROC_FAMILY_INTERFACE_H
#define _PROC_FAMILY_INTERFACE_H

#include "../condor_procapi/procapi.h"
#include "../condor_procd/proc_family_io.h"

class ProcFamilyClient;
struct FamilyInfo;

class ProcFamilyInterface {

public:

	static ProcFamilyInterface* create(FamilyInfo *fi, const char* subsys);

	virtual ~ProcFamilyInterface() { }

#if !defined(WIN32)
	// on UNIX, depending on the ProcFamily implementation, we
	// may need to call register_subfamily from the child or the
	// parent. this method indicates to the caller which to use
	//
	virtual bool register_from_child() = 0;
#endif
	
	virtual bool register_subfamily(pid_t,
	                                pid_t,
	                                int) = 0;

	virtual bool track_family_via_environment(pid_t, PidEnvID&) = 0;

	virtual bool track_family_via_login(pid_t, const char*) = 0;

#if defined(LINUX)
	virtual bool track_family_via_allocated_supplementary_group(pid_t, gid_t&) = 0;

	virtual bool track_family_via_cgroup(pid_t, FamilyInfo *) = 0;
#endif
	virtual void assign_cgroup_for_pid(pid_t, const std::string &){}

	virtual bool get_usage(pid_t, ProcFamilyUsage&, bool) = 0;

	virtual bool signal_process(pid_t, int) = 0;

	virtual bool suspend_family(pid_t) = 0;

	virtual bool continue_family(pid_t) = 0;

	virtual bool kill_family(pid_t) = 0;
	virtual bool extend_family_lifetime(pid_t) { return true;}
	
	// Really should be named unregister_subfamily...
	virtual bool unregister_family(pid_t) = 0;

	// Have we seen an oom kill event, only implemented
	// for cgroups
	virtual bool has_been_oom_killed(pid_t) { return false;} // meaning "don't know for sure"
																 //
	// call prior to destroying the ProcFamily class to insure that cleanup happens before we exit.
	virtual bool quit(void(*notify)(void*me, int pid, int status),void*me) = 0;
};

#endif
