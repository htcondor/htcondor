#ifndef FIX_SOCKET_H
#define FIX_SOCKET_H

#if !defined(WIN32)
#include <sys/socket.h>
#endif


/*
  For some reason the g++ includes on Ultrix 4.3 fail to provide
  these prototypes, even though the Ultrix 4.2 includes did...
  Same goes for OSF1  JCP
  Same goes for SUNOS - mike
*/
#if defined(ULTRIX43) || defined(OSF1) || defined(SUNOS41)

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
	int getpeername( int, struct sockaddr *, int * );
	int socket( int, int, int );
	int connect( int, const struct sockaddr *, int );
#else
	int getpeername();
	int socket();
	int connect();
#endif

#if defined(__cplusplus)
}
#endif

#endif	/* ULTRIX43 */

#endif
