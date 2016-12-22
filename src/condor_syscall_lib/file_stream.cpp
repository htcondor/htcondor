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
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_debug.h"
#include "condor_file_info.h"
#include "internet.h"
#include "internet_obsolete.h"
#include "condor_sockfunc.h"
#include "std_univ_sock.h"

extern StdUnivSock * syscall_sock;

/* remote system call prototypes */
extern "C" {
	extern int REMOTE_CONDOR_file_info(const char *logical_name, int *fd, char **physical_name);
	extern int REMOTE_CONDOR_put_file_stream(const char *file, size_t len, unsigned int *ip_addr, u_short *port_num);
	extern int REMOTE_CONDOR_get_file_stream(const char *file, size_t *len, unsigned int *ip_addr, u_short *port_num);
}

extern "C" {
	// Used by image.cpp.
	int _condor_in_file_stream;

	// Used by remote_startup.c and xfer_file.c.
	int open_file_stream( const char *file, int flags, size_t *len );
}

// Cribbed from directConnectToSockAddr().  Could probably just call it
// between SetSyscalls(), but I don't know that SO_KEEPALIVE doesn't matter.
int open_tcp_stream( const condor_sockaddr & sa ) {
	int scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	int fd = socket( sa.get_aftype(), SOCK_STREAM, 0 );
	assert( fd != -1 );

	int rv = _condor_local_bind( TRUE, fd );
	if( rv != TRUE ) {
		close( fd );
		SetSyscalls( scm );
		return -1;
	}

	// condor_connect ends up pulling in param() via ip6_get_scope_id(),
	// which we can't allow, since this function is linked into standard
	// universe jobs.
	// rv = condor_connect( fd, sa );
	rv = connect( fd, sa.to_sockaddr(), sa.get_socklen() );
	if( rv != 0 ) {
		dprintf( D_ALWAYS, "condor_connect() failed - errno = %d (rv %d)\n", errno, rv );
		close( fd );
		SetSyscalls( scm );
		return -1;
	}

	SetSyscalls( scm );
	return fd;
}

/*
  Open a file using the stream access protocol.  If we are to access
  the file via remote system calls, then we use either CONDOR_put_file_stream
  or CONDOR_get_file_stream, according to whether the flags indicate the
  access is for reading or writing.  If we are to access the file
  some way other than remote system calls, just go ahead and do it.
*/
int open_file_stream( const char *file, int flags, size_t *len )
{
	unsigned int	addr;
	unsigned short	port;
	int				fd = -1;
	char			*local_path = NULL;
	int				pipe_fd;
	int				st;
	int				mode;

		/* first assume we'll open it locally */
	_condor_in_file_stream = FALSE;

		/* Ask the shadow how we should access this file */
	mode = REMOTE_CONDOR_file_info(file, &pipe_fd, &local_path );

	if( mode < 0 ) {
		EXCEPT( "CONDOR_file_info failed in open_file_stream" );
	}

	if( mode == IS_PRE_OPEN ) {
		fprintf( stderr, "CONDOR ERROR: The shadow says a stream file "
				 "is a pre-opened pipe!\n" );
		EXCEPT( "The shadow says a stream file is a pre-opened pipe!" );
	}

		/* Try to access it using local system calls */
	if( mode == IS_NFS || mode == IS_AFS ) {

/* This is to make PVM work because this file gets linked in with the Condor
	support daemons for the starter/shadow in addition to the user job
	and AIX doesn't have a syscall() */
#if defined(AIX)
		fd = safe_open_wrapper(local_path, flags, 0644);
#else
		fd = syscall( SYS_open, local_path, flags, 0664 );
#endif

		if( fd >= 0 ) {
			if( !(flags & O_WRONLY) ) {
/* This is to make PVM work because this file gets linked in with the Condor
	support daemons for the starter/shadow in addition to the user job
	and AIX doesn't have a syscall() */
#if defined(AIX)
				*len = lseek(fd, 0, 2);
				lseek(fd, 0, 0);
#else
				*len = syscall( SYS_lseek, fd, 0, 2 );
				syscall( SYS_lseek, fd, 0, 0 );
#endif
			}
			dprintf( D_ALWAYS, "Opened \"%s\" via local syscalls\n",local_path);
		}
	}

		/* Try to access it using the file stream protocol  */
	if( fd < 0 ) {
		if( flags & O_WRONLY ) {
			st = REMOTE_CONDOR_put_file_stream(file,*len,&addr,&port);
		} else {
			st = REMOTE_CONDOR_get_file_stream(file, len,&addr,&port);
		}

		if( st < 0 ) {
			dprintf( D_ALWAYS, "File stream access \"%s\" failed\n",local_path);
			free( local_path );
			return -1;
		}

		//
		// The shadows sends an address of 0 if we should instead assume that
		// the socket we're making the system call over shares an address
		// with the file server.  This makes it possible to support IPv6.
		//
		condor_sockaddr saFileServer;
		if( addr == 0 ) {
			 saFileServer = syscall_sock->peer_addr();
		} else {
			struct in_addr ip = { htonl( addr ) };
			saFileServer = condor_sockaddr( ip );
		}
		saFileServer.set_port( port );

			/* Connect to the specified party */
        dprintf( D_ALWAYS, "Opening TCP stream to %s\n", saFileServer.to_sinful().c_str() );
		fd = open_tcp_stream( saFileServer );
		if( fd < 0 ) {
			dprintf( D_ALWAYS, "open_tcp_stream() failed\n" );
			free( local_path );
			return -1;
		} else {
			dprintf( D_ALWAYS,"Opened \"%s\" via file stream\n", local_path);
		}
		_condor_in_file_stream = TRUE;
	}

	free( local_path );
	return fd;
}

