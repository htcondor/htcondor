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

#ifndef CONDOR_MACROS_H
#define CONDOR_MACROS_H

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

	/* When using pthreads, pthread_sigmask() in place of sigprocmask(),
	 * except in the ckpt or remote syscall code (which doesn't deal
	 * with pthreads anyhow).
	 */
#if !defined(IN_CKPT_LIB) && !defined(REMOTE_SYSCALLS) && defined(HAVE_PTHREAD_SIGMASK)
#	ifdef sigprocmask
#		undef sigprocmask
#	endif
#	define sigprocmask pthread_sigmask
#endif

	/* Don't prevent calls to open() or fopen() in the ckpt or 
	 * remote syscalls code.
	 */
#if defined(IN_CKPT_LIB) || defined(REMOTE_SYSCALLS)
#	if !defined(_CONDOR_ALLOW_OPEN_AND_FOPEN)
#		define _CONDOR_ALLOW_OPEN_AND_FOPEN 1
#	endif
#endif

	/* For security purposes, do not allow open() or fopen().
	 * Folks should use the wrapper functions in 
	 * src/condor_c++_util/condor_open.[hC] 
	 */
#ifdef _CONDOR_ALLOW_OPEN_AND_FOPEN
#  ifndef _CONDOR_ALLOW_OPEN
#     define _CONDOR_ALLOW_OPEN 1
#  endif
#  ifndef _CONDOR_ALLOW_FOPEN
#     define _CONDOR_ALLOW_FOPEN 1
#  endif
#endif

#ifndef _CONDOR_ALLOW_OPEN
#  include "../condor_c++_util/condor_open.h"
#  ifdef open
#    undef open
#  endif
#  define open (Calls_to_open_must_use___safe_open_wrapper___instead)   
#  ifdef __GNUC__
#    pragma GCC poison Calls_to_open_must_use___safe_open_wrapper___instead
#  endif
#  endif

#ifndef _CONDOR_ALLOW_FOPEN
#  include "../condor_c++_util/condor_open.h"
#  ifdef fopen
#    undef fopen
#  endif
/* Condor's fopen macro does not play well with the new 
   version of the Platform SDKs (see cstdio for details) */
#if !defined (WIN32)
#   define fopen (Calls_to_fopen_must_use___safe_fopen_wrapper___instead)   
#endif
#  ifdef __GNUC__
#    pragma GCC poison Calls_to_fopen_must_use___safe_fopen_wrapper___instead
#  endif
#endif 

#endif /* CONDOR_MACROS_H */
