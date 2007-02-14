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
#include "named_pipe_writer.h"

NamedPipeWriter::NamedPipeWriter(const char* addr)
{
	// open a write-only connection to the server
	//
	m_pipe = safe_open_wrapper(addr, O_WRONLY | O_NONBLOCK);
	if (m_pipe == -1) {
		EXCEPT("error opening %s: %s (%d)",
		       addr,
		       strerror(errno),
		       errno);
	}

	// set it back into blocking mode
	//
	int flags = fcntl(m_pipe, F_GETFL);
	if (flags == -1) {
		EXCEPT("fcntl error: %s (%d)", strerror(errno), errno);
	}
	if (fcntl(m_pipe, F_SETFL, flags & ~O_NONBLOCK) == -1) {
		EXCEPT("fcntl error: %s (%d)", strerror(errno), errno);
	}
}
	
NamedPipeWriter::~NamedPipeWriter()
{
	// close our pipe FD
	//
	close(m_pipe);
}

void
NamedPipeWriter::write_data(void* buffer, int len)
{
	ASSERT(len < PIPE_BUF);
	
	// do the write
	//
	int bytes = write(m_pipe, buffer, len);
	if (bytes != len) {
		if (bytes == -1) {
			EXCEPT("write error: %s (%d)", strerror(errno), errno);
		}
		else {
			EXCEPT("error: wrote %d of %d bytes", bytes, len);
		}
	}
}

void
NamedPipeWriter::write_data_vector(struct iovec* vector, int count)
{
	// calculate the total length of the vectored data
	//
	int total_len = 0;
	for (int i = 0; i < count; i++) {
		total_len += vector[i].iov_len;
	}

	ASSERT(total_len < PIPE_BUF);

	// do the writev
	//
	int bytes = writev(m_pipe, vector, count);
	if (bytes != total_len) {
		if (bytes == -1) {
			EXCEPT("write error: %s (%d)", strerror(errno), errno);
		}
		else {
			EXCEPT("error: wrote %d of %d bytes", bytes, total_len);
		}
	}
}
