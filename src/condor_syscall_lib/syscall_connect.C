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
