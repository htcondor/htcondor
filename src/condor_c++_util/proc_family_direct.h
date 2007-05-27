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

#ifndef _PROC_FAMILY_DIRECT_H
#define _PROC_FAMILY_DIRECT_H

#include "proc_family_interface.h"
#include "HashTable.h"

class KillFamily;
struct ProcFamilyDirectContainer;

class ProcFamilyDirect : public ProcFamilyInterface {

public:

	ProcFamilyDirect();

	// all the real register_subfamily work is done in the
	// parent when we are doing direct (as opposed to ProcD-
	// based) process family tracking
	//
	bool register_subfamily_parent(pid_t,
	                               pid_t,
	                               int,
	                               PidEnvID*,
	                               const char*);

	bool register_subfamily_child(pid_t,
	                              pid_t,
	                              int,
	                              PidEnvID*,
	                              const char*)
	{
		return true;
	}

	// on Windows, just call the function that does the real
	// work
	//
	bool register_subfamily(pid_t root,
	                        pid_t watcher,
	                        int snapshot_interval,
	                        PidEnvID* penvid,
	                        const char* login)
	{
		return register_subfamily_parent(root,
		                                 watcher,
		                                 snapshot_interval,
		                                 penvid,
		                                 login);
	}

	bool get_usage(pid_t, ProcFamilyUsage&, bool);

	bool signal_process(pid_t, int);

	bool suspend_family(pid_t);

	bool continue_family(pid_t);

	bool kill_family(pid_t);

	bool unregister_family(pid_t);

private:

	HashTable<pid_t, ProcFamilyDirectContainer*> m_table;

	KillFamily* lookup(pid_t);
};

#endif
