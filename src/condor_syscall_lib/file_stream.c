/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#if defined(OSF1) && !defined(__GNUC__)
#define __STDC__
#endif

#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_fix_assert.h"

#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

#include "condor_debug.h"
#include "condor_file_info.h"
static char *_FileName_ = __FILE__;

int open_tcp_stream( unsigned int ip_addr, unsigned short port );
int open_file_stream( const char *file, int flags, size_t *len );

/*
  Open a file using the stream access protocol.  If we are to access
  the file via remote system calls, then we use either CONDOR_put_file_stream
  or CONDOR_get_file_stream, according to whether the flags indicate the
  access is for reading or writing.  If we are to access the file
  some way other than remote system calls, just go ahead and do it.
*/
int
open_file_stream( const char *file, int flags, size_t *len )
{
	unsigned int	addr;
	unsigned short	port;
	int				fd = -1;
	char			local_path[ _POSIX_PATH_MAX ];
	int				pipe_fd;
	int				st;
	int				mode;
	int				scm;


		/* Ask the shadow how we should access this file */
	scm = SetSyscalls( SYS_REMOTE | SYS_MAPPED );
	mode = REMOTE_syscall( CONDOR_file_info, file, &pipe_fd, local_path );
	SetSyscalls( scm );

	if( mode < 0 ) {
		EXCEPT( "CONDOR_file_info" );
	}

	if( mode == IS_PRE_OPEN ) {
		exit( __LINE__ );
		EXCEPT( "The shadow says a stream file is a pre-opened pipe!\n" );
	}

		/* Try to access it using local system calls */
	if( mode == IS_NFS || mode == IS_AFS ) {
		fd = syscall( SYS_open, local_path, flags, 0664 );
		if( fd >= 0 ) {
			if( !(flags & O_WRONLY) ) {
				*len = syscall( SYS_lseek, fd, 0, 2 );
				syscall( SYS_lseek, fd, 0, 0 );
			}
			dprintf( D_ALWAYS, "Opened \"%s\" via local syscalls\n",local_path);
		}
	}

		/* Try to access it using the file stream protocol  */
	if( fd < 0 ) {
		if( flags & O_WRONLY ) {
			st = REMOTE_syscall(CONDOR_put_file_stream, file,*len,&addr,&port);
		} else {
			st = REMOTE_syscall(CONDOR_get_file_stream, file, len,&addr,&port);
		}

		if( st < 0 ) {
			dprintf( D_ALWAYS, "File stream access \"%s\" failed\n",local_path);
			return -1;
		}

			/* Connect to the specified party */
		fd = open_tcp_stream( addr, port );
		if( fd < 0 ) {
			dprintf( D_ALWAYS, "open_tcp_stream() failed\n" );
			return -1;
		} else {
			dprintf( D_ALWAYS,"Opened \"%s\" via file stream\n", local_path);
		}
	}

	if( MappingFileDescriptors() ) {
		fd = MarkOpen( file, flags, fd, 0 );
	}

	return fd;
}

/*
  Open a TCP connection at the given hostname and port number.  This
  will result in a file descriptor where we can read or write the
  file.  N.B. both the IP address and the port number are given in host
  byte order.
*/
int
open_tcp_stream( unsigned int ip_addr, unsigned short port )
{
	struct sockaddr_in	sin;
	int		fd;
	int		status;
	int		scm;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );


		/* generate a socket */
	fd = socket( AF_INET, SOCK_STREAM, 0 );
	assert( fd >= 0 );
	dprintf( D_FULLDEBUG, "Generated a data socket - fd = %d\n", fd );
		
		/* set the address */
	ip_addr = htonl( ip_addr );
	memset( &sin, '\0', sizeof sin );
	memcpy( &sin.sin_addr, &ip_addr, sizeof(ip_addr) );
	sin.sin_family = AF_INET;
	sin.sin_port = htons( port );

	dprintf( D_FULLDEBUG, "Internet address structure set up\n" );

	status = connect( fd,( struct sockaddr *)&sin, sizeof(sin) );
	if( status < 0 ) {
		dprintf( D_ALWAYS, "connect() failed - errno = %d\n", errno );
		exit( 1 );
	}
	dprintf( D_FULLDEBUG, "Connection completed - returning fd %d\n", fd );

	SetSyscalls( scm );

	return fd;
}
