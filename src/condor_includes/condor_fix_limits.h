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
#ifndef FIX_LIMITS_H
#define FIX_LIMITS_H

#if defined(HPUX9)
#   define select _hide_select
#endif

#include <limits.h>

#if defined(HPUX9)
#   undef select
#endif

/* 
** Linux doesn't define WORD_BIT unless __SVR4_I386_ABI_L1__ is
** defined, and this is all we need, so lets just define it.
** OSF doesn't define it unless _XOPEN_SOURCE is defined...
** IRIX needs some other whacky thing defined...
*/ 
#if defined(LINUX) || defined(OSF1) || defined(IRIX53)
#   if !defined(WORD_BIT)
#		define WORD_BIT 32
#	endif 
#endif /* LINUX || OSF1 || IRIX */

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX 255
#endif

#ifndef _POSIX_ARG_MAX
#define _POSIX_ARG_MAX 4096
#endif

#endif /* FIX_LIMITS_H */
