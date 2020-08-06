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
#include "local_server.h"

LocalServer::LocalServer() :
	m_initialized(false),
	m_accept_overlapped(NULL)
{
}

bool
LocalServer::initialize(const char* pipe_addr)
{
	ASSERT(!m_initialized);
	ASSERT(pipe_addr != NULL);

	if( strlen(pipe_addr) >= 256 ) {
		dprintf( D_ALWAYS, "LocalServer::initialize(%s): pipe address is too long.\n", pipe_addr );
	}

	// TODO: create the named pipe with a security descriptor that denies
	// access to NT AUTHORITY\NETWORK and grants access to the "condor"
	// user
	//
	m_pipe =
		CreateNamedPipe(pipe_addr,                                  // name of the pipe
	                    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,  // allow two-way communication
	                    PIPE_TYPE_BYTE,                             // stream-oriented
	                    1,                                          // only one connection at a time
						4096,                                       // suggested output buffer size
						4096,                                       // suggested input buffer size
						0,                                          // WaitNamedPipe default timeout
						NULL);                                      // default security
	if (m_pipe == INVALID_HANDLE_VALUE) {
		dprintf(D_ALWAYS,
		        "CreateNamedPipe(%s) error: %u\n",
				pipe_addr,
		        GetLastError());
		return false;
	}

	m_event = CreateEvent(NULL,   // default security
	                      TRUE,   // manual reset
	                      TRUE,   // starts signalled
	                      NULL);  // no name
	if (m_event == NULL) {
		dprintf(D_ALWAYS,
		        "CreateEvent error: %u\n",
		        GetLastError());
		CloseHandle(m_pipe);
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

	CloseHandle(m_event);
	CloseHandle(m_pipe);
}

bool
LocalServer::set_client_principal(const char*)
{
	// TODO: add the condor account to the DACL
	// for the named pipe
	//
	return true;
}

bool
LocalServer::accept_connection(int timeout, bool& ready)
{
	// initiate a nonblocking "accept", if one isn't already pending
	//
	if (m_accept_overlapped == NULL) {

		m_accept_overlapped = new OVERLAPPED;
		ASSERT(m_accept_overlapped != NULL);
		m_accept_overlapped->Offset = 0;
		m_accept_overlapped->OffsetHigh = 0;
		m_accept_overlapped->hEvent = m_event;

		BOOL bresult = ConnectNamedPipe(m_pipe, m_accept_overlapped);
		DWORD error = GetLastError();

		// if ConnectNamedPipe returns TRUE or if GetLastError returns
		// ERROR_PIPE_CONNECTED, then we have a connection and can just
		// return
		//
		if ((bresult == TRUE) || (error == ERROR_PIPE_CONNECTED)) {
			delete m_accept_overlapped;
			m_accept_overlapped = NULL;
			ready = true;
			return true;
		}

		// ConnectNamedPipe did not return immediate success; make sure
		// the error code indicates a pending operation
		//
		if (error != ERROR_IO_PENDING) {
			dprintf(D_ALWAYS, "ConnectNamedPipe error: %u\n", error);
			return false;
		}
	}

	// wait up to timeout seconds for a client connection
	//
	DWORD result = WaitForSingleObject(m_event, timeout * 1000);
	if (result == WAIT_FAILED) {
		dprintf(D_ALWAYS, "WaitForSingleObject error: %u\n", GetLastError());
		return false;
	}
	if (result == WAIT_TIMEOUT) {
		ready = false;
		return true;
	}

	// a connection has been established; finalize the overlapped operation
	//
	ASSERT(result == WAIT_OBJECT_0);
	DWORD ignored;
	if (GetOverlappedResult(m_pipe, m_accept_overlapped, &ignored, TRUE) == FALSE) {
		dprintf(D_ALWAYS, "GetOverlappedResult error: %u\n", GetLastError());
		return false;
	}
	delete m_accept_overlapped;
	m_accept_overlapped = NULL;

	ready = true;
	return true;
}

bool
LocalServer::close_connection()
{
	if (FlushFileBuffers(m_pipe) == FALSE) {
		dprintf(D_ALWAYS, "FlushFileBuffers error: %u\n", GetLastError());
		return false;
	}
	if (DisconnectNamedPipe(m_pipe) == FALSE) {
		dprintf(D_ALWAYS, "DisconnectNamedPipe error: %u\n", GetLastError());
		return false;
	}
	return true;
}

bool
LocalServer::read_data(void* buffer, int len)
{
	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = 0;
	DWORD bytes;
	BOOL done = ReadFile(m_pipe, buffer, len, &bytes, &overlapped);
	if (done == FALSE) {
		DWORD error = GetLastError();
		if (error != ERROR_IO_PENDING) {
			dprintf(D_ALWAYS, "ReadFileError: %u\n", error);
			return false;
		}
		if (GetOverlappedResult(m_pipe, &overlapped, &bytes, TRUE) == FALSE) {
			dprintf(D_ALWAYS, "GetOverlappedResult error: %u\n", GetLastError());
			return false;
		}
	}
	ASSERT(bytes == len);

	return true;
}

bool
LocalServer::write_data(void* buffer, int len)
{
	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = 0;
	DWORD bytes;
	BOOL done = WriteFile(m_pipe, buffer, len, &bytes, &overlapped);
	if (done == FALSE) {
		DWORD error = GetLastError();
		if (error != ERROR_IO_PENDING) {
			dprintf(D_ALWAYS, "WriteFile error: %u\n", error);
			return false;
		}
		if (GetOverlappedResult(m_pipe, &overlapped, &bytes, TRUE) == FALSE) {
			dprintf(D_ALWAYS, "GetOverlappedResult error: %u\n", GetLastError());
			return false;
		}
	}
	ASSERT(bytes == len);

	return true;
}

void
LocalServer::touch()
{
	// we don't have to worry about our named pipe being cleaned up
	// by things like tmpwatch on Windows
}


bool LocalServer::consistent(void)
{
	// AFAIK, opened named pipes can't be deleted on windows.
	// -psilord, 2011/09/28

	return true;
}


