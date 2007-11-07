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
