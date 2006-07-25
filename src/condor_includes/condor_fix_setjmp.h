/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _FIX_SETJMP

#define _FIX_SETJMP

#ifdef IRIX
/* #define _save_posix _POSIX90 - **WRONG** */
/* Now, technically, this might cause some problems because _POSIX90
   is generally defined to be something like defined(FOO) || (defined...
   but as we don't go changing what those defines are, we can safely
   reset these to be true or false. */
#if _POSIX90 /* Right! */
#	define _save_posix 1
#else
#	define _save_posix 0
#endif

/* #define _save_ansi _NO_ANSIMODE - **WRONG** */
#if _NO_ANSIMODE /* Right! */
#	define _save_ansi 1
#else
#	define _save_ansi 0
#endif
#undef _POSIX90
#undef _NO_ANSIMODE
#define _POSIX90 1
#define _NO_ANSIMODE 1
#endif

#if !defined(AIX32)

#if defined(LINUX) && defined(__FAVOR_BSD)
#	define CONDOR_FAVOR_BSD
#	undef __FAVOR_BSD
#endif

#include <setjmp.h>

#if defined(LINUX) && defined(CONDOR_FAVOR_BSD)
#	undef CONDOR_FAVOR_BSD
#	define __FAVOR_BSD
#endif

#ifdef IRIX
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
