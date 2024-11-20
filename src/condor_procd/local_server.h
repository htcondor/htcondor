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
	bool accept_connection(time_t, bool&);

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

	// update the file timestamps on our named pipe(s). this prevents
	// things like tmpwatch from removing them from the file system
	//
	void touch();

	// Determine if the named pipe on disk is the one that we've actually
	// opened. Return true if ok.
	bool consistent(void);

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
	NamedPipeWatchdogServer* m_watchdog_server;
	NamedPipeReader*         m_reader;
	NamedPipeWriter*         m_writer;
#endif
};

#endif
