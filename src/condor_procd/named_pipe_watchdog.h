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
