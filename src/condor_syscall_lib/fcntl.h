#if defined(__cplusplus) && !defined(OSF1)	/* GNU G++ */
#	include "fix_gnu_fcntl.h"
#elif defined(AIX32)						/* AIX bsdcc */
#	if !defined(_ALL_SOURCE)
		typedef ushort_t    ushort;
#	endif
#	include <fcntl.h>
#elif defined(HPUX9) | defined(OSF1)
#	define fcntl __condor_hide_fcntl
#	include <fcntl.h>
#	undef fcntl
	int fcntl( int fd, int req, int arg );
#else										/* Everybody else */
#	include <fcntl.h>
#endif
