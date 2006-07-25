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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "condor_syscall_mode.h"

extern "C" {

void open_named_pipe( const char *name, int access_mode, int target_fd );
ReliSock *RSC_Init( int, int );

/*
  When we're debugging, we want to run the shadow and its client each
  under a debugger in separate windows.  This is normally either
  difficult or impossible to do since both the user program and the
  condor starter are generally born with two TCP connections to the
  shadow already open - one connection for remote system calls, and the
  other for logging.  To do the simplified debugging, we use named
  pipes to simulate the TCP sockets.  Since this is only used for
  debugging purposes, we just hard wire in the names of the
  pipes:
		"/tmp/syscall_req"	System call request from the client to the shadow
		"/tmp/syscall_rpl"	System call replies from the shadow to the client
		"/tmp/log"			Logging info from the client to the shadow

  If the parameter "want_debug_mode" is TRUE, we set up the named pipes at
  the correct file descriptors for each connection.  If the "want_debug_mode"
  parameter is FALSE, the we assume the TCP connections already exist
  at the correct file descriptor numbers.

  In the case of debugging a user program, the idea is to test its use
  of remote system calls, so we would want to start it up in REMOTE
  system call mode - however in the case of debugging the
  condor_starter, it normally does local system calls, and calls the
  remote ones explicitly, so we woule want to start it up in LOCAL
  system call mode.  We therefore use "syscall_mode" parameter to get
  the desired behavior in either case.
*/
extern int InDebugMode;


ReliSock *
init_syscall_connection( int want_debug_mode )
{
	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	if( want_debug_mode ) {
		open_named_pipe( "/tmp/syscall_req", O_WRONLY, REQ_SOCK );
		open_named_pipe( "/tmp/syscall_rpl", O_RDONLY, RPL_SOCK );
		open_named_pipe( "/tmp/log", O_WRONLY, CLIENT_LOG );
		InDebugMode = TRUE;
	}

	return RSC_Init( RSC_SOCK, CLIENT_LOG );
}


/*
  Open the named pipe in the given mode, and get the file descriptor to
  be "target_fd".
*/
void
open_named_pipe( const char *name, int access_mode, int target_fd )
{
	int		fd;

	if( (fd=open(name,access_mode)) < 0 ) {
		assert( fd >= 0 );
	}

	if( fd != target_fd ) {
		assert( dup2(fd,target_fd) >= 0 );
		(void)close(fd);
	}
}

} /* extern "C" */
