#ifndef FDSET_H
#define FDSET_H


#define NBBY 8


#if defined( ULTRIX42 ) || defined( ULTRIX43 )
#	define	FD_SETSIZE	4096
#	define NFDBITS (sizeof (fd_mask) * NBBY)   /* bits per mask */
#	define _DEFINE_FD_SET
	typedef long fd_mask;
#elif defined( OSF1 )
#	define	FD_SETSIZE	4096
#	define NFDBITS (sizeof (fd_mask) * NBBY)   /* bits per mask */
#	define _DEFINE_FD_SET
	typedef int	fd_mask;
#elif defined( SUNOS41 )
#	define	FD_SETSIZE	256
#	define NFDBITS (sizeof (fd_mask) * NBBY)   /* bits per mask */
#	define _DEFINE_FD_SET
	typedef long fd_mask;
#elif defined( AIX32 )
#	include <sys/select.h>
#else
#	error "Don't know how to build fd_set for this platform"
#endif





#if defined( _DEFINE_FD_SET )
#define	howmany(x, y)	(((x)+((y)-1))/(y))

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;
#define FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)  memset((char *)(p), 0, sizeof(*(p)))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(AIX32)
int select (int , fd_set *, fd_set *, fd_set *, struct timeval *);
#define NFDS(x) (x)
#endif

#if defined(__cplusplus)
}
#endif


#endif
