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
#include "PipeBuffer.h"
#include "thread_control.h"

PipeBuffer::PipeBuffer (int _pipe_end) {
	pipe_end = _pipe_end;
	error = false;
	eof = false;
	last_char_was_escape = false;

	readBufferSize = 0;
	readBufferIndex = 0;
	readBuffer[0] = '\0';
}

PipeBuffer::PipeBuffer() {
	pipe_end = -1;
	error = false;
	eof = false;
	last_char_was_escape = false;

	readBufferSize = 0;
	readBufferIndex = 0;
	readBuffer[0] = '\0';
}

bool
PipeBuffer::GetNextLine( char * buffer, int length ) {
	int copyToIndex = 0;

	while( true ) {
		// If we've processed our whole buffer, read some more.
		if( readBufferIndex == readBufferSize ) {
			amazon_gahp_release_big_mutex();
			readBufferSize = read( pipe_end, readBuffer, PIPE_BUFFER_READAHEAD_SIZE );
			amazon_gahp_grab_big_mutex();

			if( readBufferSize < 0 ) {
				dprintf( D_ALWAYS | D_ERROR, "Error reading from pipe %d: %d (%s).\n", pipe_end, errno, strerror( errno ) );
				error = true;
				return false;
			} else if( readBufferSize == 0 ) {
				dprintf( D_ALWAYS, "EOF reached on pipe %d\n", pipe_end );
				eof = true;
				return false;
			}

			readBufferIndex = 0;
		}

		// Process the buffer, looking for an unescaped newline.
		int copyFromIndex = readBufferIndex;
		for( ; readBufferIndex < readBufferSize; ++readBufferIndex ) {
			char c = readBuffer[ readBufferIndex ];

			if( c == '\n' && ! last_char_was_escape ) {
				int amount = readBufferIndex - copyFromIndex;
				if( copyToIndex + amount >= length ) {
					dprintf( D_ALWAYS, "Output buffer too short, truncating.\n" );
					buffer[ copyToIndex ] = '\0';
					return false;
				}
				strncpy( & buffer[ copyToIndex ], & readBuffer[ copyFromIndex ], amount );
				buffer[ copyToIndex + amount ] = '\0';
				++readBufferIndex;
				return true;
			} else if( c == '\r' && ! last_char_was_escape ) {
				// Skip the \r; copy the partial line to the output buffer.
				int amount = readBufferIndex - copyFromIndex;
				strncpy( &buffer[ copyToIndex ], & readBuffer[ copyFromIndex ], amount );
				copyToIndex += amount;
				copyFromIndex = readBufferIndex + 1;
			} else {
				if( c == '\\' ) { last_char_was_escape = !last_char_was_escape; }
				else { last_char_was_escape = false; }
			}
		}

		// Copy the valid read buffer to the output buffer.
		int amount = readBufferIndex - copyFromIndex;
		if( copyToIndex + amount >= length ) {
			dprintf( D_ALWAYS, "Output buffer too short, truncating.\n" );
			buffer[ copyToIndex ] = '\0';
			return false;
		}
		strncpy( & buffer[ copyToIndex ], & readBuffer[ copyFromIndex ], amount );
		copyToIndex += amount;
	}
}
