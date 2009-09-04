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
#include "internet.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "condor_netdb.h"
#include "util_lib_proto.h"
#include "condor_network.h"

/*
 * FYI: This code is used by the old shadow/starter and by the syscall lib
 * lib linked inside the job. The other daemons use the do_connect in
 * condor_io/sock.C
 *
 */
unsigned short find_port_num( const char *service_name, 
							  unsigned short dflt_port );
char *mk_config_name( const char *service_name );
char *param( const char *name );

int tcp_connect_timeout( int sockfd, struct sockaddr *sinful, int len,
						int timeout );
int do_connect_with_timeout( const char* host, const char* service, 
							 u_short port, int timeout );
int set_fd_blocking(int fd);
int set_fd_nonblocking(int fd);

int
do_connect( const char* host, const char* service, u_short port )
{
	return do_connect_with_timeout(host, service, port, 0);
}


int
do_connect_with_timeout( const char* host, const char* service, 
						 u_short port, int timeout ) 
{
	struct sockaddr_in	sinful;
	struct hostent		*hostentp;
	int					status;
	int					fd;
	int					True = 1;

	if( (fd=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

	if( setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,(const char *)&True,sizeof(True)) < 0 ) {
		close(fd);
		EXCEPT( "setsockopt( SO_KEEPALIVE )" );
	}

		/* Now, bind this socket to the right interface. */
		/* TRUE means this is an outgoing connection */
	_condor_local_bind( TRUE, fd );

    if (host[0]=='<'){ /* dhaval */
    	string_to_sin(host,&sinful);
    } else {
		hostentp = condor_gethostbyname( host );
		if( hostentp == NULL ) {
		#if defined(vax) && !defined(ultrix)
			herror( "gethostbyname" );
		#endif
			dprintf( D_ALWAYS, "Can't find host \"%s\" (Nameserver down?)\n",
								host );
			close(fd);
			return( -1 );
		}
		port = find_port_num( service, port );
		memset( (char *)&sinful,0,sizeof(sinful) );
		memcpy( (char *)&sinful.sin_addr, hostentp->h_addr, (unsigned)hostentp->h_length );
		sinful.sin_family = hostentp->h_addrtype;
		sinful.sin_port = htons(port);
	}

	if (timeout == 0) {
		status = connect(fd,(struct sockaddr *)&sinful,sizeof(sinful));
	} else {
		// This code path is available if one calls this function with
		// a timeout > 0. As of the writing of this comment, all invocations
		// of this function used 0 for a timeout so this codepath never
		// got called in practice. During the time previous to this comment,
		// tcp_connect_timeout() was a broken piece of garabge which didn't
		// work. However, I had to fix up that function into working state
		// for an unrelated feature elsewhere in the code, and found this
		// code path used it. Determining this code path's usage of
		// tcp_connect_timeout() is beyond the scope of my changes which 
		// led to this comment--hence, the EXCEPT. Your job is to figure
		// out if tcp_connect_timeout() is being called corectly here and
		// semantically does what you think it should in accordance to
		// the surrounding code in this function. If not, the code in this
		// function is likely the stuff to change.
		EXCEPT("This is the first time this code path has been taken, please "
			"ensure it does what you think it does.");
		status = tcp_connect_timeout(fd, (struct sockaddr*)&sinful, 
									 sizeof(sinful), timeout);
		if (status == fd) {
			status = 0;
		}
	}

	if( status == 0 ) {
		return fd;
	} else {
		dprintf( D_ALWAYS, "connect returns %d, errno = %d\n", status, errno );
		(void)close( fd );
		return -1;
	}
}


int
udp_connect( char* host, u_short port )
{
	int		sock;
	struct sockaddr_in	sinful;
	struct hostent		*hostentp;

	hostentp = condor_gethostbyname( host );
	if( hostentp == NULL ) {
#if defined(vax) && !defined(ultrix)
		herror( "gethostbyname" );
#endif
		printf( "Can't find host \"%s\" (Nameserver down?)\n",
							host );
		return( -1 );
	}

	if( (sock=socket(AF_INET,SOCK_DGRAM,0)) < 0 ) {
		perror( "socket" );
		exit( 1 );
	}

		/* Now, bind this socket to the right interface. */
		/* TRUE means this is an outgoing connection */
	_condor_local_bind( TRUE, sock );

	memset( (char *)&sinful,0,sizeof(sinful) );
	memcpy( (char *)&sinful.sin_addr, hostentp->h_addr, (unsigned)hostentp->h_length );
	sinful.sin_family = hostentp->h_addrtype;
	sinful.sin_port = htons( (u_short)port );

	if( connect(sock,(struct sockaddr *)&sinful,sizeof(sinful)) < 0 ) {
		perror( "connect" );
		exit( 1 );
	}

	return sock;
}


unsigned short
find_port_num( const char* service_name, unsigned short dflt_port )
{
	struct servent		*servp;
	char				*config_name;
	char				*pval;

	if( service_name == NULL || service_name[0] == '\0' ) {
		return dflt_port;
	}

		/* Try to look up port number in config file */
	config_name = mk_config_name( service_name );
	pval = param( config_name );
	if( pval != NULL ) {
		unsigned short rc = atoi( pval );
		free( pval );
		return rc;
	}

		/* Try to find in "/etc/services" */
	if( service_name && service_name[0] ) {
		servp = getservbyname(service_name, "tcp");
		if( servp != NULL ) {
			return servp->s_port;
		}
	}

		/* Fall back on the default */
	return dflt_port;
}

/*
  Convert a condor service name which looks like:

	condor_schedd

  to a macro name for a port number which we can look up in our config
  file.  The macro name looks like:

	SCHEDD_PORT

*/
char *
mk_config_name( const char *service_name )
{
	static char answer[ 512 ];
	char	*ptr;

		/* Copy part after the '_' to our answer */
	ptr = (char *)strchr((const char *) service_name, '_' );
	if( ptr == NULL ) {
		return NULL;
	}
	strcpy( answer, ptr + 1 );

		/* Transform it to upper case */
	for( ptr=answer; *ptr; ptr++ ) {
		if( islower(*ptr) ) {
			*ptr = toupper(*ptr);
		}
	}

		/* add on the last part */
	strcat( answer, "_PORT" );

	return answer;
}


/* tcp_connect_timeout() returns -1  on error
                                 -2  on timeout
                                 sockfd itself if connect succeeds
*/



/* This is only to be used on blocking sockets. Also, if this function returns
	anything other than success, it is probably good practice to close the
	socket and try again since you can't tell if it is left in a blocking
	or nonblocking state depending where the error happened. A timeout
	of zero or a negative value means block forever in the connect. */
int tcp_connect_timeout( int sockfd, struct sockaddr *sinful, int len,
						int timeout )
{
	struct timeval  timer;
	fd_set			writefds;
	int				nfound;
	int				nfds;
	int				tmp_errno;
	socklen_t		sz;
	int				val = 0;
	int				save_errno;

	/* if we don't want a timeout, then just call connect by itself in 
		a blocking manner. */
	if (timeout == 0) {
		if (connect(sockfd, sinful, len) < 0) {
			return -1;
		}

		return sockfd;
	}

	/* else do the circus act with a blocking connect and a timeout */

	if (set_fd_nonblocking(sockfd) < 0) {
		return -1;
	}

	/* try the connect, which will return immediately */
	if( connect(sockfd, sinful,len) < 0 ) {
		tmp_errno = errno;
		switch( errno ) {
			case EAGAIN:
			case EINPROGRESS:
				break;
			default:
				if (set_fd_blocking(sockfd) < 0) {
					return -1;
				}
				return -1;
			}
	}

	/* set up the wait for the timeout. yeah, I know, dueling select loops
		and all, it sucks a lot when it is the schedd doing it. :(
	*/
	timer.tv_sec = timeout;
	timer.tv_usec = 0;
	nfds = sockfd + 1;
	FD_ZERO( &writefds );
	FD_SET( sockfd, &writefds );

	DO_AGAIN:
	nfound = select( nfds,
					(fd_set*) 0,
					(fd_set*) &writefds,
					(fd_set*) 0,
					(struct timeval *)&timer );
	
	if (nfound < 0) {
		if (errno == EINTR) {

			/* Got a signal, try again. Under the right frequency of signals
				this may never end. This is improbable... */
			timer.tv_sec = timeout;
			timer.tv_usec = 0;
			nfds = sockfd + 1;
			FD_ZERO( &writefds );
			FD_SET( sockfd, &writefds );
			goto DO_AGAIN;
		}

		/* wasn't a signal, so select failed with some other error */
		save_errno = errno;
		if (set_fd_blocking(sockfd) < 0) {
			return -1;
		}
		errno = save_errno;
		/* The errno I'm returning here isn't right, since it is the
			one from select(), not the one from connect(), but there really
			isn't a good option at this point. */
		return -1;

	} else if (nfound == 0) {
		/* connection timed out */

		if (set_fd_blocking(sockfd) < 0) {
			return -1;
		}
		return -2;
	}
	
	/* just because the select returned with something doesn't really mean the
		connect is ok, so we'll check it before returning it to the caller
		to ensure things went ok. */
	sz = sizeof(int);
	if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&val), &sz) < 0) {
		/* on some architectures, this call *itself* returns with the 
			error instead of putting it into val, so we'll deal with that
			here.  Of course if it fails for some other legitimate reason, 
			the caller would be hard pressed to figure it out...
		*/
		save_errno = errno;
		if (set_fd_blocking(sockfd) < 0) {
			return -1;
		}
		errno = save_errno;
		return -1;
	}

	/* Now we check the actual value the call returned to us. */
	if (val != 0) {
		/* otherwise, the getsockopt will know how the connect failed */
		save_errno = errno;
		if (set_fd_blocking(sockfd) < 0) {
			return -1;
		}
		errno = save_errno;
		return -1;
	}

	/* set it back to blocking so the caller expects the right behavior */
	if (set_fd_blocking(sockfd) < 0) {
		return -1;
	}
	
	/* if I got to here, the connect succeeded */
	return sockfd;
}

