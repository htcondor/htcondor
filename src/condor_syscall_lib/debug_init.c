#define _POSIX_SOURCE

#include "condor_syscall_mode.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include <assert.h>

void open_named_pipe( const char *name, int mode, int target_fd );

/*
  When we're debugging, we want to run the shadow and the user program
  each under a debugger in separate windows.  This is normally either
  difficult or impossible to do since the user program is generally born
  with two TCP connections to the shadow already open - one connection
  for remote system calls, and the other for logging.  To do the
  simplified debugging, we use named pipes to simulate the TCP sockets.
  Here we set up the named pipes at the correct file descriptors for
  each connection.
*/
debug_init()
{
	SetSyscalls( SYS_LOCAL | SYS_MAPPED );
	open_named_pipe( "/tmp/syscall_req", O_WRONLY, REQ_SOCK );
	open_named_pipe( "/tmp/syscall_rpl", O_RDONLY, RPL_SOCK );
	open_named_pipe( "/tmp/log", O_WRONLY, CLIENT_LOG );
	RSC_Init( RSC_SOCK, CLIENT_LOG );
	dprintf_init( CLIENT_LOG );
	DebugFlags = D_ALWAYS | D_NOHEADER;
	SetSyscalls( SYS_REMOTE | SYS_MAPPED );
}

/*
  Open the named pipe in the given mode, and get the file descriptor to
  be "target_fd".
*/
void
open_named_pipe( const char *name, int mode, int target_fd )
{
	int		fd;

	if( (fd=open(name,mode)) < 0 ) {
		assert( fd >= 0 );
	}

	if( fd != target_fd ) {
		assert( dup2(fd,target_fd) >= 0 );
		(void)close(fd);
	}
}
