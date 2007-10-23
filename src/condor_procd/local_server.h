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

#ifndef _LOCAL_SERVER_H
#define _LOCAL_SERVER_H

#include "condor_common.h"

#if !defined(WIN32)
class NamedPipeWatchdogServer;
class NamedPipeReader;
class NamedPipeWriter;
#endif

class LocalServer {

public:

	LocalServer();

	// we use a plain old member function instead of the constructor
	// to do the real initialization work so that we can give our
	// caller an indication if something goes wrong
	//
	// init a new local server that listens for commands at the given
	// "address" (which is a FIFO pathname on UNIX and a named pipe name
	// on Windows)
	//
	bool initialize(const char*);

	// clean up
	//
	~LocalServer();

	// set the principal that is allowed to connect to this server
	// (on Windows, this will be a SID; on UNIX, a UID)
	//
	bool set_client_principal(const char*);

	// wait up to the specified number of seconds to receive a client
	// connection; second param is set to true if one is received,
	// false otherwise
	//
	bool accept_connection(int, bool&);

	// close a connection, making it possible to accept another one
	// via the accept_connection method
	//
	bool close_connection();

	// read data from a connected client
	//
	bool read_data(void*, int);

	// write data to a connected client
	//
	bool write_data(void*, int);

private:

	// set once we're successfully initialized
	//
	bool m_initialized;

	// implementation is totally different depending on whether we're
	// on Windows or UNIX. both use named pipes, but named pipes are not
	// (nearly) the same beast between the two
	//
#if defined(WIN32)
	HANDLE      m_pipe;
	HANDLE      m_event;
	OVERLAPPED* m_accept_overlapped;
#else
	char*                    m_pipe_addr;
	NamedPipeWatchdogServer* m_watchdog_server;
	NamedPipeReader*         m_reader;
	NamedPipeWriter*         m_writer;
#endif
};

#endif
