#ifndef TIMEVAL_H
#define TIMEVAL_H

#if defined(ULTRIX43) && !defined(_ALL_SOURCE)
	struct timeval {
		long    tv_sec;     /* seconds */
		long    tv_usec;    /* and microseconds */
	};
	struct  itimerval {
		struct  timeval it_interval;    /* timer interval */
		struct  timeval it_value;   /* current value */
	};
#else
#	include <sys/time.h>
#endif

#endif
