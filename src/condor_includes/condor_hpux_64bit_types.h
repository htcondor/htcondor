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
