#ifndef TIMEVAL_H
#define TIMEVAL_H

#if defined(ULTRIX42) || defined(ULTRIX43) || defined(HPUX9)
#if !defined(_ALL_SOURCE)
struct timeval {
	long    tv_sec;     /* seconds */
	long    tv_usec;    /* and microseconds */
};
#endif	/* _ALL_SOURCE */
#else
#include <sys/time.h>
#endif

#endif
