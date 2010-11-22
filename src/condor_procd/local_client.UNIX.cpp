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
#include "local_client.h"
#include "named_pipe_writer.unix.h"
#include "named_pipe_reader.unix.h"
#include "named_pipe_watchdog.unix.h"
#include "named_pipe_util.unix.h"

int LocalClient::s_next_serial_number = 0;

LocalClient::LocalClient() :
	m_initialized(false),
	m_serial_number(-1),
	m_pid(0),
	m_addr(NULL),
	m_writer(NULL),
	m_reader(NULL),
	m_watchdog(NULL)
{
}

bool
LocalClient::initialize(const char* server_addr)
{
	ASSERT(!m_initialized);

	// first, connect to the server's "watchdog server" pipe
	//
	char* watchdog_addr = named_pipe_make_watchdog_addr(server_addr);
	m_watchdog = new NamedPipeWatchdog;
	ASSERT(m_watchdog != NULL);
	bool ok = m_watchdog->initialize(watchdog_addr);
	delete[] watchdog_addr;
	if (!ok) {
		delete m_watchdog;
		m_watchdog = NULL;
		return false;
	}

	// now, connect to the server's "command" pipe
	//
	m_writer = new NamedPipeWriter;
	ASSERT(m_writer != NULL);
	if (!m_writer->initialize(server_addr)) {
		delete m_writer;
		m_writer = NULL;
		delete m_watchdog;
		m_watchdog = NULL;
		return false;
	}
	m_writer->set_watchdog(m_watchdog);

	// make sure that each time this process instantiates another local
	// client object, a different serial number is used
	//
	m_serial_number = s_next_serial_number;
	s_next_serial_number++;

	// stash our pid, since we send it to the server with each command
	// so it knows how to contact us
	//
	m_pid = getpid();

	// store the address that we'll use to receive responses from
	// the server
	//
	m_addr = named_pipe_make_client_addr(server_addr,
	                                     m_pid,
	                                     m_serial_number);

	m_initialized = true;
	return true;
}

LocalClient::~LocalClient()
{
	// nothing to do if we're not initialized
	//
	if (!m_initialized) {
		return;
	}

	delete[] m_addr;

	if (m_reader != NULL) {
		delete m_reader;
	}

	delete m_writer;
	delete m_watchdog;
}

bool
LocalClient::start_connection(void* payload_buf, int payload_len)
{
	ASSERT(m_initialized);

	// create a named pipe reader to receive the response
	//
	m_reader = new NamedPipeReader;
	ASSERT(m_reader != NULL);
	if (!m_reader->initialize(m_addr)) {
		dprintf(D_ALWAYS,
		        "LocalClient: error initializing NamedPipeReader\n");
		delete m_reader;
		m_reader = NULL;
		return false;
	}
	m_reader->set_watchdog(m_watchdog);

	// we need to write our pid and serial number, followed by the given
	// payload. the trick here is that this write needs to be atomic so
	// that requests aren't interleaved when the server reads them. thus,
	// we use a single write

	// copy into the write buffer:
	//   1) our pid
	//   2) our serial number
	//   3) the payload
	//
	int msg_len = sizeof(pid_t) + sizeof(int) + payload_len;
	char* msg_buf = new char[msg_len];
	ASSERT(msg_buf != NULL);
	char* ptr = msg_buf;
	memcpy(ptr, &m_pid, sizeof(pid_t));
	ptr += sizeof(pid_t);
	memcpy(ptr, &m_serial_number, sizeof(int));
	ptr += sizeof(int);
	memcpy(ptr, payload_buf, payload_len);

	// now call out to the named pipe writer
	//
	if (!m_writer->write_data(msg_buf, msg_len)) {
		dprintf(D_ALWAYS,
		        "LocalClient: error sending message to server\n");
		delete[] msg_buf;
		return false;
	}
	delete[] msg_buf;

	return true;
}

void
LocalClient::end_connection()
{
	ASSERT(m_initialized);

	ASSERT(m_reader != NULL);
	delete m_reader;
	m_reader = NULL;
}

bool
LocalClient::read_data(void* buffer, int len)
{
	ASSERT(m_initialized);
	return m_reader->read_data(buffer, len);
}
