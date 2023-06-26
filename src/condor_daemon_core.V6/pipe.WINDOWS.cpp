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
#include "pipe.WINDOWS.h"
#include "stl_string_utils.h"

extern CRITICAL_SECTION Big_fat_mutex;

PipeEnd::PipeEnd(HANDLE handle, bool overlapped, bool nonblocking, int pipe_size) :
		m_handle(handle), m_overlapped(overlapped),
		m_nonblocking(nonblocking), m_pipe_size(pipe_size),
		m_registered(false), m_watched_event(NULL)
{
	std::string event_name;

	// create a manual reset Event, set initially to signaled
	// to be used for overlapped operations
	formatstr(event_name, "pipe_event_%d_%x", GetCurrentProcessId(), m_handle);
	m_event = CreateEvent( NULL, TRUE, TRUE, event_name.c_str() );
	ASSERT(m_event);
	
	// create the event for waiting until a PID-watcher
	// is done using this object
	formatstr(event_name, "pipe_watched_event_%d_%x", 
		GetCurrentProcessId(), m_handle);
	m_watched_event = CreateEvent(NULL, TRUE, TRUE, event_name.c_str());
	ASSERT(m_watched_event);
}

// this will be called from Close_Pipe
PipeEnd::~PipeEnd()
{
	// make sure we've been cancelled first
	ASSERT(!m_registered);

	CloseHandle(m_watched_event);
	CloseHandle(m_event);
	CloseHandle(m_handle);
}

// method to wrap WaitForSingleObject so we release our mutex that
// is shared with the daemoncore pid watcher thread.  if we don't
// do this, we will deadlock!!!
DWORD PipeEnd::WaitForSingleObject(HANDLE handle, DWORD millisecs)
{
	DWORD result;

	::LeaveCriticalSection(&Big_fat_mutex); // release big fat mutex
	result = ::WaitForSingleObject(handle,millisecs);
	::EnterCriticalSection(&Big_fat_mutex); // enter big fat mutex

	return result;
}

// this will be called from Register_Pipe, before WatchPid
void PipeEnd::set_registered()
{
	ASSERT(m_overlapped && !m_registered);

	// mark ourself as registered
	m_registered = true;
}

// the is called in DaemonCore from WatchPid
void PipeEnd::set_watched()
{
	ASSERT(m_registered && m_watched_event);
	ResetEvent(m_watched_event);
}

// this is either called from the PID-watcher if it detects the
// deallocate flag is set, or from cancel() if there is no
// PID-watcher for this object
void PipeEnd::set_unregistered()
{
	ASSERT(m_registered && m_watched_event);

	// mark ourselves unregistered
	m_registered = false;

	// tell the main thread that the PID-watcher thread
	// will no longer touch this object
	SetEvent(m_watched_event);
}

// this is called from Cancel_Pipe - it returns
// when we know that there is no longer a PID-watcher
// thread watching this PipeEnd and this PipeEnd
// has been unregistered
void PipeEnd::cancel()
{
	// wait until we know a PID-watcher is no longer using this object
	DWORD result = WaitForSingleObject(m_watched_event, INFINITE);
	ASSERT(result == WAIT_OBJECT_0);

	// if the PID-watcher did not call set_unregistered, do it here
	if (m_registered) {
		set_unregistered();
	}
}

HANDLE ReadPipeEnd::pre_wait()
{
	ASSERT(m_registered);

	if (m_async_io_state == IO_UNSTARTED) {

		// begin an overlapped read of one byte

		ZeroMemory(&m_overlapped_struct, sizeof(OVERLAPPED));
		m_overlapped_struct.hEvent = m_event;

		DWORD bytes;
		if (ReadFile(m_handle,
					 &m_async_io_buf,
					 1,
					 &bytes,
					 &m_overlapped_struct)
		) {
			// read succeeded immediately and so the event is left signaled
			m_async_io_state = IO_DONE;
			if (bytes == 0) {
				m_async_io_error = ERROR_HANDLE_EOF;
			}
			else {
				m_async_io_error = 0;
			}
		}
		else if (GetLastError() == ERROR_IO_PENDING) {
			// event is now unsignaled and will go to signaled when
			// the operation is complete
			m_async_io_state = IO_PENDING;
			m_async_io_error = 0;
		}
		else {
			// An error occured. Make sure our event object is set
			// so that a read won't block
			m_async_io_state = IO_DONE;
			m_async_io_error = GetLastError();
			SetEvent(m_event);
		}
	}

	return m_event;
}

