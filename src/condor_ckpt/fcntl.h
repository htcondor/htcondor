#if defined(__cplusplus) && !defined(OSF1)	/* GNU G++ */
#	include "fix_gnu_fcntl.h"
#elif defined(AIX32)						/* AIX bsdcc */
	typedef unsigned short ushort;
#	include <fcntl.h>
#elif defined(HPUX9)						/* HPUX 9 */
#	define fcntl __condor_hide_fcntl
#	include <fcntl.h>
#	undef fcntl
	int fcntl( int fd, int req, int arg );
#else										/* Everybody else */
#	include <fcntl.h>
#endif
