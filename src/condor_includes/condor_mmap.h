/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


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
#	if defined(REMOTE_SYSCALLS)
		/* Stupid solaris, stupid compilers, stupid life. */
#		define MMAP_T void*
#	else
#		define MMAP_T caddr_t
#	endif
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

