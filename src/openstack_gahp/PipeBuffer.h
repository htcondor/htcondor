/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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

/**
 *
 * This class encapsulates a DaemonCore pipe and provides buffering.
 * E.g. it allows you to hold a result read out of the pipe until
 * a full line is read. The getNextLine() and Write() methods are
 * designed to be used in read pipe and write pipe handlers,
 * respectively. Note that an object of this type is to be used
 * either strictly for reading (using getNextLine()) or strictly
 * for writing (using Write()).
 *
 **/

const int PIPE_BUFFER_READAHEAD_SIZE = 1024;

class PipeBuffer {
public:
	PipeBuffer ();
	PipeBuffer (int pipe_end);

	std::string * GetNextLine();
	int Write (const char * toWrite = NULL);

	int getPipeEnd() const { return pipe_end; }
	void setPipeEnd(const int _pipe_end) { pipe_end = _pipe_end; }
	const char * getBuffer() { return buffer.c_str(); }

	int IsEmpty();
	bool IsError() const { return error; }
	bool IsEOF() const { return eof; }

protected:
	int pipe_end;
	std::string buffer;

	bool error;
	bool eof;

	bool last_char_was_escape;

	char readahead_buffer[PIPE_BUFFER_READAHEAD_SIZE];
	int readahead_length;
	int readahead_index;
};

#endif /* PIPE_BUFFER_H */
