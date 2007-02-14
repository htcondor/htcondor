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

#ifndef _PROC_FAMILY_SERVER_H
#define _PROC_FAMILY_SERVER_H

#include "proc_family_io.h"
#include "proc_family_monitor.h"
#include "local_server.h"

class ProcFamilyServer {

public:
	// construct a new ProcFamilyServer instance with the given monitor
	// back end and the given pipe "address"
	//
	ProcFamilyServer(ProcFamilyMonitor&, const char*);

	// set the client principal who will be able to connect to this server
	//
	void set_client_principal(char* p) { m_server.set_client_principal(p); }

	// the server wait loop
	//
	void wait_loop();

private:
	// handle a "register subfamily" command
	//
	void register_subfamily();

	// handle a "get family usage" command
	//
	void get_usage();

	// handle a "signal process" command
	//
	void signal_process();

	// handle a "suspend family" command
	//
	void suspend_family();

	// handle a "continue family" command
	//
	void continue_family();

	// handle a "kill family" command
	//
	void kill_family();

	// handle a "unregister family" command
	//
	void unregister_family();

	// handle a "snapshot" command
	//
	void snapshot();

#if defined(PROCD_DEBUG)
	// handle a "dump" command
	//
	void dump();
#endif

	// handle a "quit" command
	//
	void quit();

	// our monitor
	//
	ProcFamilyMonitor& m_monitor;

	// our secure local "server port"
	//
	LocalServer m_server;
};

#endif