bool ReadPipeEnd::post_wait()
{
	if (m_async_io_state == IO_PENDING) {
		DWORD bytes;
		if (!GetOverlappedResult(m_handle, &m_overlapped_struct, &bytes, TRUE)) {
			m_async_io_error = GetLastError();
		}
		else if (bytes == 0) {
			m_async_io_error = ERROR_HANDLE_EOF;
		}
		else {
			m_async_io_error = 0;
		}
		m_async_io_state = IO_DONE;
	}

	ASSERT(m_async_io_state == IO_DONE);

	// tell the main thread that the PID-watcher is done with
	// this object
	SetEvent(m_watched_event);

	return true;
}

// this is called from DaemonCore::Driver to ensure that it is safe
// to fire the handler without fear of blocking
bool ReadPipeEnd::io_ready()
{
	return m_async_io_state == IO_DONE;
}

int ReadPipeEnd::read(void* buffer, int len)
{
	int ret;

	// len can be at most the size of the pipe buffer.
	// The caller should be able to handle short reads
	if (len > m_pipe_size) {
		len = m_pipe_size;
	}
	
	// start with errno set to something other than EWOULDBLOCK
	errno = EINVAL;

	// wait until we are not being managed by a PID-watcher
	DWORD result = WaitForSingleObject(m_watched_event, m_nonblocking ? 0 : INFINITE);
	if (result == WAIT_TIMEOUT) {
		errno = EWOULDBLOCK;
		return -1;
	}
	ASSERT(result == WAIT_OBJECT_0);

	// if we're unregistered and there's a pending ReadFile, then we
	// need to check the operation's status, since the PID-watcher thread
	// is no longer around to monitor it
	if (m_async_io_state == IO_PENDING) {

		ASSERT(!m_registered);

		DWORD res = WaitForSingleObject(m_event, m_nonblocking ? 0 : INFINITE);
		if (res == WAIT_TIMEOUT) {
			errno = EWOULDBLOCK;
			return -1;
		}

		DWORD bytes;
		if (!GetOverlappedResult(m_handle, &m_overlapped_struct, &bytes, TRUE)) {
			m_async_io_error = GetLastError();
		}
		else if (bytes == 0) {
			m_async_io_error = ERROR_HANDLE_EOF;
		}
		else {
			m_async_io_error = 0;
		}
		m_async_io_state = IO_DONE;
	}

	ASSERT(m_async_io_state != IO_PENDING);

	if (m_async_io_state == IO_DONE) {
		
		if (m_async_io_error) {
			if (m_async_io_error == ERROR_HANDLE_EOF || m_async_io_error == ERROR_BROKEN_PIPE) {
				ret = 0;
			}
			else {
				ret = -1;
			}
		}
		else {
			// an asynchronous operation has completed, we copy out the
			// single byte and then peek ahead to see if there's anything
			// else to read
			*(char*)buffer = m_async_io_buf;
		
			DWORD bytes_left;
			if (!PeekNamedPipe(m_handle, NULL, 0, NULL, &bytes_left, NULL)) {
				dprintf(D_ALWAYS, "PeekNamedPipe error: %d\n", GetLastError());	
				ret = -1;
			}
			else if (bytes_left > 0) {
				// read what's left on the pipe - we know this won't block or return
				// with errno == EWOULDBLOCK because of the peek
				ret = read_helper((char*)buffer + 1, len - 1);
				if (ret != -1) {
					// we want to return the total number of bytes placed in the buffer,
					// which is whatever we just read plus the one from the async_io_buf
					ret += 1;
				}
			}
			else {
				// no more data on the pipe; just 1 byte was read
				ret = 1;
			}
			m_async_io_state = IO_UNSTARTED;
		}
	}
	else {
			// we're not registered and there's no outstanding async I/O - just read
			ret = read_helper(buffer, len);
	}

	return ret;
}

