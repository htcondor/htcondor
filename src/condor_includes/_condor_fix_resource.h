#ifndef _RESOURCE_H
#define _RESOURCE_H

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(ULTRIX43)
#	include <time.h>
#endif

#if defined(SUNOS41)
	typedef int rlim_t;
#endif

#if !defined(HPUX9)
#include <sys/resource.h>
#else

/*
 * Resource utilization information.
 */

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	-1

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
					/* actual values kept in proc struct */
	long	ru_maxrss;
#define	ru_first	ru_ixrss
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data " */
	long	ru_isrss;		/* integral unshared stack " */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_ioch;		/* # of characters read/written */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
#define	ru_last		ru_nivcsw
};
#endif /* HPUX9 */

#if defined(ULTRIX43)
#if defined(__STDC__) || defined(__cplusplus)
	int getrlimit( int resource, struct rlimit *rlp );
	int setrlimit( int resource, const struct rlimit *rlp );
#else
	int getrlimit();
	int setrlimit();
#endif
#endif /* ULTRIX43 */

#if defined(__cplusplus)
}
#endif

#endif /* _RESOURCE_h */
