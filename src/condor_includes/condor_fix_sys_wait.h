#ifndef CONDOR_FIX_SYS_WAIT_H
#define CONDOR_FIX_SYS_WAIT_H


#if defined(Solaris) && defined(_POSIX_C_SOURCE)
#define HOLD_POSIX_C_SOURCE _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#include <sys/wait.h>

#if defined(LINUX) && !defined( WCOREFLG )
#define WCOREFLG 0200
#endif 

#if defined(HOLD_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE HOLD_POSIX_C_SOURCE 
#endif

#endif CONDOR_FIX_SYS_WAIT_H
