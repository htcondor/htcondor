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
#include "local_client.h"

LocalClient::LocalClient() :
	m_initialized(false),
	m_pipe_addr(NULL),
	m_pipe(INVALID_HANDLE_VALUE)
{
}

bool
LocalClient::initialize(const char* pipe_addr)
{
	ASSERT(!m_initialized);

	m_pipe_addr = strdup(pipe_addr);
	ASSERT(m_pipe_addr != NULL);

	m_initialized = true;
	return true;
}

LocalClient::~LocalClient()
{
	if (!m_initialized) {
		return;
	}

	if (m_pipe != INVALID_HANDLE_VALUE) {
		CloseHandle(m_pipe);
	}

	free(m_pipe_addr);
}

bool
LocalClient::start_connection(void* buffer, int len)
{
	ASSERT(m_initialized);

	ASSERT(m_pipe == INVALID_HANDLE_VALUE);
	while (true) {
		m_pipe = CreateFile(m_pipe_addr,                   // path to pipe
		                    GENERIC_READ | GENERIC_WRITE,  // read-write access
		                    0,                             // no sharing
	                        NULL,                          // default security
	                        OPEN_EXISTING,                 // fail if not there
	                        0,                             // default attributes
		                    NULL);                         // no template file
		if (m_pipe != INVALID_HANDLE_VALUE) {
			break;
		}
		DWORD error = GetLastError();
		if (error != ERROR_PIPE_BUSY) {
			dprintf(D_ALWAYS, "CreateFile error: %u\n", error);
			return false;
		}
		if (WaitNamedPipe(m_pipe_addr, NMPWAIT_WAIT_FOREVER) == FALSE) {
			dprintf(D_ALWAYS, "WaitNamedPipe error: %u\n", GetLastError());
			return false;
		}
	}

	DWORD bytes;
	if (WriteFile(m_pipe, buffer, len, &bytes, NULL) == FALSE) {
		dprintf(D_ALWAYS, "WriteFile error: %u\n", GetLastError());
		CloseHandle(m_pipe);
		m_pipe = INVALID_HANDLE_VALUE;
		return false;
	}
	ASSERT(bytes == len);

	return true;
}

void
LocalClient::end_connection()
{
	ASSERT(m_initialized);

	ASSERT(m_pipe != INVALID_HANDLE_VALUE);
	CloseHandle(m_pipe);
	m_pipe = INVALID_HANDLE_VALUE;
}

bool
LocalClient::read_data(void* buffer, int len)
{
	ASSERT(m_initialized);

	DWORD bytes;
	if (ReadFile(m_pipe, buffer, len, &bytes, NULL) == FALSE) {
		dprintf(D_ALWAYS, "ReadFile error: %u\n", GetLastError());
		return false;
	}
	ASSERT(bytes == len);

	return true;
}
