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

#ifndef _PROC_FAMILY_CLIENT_H
#define _PROC_FAMILY_CLIENT_H

#include "proc_family_io.h"
#include "../condor_procapi/procapi.h"

class LocalClient;

class ProcFamilyClient {

public:

	ProcFamilyClient() : m_initialized(false), m_client(NULL) { }

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	bool initialize(const char*);

	~ProcFamilyClient();

	// tell the procd to start tracking a new subfamily
	//
	bool register_subfamily(pid_t root_pid,
	                        pid_t watcher_pid,
	                        int   max_snapshot_interval,
	                        bool& response);

	// tell ProcD to track a family via environment variables
	//
	bool track_family_via_environment(pid_t pid, PidEnvID& penvid, bool&);

	// tell ProcD to track a family via login
	//
	bool track_family_via_login(pid_t pid, const char* login, bool&);

#if defined(LINUX)
	// tell ProcD to track a family via supplementary group id
	//
	bool track_family_via_supplementary_group(pid_t pid, bool&, gid_t&);
#endif

	// ask the procd for usage information about a process
	// family
	//
	bool get_usage(pid_t, ProcFamilyUsage&, bool&);

	// tell the procd to send a signal to a single process
	//
	bool signal_process(pid_t, int, bool&);

	// tell the procd to suspend a family tree
	//
	bool suspend_family(pid_t, bool&);

	// tell the procd to continue a suspended family tree
	//
	bool continue_family(pid_t, bool&);

	// tell the procd to kill an entire family (and all
	// subfamilies of that family)
	//
	bool kill_family(pid_t, bool&);

	// tell the procd we don't care about this family any
	// more
	//
	bool unregister_family(pid_t, bool&);

	// tell the procd to take a snapshot
	//
	bool snapshot(bool&);

	// tell the procd to exit
	//
	bool quit(bool&);

#if defined(PROCD_DEBUG)
	// tell the procd to dump out its state, then return our
	// LocalClient object so that a tester can read the state
	// and compare it against reality. the caller must call
	// end_connection on this object once it's done
	//
	LocalClient* dump(pid_t);
#endif

private:

	// common code for killing, suspending, and
	// continuing a family
	//
	bool signal_family(pid_t, proc_family_command_t, bool&);

	// set true once we're properly initialized
	//
	bool m_initialized;

	// object for managing our connection to the ProcD
	//
	LocalClient* m_client;
};

#endif
