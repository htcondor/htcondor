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

LocalClient::LocalClient(const char* pipe_addr) :
	m_pipe(INVALID_HANDLE_VALUE)
{
	m_pipe_addr = strdup(pipe_addr);
	ASSERT(m_pipe_addr != NULL);
}

LocalClient::~LocalClient()
{
	free(m_pipe_addr);
}

void
LocalClient::start_connection(void* buffer, int len)
{
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
			EXCEPT("CreateFile error: %u", error);
		}
		if (WaitNamedPipe(m_pipe_addr, NMPWAIT_WAIT_FOREVER) == FALSE) {
			EXCEPT("WaitNamedPipe error: %u", GetLastError());
		}
	}

	DWORD bytes;
	if (WriteFile(m_pipe, buffer, len, &bytes, NULL) == FALSE) {
		EXCEPT("WriteFile error: %u", GetLastError());
	}
	ASSERT(bytes == len);
}

void
LocalClient::end_connection()
{
	CloseHandle(m_pipe);
}

void
LocalClient::read_data(void* buffer, int len)
{
	DWORD bytes;
	if (ReadFile(m_pipe, buffer, len, &bytes, NULL) == FALSE) {
		EXCEPT("ReadFile error: %u", GetLastError());
	}
	ASSERT(bytes == len);
}
