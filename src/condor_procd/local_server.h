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
#include "named_pipe_reader.h"
#include "named_pipe_writer.h"
#endif

class LocalServer {

public:

	// construct a new local server that listen for commands at the given
	// "address" (which is a FIFO pathname on UNIX and a named pipe name
	// on Windows)
	//
	LocalServer(const char*);

	// clean up
	//
	~LocalServer();

	// set the principal that is allows to connect to this server
	// (on Windows, this will be a SID; on UNIX, as UID)
	//
	void set_client_principal(char*);

	// wait up to the specified number of seconds to receive a client
	// connection, returning true if one is received
	//
	bool accept_connection(int);

	// close a connection, making it possible to accept another one
	// via the accept_connection method
	//
	void close_connection();

	// read data from a connected client
	//
	void read_data(void*, int);

	// write data to a connected client
	//
	void write_data(void*, int);

private:

	// implementation is totally different depending on whether we're
	// on Windows or UNIX. both use named pipes, but named pipes are not
	// (nearly) the same beast between the two
	//
#if defined(WIN32)
	HANDLE m_pipe;
	HANDLE m_event;
#else
	char* m_pipe_addr;
	NamedPipeReader m_reader;
	NamedPipeWriter* m_writer;
#endif
};

#endif
