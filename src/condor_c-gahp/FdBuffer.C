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

#include "condor_common.h"
#include "condor_debug.h"
#include "FdBuffer.h"

FdBuffer::FdBuffer (int _fd) {
	fd = _fd;
 	buffer.reserve (5000);
	error = FALSE;
	newlineCount=0;
	last_char_was_escape = false;
}

FdBuffer::FdBuffer() {
	fd = -1;
 	buffer.reserve (5000);
 	newlineCount=0;
	last_char_was_escape = false;
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
		// Read another byte from fd
		// we only read one at a time to avoid blocking

		char buff;

		if (read (this->fd, &buff, 1) > 0) {

			buffer += buff;

			if (buff == '\\') {
				last_char_was_escape = !last_char_was_escape;
			}
			else {
				last_char_was_escape = false;

				if (buff == '\n' && !last_char_was_escape) {
						// We got a completed line!
					newlineCount++;
					result = ReadNextLineFromBuffer();
				}
			} 
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
FdBuffer::Write (const char * towrite) {
	
	if (towrite)
		buffer += towrite;

	int len = buffer.Length();
	if (len == 0)
		return 0;

	int numwritten = write (fd, buffer.Value(), len);
	if (numwritten > 0) {
			// shorten the buffer
		buffer = buffer.Substr (numwritten, len);
		return numwritten;
	} else if ( numwritten == 0 ) {
		return 0;
	} else {
		dprintf (D_ALWAYS, "Error %d writing to fd %d\n", numwritten, fd);
		error = TRUE;
		return -1;
	}
}

int
FdBuffer::IsEmpty() {
	return (buffer.Length() == 0);
}
