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

#include "condor_common.h"
#include "condor_debug.h"
#include "local_server.h"
#include "named_pipe_watchdog_server.h"
#include "named_pipe_reader.h"
#include "named_pipe_writer.h"
#include "named_pipe_util.h"

LocalServer::LocalServer() :
	m_initialized(false),
	m_pipe_addr(NULL),
	m_watchdog_server(NULL),
	m_reader(NULL),
	m_writer(NULL)
{
}

bool
LocalServer::initialize(const char* pipe_addr)
{
	// now create a watchdog server; this allows clients
	// to detect when we crash and avoid getting stuck trying
	// to talk to us
	//
	char* watchdog_addr = named_pipe_make_watchdog_addr(pipe_addr);
	m_watchdog_server = new NamedPipeWatchdogServer;
	ASSERT(m_watchdog_server != NULL);
	bool ok = m_watchdog_server->initialize(watchdog_addr);
	delete[] watchdog_addr;
	if (!ok) {
		delete m_watchdog_server;
		m_watchdog_server = NULL;
		return false;
	}

	// now create our reader for getting client requests
	//
	m_reader = new NamedPipeReader;
	ASSERT(m_reader != NULL);
	if (!m_reader->initialize(pipe_addr)) {
		delete m_watchdog_server;
		m_watchdog_server = NULL;
		delete m_reader;
		m_reader = NULL;
		return false;
	}

	// and stash a copy of our address, since its used as a
	// component in the addresses of our clients
	//
	m_pipe_addr = strdup(pipe_addr);
	ASSERT(m_pipe_addr != NULL);

	m_initialized = true;
	return true;
}

LocalServer::~LocalServer()
{
	// nothing to do if we're not initialized
	//
	if (!m_initialized) {
		return;
	}

	free(m_pipe_addr);

	// it's important to delete the watchdog server after
	// the reader
	//
	delete m_reader;
	delete m_watchdog_server;
}

bool
LocalServer::set_client_principal(const char* uid_str)
{
	ASSERT(m_initialized);

	uid_t my_uid = geteuid();

	// determine what the client UID should be. if we were called
	// with a NULL argument and our effective ID is root, we'll use
	// our real UID - this will allow us to support being called as
	// a setuid root binary
	//
	uid_t client_uid;
	if (uid_str != NULL) {
		client_uid = atoi(uid_str);
	}
	else if (my_uid == 0) {
		client_uid = getuid();
	}
	else {
		client_uid = my_uid;
	}

	// if the client UID is the same is our effective UID, then the
	// named pipes will already have the correct ownership
	//
	if (client_uid == my_uid) {
		return true;
	}

	// if we're root, we can chown the named pipes to the client
	// so they can connect
	//
	if (my_uid == 0) {
		if (!m_reader->change_owner(client_uid)) {
			dprintf(D_ALWAYS,
			        "LocalServer: error changing ownership on command pipe\n");
			return false;
		}
		if (!m_watchdog_server->change_owner(client_uid)) {
			dprintf(D_ALWAYS,
			        "LocalServer: error changing ownership on watchdog pipe\n");
			return false;
		}
		return true;
	}

	// if we got here, we're stuck
	//
	dprintf(D_ALWAYS,
	        "running as UID %u; can't allow connections from UID %u\n",
	        my_uid,
	        client_uid);
	return false;
}

bool
LocalServer::accept_connection(int timeout, bool &accepted)
{
	ASSERT(m_initialized);

	// at this point, we should not have a writer (since we
	// start without one and end_connection will have deleted
	// any previous one)
	//
	ASSERT(m_writer == NULL);

	// see if a connection arrives within the timeout period
	//
	bool ready;
	if (!m_reader->poll(timeout, ready)) {
		return false;
	}
	if (!ready) {
		accepted = false;
		return true;
	}

	// pipe is ready for reading; first grab the client's pid
	// and serial number off the pipe
	//
	pid_t client_pid;
	if (!m_reader->read_data(&client_pid, sizeof(pid_t))) {
		dprintf(D_ALWAYS,
		        "LocalServer: read of client PID failed\n");
		return false;
	}
	int client_sn;
	if (!m_reader->read_data(&client_sn, sizeof(int))) {
		dprintf(D_ALWAYS,
		        "LocalServer: read of client SN failed\n");
		return false;
	}

	// instantiate the NamedPipeWriter for responding to the
	// client
	//
	m_writer = new NamedPipeWriter;
	ASSERT(m_writer != NULL);
	char* client_addr = named_pipe_make_client_addr(m_pipe_addr,
	                                                client_pid,
	                                                client_sn);
	if (!m_writer->initialize(client_addr)) {
		delete[] client_addr;
		return false;
	}
	delete[] client_addr;

	accepted = true;
	return true;
}

bool
LocalServer::close_connection()
{
	ASSERT(m_initialized);

	// at this point, we should have a writer from the previous
	// call to accept_connection
	//
	ASSERT(m_writer != NULL);

	delete m_writer;
	m_writer = NULL;

	return true;
}

bool
LocalServer::read_data(void* buffer, int len)
{
	ASSERT(m_writer != NULL);
	return m_reader->read_data(buffer, len);
}

bool
LocalServer::write_data(void* buffer, int len)
{
	ASSERT(m_writer != NULL);
	return m_writer->write_data(buffer, len);
}
