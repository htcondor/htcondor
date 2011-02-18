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

#ifndef CONDOR_SYS_FORMATS_H
#define CONDOR_SYS_FORMATS_H

#if defined( HAVE_SYS_TYPES_H )
#  include <sys/types.h>
#endif
#if defined( HAVE_INTTYPES_H )
#  define __STDC_FORMAT_MACROS
#  include <inttypes.h>
#endif

#include "condor_sys_types.h"

/* If no inttypes, try to define our own (make a guess) */
/* The win 32 include file defines the win32 versions */
#if !defined( PRId64 )
# define PRId64 "lld"
# define PRId64_t long long
#endif

#if !defined( PRIi64 )
# define PRIi64 "lli"
# define PRIi64_t long long
#endif

#if !defined( PRIu64 )
# define PRIu64 "llu"
# define PRIu64_t unsigned long long
#endif

/* Define a 'filesize_t' type and FILESIZE_T_FORMAT printf format string */
#if defined HAVE_INT64_T
# define FILESIZE_T_FORMAT "%" PRId64
# define PRIfs PRId64
# define PRIfs_t PRId64_t

#else
# define FILESIZE_T_FORMAT "%l"
# define PRIfs PRId64
# define PRIfs_t long

#endif

/* Define types that match the PRIx64_t print format strings */
#if defined( HAVE___INT64 )
# define PRId64_t __int64
# define PRIi64_t __int64
# define PRIu64_t unsigned __int64

#else
# define PRId64_t int64_t
# define PRIi64_t int64_t
# define PRIu64_t uint64_t

#endif


#endif /* CONDOR_SYS_FORMATS_H */
