#ifndef FIX_TYPES_H
#define FIX_TYPES_H

#include <sys/types.h>

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

typedef unsigned int u_int;

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
