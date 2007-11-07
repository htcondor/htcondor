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
