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


#ifndef _NAMED_PIPE_WATCHDOG_H
#define _NAMED_PIPE_WATCHDOG_H

// This class can be used in conjuntion with the NamedPipeWatchdogServer
// class to provide clients (e.g. using our LocalClient class to
// communicate with another process that is using LocalServer) with
// notifcation when the server has crashed. Specifically, when the
// server crashes, the OS will close all its FDs, and as a result the
// pipe that this class opens to the server will get an EOF.

class NamedPipeWatchdog {

public:

	NamedPipeWatchdog() : m_initialized(false), m_pipe_fd(-1) { }

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new watchdog to watch the server with the given
	// "watchdog address"
	//
	bool initialize(const char*);

	// method for accessing the watchdog file descriptor
	//
	int get_file_descriptor();

	// clean up open FDs, file system droppings, and
	// dynamically allocated memory
	//
	~NamedPipeWatchdog();

private:

	// set true once we're properly initialized
	//
	bool m_initialized;

	// the file descriptor to the watchdog pipe; this is a descriptor
	// for a read pipe end that will be closed when the its associated
	// server shuts down or crashes
	//
	int m_pipe_fd;
};

#endif
