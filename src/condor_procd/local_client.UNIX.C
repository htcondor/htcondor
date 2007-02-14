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
	m_writer(server_addr)
{
	// make sure that each time this process instantiates another local
	// client object, a different serial number is used
	//
	m_serial_number = m_next_serial_number;
	m_next_serial_number++;

	m_pid = getpid();

	char* my_addr = named_pipe_make_addr(server_addr,
	                                     m_pid,
	                                     m_serial_number);
	m_reader = new NamedPipeReader(my_addr);
	ASSERT(m_reader != NULL);
	delete[] my_addr;
}

LocalClient::~LocalClient()
{
	delete m_reader;
}

void
LocalClient::start_connection(void* buffer, int len)
{
	// we need to write our pid and serial number, followed by the given
	// payload. the trick here is that this write needs to be atomic so
	// that requests aren't interleaved when the server reads them. thus,
	// we use a gather-write
	//
	struct iovec vector[3];

	// set up iovec structs for the pid, the serial number, and the payload
	//
	vector[0].iov_base = &m_pid;
	vector[0].iov_len = sizeof(pid_t);
	vector[1].iov_base = &m_serial_number;
	vector[1].iov_len = sizeof(int);
	vector[2].iov_base = buffer;
	vector[2].iov_len = len;

	// finally, call out to the named pipe writer
	//
	m_writer.write_data_vector(vector, 3);
}

void
LocalClient::end_connection()
{
}

void
LocalClient::read_data(void* buffer, int len)
{
	m_reader->read_data(buffer, len);
}
