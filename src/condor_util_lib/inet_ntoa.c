#include <arpa/inet.h>
#include <sys/socket.h>

/* defined in <netinet/in.h> on some systems ifdef INET6 */
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16 
#endif

/* inet_ntoa is broken in current version of gcc in IRIX for n32 ABI */
char *
inet_ntoa( struct in_addr inaddr ) {
	static char buffer[INET_ADDRSTRLEN];

	return( inet_ntop( AF_INET, (void *) &inaddr, buffer, 
			(size_t) INET_ADDRSTRLEN ) );
}
