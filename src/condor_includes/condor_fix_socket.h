#ifndef FIX_SOCKET_H
#define FIX_SOCKET_H

#include <sys/socket.h>


/*
  For some reason the g++ includes on Ultrix 4.3 fail to provide
  these prototypes, even though the Ultrix 4.2 includes did...
*/
#if defined(ULTRIX43)

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
