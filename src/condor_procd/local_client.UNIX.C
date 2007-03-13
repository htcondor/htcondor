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
LocalClient::start_connection(void* buffer, int len)
{
	// create a named pipe reader to receive the response
	//
	m_reader = new NamedPipeReader(m_addr);
	ASSERT(m_reader != NULL);

	// we need to write our pid and serial number, followed by the given
	// payload. the trick here is that this write needs to be atomic so
	// that requests aren't interleaved when the server reads them. thus,
	// we use a gather-write
	//
	struct iovec vector[3];

	// set up iovec structs for the pid, the serial number, and the payload
	//
	// memcpy is used to set the iov_base because its type varies.  on some
	// platforms it is (void*) and on some it is (char*).
	//
	void *tmpptr;

	tmpptr = &m_pid;
	memcpy(&(vector[0].iov_base), &tmpptr, sizeof(void*));
	vector[0].iov_len = sizeof(pid_t);

	tmpptr = &m_serial_number;
	memcpy(&(vector[1].iov_base), &tmpptr, sizeof(void*));
	vector[1].iov_len = sizeof(int);

	tmpptr = buffer;
	memcpy(&(vector[2].iov_base), &tmpptr, sizeof(void*));
	vector[2].iov_len = len;

	// finally, call out to the named pipe writer
	//
	m_writer.write_data_vector(vector, 3);
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
