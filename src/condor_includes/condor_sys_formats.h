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

#ifndef CONDOR_SYS_FORMATS_H
#define CONDOR_SYS_FORMATS_H

#if defined( HAVE_SYS_TYPES_H )
#  include <sys/types.h>
#endif

#if defined( HAVE_INTTYPES_H )
#  define __STDC_FORMAT_MACROS
#  include <inttypes.h>
#endif

/* If no inttypes, try to define our own (make a guess) */
/* The win 32 include file defines the win32 versions */
#if !defined( PRId64 )
#  define PRId64 "lld"
#endif
#if !defined( PRIi64 )
#  define PRIi64 "lli"
#endif
#if !defined( PRIu64 )
#  define PRIu64 "llu"
#endif

// Define a 'filesize_t' type and FILESIZE_T_FORMAT printf format string
#if defined HAVE_INT64_T
  typedef int64_t filesize_t;
# define FILESIZE_T_FORMAT "%" PRId64

#elif defined HAVE___INT64_T
  typedef __int64 filesize_t;
# define FILESIZE_T_FORMAT "%" PRId64

#else
  typedef long filesize_t;
# define FILESIZE_T_FORMAT "%l"
#endif


#endif /* CONDOR_SYS_FORMATS_H */