int ReadPipeEnd::read_helper(void* buffer, int len)
{
	// set errno to something other than EWOULDBLOCK
	errno = EINVAL;

	// if we're nonblocking, we peek ahead and only read
	// if we know there's data to be read
	if (m_nonblocking) {
		DWORD bytes_avail;
		if (!PeekNamedPipe(m_handle, NULL, 0, NULL, &bytes_avail, NULL)) {
			if (GetLastError() == ERROR_BROKEN_PIPE) {
				// this happens on EOF
				return 0;
			}
			dprintf(D_ALWAYS, "PeekNamedPipe error: %d\n", GetLastError());
			return -1;
		}
		if (bytes_avail == 0) {
			errno = EWOULDBLOCK;
			return -1;
		}
	}

	// setup an overlapped structure if the pipe is
	// opened in overlapped mode
	OVERLAPPED tmp_overlapped;
	OVERLAPPED *p_overlapped = NULL;
	if (m_overlapped) {
		ZeroMemory(&tmp_overlapped, sizeof(OVERLAPPED));
		p_overlapped = &tmp_overlapped;
	}

	// do the read
	DWORD bytes;
	if (!ReadFile(m_handle, buffer, len, &bytes, p_overlapped)) {
		if (m_overlapped) {
			if (GetLastError() == ERROR_IO_PENDING) {
				if (!GetOverlappedResult(m_handle, p_overlapped, &bytes, TRUE)) {
					if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
						bytes = 0;
					}
					else {
						dprintf(D_ALWAYS | D_BACKTRACE, "read_helper: GetOverlappedResult error: %d\n", GetLastError());
						return -1;
					}
				}
			}
		}
		else if (GetLastError() == ERROR_HANDLE_EOF || GetLastError() == ERROR_BROKEN_PIPE) {
			bytes = 0;
		}
		else {
			dprintf(D_ALWAYS, "ReadFile error: %d\n", GetLastError());
			return -1;
		}
	}

	return bytes;
}

// just return the Event handle to wait for an  outstanding operation to complete,
// if one exists
HANDLE WritePipeEnd::pre_wait()
{
	return m_event;
}

// an outstanding operation has completed, so issue
// the GetOverlappedResult and update our internal
// state based on the results
bool WritePipeEnd::post_wait()
{
	if (m_async_io_buf) {

		// an asynchronous operation was pending; get
		// the result
		DWORD bytes;
		GetOverlappedResult(m_handle, &m_overlapped_struct, &bytes, TRUE);
		if (bytes < m_async_io_size) {

			// only a portion of the buffer was written
			// update our buffer data
			m_async_io_size -= bytes;
			char* buf = new char[m_async_io_size];
			memcpy(buf, m_async_io_buf + bytes, m_async_io_size);
			delete[] m_async_io_buf;
			m_async_io_buf = buf;

			// attempt to write the rest of the buffer; note that it is
			// possible that this will result in another outstanding
			// overlapped I/O.
			async_write_helper();


		}
		else {
			delete[] m_async_io_buf;
			m_async_io_buf = NULL;
		}
	}

	if (m_async_io_buf) {
		// if there's still and outstanding write, we aren't done yet
		return false;
	}
	else {
		// the write is complete! signal the Event object that the
		// PID-watcher is done with this thread
		SetEvent(m_watched_event);

		return true;
	}
}

// called from DaemonCore::Driver by main thread after
// the PID-watcher thread has called post_wait
bool WritePipeEnd::io_ready()
{
	return m_async_io_buf == NULL;
}

