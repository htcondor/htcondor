#if !defined(_TIMEVAL_H)
#define _TIMEVAL_H

#if defined(ULTRIX43) && !defined(_ALL_SOURCE)
	struct timeval {
		long    tv_sec;     /* seconds */
		long    tv_usec;    /* and microseconds */
	};
	struct  itimerval {
		struct  timeval it_interval;    /* timer interval */
		struct  timeval it_value;   /* current value */
	};
	struct timezone {
    	int tz_minuteswest; /* minutes west of Greenwich */
    	int tz_dsttime; /* type of dst correction */
	};

#elif defined(Solaris251)
#  define __EXTENSIONS__
#	 include <sys/time.h>
#  undef __EXTENSIONS__

#if 0

#elif defined(Solaris)
/* Solaris specific change ..dhaval 6/26 */
#  if defined(_POSIX_SOURCE)
#    define HOLD_POSIX_SOURCE
#    undef _POSIX_SOURCE
#  endif /* _POSIX_SOURCE */
#  include <sys/types.h>
#  if defined(HOLD_POSIX_SOURCE)
#    define _POSIX_SOURCE
#  endif

#endif 0


#else  
#  include <sys/time.h> 

#endif 


#endif
