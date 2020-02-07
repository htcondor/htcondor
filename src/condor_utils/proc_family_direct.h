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


#ifndef _PROC_FAMILY_DIRECT_H
#define _PROC_FAMILY_DIRECT_H

#include "proc_family_interface.h"
#include "HashTable.h"

class KillFamily;
struct ProcFamilyDirectContainer;

class ProcFamilyDirect : public ProcFamilyInterface {

public:

	//constructor and destructor
	ProcFamilyDirect();
	~ProcFamilyDirect();

#if !defined(WIN32)
	// on UNIX, the registration logic should be
	// called from the parent for this class
	//
	bool register_from_child() { return false; }
#endif

	bool register_subfamily(pid_t,
	                        pid_t,
	                        int);

	bool track_family_via_environment(pid_t, PidEnvID&);
	bool track_family_via_login(pid_t, const char*);

#if defined(LINUX)
	// this class doesn't support tracking via supplementary
	// group
	//
	bool track_family_via_allocated_supplementary_group(pid_t, gid_t&) { return false; }
#endif

#if defined(HAVE_EXT_LIBCGROUP)
	// This class doesn't support cgroups
	bool track_family_via_cgroup(pid_t, const char*) { return false; }
#endif

	bool get_usage(pid_t, ProcFamilyUsage&, bool);

	bool signal_process(pid_t, int);

	bool suspend_family(pid_t);

	bool continue_family(pid_t);

	bool kill_family(pid_t);
	
	bool unregister_family(pid_t);

	// this class currently doesn't support GLEXEC_JOB
	//
	bool use_glexec_for_family(pid_t, const char*) { return false; }

	bool quit(void(*)(void*, int, int),void*) { return false; }; // nothing to do here, only needed when there is a procd

private:

	HashTable<pid_t, ProcFamilyDirectContainer*> m_table;

	KillFamily* lookup(pid_t);
};

#endif
