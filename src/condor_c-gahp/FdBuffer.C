/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "FdBuffer.h"
#include "condor_common.h"
#include "condor_debug.h"

FdBuffer::FdBuffer (int _fd) {
	fd = _fd;
 	buffer.reserve (5000);
	error = FALSE;
	newlineCount=0;
}

FdBuffer::FdBuffer() {
	fd = -1;
 	buffer.reserve (5000);
 	newlineCount=0;
}


MyString *
FdBuffer::ReadNextLineFromBuffer () {
	if (!HasNextLineInBuffer())
		return NULL;

    const int len = buffer.Length();
	const char * chars = buffer.Value();
	int i=0;

	for (i=0; i<len; i++) {
		if (chars[i] == '\\') {
			i++;	// skip the next char, since it was escaped
			continue;
		}

		if (buffer[i] == '\n') {
			int end_char = i-1;
			if (i != 0 && buffer[i-1] == '\r')		// if command is terminated w '\r\n'
				end_char--;

			// "Pop" the line
			MyString * result = new MyString ((MyString)buffer.Substr (0,end_char));

			// Shorten the buffer
            buffer=buffer.Substr (i+1, len);

			newlineCount--;

			return result;
		}
	}

	// Haven't found \n yet...
	return NULL;
}

int
FdBuffer::HasNextLineInBuffer() {
	if (newlineCount > 0)
		return TRUE;

	return FALSE;
}

MyString *
FdBuffer::GetNextLine () {

	// First try buffer
	// We should always check the buffer first, b/c otherwise
	// the select() on the underlying fd might keep blocking
	// even though there are commands in the buffer
	MyString * result = ReadNextLineFromBuffer();


	if (result == NULL) {
		// Read another chunk from fd

		char buff[5001];
		ssize_t num_read = 0;

		if ((num_read = read (this->fd, buff, 5000)) > 0) {

			buff[num_read]='\0';
			dprintf (D_FULLDEBUG, "reading %d  bytes, from buffer %d\n", num_read, this->fd);

			// Count the number of newlines in the newly-read chunk
			int i=0;
			if ((buffer.Length() > 0) && buffer[buffer.Length()-1] == '\\')
				i++;
			for (; i<num_read; i++) {
				if (buff[i] == '\\') {
					i++;	// skip the next char, since it was escaped
					continue;
				}
				if (buff[i] == '\n')
					newlineCount++;
			}

			buffer += buff;

			// Try the buffer again
			result = ReadNextLineFromBuffer();
		} else {
			dprintf (D_ALWAYS, "Read 0 bytes from fd %d ==> fd must be closed\n", this->fd);
			// If the select (supposedly) triggered BUT we can't read anything
			// the stream must have been closed
			error = TRUE;
		}
	}

	return result;
}

int
FdBuffer::Select (FdBuffer** wait_on, int count, int timeout, int * result) {

	// Check the caches first
	int found = 0;
	for (int i=0; i<count; i++ ) {
		if (wait_on[i]->HasNextLineInBuffer()) {
			result[i] = TRUE;
			found ++;
		} else {
			result[i] = FALSE;
		}
	}

	if (found > 0) {
		return found;
	}

	// Otherwise do a plain old select()
	fd_set my_fds;
	FD_ZERO(&my_fds);
	int max_fd = 0;
	for (int i=0; i<count; i++ ) {
		FD_SET (wait_on[i]->fd, &my_fds);
		if (max_fd < wait_on[i]->fd)
			max_fd = wait_on[i]->fd;
	}

	struct timeval wait_time;
	wait_time.tv_sec = timeout;
	wait_time.tv_usec = 0;

	int select_result = select (max_fd + 1,
				      &my_fds,
				      NULL,
				      NULL,
				      &wait_time);

	if (select_result == -1) {
		// Something is wrong, return -1
		dprintf (D_ALWAYS, "Error errno=%d from select()\n", errno);
		return -1;
	}

	for (int i=0; i<count; i++ ) {
		if (FD_ISSET (wait_on[i]->fd, &my_fds)) {
			result[i] = TRUE;
			found ++;
		} else {
			result[i] = FALSE;
		}

	}

	return found;
}

