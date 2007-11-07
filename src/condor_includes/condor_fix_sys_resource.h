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

#ifndef CONDOR_FIX_SYS_RESOURCE_H
#define CONDOR_FIX_SYS_RESOURCE_H

#if defined(LINUX) && defined(GLIBC)
/* glibc defines prototypes for a bunch of functions that are supposed
   to take ints (according to the man page, POSIX, whatever) that
   really take enums.  -Derek Wright 4/17/98 */
#define getpriority __hide_getpriority
#define setpriority __hide_setpriority
#define getrlimit __hide_getrlimit
#define __getrlimit __hide__getrlimit
#define setrlimit __hide_setrlimit
#define getrusage __hide_getrusage
#define __getrusage __hide___getrusage

#endif /* LINUX && GLIBC */

#if !defined(WIN32)
#include <sys/resource.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(LINUX) && defined(GLIBC)
#undef getpriority
#undef setpriority
#undef getrlimit
#undef __getrlimit
#undef setrlimit
#undef getrusage
#undef __getrusage

int getrlimit(int, struct rlimit *);
int __getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
int getpriority(int, int);
int setpriority(int, int, int);
int getrusage(int, struct rusage * );
int __getrusage(int, struct rusage * );
#endif /* LINUX && GLIBC */

/* on a glibc 2.1+ machine r_lim_t is defined, so we undef it and let the typdef
	through */
#if defined(LINUX) && !defined(GLIBC20)
#	undef rlim_t
#else
	typedef long rlim_t;
#endif


#if defined(__cplusplus)
}
#endif

#endif /* CONDOR_FIX_SYS_RESOURCE_H */

