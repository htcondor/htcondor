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


#ifndef _NAMED_PIPE_WATCHDOG_SERVER_H
#define _NAMED_PIPE_WATCHDOG_SERVER_H

// see the comment at the top of NamedPipeWatchdog for a description
// of these classes' functionality

class NamedPipeWatchdogServer {

public:

	NamedPipeWatchdogServer() : m_initialized(false),
	                            m_path(NULL),
	                            m_read_fd(-1),
	                            m_write_fd(-1) { }

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new watchdog server with the given "watchdog address"
	//
	bool initialize(const char*);
	
	// clean up open FDs, file system droppings, and
	// dynamically allocated memory
	//
	~NamedPipeWatchdogServer();

	// get the named pipe path
	//
	char* get_path();

private:

	// set true once we're properly initialized
	//
	bool m_initialized;

	// the filesystem name for the named pipe that will serve
	// as the "address" for this watchdog
	//
	char* m_path;

	// the pipe ends that serve as our implementation. specifically,
	// if our process exits, the system will close these for us.
	// then, since we were the only writer on this pipe, any processes
	// that are select()ing on a read FD for the pipe will be signaled
	// due to EOF
	//
	int m_read_fd;
	int m_write_fd;
};

#endif
