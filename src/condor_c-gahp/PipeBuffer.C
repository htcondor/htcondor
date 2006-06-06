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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "PipeBuffer.h"

PipeBuffer::PipeBuffer (int _pipe_end) {
	pipe_end = _pipe_end;
 	buffer.reserve (5000);
	error = false;
	eof = false;
	last_char_was_escape = false;
	readahead_length = 0;
}

PipeBuffer::PipeBuffer() {
	pipe_end = -1;
 	buffer.reserve (5000);
	error = false;
	eof = false;
	last_char_was_escape = false;
	readahead_length = 0;
}

MyString *
PipeBuffer::GetNextLine () {

	if (readahead_length == 0) {

		// our readahead buffer is spent; read a new one in
		readahead_length = daemonCore->Read_Pipe(pipe_end, readahead_buffer, PIPE_BUFFER_READAHEAD_SIZE);
		if (readahead_length < 0) {
			error = true;
			dprintf (D_ALWAYS, "error reading from DaemonCore pipe %d\n", pipe_end);
			return NULL;
		}
		if (readahead_length == 0) {
			eof = true;
			dprintf(D_ALWAYS, "EOF reached on DaemonCore pipe %d\n", pipe_end);
			return NULL;
		}

		// buffer has new data; reset the readahead index back to the beginning
		readahead_index = 0;
	}

	while (readahead_index != readahead_length) {

		char c = readahead_buffer[readahead_index++];

		if (c == '\n' && !last_char_was_escape) {
					// We got a completed line!
			MyString* result = new MyString(buffer);
			buffer = "";
			return result;
		} else if (c == '\r' && !last_char_was_escape) {
				// Ignore \r
		} else {
			buffer += c;

			if (c == '\\') {
				last_char_was_escape = !last_char_was_escape;
			}
			else {
				last_char_was_escape = false;
			}
		}
	}

	// we have used up our readahead buffer and have a partially completed line
	readahead_length = 0;
	return NULL;
}

int
PipeBuffer::Write (const char * towrite) {
	
	if (towrite)
		buffer += towrite;

	int len = buffer.Length();
	if (len == 0)
		return 0;

	int numwritten = daemonCore->Write_Pipe (pipe_end, buffer.Value(), len);
	if (numwritten > 0) {
			// shorten the buffer
		buffer = buffer.Substr (numwritten, len);
		return numwritten;
	} else if ( numwritten == 0 ) {
		return 0;
	} else {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		dprintf (D_ALWAYS, "Error %d writing to DaemonCore pipe %d\n", errno, pipe_end);
		error = true;
		return -1;
	}
}

int
PipeBuffer::IsEmpty() {
	return (buffer.Length() == 0);
}
