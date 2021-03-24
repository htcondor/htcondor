/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "PipeBuffer.h"
#include "thread_control.h"

PipeBuffer::PipeBuffer (int _pipe_end) {
	pipe_end = _pipe_end;
	buffer.reserve (5000);
	error = false;
	eof = false;
	last_char_was_escape = false;
	readahead_length = 0;
	readahead_index = 0;
	readahead_buffer[0] = '\0';
}

PipeBuffer::PipeBuffer() {
	pipe_end = -1;
	buffer.reserve (5000);
	error = false;
	eof = false;
	last_char_was_escape = false;
	readahead_length = 0;
	readahead_index = 0;
	readahead_buffer[0] = '\0';
}

std::string *
PipeBuffer::GetNextLine () {

	if (readahead_length == 0) {

		// our readahead buffer is spent; read a new one in.
		// note: read could block here on I/O, so release mutex to let
		// other run.
		arc_gahp_release_big_mutex();
		readahead_length = read(pipe_end, readahead_buffer, PIPE_BUFFER_READAHEAD_SIZE);
		arc_gahp_grab_big_mutex();
		if (readahead_length < 0) {
			error = true;
			dprintf (D_ALWAYS, "error reading from pipe %d\n", pipe_end);
			return NULL;
		}
		if (readahead_length == 0) {
			eof = true;
			dprintf(D_ALWAYS, "EOF reached on pipe %d\n", pipe_end);
			return NULL;
		}

		// buffer has new data; reset the readahead index back to the beginning
		readahead_index = 0;
	}

	while (readahead_index != readahead_length) {

		char c = readahead_buffer[readahead_index++];

		if (c == '\n' && !last_char_was_escape) {
					// We got a completed line!
			std::string* result = new std::string(buffer);
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

	int len = buffer.length();
	if (len == 0)
		return 0;

		// write() could block, so let other threads run...
	arc_gahp_release_big_mutex();
	int numwritten = write(pipe_end, buffer.c_str(), len);
	arc_gahp_grab_big_mutex();

	if (numwritten > 0) {
			// shorten the buffer
		buffer = buffer.erase (0, numwritten);
		return numwritten;
	} else if ( numwritten == 0 ) {
		return 0;
	} else {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;

		dprintf (D_ALWAYS, "Error %d writing to pipe %d\n", errno, pipe_end);
		error = true;
		return -1;
	}
}

int
PipeBuffer::IsEmpty() {
	return (buffer.length() == 0);
}
