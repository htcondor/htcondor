#ifndef CONDOR_FIX_SYS_WAIT_H
#define CONDOR_FIX_SYS_WAIT_H

#if defined(Solaris) && defined(_POSIX_C_SOURCE)
#	define HOLD_POSIX_C_SOURCE _POSIX_C_SOURCE
#	undef _POSIX_C_SOURCE
#endif

#if defined(OSF1) && !defined(_XOPEN_SOURCE)
#	define CONDOR_XOPEN_DEFINED
#	define _XOPEN_SOURCE
#endif

#if defined(IRIX53)
#	define _save_ansimode _NO_ANSIMODE
#	undef _NO_ANSIMODE
#	define _NO_ANSIMODE 0
#	define _save_xopen _XOPEN4UX
#	undef _XOPEN4UX
#	define _XOPEN4UX 1
#endif

#include <sys/wait.h>

#if defined(OSF1) && !defined(_XOPEN_SOURCE)
#	define CONDOR_XOPEN_DEFINED
#	define _XOPEN_SOURCE
#endif

#if defined(IRIX53)
#	undef _NO_ANSIMODE
#	define _NO_ANSIMODE _save_ansimode
#	undef _save_ansimode
#	undef _XOPEN4UX
#	define _XOPEN4UX _save_xopen
#	undef _save_xopen
#endif


#if defined(LINUX) && !defined( WCOREFLG )
#	define WCOREFLG 0200
#endif 

#if defined(WCOREFLAG) && !defined(WCOREFLG)
#	define WCOREFLG WCOREFLAG
#endif

#if defined(HOLD_POSIX_C_SOURCE)
#	define _POSIX_C_SOURCE HOLD_POSIX_C_SOURCE 
#	undef HOLD_POSIX_C_SOURCE 
#endif

#endif CONDOR_FIX_SYS_WAIT_H
