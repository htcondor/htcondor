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
#include "condor_debug.h"
#include "selector.h"
#include "named_pipe_writer.unix.h"
#include "named_pipe_watchdog.unix.h"

bool
NamedPipeWriter::initialize(const char* addr)
{
	// open a write-only connection to the server
	//
	m_pipe = safe_open_wrapper_follow(addr, O_WRONLY | O_NONBLOCK);
	if (m_pipe == -1) {
		dprintf(D_ALWAYS,
		        "error opening %s: %s (%d)\n",
		        addr,
		        strerror(errno),
		        errno);
		return false;
	}

	// set it back into blocking mode
	//
	int flags = fcntl(m_pipe, F_GETFL);
	if (flags == -1) {
		dprintf(D_ALWAYS,
		        "fcntl error: %s (%d)\n",
		        strerror(errno),
		        errno);
		close(m_pipe);
		m_pipe = -1;
		return false;
	}
	if (fcntl(m_pipe, F_SETFL, flags & ~O_NONBLOCK) == -1) {
		dprintf(D_ALWAYS,
		        "fcntl error: %s (%d)\n",
		        strerror(errno),
		        errno);
		close(m_pipe);
		m_pipe = -1;
		return false;
	}

	m_initialized = true;
	return true;
}
	
NamedPipeWriter::~NamedPipeWriter()
{
	// nothing to do if we're not initialized
	//
	if (!m_initialized) {
		return;
	}

	// close our pipe FD
	//
	close(m_pipe);
}

void
NamedPipeWriter::set_watchdog(NamedPipeWatchdog* watchdog)
{
	assert(m_initialized);
	m_watchdog = watchdog;
}

bool
NamedPipeWriter::write_data(void* buffer, int len)
{
	assert(m_initialized);

	// if we're writing to a pipe that has multiple writers,
	// we need to make sure our messages are no larger than
	// PIPE_BUF to guarantee atomic writes
	//
	assert(len <= PIPE_BUF);

	// if we have a watchdog, we don't go right into a blocking
	// write. instead, we select with both the real pipe and the
	// watchdog pipe, which will close if our peer shuts down or
	// crashes
	//
	if (m_watchdog != NULL) {
		int watchdog_pipe = m_watchdog->get_file_descriptor();
		Selector selector;
		selector.add_fd( m_pipe, Selector::IO_WRITE );
		selector.add_fd( watchdog_pipe, Selector::IO_READ );
		selector.execute();
		if ( selector.failed() || selector.signalled() ) {
			dprintf(D_ALWAYS,
			        "select error: %s (%d)\n",
			        strerror(selector.select_errno()),
			        selector.select_errno());
			return false;
		}
		if ( selector.fd_ready( watchdog_pipe, Selector::IO_READ ) ) {
			dprintf(D_ALWAYS,
			        "error writing to named pipe: "
			            "watchdog pipe has closed\n");
			return false;
		}
	}

	// do the write
	//
	int bytes = write(m_pipe, buffer, len);
	if (bytes != len) {
		if (bytes == -1) {
			dprintf(D_ALWAYS,
			        "write error: %s (%d)\n",
			        strerror(errno),
			        errno);
		}
		else {
			dprintf(D_ALWAYS,
			        "error: wrote %d of %d bytes\n",
			        bytes,
			        len);
		}
		return false;
	}

	return true;
}
