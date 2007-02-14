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
#include "local_server.h"

LocalServer::LocalServer(const char* pipe_addr)
{
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
		EXCEPT("CreateNamedPipe error: %u", GetLastError());
	}

	m_event = CreateEvent(NULL,   // default security
	                      TRUE,   // manual reset
	                      TRUE,   // starts signalled
	                      NULL);  // no name
	if (m_event == NULL) {
		EXCEPT("CreateEvent error: %u", GetLastError());
	}
}

LocalServer::~LocalServer()
{
	CloseHandle(m_event);
	CloseHandle(m_pipe);
}

void
LocalServer::set_client_principal(char*)
{
}

bool LocalServer::accept_connection(int timeout)
{
	// initiate a nonblocking "accept"
	//
	OVERLAPPED overlapped;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;
	overlapped.hEvent = m_event;
	if (ConnectNamedPipe(m_pipe, &overlapped) == TRUE) {
		return true;
	}

	// ConnectNamedPipe did not return immediate success; make sure
	// the error code indicates a pending operation
	//
	DWORD error = GetLastError();
	if (error != ERROR_IO_PENDING) {
		EXCEPT("ConnectNamedPipe error: %u", error);
	}

	// wait up to timeout seconds for a client connection
	//
	DWORD result = WaitForSingleObject(m_event, timeout * 1000);
	if (result == WAIT_FAILED) {
		EXCEPT("WaitForSingleObject error: %u", GetLastError());
	}
	if (result == WAIT_TIMEOUT) {
		return false;
	}

	// a connection has been established; finalize the overlapped operation
	//
	ASSERT(result == WAIT_OBJECT_0);
	DWORD ignored;
	if (GetOverlappedResult(m_pipe, &overlapped, &ignored, TRUE) == FALSE) {
		EXCEPT("GetOverlappedResult error: %u", GetLastError());
	}

	return true;
}

void
LocalServer::close_connection()
{
	if (FlushFileBuffers(m_pipe) == FALSE) {
		EXCEPT("FlushFileBuffers error: %u", GetLastError());
	}
	if (DisconnectNamedPipe(m_pipe) == FALSE) {
		EXCEPT("DisconnectNamedPipe error: %u", GetLastError());
	}
}

void
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
			EXCEPT("ReadFileError: %u", error);
		}
		if (GetOverlappedResult(m_pipe, &overlapped, &bytes, TRUE) == FALSE) {
			EXCEPT("GetOverlappedResult error: %u", GetLastError());
		}
	}
	ASSERT(bytes == len);
}

void
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
			EXCEPT("WriteFile error: %u", error);
		}
		if (GetOverlappedResult(m_pipe, &overlapped, &bytes, TRUE) == FALSE) {
			EXCEPT("GetOverlappedResult error: %u", GetLastError());
		}
	}
	ASSERT(bytes == len);
}
