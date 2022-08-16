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


#ifndef _PROC_FAMILY_SERVER_H
#define _PROC_FAMILY_SERVER_H

#include "proc_family_io.h"
#include "proc_family_monitor.h"

class LocalServer;

class ProcFamilyServer {

public:
	// construct a new ProcFamilyServer instance with the given monitor
	// back end and the given pipe "address"
	//
	ProcFamilyServer(ProcFamilyMonitor&, const char*);

	// clean up
	//
	~ProcFamilyServer();

	// set the client principal who will be able to connect to this server
	//
	void set_client_principal(char*);

	// the server wait loop
	//
	void wait_loop();

private:

	// helper for reading from the connected client
	//
	void read_from_client(void*, int);

	// helper for writing to the connected client
	//
	void write_to_client(void*, int);

	// handlers for various commands
	//
	void register_subfamily();
	void track_family_via_environment();
	void track_family_via_login();
#if defined(LINUX)
	void track_family_via_allocated_supplementary_group();
	void track_family_via_associated_supplementary_group();
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	void track_family_via_cgroup();
#endif
	void get_usage();
	void signal_process();
	void suspend_family();
	void continue_family();
	void kill_family();
	void unregister_family();
	void snapshot();
	void quit();
	void dump();

	// our monitor
	//
	ProcFamilyMonitor& m_monitor;

	// our secure local "server port"
	//
	LocalServer* m_server;
};

#endif
