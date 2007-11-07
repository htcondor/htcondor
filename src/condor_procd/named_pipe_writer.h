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


#ifndef _NAMED_PIPE_WRITER_H
#define _NAMED_PIPE_WRITER_H

class NamedPipeWatchdog;

class NamedPipeWriter {

public:

	NamedPipeWriter() : m_initialized(false),
	                    m_pipe(-1),
	                    m_watchdog(NULL) { }

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new FIFO for writing with the given
	// "address" (which is really a FIFO node in the
	// filesystem)
	//
	bool initialize(const char*);
	
	// clean up
	//
	~NamedPipeWriter();

	// enable a watchdog on this writer
	//
	void set_watchdog(NamedPipeWatchdog*);

	// write to the pipe
	//
	bool write_data(void*, int);

private:

	// set true one we're successfully initialized
	//
	bool m_initialized;

	// a O_WRONLY file descriptor for our FIFO
	//
	int m_pipe;

	// an optional watchdog; if set this gives us
	// protection against hanging trying to talk to a
	// crashed process
	//
	NamedPipeWatchdog* m_watchdog;
};

#endif
