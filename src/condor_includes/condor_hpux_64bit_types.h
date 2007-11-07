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

#ifndef CONDOR_HPUX_64BIT_TYPES_H
#define CONDOR_HPUX_64BIT_TYPES_H

/* This is pretty ugly, but it's how the system header files define
   struct stat64 and struct rlimit64 when _LARGEFILE64_SOURCE is
   defined.  We don't want that always defined on HPUX, so we just do
   this directly here. */

#if defined(HPUX10)
typedef int64_t off64_t;
typedef uint64_t rlim64_t;
typedef int64_t blkcnt64_t;
#endif

#define __stat stat64
#if defined(HPUX10)
#define blkcnt_t blkcnt64_t
#define off_t off64_t
#define ad_long_t long
#elif defined(HPUX11)
#define _T_INO_T      ino_t
#define _T_OFF_T      off64_t
#define _T_BLKCNT_T   blkcnt64_t
#else
#error Inspect the 64 bit stat type specification!
#endif
#include <sys/_stat_body.h>
#if defined(HPUX10)
#undef ad_long_t
#undef off_t
#undef blkcnt_t
#elif defined(HPUX11)
#undef _T_INO_T
#undef _T_OFF_T
#undef _T_BLKCNT_T
#else
#error Inspect the 64 bit stat type specification!
#endif
#undef __stat

#define __rlimit  rlimit64
#if defined(HPUX10)
#define rlim_t rlim64_t
#elif defined(HPUX11)
#define _T_RLIM_T rlim64_t
#else
#error Inspect the 64 bit rlimit type specification!
#endif
#include <sys/_rlimit_body.h>
#if defined(HPUX10)
#undef rlim_t
#elif defined(HPUX11)
#undef _T_RLIM_T
#else
#error Inspect the 64 bit rlimit type specification!
#endif
#undef __rlimit

#endif /* CONDOR_HPUX_64BIT_TYPES_H */
