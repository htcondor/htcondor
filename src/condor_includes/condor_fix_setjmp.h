/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
