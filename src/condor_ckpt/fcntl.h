#if defined(AIX32) && defined(__cplusplus )
#	include "fix_gnu_fcntl.h"
#elif defined(AIX32) && !defined(__cplusplus )
	typedef unsigned short ushort;
#	include <fcntl.h>
#elif defined(ULTRIX42) && defined(__cplusplus )
#	include "fix_gnu_fcntl.h"
#else
#	include <fcntl.h>
#endif
