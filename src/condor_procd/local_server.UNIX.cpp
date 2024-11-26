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


#include "condor_common.h"
#include "condor_debug.h"
#include "local_server.h"
#include "named_pipe_watchdog_server.unix.h"
#include "named_pipe_reader.unix.h"
#include "named_pipe_writer.unix.h"
#include "named_pipe_util.unix.h"

LocalServer::LocalServer() :
	m_initialized(false),
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
		if (chown(m_reader->get_path(), client_uid, (gid_t)-1) == -1) {
			dprintf(D_ALWAYS,
			        "LocalServer: chown error on %s: %s\n",
			        m_reader->get_path(),
			        strerror(errno));
			return false;
		}
		if (chown(m_watchdog_server->get_path(), client_uid, (gid_t)-1) == -1) {
			dprintf(D_ALWAYS,
			        "LocalServer: chown error on %s: %s\n",
			        m_watchdog_server->get_path(),
			        strerror(errno));
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
LocalServer::accept_connection(time_t timeout, bool &accepted)
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
	char* client_addr = named_pipe_make_client_addr(m_reader->get_path(),
	                                                client_pid,
	                                                client_sn);
	if (!m_writer->initialize(client_addr)) {
		delete[] client_addr;
		delete m_writer;
		m_writer = NULL;

			// We can get here if the client got killed before we
			// connected.  This should not be a fatal exception.  We
			// used to return false, which was treated as fatal.

		accepted = false;
		return true;
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

void
LocalServer::touch()
{
	if (utimes(m_reader->get_path(), NULL) == -1) {
		dprintf(D_ALWAYS,
		        "LocalServer: utimes error on %s: %s\n",
		        m_reader->get_path(),
		        strerror(errno));
	}
	if (utimes(m_watchdog_server->get_path(), NULL) == -1) {
		dprintf(D_ALWAYS,
		        "LocalServer: utimes error on %s: %s\n",
		        m_watchdog_server->get_path(),
		        strerror(errno));
	}
}

bool LocalServer::consistent(void)
{
	ASSERT(m_reader != NULL);

	return m_reader->consistent();
}




