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

/* on a glibc 2.1 machine r_lim_t is defined, so we undef it and let the typdef
	through */
#if defined(LINUX) && (defined(GLIBC21) || defined(GLIBC22) || defined(GLIBC23))
#	undef rlim_t
#else
	typedef long rlim_t;
#endif


#if defined(__cplusplus)
}
#endif

#endif /* CONDOR_FIX_SYS_RESOURCE_H */