int WritePipeEnd::write(const void* buffer, int len)
{
	// set errno to something other than EWOULDBLOCK
	errno = EINVAL;

	if (!m_overlapped) {
	
		// we're not overlapped - just issue a blocking write
		// (if Create_Pipe had write_nonblocking set, we'd be overlapped)
		DWORD bytes;
		if (!WriteFile(m_handle, buffer, len, &bytes, NULL)) {
			dprintf(D_ALWAYS, "WriteFile error: %d\n", GetLastError());
			return -1;
		}
		return bytes;
	}
	else {

		// wait for the PID-watcher to stop using this object
		DWORD result = WaitForSingleObject(m_watched_event, m_nonblocking ? 0 : INFINITE);
		if (result == WAIT_TIMEOUT) {
			errno = EWOULDBLOCK;
			return -1;
		}
		ASSERT(result == WAIT_OBJECT_0);
		
		// can't proceed unless m_async_io_buf is NULL
		while (m_async_io_buf) {

			// attempt to complete the outstanding overlapped operation
			if (!complete_async_write(m_nonblocking)) {
				errno = EWOULDBLOCK;
				return -1;
			}
		}

		int ret = len;

		if (m_async_io_error) {
			if (ERROR_BROKEN_PIPE == m_async_io_error) errno = EPIPE;
			ret = -1;
		}
		else {
			// ok, so now we can proceed with the write
			m_async_io_size = len;
			m_async_io_buf = new char[m_async_io_size];
			memcpy(m_async_io_buf, buffer, m_async_io_size);
			if (async_write_helper() == -1) {
				if (ERROR_BROKEN_PIPE == m_async_io_error) errno = EPIPE;
				ret = -1;
			}
			else {
				ret = len;
			}
		}

		return ret;
	}
}

// issues an asyncronous write and handles the case when the async
// operation completes right away with a short write
// possible outcomes:
//   - the write operation completes
//   - the wrtie operation is pending
//   - some number of writes succeed but do not write all the data,
//     leaving us with a shorter pending write
//   - an error occurs
int WritePipeEnd::async_write_helper()
{
	int ret;

	m_async_io_error = 0;

	bool keep_going;
	do {
		keep_going = false;
		ZeroMemory(&m_overlapped_struct, sizeof(OVERLAPPED));
		m_overlapped_struct.hEvent = m_event;
		DWORD bytes;
		if (WriteFile(m_handle, m_async_io_buf, m_async_io_size, &bytes, &m_overlapped_struct)) {
			if (bytes == m_async_io_size) {
				// the write succeeded in full; deallocate the buffer and return
				delete[] m_async_io_buf;
				m_async_io_buf = NULL;
				m_async_io_size = 0;
				ret = 0;
			}
			else {
				// the write is partially completed; adjust buffer and keep going
				m_async_io_size -= bytes;
				char* buf = new char[m_async_io_size];
				memcpy(buf, m_async_io_buf + bytes, m_async_io_size);
				delete[] m_async_io_buf;
				m_async_io_buf = buf;
				keep_going = true;
			}
		}
		else if (GetLastError() == ERROR_IO_PENDING) {
			// the write is pending; nothing more we can do right now
			ret = 0;
		}
		else {
			// some type of failure occurred
			m_async_io_error = GetLastError();
			delete[] m_async_io_buf;
			m_async_io_buf = NULL;
			ret = -1;

			// make sure our Event object is signaled
			SetEvent(m_event);
		}
	} while (keep_going);

	return ret;
}

bool WritePipeEnd::complete_async_write(bool nonblocking)
{
	while (m_async_io_buf) {

		// wait on the overlapped event
		DWORD result = WaitForSingleObject(m_event, nonblocking ? 0 : INFINITE);
		if (result == WAIT_TIMEOUT) {
			return false;
		}
		ASSERT(result == WAIT_OBJECT_0);
		
		DWORD bytes;
		if (!GetOverlappedResult(m_handle, &m_overlapped_struct, &bytes, TRUE)) {
			dprintf(D_ALWAYS | D_BACKTRACE, "complete_async_write: GetOverlappedResult error: %d\n", GetLastError());
			m_async_io_error = GetLastError();
			if (m_async_io_error == ERROR_PIPE_NOT_CONNECTED || m_async_io_error == ERROR_BROKEN_PIPE) {
				// the operation cannot complete
				delete[] m_async_io_buf;
				m_async_io_buf = NULL;
			}

			// return true since false indicates EWOULDBLOCK, which is not what we want
			return true;
		}
		if (bytes < m_async_io_size) {
			// only a portion of the buffer was written
			// start a new async I/O with the remaining bytes
			m_async_io_size -= bytes;
			char* buf = new char[m_async_io_size];
			memcpy(buf, m_async_io_buf + bytes, m_async_io_size);
			delete[] m_async_io_buf;
			m_async_io_buf = buf;
			async_write_helper();
		}
		else {
			// the operation is complete
			delete[] m_async_io_buf;
			m_async_io_buf = NULL;
		}
	}

	return true;
}

