/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
