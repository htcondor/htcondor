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

#include "condor_common.h"
#include "url_condor.h"
#include "internet.h"
#include "condor_debug.h"

int condor_bytes_stream_open_ckpt_file( const char *name, int flags, 
									   size_t n_bytes )
{
	struct sockaddr_in	sin;
	int		sock_fd;
	int		status;

	// cast discards const	
	string_to_sin((char *) name, &sin);
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		fprintf(stderr, "socket() failed, errno = %d\n", errno);
		fflush(stderr);
		return sock_fd;
	}

	if( ! _condor_local_bind(sock_fd) ) {
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
