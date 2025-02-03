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


#ifndef _NAMED_PIPE_READER_H
#define _NAMED_PIPE_READER_H

class NamedPipeWatchdog;

class NamedPipeReader {

public:

	NamedPipeReader() : m_initialized(false),
	                    m_addr(NULL),
	                    m_pipe(-1),
	                    m_dummy_pipe(-1),
	                    m_watchdog(NULL) { }

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new FIFO for reading with the given
	// "address" (which is really a node in the
	// filesystem)
	//
	bool initialize(const char*);
	
	// clean up open FDs, file system droppings, and
	// dynamically allocated memory
	//
	~NamedPipeReader();

	// get the named pipe path
	//
	char* get_path();

	// enable a watchdog on this reader
	//
	void set_watchdog(NamedPipeWatchdog*);

	// read data off the pipe
	//
	bool read_data(void*, int);

	// second parameter is set to true if the named pipe
	// becomes ready for reading within the given timeout
	// period, otherwise it's set to false
	//
	bool poll(time_t, bool&);

	// Determine if the named pipe on the disk is the actual named pipe that
	// was initially opened. In practice it means that the dev and inode fields
	// of a stat() on the named pipe filename must equal to the fstat
	// equivalents of the fd
	//
	bool consistent(void);

private:

	// set true once we're properly initialized
	//
	bool m_initialized;

	// the filesystem name for our FIFO
	//
	char* m_addr;

	// an O_RDONLY file descriptor for our FIFO
	//
	int m_pipe;

	// a O_WRONLY file descriptor for the FIFO; this
	// never gets used but we keep it open so that EOF
	// is never read from m_pipe
	//
	int m_dummy_pipe;

	// an optional watchdog; if set this gives us
	// protection against hanging trying to talk to a
	// crashed process
	//
	NamedPipeWatchdog* m_watchdog;
};

#endif
