/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#elif defined(AIX)
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

