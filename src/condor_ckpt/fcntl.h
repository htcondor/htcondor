#if defined(__cplusplus) && !defined(OSF1)	/* GNU G++ */
#	include "fix_gnu_fcntl.h"
#elif defined(AIX32)						/* AIX bsdcc */
#	typedef unsigned short ushort;
#	include <fcntl.h>
#else										/* Everybody else */
#	include <fcntl.h>
#endif
