#ifndef FIX_WAIT_H
#define FIX_WAIT_H

#if defined(_POSIX_SOURCE)
#define HOLD_POSIX_SOURCE
#undef _POSIX_SOURCE
#endif
/* To get union wait on OSF1, _OSF_SOURCE & _BSD must be defined */

#if defined(OSF1)
#	if defined(_OSF_SOURCE)
#		define _TMP_OSF_SOURCE
#	else
#		define _OSF_SOURCE
#	endif

#	if defined(_BSD)
#		define _TMP_BSD
#	else
#		define _BSD
#	endif
#	define _POSIX_SOURCE
typedef pid_t		id_t;
#	include <sys/wait.h>
#endif

#if defined(IRIX62)
#	if defined(_NO_ANSIMODE)
#		define _TMP_NO_ANSIMODE
#	endif
#endif

#if defined(AIX32)
#       undef _POSIX_SOURCE
#       define _BSD
#       include <sys/m_wait.h>
#elif defined(OSF1)
#       undef _POSIX_SOURCE
#       define _BSD
#       define _OSF_SOURCE
#       include <sys/wait.h>
#elif defined(SUNOS41)
#       undef _POSIX_SOURCE
#       include <sys/wait.h>
#elif defined(ULTRIX43)
#       undef _POSIX_SOURCE
#       include <sys/wait.h>
#elif defined(HPUX9)
#       undef _POSIX_SOURCE
#		define _BSD
#       include <sys/wait.h>
#elif defined(LINUX)
#       undef _POSIX_SOURCE
#       define __USE_BSD
#       include <sys/wait.h>
#elif defined(IRIX62)
#		undef _POSIX_SOURCE
#		undef _NO_ANSIMODE
#		include <sys/wait.h>
#elif defined(IRIX53)
#		undef _POSIX_SOURCE
#		include <sys/wait.h>
#elif defined(Solaris)
#		include </usr/ucbinclude/sys/wait.h>
#endif

/*
#if defined(AIX32)
#	include <sys/m_wait.h>
#	define WNOHANG 1
#else
#	include <sys/wait.h>
#endif
*/

#if defined(OSF1)

#	if !defined(_TMP_OSF_SOURCE)
#		undef _OSF_SOURCE
#		endif
#	undef _TMP_OSF_SOURCE

#	if !defined(_TMP_BSD)
#		undef _BSD
#		endif
#	undef _TMP_BSD

#endif /* OSF1 */

#if defined(IRIX62)
#	if defined(_TMP_NO_ANSIMODE)
#		define _NO_ANSIMODE
#	endif
#	undef _TMP_NO_ANSIMODE
#endif   /* IRIX62 */

#if defined(HOLD_POSIX_SOURCE)
#define _POSIX_SOURCE
#undef HOLD_POSIX_SOURCE
#endif

#endif
