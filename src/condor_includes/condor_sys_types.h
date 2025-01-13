/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef CONDOR_SYS_TYPES_H
#define CONDOR_SYS_TYPES_H

#if defined( HAVE_SYS_TYPES_H )
#  include <sys/types.h>
#endif

/* Define 64 bit integer types */
#if defined( HAVE_INT64_T )
  /* int64_t already defined; do nothing */

#elif defined( HAVE___INT64 )
  /* Use the compiler's __int64 type for int64_t (Win32) */
  typedef __int64 int64_t;
  typedef unsigned __int64 uint64_t;
  typedef __int64 filesize_t;
# define HAVE_INT64_T 1

#elif ( defined(HAVE_LONG) && (SIZEOF_LONG==8) )
  /* Use the compiler's long type for int64_t (gcc on X86/64) */
  typedef long int64_t;
  typedef unsigned long uint64_t;
# define HAVE_INT64_T 1

#elif ( defined(HAVE_LONG_LONG) && (SIZEOF_LONG_LONG==8) )
  /* Use the compiler's long long type for int64_t (GCC) */
  typedef long long int64_t;
  typedef unsigned long long uint64_t;
# define HAVE_INT64_T 1

#else
# warning No 64-bit integer type found

#endif

// Finally, define a consistent filesize_t
#if defined( HAVE_INT64_T )
  typedef int64_t filesize_t;
#else
  typedef long filesize_t;
#endif

#endif /* CONDOR_SYS_TYPES_H */
