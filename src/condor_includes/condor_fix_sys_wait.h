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

#if defined(OSF1) && defined(CONDOR_XOPEN_DEFINED)
#	undef CONDOR_XOPEN_DEFINED
#	undef _XOPEN_SOURCE
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
