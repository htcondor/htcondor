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


#ifndef _LOCAL_CLIENT_H
#define _LOCAL_CLIENT_H

#include "condor_common.h"

#if !defined(WIN32)
class NamedPipeWriter;
class NamedPipeReader;
class NamedPipeWatchdog;
#endif

class LocalClient {

public:

	LocalClient();

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new LocalClient that will connect to a LocalServer at
	// the given named pipe
	//
	bool initialize(const char*);

	// clean up
	//
	~LocalClient();

	// send the command contained in the given buffer
	//
	bool start_connection(void*, int);

	// end a command
	//
	void end_connection();

	// read response data from the server
	//
	bool read_data(void*, int);

private:

	// set true once we've been properly initialized
	//
	bool m_initialized;

	// implementation is totally different depending on whether we're
	// on Windows or UNIX. both use named pipes, but named pipes are not
	// (nearly) the same beast between the two
	//
#if defined(WIN32)
	char*  m_pipe_addr;
	HANDLE m_pipe;
#else
	static int         s_next_serial_number;
	int                m_serial_number;
	pid_t              m_pid;
	char*              m_addr;
	NamedPipeWriter*   m_writer;
	NamedPipeReader*   m_reader;
	NamedPipeWatchdog* m_watchdog;
#endif
};

#endif
