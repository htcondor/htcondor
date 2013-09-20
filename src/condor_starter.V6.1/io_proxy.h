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


#ifndef IO_PROXY_H
#define IO_PROXY_H

/*
The IOProxy object listens on a socket for incoming
connections, and forks IOProxyHandler children to
handle the incoming connections.
*/

#include "condor_daemon_core.h"

class JICShadow;

class IOProxy : public Service {
public:
	IOProxy();
	~IOProxy();

	bool init( JICShadow * shadow, const char *config_file, bool want_io, bool want_updates, bool want_delayed );

private:
	int connect_callback( Stream * );

	ReliSock *server;
	JICShadow *m_shadow;
	char *cookie;
	bool socket_registered;

	bool m_want_io;
	bool m_want_updates;
	bool m_want_delayed;
};

#endif
