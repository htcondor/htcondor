#if !defined(_FDSET_H)
#define _FDSET_H

#ifndef NBBY
#define NBBY 8
#endif

#if defined( ULTRIX42 ) || defined( ULTRIX43 )
#	define	FD_SETSIZE	4096
#	define NFDBITS (sizeof (fd_mask) * NBBY)   /* bits per mask */
#	define _DEFINE_FD_SET
	typedef long fd_mask;
#elif defined( OSF1 )
#   undef _DEFINE_FD_SET
#	ifdef _OSF_SOURCE
#		include <sys/select.h>
#	else
#		define _OSF_SOURCE
#		include <sys/select.h>
#		undef _OSF_SOURCE
#	endif
#elif defined( SUNOS41 )
#	undef _DEFINE_FD_SET
#elif defined(HPUX9)
#	undef _DEFINE_FD_SET
#elif defined(LINUX)
#	undef _DEFINE_FD_SET
#elif defined( AIX32 )
#	include <sys/select.h>
#elif defined( Solaris )
#       include <sys/select.h>
#elif defined( IRIX62 )
#	undef _DEFINE_FD_SET
#	ifdef  _BSD_TYPES 
#	    include <sys/select.h>
#	else
#	    define _BSD_TYPES	/* need to define this to get fd_set from select.h */
#	    include <sys/select.h>	/* IRIX defines all we need here, so use it! */
#		undef _BSD_TYPES
#	endif
#elif defined( IRIX53 )
#	undef _DEFINE_FD_SET 
#else
#	error "Don't know how to build fd_set for this platform"
#endif

#if defined( _DEFINE_FD_SET )
#	define	howmany(x, y)	(((x)+((y)-1))/(y))

	typedef	struct fd_set {
		fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
	} fd_set;
#	define FD_SET(n, p)  ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#	define FD_CLR(n, p)  ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#	define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#	define FD_ZERO(p)  memset((char *)(p), 0, sizeof(*(p)))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(AIX32) && !defined(HPUX9)
int select (int , fd_set *, fd_set *, fd_set *, struct timeval *);
#endif

#if !defined(AIX32)
#define NFDS(x) (x)
#endif


/*
   In HPUX9, the select call takes (int *) params, wheras SunOS, Solaris
   take (fd_set *) params.  We define an intermediate type to handle
   this.    -- Rajesh
*/
#if defined (HPUX9) && !defined(HPUX10)
typedef int *SELECT_FDSET_PTR;
#else
typedef fd_set *SELECT_FDSET_PTR;
#endif

#if defined(__cplusplus)
}
#endif


#endif
