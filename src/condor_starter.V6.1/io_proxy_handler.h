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


#ifndef IO_PROXY_HANDLER_H
#define IO_PROXY_HANDLER_H

/*
The IOProxyHandler object reads data from an incoming Stream,
and translates the Chirp protocol into remote system calls
back to the shadow.
*/

#include "condor_daemon_core.h"

class JICShadow;

class IOProxyHandler : public Service {
public:
	IOProxyHandler(JICShadow *shadow, bool enable_file, bool enable_updates, bool enable_volatile);
	~IOProxyHandler();

	bool init( Stream *stream, const char *cookie );

private:
	int handle_request( Stream *s );
	void handle_cookie_request( ReliSock *r, char *line );
	void handle_standard_request( ReliSock *r, char *line );
	int convert( int result, int unix_errno );
	void fix_chirp_path( char *path );

	JICShadow *m_shadow;
	char *cookie;
	int got_cookie;
	bool m_enable_files;
	bool m_enable_updates;
	bool m_enable_volatile;
};

#endif



