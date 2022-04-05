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
#include "pbuffer.h"
#include "condor_daemon_core.h"
#include "vmgahp_common.h"
 
PBuffer::PBuffer (int _pipe_end) {
	pipe_end = _pipe_end;
 	buffer.reserve(5000);
	error = false;
	eof = false;
	last_char_was_escape = false;
	readahead_length = 0;
	readahead_index = -1;
	memset((void *) readahead_buffer, 0, P_BUFFER_READAHEAD_SIZE);

}

PBuffer::PBuffer() {
	pipe_end = -1;
 	buffer.reserve(5000);
	error = false;
	eof = false;
	last_char_was_escape = false;
	readahead_length = 0;
	readahead_index = -1;
	memset((void *) readahead_buffer, 0, P_BUFFER_READAHEAD_SIZE);
}

std::string *
PBuffer::GetNextLine () {
	if( pipe_end == -1 ) {
		error = true;
		return NULL;
	}
	//	vmprintf(D_ALWAYS, "Reading next line from pipe\n");
	if (readahead_length == 0) {
		// our readahead buffer is spent; read a new one in
		readahead_length = daemonCore->Read_Pipe(pipe_end, readahead_buffer, 
				P_BUFFER_READAHEAD_SIZE);
		if (readahead_length < 0) {
			if( (errno != EAGAIN) && (errno != EWOULDBLOCK) ) {
				error = true;
				vmprintf(D_ALWAYS, "error reading from DaemonCore "
						"pipe %d\n", pipe_end);
			}
			return NULL;
		}
		if (readahead_length == 0) {
			eof = true;
			vmprintf(D_ALWAYS, "EOF reached on DaemonCore pipe %d\n", 
					pipe_end);
			return NULL;
		}

		// buffer has new data 
		// reset the readahead index back to the beginning
		readahead_index = 0;
	}

	while (readahead_index != readahead_length) {

		char c = readahead_buffer[readahead_index++];

		if (c == '\n' && !last_char_was_escape) {
			// We got a completed line!
			std::string* result = new std::string(buffer);
			ASSERT(result);
			buffer = "";
			return result;
		} else if (c == '\r' && !last_char_was_escape) {
			// Ignore \r
		} else {
			buffer += c;

			if (c == '\\') {
				last_char_was_escape = !last_char_was_escape;
			} else {
				last_char_was_escape = false;
			}
		}
	}

	// we have used up our readahead buffer and 
	// have a partially completed line.
	readahead_length = 0;
	return NULL;
}

int
PBuffer::Write (const char * towrite) {

	if( pipe_end == -1 ) {
		return 0;
	}
	
	if (towrite) {
		buffer += towrite;
	}

	int len = buffer.length();
	if (len == 0) {
		return 0;
	}

	if( len > 4096 ) {
		len = 4096;
	}

	int numwritten = daemonCore->Write_Pipe (pipe_end, buffer.c_str(), len);
	if (numwritten > 0) {
		// shorten the buffer
		buffer = buffer.substr(numwritten);
		return numwritten;
	} else if ( numwritten == 0 ) {
		return 0;
	} else {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}

		vmprintf(D_ALWAYS, "Error %d writing to DaemonCore pipe %d\n", 
				errno, pipe_end);
		error = true;
		return -1;
	}
}

int
PBuffer::IsEmpty() {
	return (buffer.length() == 0);
}
