/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#ifndef IO_PROXY_HANDLER_H
#define IO_PROXY_HANDLER_H

/*
The IOProxyHandler object reads data from an incoming Stream,
and translates the Chirp protocol into remote system calls
back to the shadow.
*/

#include "../condor_daemon_core.V6/condor_daemon_core.h"

class IOProxyHandler : public Service {
public:
	IOProxyHandler();
	~IOProxyHandler();

	bool init( Stream *stream, const char *cookie );

private:
	int handle_request( Stream *s );
	void handle_cookie_request( ReliSock *r, char *line );
	void handle_standard_request( ReliSock *r, char *line );
	int convert( int result, int unix_errno );

	char *cookie;
	int got_cookie;
};

#endif



