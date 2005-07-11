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
FdBuffer::GetNextLine () {
	MyString * result = NULL;
	
		// Read another byte from fd
		// we only read one at a time to avoid blocking

	char buff;
	if (read (this->fd, &buff, 1) > 0) {

		if (buff == '\n' && !last_char_was_escape) {
					// We got a completed line!
			newlineCount++;
			result = new MyString(buffer);
			buffer = "";
		} else if (buff == '\r' && !last_char_was_escape) {
				// Ignore \r
		} else {
			buffer += buff;

			if (buff == '\\') {
				last_char_was_escape = !last_char_was_escape;
			}
			else {
				last_char_was_escape = false;
			}
		} 
	} else {
		dprintf (D_ALWAYS, 
				 "Read 0 bytes from fd %d ==> fd must be closed\n", 
				 this->fd);
			// If the select (supposedly) triggered BUT we can't read anything
			// the stream must have been closed
		error = TRUE;
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
