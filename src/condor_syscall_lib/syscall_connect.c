#define _POSIX_SOURCE

#include "condor_syscall_mode.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include <assert.h>

void open_named_pipe( const char *name, int access_mode, int target_fd );
XDR *RSC_Init( int, int );

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


XDR *
init_syscall_connection( int want_debug_mode )
{
	XDR	*answer;

	SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	if( want_debug_mode ) {
		open_named_pipe( "/tmp/syscall_req", O_WRONLY, REQ_SOCK );
		pre_open( REQ_SOCK, FALSE, TRUE );
		open_named_pipe( "/tmp/syscall_rpl", O_RDONLY, RPL_SOCK );
		pre_open( RPL_SOCK, TRUE, FALSE );
		open_named_pipe( "/tmp/log", O_WRONLY, CLIENT_LOG );
		pre_open( CLIENT_LOG, FALSE, TRUE );
		InDebugMode = TRUE;
	} else {
		pre_open( RSC_SOCK, TRUE, TRUE );
		pre_open( CLIENT_LOG, FALSE, TRUE );
	}

	answer = RSC_Init( RSC_SOCK, CLIENT_LOG );
	dprintf_init( CLIENT_LOG );
	DebugFlags = D_ALWAYS | D_NOHEADER;

	return answer;
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
