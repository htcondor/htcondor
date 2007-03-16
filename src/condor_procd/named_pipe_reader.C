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

NamedPipeReader::NamedPipeReader(const char* addr)
{
	ASSERT(addr != NULL);
	m_addr = strdup(addr);
	ASSERT(m_addr != NULL);

	// ensure the FIFO doesn't already exist
	//
	unlink(addr);

	// make the FIFO node in the filesystem
	//
	if (mkfifo(addr, 0600) == -1) {
		EXCEPT("mkfifo of %s error: %s (%d)", addr, strerror(errno), errno);
	}

	// open (as the current uid) the pipe end that we'll be reading requests 
	// from (we do this with O_NONBLOCK because otherwise we'd deadlock
	//  waiting for someone to open the pipe for writing)
	//
	m_pipe = safe_open_wrapper(addr, O_RDONLY | O_NONBLOCK);
	if (m_pipe == -1) {
		EXCEPT("open for read-only of %s failed: %s (%d)",
		       addr,
		       strerror(errno),
		       errno);
	}

	// set the pipe back into blocking mode now that we have it open
	//
	int flags = fcntl(m_pipe, F_GETFL);
	if (flags == -1) {
		EXCEPT("fcntl error: %s (%d)", strerror(errno), errno);
	}
	if (fcntl(m_pipe, F_SETFL, flags & ~O_NONBLOCK) == -1) {
		EXCEPT("fcntl error: %s (%d)", strerror(errno), errno);
	}

	// open our FIFO for writing as well; we won't actually ever write
	// to this FD, but we keep this open to prevent EOF from ever being
	// read from m_pipe
	//
	m_dummy_pipe = safe_open_wrapper(m_addr, O_WRONLY);
	if (m_dummy_pipe == -1) {
		EXCEPT("open for write-only of %s failed: %s (%d)",
		       addr,
		       strerror(errno),
		       errno);
	}
}

NamedPipeReader::~NamedPipeReader()
{
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
NamedPipeReader::change_owner(uid_t uid)
{
	// chown the pipe to the uid in question
	//
	if (chown(m_addr, uid, (gid_t)-1) < 0) {
		EXCEPT("chown of %s error: %s (%d)", m_addr, strerror(errno), errno);
	}
}

void
NamedPipeReader::read_data(void* buffer, int len)
{
	ASSERT(len < PIPE_BUF);

	int bytes = read(m_pipe, buffer, len);
	if (bytes != len) {
		if (bytes == -1) {
			EXCEPT("read error: %s (%d)",
			       strerror(errno),
			       errno);
		}
		else {
			EXCEPT("error: read %d of %d bytes",
			       bytes,
			       len);
		}
	}
}

bool
NamedPipeReader::poll(int timeout)
{
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
		EXCEPT("select error: %s (%d)", strerror(errno), errno);
	}

	return FD_ISSET(m_pipe, &read_fd_set);
}
