#ifndef FIX_TYPES_H
#define FIX_TYPES_H

	 /*
	 OSF/1 has this as an "unsigned long", but this is incorrect.  It
	 is used by lseek(), and must be able to hold negative values.
	 */
#if defined(OSF1)
#define off_t _hide_off_t
#endif

#include <sys/types.h>

#if defined(OSF1)
#undef off_t
typedef long off_t;
#endif

/*
The system include file defines this in terms of bzero(), but ANSI says
we should use memset().
*/
#if defined(OSF1)
#undef FD_ZERO
#define FD_ZERO(p)     memset((char *)(p), 0, sizeof(*(p)))
#endif

/*
Various non-POSIX conforming files which depend on sys/types.h will
need these extra definitions...
*/

#if !defined(HPUX9)
	typedef unsigned int u_int;
	typedef unsigned char   u_char;
	typedef unsigned short  u_short;
	typedef unsigned long   u_long;
#endif

#if defined(AIX32)
typedef unsigned short ushort;
#endif

#if defined(ULTRIX42) ||  defined(ULTRIX43)
typedef char * caddr_t;
#endif

#if defined(AIX32)
typedef unsigned long rlim_t;
#endif

#endif
