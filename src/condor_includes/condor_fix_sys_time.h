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
#ifndef CONDOR_FIX_SYS_TIME_H
#define CONDOR_FIX_SYS_TIME_H

#if defined(LINUX) && defined(GLIBC)
/* glibc defines prototypes for a bunch of functions that are supposed
   to take ints (according to the man page, POSIX, whatever) that
   really take enums.  -Derek Wright 4/17/98 */
#define getitimer __hide_getitimer
#define __getitimer __hide__getitimer
#define setitimer __hide_setitimer
#define __setitimer __hide__setitimer
#endif /* LINUX && GLIBC */

#if !defined(WIN32)
#include <sys/time.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(LINUX) && defined(GLIBC)
#undef getitimer
#undef __getitimer
#undef setitimer
#undef __setitimer

int getitimer(int, struct itimerval *);
int __getitimer(int, struct itimerval *);
int setitimer(int, const struct itimerval *,  struct itimerval *);
int __setitimer(int, const struct itimerval *,  struct itimerval *);

#endif /* LINUX && GLIBC */

#if defined(__cplusplus)
}
#endif

#endif /* CONDOR_FIX_SYS_TIME_H */