int set_fd_nonblocking(int fd)
{
	int flags;

	/* do it like how we do it in daemoncore */
	if ( (flags=fcntl(fd, F_GETFL)) < 0 ) {
		return -1;
	} else {
		flags |= O_NONBLOCK;  // set nonblocking mode
		if ( fcntl(fd,F_SETFL,flags) == -1 ) {
			return -1;
		}
	}

	return 0;
}

int set_fd_blocking(int fd)
{
	int flags;

	/* do it like how we do it in daemoncore */
	if ( (flags=fcntl(fd, F_GETFL)) < 0 ) {
		return -1;
	} else {
		flags &= ~O_NONBLOCK;  // unset nonblocking mode
		if ( fcntl(fd,F_SETFL,flags) == -1 ) {
			return -1;
		}
	}

	return 0;
}


/*
 tcp_accept_timeout() : accept with timeout
 most of this code is got from negotiator.c
 
  returns       -1 on error
                -2 on timeout
                -3 if there is an interrupt
                a newfd > 0  on success

 NOT TESTED.  --raghu 5/23
*/

int tcp_accept_timeout(int ConnectionSock, struct sockaddr *sinful, int *len,
                       int timeout) 
{
	int             count;
	fd_set  		readfds;
	struct timeval 	timer;
	int				newsock;
	SOCKET_LENGTH_TYPE slt_len;

	slt_len = *len;

    timer.tv_sec = timeout;
    timer.tv_usec = 0;
    FD_ZERO( &readfds );
    FD_SET( ConnectionSock, &readfds );
#if defined(AIX31) || defined(AIX32)
	errno = EINTR;  /* Shouldn't have to do this... */
#endif
    count = select(ConnectionSock+1, 
				   (SELECT_FDSET_PTR) &readfds, 
				   (SELECT_FDSET_PTR) 0, 
				   (SELECT_FDSET_PTR) 0,
                   (struct timeval *)&timer );
    if( count < 0 ) {
		if( errno == EINTR ) {
			dprintf( D_ALWAYS, "select() interrupted, restarting...\n");
			return -3;
		} else {
			EXCEPT( "select() returns %d, errno = %d", count, errno );
		}
    }
	/*
	 ** dprintf( D_FULLDEBUG, "Select returned %d\n", NFDS(count) );	
	 */

    if( count == 0 ) {
		return -2;
    } else if( FD_ISSET(ConnectionSock,&readfds) ) {
		newsock =  accept( ConnectionSock, (struct sockaddr *)sinful, &slt_len);
		if ( newsock > -1 ) {
			int on = 1;
			setsockopt( newsock, SOL_SOCKET, SO_KEEPALIVE, (char*)&on,
				sizeof(on) );
		}
		return (newsock);
    } else {
		EXCEPT( "select: unknown connection, count = %d", count );
    }
   
    return -1; 
}

