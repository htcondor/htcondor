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
#ifndef FIX_TYPES_H
#define FIX_TYPES_H

#if defined(OSF1)
/* We need to define _OSF_SOURCE so that type quad, and
   u_int and friends get defined.  We should leave it on since we'll
   need it later as well.*/
#define _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE
#define _OSF_SOURCE

#endif

#if	defined(ULTRIX43)
#define key_t       long
typedef int		bool_t;
#endif

/* define __EXTENSIONS__ on suns so u_int & friends get defined */
#if defined(Solaris) || defined(SUNOS41)
#define __EXTENSIONS__
#endif /* Solaris */

/* for IRIX62, we want _BSD_TYPES defined when we include sys/types.h, but
 * then we want to set it back to how it was. -Todd, 1/31/97
 */
#if defined(IRIX62)
#	ifdef _BSD_TYPES
#		include <sys/types.h>
#	else
#		define _BSD_TYPES
#		include <sys/types.h>
#		undef _BSD_TYPES
#	endif
#endif

#if defined(LINUX) && defined(GLIBC)
#define _BSD_SOURCE
#endif

#include <sys/types.h>

/*
The system include file defines this in terms of bzero(), but ANSI says
we should use memset().
*/
#if defined(OSF1)
#undef FD_ZERO
#define FD_ZERO(p)     memset((char *)(p), 0, sizeof(*(p)))

#endif /* OSF1 */

/*
Various non-POSIX conforming files which depend on sys/types.h will
need these extra definitions...
*/

#if defined(HPUX9) || defined(LINUX) || defined(Solaris) || defined(IRIX53) || defined(SUNOS41) || defined(OSF1)
#	define HAS_U_TYPES
#endif

#if !defined(HAS_U_TYPES)
	typedef unsigned int u_int;
	typedef unsigned char   u_char;
	typedef unsigned short  u_short;
	typedef unsigned long   u_long;
#endif


#if defined(LINUX)
typedef long rlim_t;
#endif

/* On IRIX, we need to compile senders.c with the stupid native CC compiler
 * because gcc/g++ is rather broken on this platform.  BUT, CC does not have
 * bool defined, so do it here. */
#if !defined(__GNUC__) && defined(IRIX53)
enum bool {false, true};
#endif

#endif





