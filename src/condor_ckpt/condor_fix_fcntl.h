#if defined(AIX32) && defined(__cplusplus )
#	include "fcntl.aix32.h"
#elif defined(AIX32) && !defined(__cplusplus )
	typedef unsigned short ushort;
#	include <fcntl.h>
#else
#	include <fcntl.h>
#endif
