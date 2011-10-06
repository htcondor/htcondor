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
#include "named_pipe_reader.unix.h"
#include "named_pipe_watchdog.unix.h"
#include "named_pipe_util.unix.h"

bool
NamedPipeReader::initialize(const char* addr)
{
	ASSERT(!m_initialized);

	ASSERT(addr != NULL);
	m_addr = strdup(addr);
	ASSERT(m_addr != NULL);

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
	ASSERT(m_initialized);
	return m_addr;
}

void
NamedPipeReader::set_watchdog(NamedPipeWatchdog* watchdog)
{
	ASSERT(m_initialized);
	m_watchdog = watchdog;
}

bool
NamedPipeReader::read_data(void* buffer, int len)
{
	ASSERT(m_initialized);

	// if this pipe has multiple writers, we need to ensure
	// that our message size is no more than PIPE_BUF, otherwise
	// messages could be interleaved
	//
	ASSERT(len <= PIPE_BUF);

	// if we have a watchdog, we don't go right into a blocking
	// read. instead, we select with both the real pipe and the
	// watchdog pipe, which will close if our peer shuts down or
	// crashes. note that we only return failure if the watchdog
	// pipe has closed and the "real" pipe has nothing to read -
	// this is to handle the case where our peer has written
	// something for us to read and then exited
	//
	if (m_watchdog != NULL) {
		fd_set read_fd_set;
		FD_ZERO(&read_fd_set);
		FD_SET(m_pipe, &read_fd_set);
		int watchdog_pipe = m_watchdog->get_file_descriptor();
		FD_SET(watchdog_pipe, &read_fd_set);
		int max_fd = (m_pipe > watchdog_pipe) ? m_pipe : watchdog_pipe;
		int ret = select(max_fd + 1, &read_fd_set, NULL, NULL, NULL);
		if (ret == -1) {
			dprintf(D_ALWAYS,
			        "select error: %s (%d)\n",
			        strerror(errno),
			        errno);
			return false;
		}
		if (FD_ISSET(watchdog_pipe, &read_fd_set) &&
		    !FD_ISSET(m_pipe, &read_fd_set)
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
NamedPipeReader::poll(int timeout, bool& ready)
{
	// TODO: select on the watchdog pipe, if we have one. this
	// currently isn't a big deal since we only use poll() on
	// the server-side - which doesn't set a watchdog pipe

	ASSERT(m_initialized);

	ASSERT(timeout >= -1);

	fd_set read_fd_set;
	FD_ZERO(&read_fd_set);
	FD_SET(m_pipe, &read_fd_set);

	struct timeval* tv_ptr = NULL;
	struct timeval tv;
	if (timeout != -1) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		tv_ptr = &tv;
	}

	int ret = select(m_pipe + 1, &read_fd_set, NULL, NULL, tv_ptr);
	if (ret == -1) {
		if( errno == EINTR ) {
				// We could keep looping until all of the time has passed,
				// but currently nothing depends on this, so just return
				// true here to avoid having the whole procd die when
				// somebody attaches with a debugger.
			ready = false;
			return true;
		}
		dprintf(D_ALWAYS,
		        "select error: %s (%d)\n",
		        strerror(errno),
		        errno);
		return false;
	}

	ready = FD_ISSET(m_pipe, &read_fd_set);

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

	ASSERT(m_initialized);

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





