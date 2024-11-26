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
#include "named_pipe_reader.unix.h"
#include "named_pipe_watchdog.unix.h"
#include "named_pipe_util.unix.h"

bool
NamedPipeReader::initialize(const char* addr)
{
	assert(!m_initialized);

	assert(addr != NULL);
	m_addr = strdup(addr);
	assert(m_addr != NULL);

	if (!named_pipe_create(addr, m_pipe, m_dummy_pipe)) {
		dprintf(D_ALWAYS,
		        "failed to initialize named pipe at %s\n",
		        addr);
		return false;
	}

	m_initialized = true;
	return true;
}

NamedPipeReader::~NamedPipeReader()
{
	// nothing to do if we're not initialized
	//
	if (!m_initialized) {
		return;
	}

	// close both FIFO FDs we have open
	//
	close(m_dummy_pipe);
	close(m_pipe);

	// remove our droppings from the FS
	//
	unlink(m_addr);

	// and free up our memeory
	//
	free(m_addr);
}

char*
NamedPipeReader::get_path()
{
	assert(m_initialized);
	return m_addr;
}

void
NamedPipeReader::set_watchdog(NamedPipeWatchdog* watchdog)
{
	assert(m_initialized);
	m_watchdog = watchdog;
}

bool
NamedPipeReader::read_data(void* buffer, int len)
{
	assert(m_initialized);

	// if this pipe has multiple writers, we need to ensure
	// that our message size is no more than PIPE_BUF, otherwise
	// messages could be interleaved
	//
	assert(len <= PIPE_BUF);

	// if we have a watchdog, we don't go right into a blocking
	// read. instead, we select with both the real pipe and the
	// watchdog pipe, which will close if our peer shuts down or
	// crashes. note that we only return failure if the watchdog
	// pipe has closed and the "real" pipe has nothing to read -
	// this is to handle the case where our peer has written
	// something for us to read and then exited
	//
	if (m_watchdog != NULL) {
		int watchdog_pipe = m_watchdog->get_file_descriptor();
		Selector selector;
		selector.add_fd( m_pipe, Selector::IO_READ );
		selector.add_fd( watchdog_pipe, Selector::IO_READ );
		selector.execute();
		if ( selector.failed() || selector.signalled() ) {
			dprintf(D_ALWAYS,
			        "select error: %s (%d)\n",
			        strerror(selector.select_errno()),
			        selector.select_errno());
			return false;
		}
		if ( selector.fd_ready( watchdog_pipe, Selector::IO_READ ) &&
		     !selector.fd_ready( m_pipe, Selector::IO_READ )
		) {
			dprintf(D_ALWAYS,
			        "error reading from named pipe: "
			            "watchdog pipe has closed\n");
			return false;
		}
	}

	int bytes = read(m_pipe, buffer, len);
	if (bytes != len) {
		if (bytes == -1) {
			dprintf(D_ALWAYS,
			        "read error: %s (%d)\n",
			        strerror(errno),
			        errno);
		}
		else {
			dprintf(D_ALWAYS,
			        "error: read %d of %d bytes\n",
			        bytes,
			        len);
		}
		return false;
	}

	return true;
}

bool
NamedPipeReader::poll(time_t timeout, bool& ready)
{
	// TODO: select on the watchdog pipe, if we have one. this
	// currently isn't a big deal since we only use poll() on
	// the server-side - which doesn't set a watchdog pipe

	assert(m_initialized);

	assert(timeout >= -1);

	Selector selector;
	selector.add_fd( m_pipe, Selector::IO_READ );

	if (timeout != -1) {
		selector.set_timeout( timeout );
	}

	selector.execute();
	if ( selector.signalled() ) {
			// We could keep looping until all of the time has passed,
			// but currently nothing depends on this, so just return
			// true here to avoid having the whole procd die when
			// somebody attaches with a debugger.
		ready = false;
		return true;
	}
	if ( selector.failed() ) {
		dprintf(D_ALWAYS,
		        "select error: %s (%d)\n",
		        strerror(selector.select_errno()),
		        selector.select_errno());
		return false;
	}

	ready = selector.fd_ready( m_pipe, Selector::IO_READ );

	return true;
}


/* Ensure that the m_addr file has the same inode as the opened m_pipe.
	If not, it means the named pipe had been deleted (or possibly recreated).
	Either way, it isn't consistent and upper layers who call this will
	probably start shutting things down.
*/
bool
NamedPipeReader::consistent(void)
{
	struct stat fbuf;
	struct stat lbuf;

	assert(m_initialized);

	/* We already have the named pipe open, so get its information */
	if (fstat(m_pipe, &fbuf) < 0) {
		dprintf(D_FULLDEBUG,
			"NamedPipeReader::consistent(): "
			"Failed to lstat() supposedly open named pipe! Named pipe is "
			"inconsistent! %s (%d)\n",
			strerror(errno),
			errno);
		return false;
	}

	/* We're going to compare it to the named pipe path on disk. */
	if (lstat(m_addr, &lbuf) < 0) {
		dprintf(D_FULLDEBUG,
			"NamedPipeReader::consistent(): "
			"Failed to stat() supposedly present named pipe! Named pipe "
			"is inconsistent! %s (%d)\n",
			strerror(errno),
			errno);
		return false;
	}

	/* They better match, or something sneaky happened */
	if (fbuf.st_dev != lbuf.st_dev || 
		fbuf.st_ino != lbuf.st_ino)
	{
		dprintf(D_ALWAYS, 
			"NamedPipeReader::consistent(): "
			"The named pipe at m_addr: '%s' is inconsistent with the "
			"originally opened m_addr when the procd was started.\n",
			m_addr);
		return false;
	}

	return true;
}





