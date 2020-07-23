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

#ifndef PIPE_BUFFER_H
#define PIPE_BUFFER_H

#include "condor_common.h"
#include <string>

const int PIPE_BUFFER_READAHEAD_SIZE = 16384;

class PipeBuffer {
public:
	PipeBuffer();
	PipeBuffer( int pipe_end );

	bool GetNextLine( char * buffer, int length );

	int getPipeEnd() const { return pipe_end; }
	void setPipeEnd( const int _pipe_end ) { pipe_end = _pipe_end; }

	bool IsError() const { return error; }
	bool IsEOF() const { return eof; }

protected:
	int pipe_end;

	bool error;
	bool eof;

	bool last_char_was_escape;

	int readBufferSize;
	int readBufferIndex;
	char readBuffer[ PIPE_BUFFER_READAHEAD_SIZE ];
};

#endif
