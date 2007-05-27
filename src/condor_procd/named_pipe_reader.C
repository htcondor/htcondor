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
#include "named_pipe_reader.h"
#include "named_pipe_watchdog.h"
#include "named_pipe_util.h"

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

void
NamedPipeReader::set_watchdog(NamedPipeWatchdog* watchdog)
{
	ASSERT(m_initialized);
	m_watchdog = watchdog;
}

bool
NamedPipeReader::change_owner(uid_t uid)
{
	ASSERT(m_initialized);

	// chown the pipe to the uid in question
	//
	if (chown(m_addr, uid, (gid_t)-1) < 0) {
		dprintf(D_ALWAYS,
		        "chown of %s error: %s (%d)\n",
		        m_addr,
		        strerror(errno),
		        errno);
		return false;
	}

	return true;
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
		dprintf(D_ALWAYS,
		        "select error: %s (%d)\n",
		        strerror(errno),
		        errno);
		return false;
	}

	ready = FD_ISSET(m_pipe, &read_fd_set);

	return true;
}
