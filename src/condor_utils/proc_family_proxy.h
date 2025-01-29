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


#ifndef _PROC_FAMILY_PROXY_H
#define _PROC_FAMILY_PROXY_H

#include "proc_family_interface.h"

class ProcFamilyClient;

class ProcFamilyProxyReaperHelper;

class ProcFamilyProxy : public ProcFamilyInterface {

public:

	ProcFamilyProxy(const char* address_suffix = NULL);
	~ProcFamilyProxy();

#if !defined(WIN32)
	// all registration with the ProcD is done in the
	// child (so we can guarantee it hasn't exited yet)
	// when using this class on UNIX
	//
	bool register_from_child() { return true; }
#endif

	// tell the procd to start tracking a new subfamily
	//
	bool register_subfamily(pid_t,
	                        pid_t,
	                        int);

	// ask procd to use environment, login, or SGID
	// based tracking
	//
	bool track_family_via_environment(pid_t, PidEnvID&);
	bool track_family_via_login(pid_t, const char*);
#if defined(LINUX)
	bool track_family_via_allocated_supplementary_group(pid_t, gid_t&);
#endif
	bool track_family_via_cgroup(pid_t, FamilyInfo *fi);

	// ask the procd for usage information about a process
	// family
	//
	bool get_usage(pid_t, ProcFamilyUsage&, bool);

	// tell the procd to send a signal to a single process
	//
	bool signal_process(pid_t, int);

	// tell the procd to suspend a family tree
	//
	bool suspend_family(pid_t);

	// tell the procd to continue a suspended family tree
	//
	bool continue_family(pid_t);

	// tell the procd to kill an entire family (and all
	// subfamilies of that family)
	//
	bool kill_family(pid_t);
	
	// tell the procd to take a snapshot
	//
	bool snapshot();
	
	// tell the procd we don't care about this family any
	// more
	//
	bool unregister_family(pid_t);

	// called by the master prior to exiting to insure that the procd child process is reaped
	//
	bool quit(void(*notify)(void*me, int pid, int status),void*me);

	// reaper for the ProcD (called by our PrcFamilyProxyReaperHelper
	// member)
	//
	int procd_reaper(int, int);

private:

	// start a procd
	//
	bool start_procd();

	// tell the procd to exit, returns true if the command was successfully delivered
	//
	bool stop_procd();

	// recover from a ProcD problem; kill and restart it if we're
	// the one who launched it
	//
	void recover_from_procd_error();

	// static flag so that we can guarantee we are only
	// instantiated once
	//
	static int s_instantiated;

	// the address that our ProcD is using
	//
	std::string m_procd_addr;

	// the log file that our ProcD will use
	//
	std::string m_procd_log;

	// the ProcD's pid, if we started one and haven't told it to exit yet;
	// otherwise, -1
	//
	int m_procd_pid;

	// The pid of the ProcD we just told to exit.  used to we can ignore
	// a subsequent request to unregister the family of the procd pid
	//
	int m_former_procd_pid;

	// object for managing our connection to the ProcD
	//
	ProcFamilyClient* m_client;

	// helper object to assist DaemonCore in sending us reap events
	//
	ProcFamilyProxyReaperHelper* m_reaper_helper;

	// ID of reaper for the ProcD, if registered
	//
	int m_reaper_id;

	// Optional callback to let an external component know that the reaper fired
	//
	void(*m_reaper_notify)(void* me, int pid, int status);
	void* m_reaper_notify_me;
};

#endif
