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
#include "local_client.h"
#include "named_pipe_util.h"

int LocalClient::m_next_serial_number = 0;

LocalClient::LocalClient(const char* server_addr) :
	m_writer(server_addr),
	m_reader(NULL)
{
	// make sure that each time this process instantiates another local
	// client object, a different serial number is used
	//
	m_serial_number = m_next_serial_number;
	m_next_serial_number++;

	m_pid = getpid();

	m_addr = named_pipe_make_addr(server_addr,
	                              m_pid,
	                              m_serial_number);
}

LocalClient::~LocalClient()
{
	delete[] m_addr;

	if (m_reader != NULL) {
		delete m_reader;
	}
}

void
LocalClient::start_connection(void* payload_buf, int payload_len)
{
	// create a named pipe reader to receive the response
	//
	m_reader = new NamedPipeReader(m_addr);
	ASSERT(m_reader != NULL);

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
	m_writer.write_data(msg_buf, msg_len);

	delete[] msg_buf;
}

void
LocalClient::end_connection()
{
	ASSERT(m_reader != NULL);
	delete m_reader;
	m_reader = NULL;
}

void
LocalClient::read_data(void* buffer, int len)
{
	m_reader->read_data(buffer, len);
}
