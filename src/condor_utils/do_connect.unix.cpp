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
#include "condor_sockfunc.h"
#include "ipv6_hostname.h"
#include "selector.h"

/*
 * FYI: This code is used by the old shadow/starter and by the syscall lib
 * lib linked inside the job. The other daemons use the do_connect in
 * condor_io/sock.C
 *
 */
extern "C" {
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
	const char	*cptr;
	char		*ptr;

		/* Copy part after the '_' to our answer */
	cptr = strchr( service_name, '_' );
	if( cptr == NULL ) {
		return NULL;
	}
	strcpy( answer, cptr + 1 );

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
int tcp_connect_timeout( int sockfd, const condor_sockaddr& serv_addr,
						int timeout )
{
	socklen_t		sz;
	int				val = 0;
	int				save_errno;

	/* if we don't want a timeout, then just call connect by itself in
		a blocking manner. */
	if (timeout == 0) {
		if (condor_connect(sockfd, serv_addr) < 0) {
			return -1;
		}

		return sockfd;
	}

	/* else do the circus act with a blocking connect and a timeout */

	if (set_fd_nonblocking(sockfd) < 0) {
		return -1;
	}

	/* try the connect, which will return immediately */
	if(condor_connect(sockfd, serv_addr) < 0) {
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
	Selector selector;
	selector.add_fd( sockfd, Selector::IO_WRITE );
	selector.set_timeout( timeout );

	/* Keep retrying if we get a signal. Under the right frequency of
	 * signals this may never end. This is improbable...
	 */
	do {
		selector.execute();
	} while ( selector.signalled() );

	if ( selector.failed() ) {
		/* wasn't a signal, so select failed with some other error */
		if (set_fd_blocking(sockfd) < 0) {
			return -1;
		}
		errno = selector.select_errno();
		/* The errno I'm returning here isn't right, since it is the
			one from select(), not the one from connect(), but there really
			isn't a good option at this point. */
		return -1;

	} else if ( selector.timed_out() ) {
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
	int				newsock;
	SOCKET_LENGTH_TYPE slt_len;

	slt_len = *len;

	Selector selector;
	selector.add_fd( ConnectionSock, Selector::IO_READ );
	selector.set_timeout( timeout );
	selector.execute();
	if ( selector.signalled() ) {
		dprintf( D_ALWAYS, "select() interrupted, restarting...\n");
		return -3;
	}
	if ( selector.failed() ) {
		EXCEPT( "select() returns %d, errno = %d", selector.select_retval(), selector.select_errno() );
    }
	/*
	 ** dprintf( D_FULLDEBUG, "Select returned %d\n", NFDS(count) );
	 */

    if( selector.timed_out() ) {
		return -2;
    } else if ( selector.fd_ready( ConnectionSock, Selector::IO_READ ) ) {
		newsock =  accept( ConnectionSock, (struct sockaddr *)sinful, (socklen_t*)&slt_len);
		if ( newsock > -1 ) {
			int on = 1;
			setsockopt( newsock, SOL_SOCKET, SO_KEEPALIVE, (char*)&on,
				sizeof(on) );
		}
		return (newsock);
    } else {
		EXCEPT( "select: unknown connection, count = %d", selector.select_retval() );
    }

    return -1;
}

