/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _PROC_FAMILY_INTERFACE_H
#define _PROC_FAMILY_INTERFACE_H

#include "../condor_procapi/procapi.h"
#include "../condor_procd/proc_family_io.h"

class ProcFamilyClient;

class ProcFamilyInterface {

public:

	static ProcFamilyInterface* create(char* subsys);

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
	virtual bool track_family_via_supplementary_group(pid_t, gid_t&) = 0;
#endif

	virtual bool get_usage(pid_t, ProcFamilyUsage&, bool) = 0;

	virtual bool signal_process(pid_t, int) = 0;

	virtual bool suspend_family(pid_t) = 0;

	virtual bool continue_family(pid_t) = 0;

	virtual bool kill_family(pid_t) = 0;

	virtual bool unregister_family(pid_t) = 0;

};

#endif
