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

#ifndef _CONDOR_MMAP_H
#define _CONDOR_MMAP_H

#include <sys/mman.h>


#if defined(LINUX)
#	if defined(GLIBC20) 
#		define MMAP_T char*
#	else
#		define MMAP_T void*
#	endif
#elif defined(IRIX)
#	define MA_SHARED	0x0008	/* mapping is a shared or mapped object */
#	define MMAP_T void*
#elif defined(Solaris)
#	define MMAP_T char*
#elif defined(HPUX)
#	define MMAP_T void*
#elif defined(OSF1)
#	define MMAP_T void*
#endif

#if !defined(MAP_FAILED)
#	define MAP_FAILED ((MMAP_T)-1)
#endif

#if defined(__cplusplus)
extern "C" {
#endif

MMAP_T	MMAP(MMAP_T, size_t, int, int, int, off_t);
MMAP_T	mmap(MMAP_T, size_t, int, int, int, off_t);

#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_MMAP_H */

