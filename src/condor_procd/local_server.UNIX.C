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
#include "named_pipe_util.h"

LocalServer::LocalServer(const char* pipe_addr) :
	m_reader(pipe_addr),
	m_writer(NULL)
{
	m_pipe_addr = strdup(pipe_addr);
	ASSERT(m_pipe_addr != NULL);
}

LocalServer::~LocalServer()
{
	free(m_pipe_addr);
}

void
LocalServer::set_client_principal(char* uid_str)
{
	uid_t uid = atoi(uid_str);
	ASSERT(uid != 0);
	m_reader.change_owner(uid);
}

bool
LocalServer::accept_connection(int timeout)
{
	ASSERT(m_writer == NULL);

	// see if a connection arrives within the timeout period
	//
	if (!m_reader.poll(timeout)) {
		return false;
	}

	// pipe is ready for reading; first grab the client's pid
	// and serial number off the pipe
	//
	pid_t client_pid;
	m_reader.read_data(&client_pid, sizeof(pid_t));
	int client_sn;
	m_reader.read_data(&client_sn, sizeof(int));

	// instantiate the NamedPipeWriter for responding to the
	// client
	//
	char* client_addr = named_pipe_make_addr(m_pipe_addr,
	                                         client_pid,
	                                         client_sn);
	m_writer = new NamedPipeWriter(client_addr);
	ASSERT(m_writer != NULL);
	delete[] client_addr;

	return true;
}

void
LocalServer::close_connection()
{
	ASSERT(m_writer != NULL);
	delete m_writer;
	m_writer = NULL;
}

void
LocalServer::read_data(void* buffer, int len)
{
	ASSERT(m_writer != NULL);
	return m_reader.read_data(buffer, len);
}

void
LocalServer::write_data(void* buffer, int len)
{
	ASSERT(m_writer != NULL);
	m_writer->write_data(buffer, len);
}
