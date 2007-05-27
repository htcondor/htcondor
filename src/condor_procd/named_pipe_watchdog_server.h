/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
