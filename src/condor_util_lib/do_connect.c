/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "except.h"
#include "debug.h"
#include "clib.h"
#include <string.h>
#include "dgram_io_handle.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

extern int	errno;

#if defined(__STDC__)
unsigned short find_port_num( char *service_name, unsigned short dflt_port );
char *mk_config_name( const char *service_name );
char *param( const char *name );
#else
unsigned short find_port_num();
char *mk_config_name();
char *param();
#endif

do_connect( host, service, port )
char	*host, *service;
u_short		port;
{
	struct sockaddr_in	sin;
	struct hostent		*hostentp;
	int					status;
	int					fd;
	int					true = 1;

	hostentp = gethostbyname( host );
	if( hostentp == NULL ) {
#if defined(vax) && !defined(ultrix)
		herror( "gethostbyname" );
#endif
		dprintf( D_ALWAYS, "Can't find host \"%s\" (Nameserver down?)\n",
							host );
		return( -1 );
	}

	port = find_port_num( service, port );


	if( (fd=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

	if( setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,(caddr_t *)&true,sizeof(true)) < 0 ) {
		EXCEPT( "setsockopt( SO_KEEPALIVE )" );
	}

	bzero( (char *)&sin, sizeof sin );
	bcopy( hostentp->h_addr, (char *)&sin.sin_addr,
											(unsigned)hostentp->h_length );
	sin.sin_family = hostentp->h_addrtype;
	sin.sin_port = htons(port);

	/*
	dprintf( D_ALWAYS, "Connecting now...\n" );
	*/
	if( (status = connect(fd,(struct sockaddr *)&sin,sizeof(sin))) == 0 ) {
		/*
		dprintf( D_ALWAYS, "Done connecting\n" );
		*/
		return fd;
	} else {
		dprintf( D_FULLDEBUG, "connect returns %d, errno = %d\n", status, errno );
		(void)close( fd );
		return -1;
	}
}

udp_connect( host, port )
char	*host;
{
	int		sock;
	struct sockaddr_in	sin;
	struct hostent		*hostentp;

	hostentp = gethostbyname( host );
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


	bzero( (char *)&sin, sizeof sin );
	bcopy( hostentp->h_addr, (char *)&sin.sin_addr,
												(unsigned)hostentp->h_length );
	sin.sin_family = hostentp->h_addrtype;
	sin.sin_port = htons( (u_short)port );

	if( connect(sock,(struct sockaddr *)&sin,sizeof(sin)) < 0 ) {
		perror( "connect" );
		exit( 1 );
	}

	return sock;
}



/* create an unconnected datagram socket and bind(?) --Raghu */

int
udp_unconnect()
{
    int sock;
    struct sockaddr_in cli_addr;

    if( (sock=socket(AF_INET,SOCK_DGRAM,0)) < 0 ) {
            dprintf(D_ALWAYS, "unconnected dgram socket()" );
            EXCEPT("socket");
        }

    /* Bind */

     bzero( (char *)&cli_addr, sizeof(cli_addr));   /* zero out */
     cli_addr.sin_family = AF_INET;
     cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
     cli_addr.sin_port = htonl(0);

     if(bind(sock,(struct sockaddr *)&cli_addr, sizeof(cli_addr))<0) {      
                dprintf(D_ALWAYS, " bind failed in unconnected dgram");
                exit(1);
     }
     return sock;
}


unsigned short
find_port_num( service_name, dflt_port )
char	*service_name;
unsigned short	dflt_port;
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
		return (unsigned short)atoi( pval );
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
	ptr = strchr( service_name, '_' );
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



fill_dgram_io_handle(DGRAM_IO_HANDLE *handle, char *chost, int sock_fd, int port_num)
{
   struct hostent *serv_p;
   handle->sock = sock_fd;

   if((serv_p = gethostbyname(chost))== NULL) {
         EXCEPT("gethostbyname()");
   }
   
   bzero((char *)&(handle->addr), sizeof(handle->addr));
   memcpy((void *) &((handle->addr).sin_addr), serv_p->h_addr, serv_p->h_length);
   
   (handle->addr).sin_family = AF_INET;
   (handle->addr).sin_port = htons(port_num);

 }


/*--------------------------------------------------------

  tcp_accept_timeout() , tcp_connect_timeout()   are used
  to incorporate timeout facility into accept and connect
  for connection-oriented sockets  -- raghu  5/23
  THIS IS NOT TESTED OUT.  MAKE SURE IT WORKS BEFORE USING

*/


/* tcp_connect_timeout() returns -1  on error
                                 -2  on timeout
                                 sockfd itself if connect succeeds
*/

int tcp_connect_timeout( int sockfd, struct sockaddr *sin, int len,
						int timeout ) 
{
	int 			on=1, off=0;
	struct timeval  timer;
	fd_set          writefds;
	int             nfound;
	int             nfds;
	int             tmp_errno;


    /* We want to attempt the connect with the socket in non-blocking
           mode.  That way if there is some problem on the other end we
           won't get hung up indefinitely waiting to connect to one host.
           For some reason on AIX if you set this here, the connect()
           fails.  In that case errno doesn't get set either... */
#if !defined(AIX31) && !defined(AIX32)
	if( ioctl(sockfd,FIONBIO,(char *)&on) < 0 ) {
		EXCEPT( "ioctl" );
	}
#endif

	if( connect(sockfd, sin,len) < 0 ) {
		tmp_errno = errno;
		switch( errno ) {
		    case EINPROGRESS:
			    break;
			default:
				dprintf( D_ALWAYS,
						"Can't connect to host , errno =%d\n",
						tmp_errno );
				(void)close( sockfd );
				return -1;
			}
	}

#ifdef AIX31    /* see note above */
	if( ioctl(sockfd,FIONBIO,(char *)&on) < 0 ) {
		EXCEPT( "ioctl" );
	}
#endif AIX31


	timer.tv_sec = timeout;
	timer.tv_usec = 0;
	nfds = sockfd + 1;
	FD_ZERO( &writefds );
	FD_SET( sockfd, &writefds );
	nfound = select( nfds, (int *)0, (int *)&writefds, (int *)0,
					(struct timeval *)&timer );
	switch( nfound ) {
	    case 0:
		    (void)close( sockfd );
			return -2;
        case 1:
			if( ioctl(sockfd,FIONBIO,(char *)&off) < 0 ) {
				EXCEPT( "ioctl" );
			}
			return sockfd;
        default:
			EXCEPT( "Select returns %d", nfound );
			return -1;
    }
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

int tcp_accept_timeout(int ConnectionSock, struct sockadrr *sin, int *len,
                       int timeout) 
{
	int             count;
	fd_set  		readfds;
	struct timeval 	timer;
	struct sockaddr dummy;

    timer.tv_sec = timeout;
    timer.tv_usec = 0;
    FD_ZERO( &readfds );
    FD_SET( ConnectionSock, &readfds );
#if defined(AIX31) || defined(AIX32)
	errno = EINTR;  /* Shouldn't have to do this... */
#endif
    count = select(FD_SETSIZE, (int *)&readfds, (int *)0, (int *)0,
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

    if( NFDS(count) == 0 ) {
		return -2;
    } else if( FD_ISSET(ConnectionSock,&readfds) ) {
		return (  accept( ConnectionSock, (struct sockaddr *)sin, len)  );    
    } else {
		EXCEPT( "select: unknown connection, readfds = 0x%x, count = %d",
			   readfds, NFDS(count) );
    }
   
    return -1; 
}
