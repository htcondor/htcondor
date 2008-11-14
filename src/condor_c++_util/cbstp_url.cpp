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


#include "condor_common.h"
#include "url_condor.h"
#include "internet.h"
#include "condor_debug.h"

// We need to match a function prototype in class URLProtocol, but we
// don't care about flags, as we're just going to connect to a socket
// and puke bytes at it.
int condor_bytes_stream_open_ckpt_file( const char *name, int /* flags */ )
{
	struct sockaddr_in	sin;
	int		sock_fd;
	int		status;

	string_to_sin(name, &sin);
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "socket() failed, errno = %d\n", errno);
		fflush(stderr);
		return sock_fd;
	}

	/* TRUE means this is an outgoing connection */
	if( ! _condor_local_bind(TRUE, sock_fd) ) {
		close( sock_fd );
		return -1;
	}

	sin.sin_family = AF_INET;
	status = connect(sock_fd, (struct sockaddr *) &sin, sizeof(sin));
	if (status < 0) {
		fprintf(stderr, "cbstp connect() FAILED, errno = %d\n", errno);
		fflush(stderr);
		close( sock_fd );
		return status;
	}
	return sock_fd;
}


#if 0
URLProtocol CBSTP_URL("cbstp", 
					 "CondorByteStreamProtocol",
					 condor_bytes_stream_open_ckpt_file);
#endif

void
init_cbstp()
{
    static URLProtocol	*CBSTP_URL = 0;

    if (CBSTP_URL == 0) {
        CBSTP_URL = new URLProtocol("cbstp", 
				    "CondorByteStreamProtocol",
				    condor_bytes_stream_open_ckpt_file);
    }
}
