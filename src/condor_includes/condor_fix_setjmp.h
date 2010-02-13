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

#ifndef _FIX_SETJMP

#define _FIX_SETJMP

#if !defined(AIX32)

#if defined(LINUX) && defined(__FAVOR_BSD)
#	define CONDOR_FAVOR_BSD
#	undef __FAVOR_BSD
#endif

#include <setjmp.h>

#if defined(LINUX) && defined(CONDOR_FAVOR_BSD)
#	undef CONDOR_FAVOR_BSD
#	define __FAVOR_BSD
#endif

#else	/* AIX32 fixups */
#ifndef _setjmp_h

extern "C" {

#ifdef __setjmp_h_recursive

#include_next <setjmp.h>

#else /* __setjmp_h_recursive */

#define __setjmp_h_recursive
#include_next <setjmp.h>
#define _setjmp_h 1

#endif /* __setjmp_h_recursive */
}

#endif	/* _setjmp_h */
#endif /* AIX32 fixups */

#endif /* _FIX_SETJMP */
