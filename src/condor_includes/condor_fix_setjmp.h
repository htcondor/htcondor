#ifndef _FIX_SETJMP

#define _FIX_SETJMP

#ifdef IRIX53
#define _save_posix _POSIX90
#define _save_ansi _NO_ANSIMODE
#undef _POSIX90
#undef _NO_ANSIMODE
#define _POSIX90 1
#define _NO_ANSIMODE 1
#endif

#if !defined(AIX32)

#include <setjmp.h>

#ifdef IRIX53
#undef _POSIX90
#undef _NO_ANSIMODE
#define _POSIX90 _save_posix
#define _NO_ANSIMODE _save_ansi
#undef _save_posix
#undef _save_ansi
#endif

#else	/* AIX32 fixups */
#ifndef _setjmp_h

extern "C" {

#ifdef __setjmp_h_recursive

#include_next <setjmp.h>

#else /* __setjmp_h_recursive */

#define __setjmp_h_recursive
#include_next <setjmp.h>
#define _setjmp_h 1

#endif /* __setjmp_h_recursive */
}

#endif	/* _setjmp_h */
#endif /* AIX32 fixups */

#endif /* _FIX_SETJMP */
