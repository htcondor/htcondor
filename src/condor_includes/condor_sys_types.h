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

#ifndef CONDOR_SYS_TYPES_H
#define CONDOR_SYS_TYPES_H


/* Define 64 bit integer types */
#if defined( HAVE_INT64_T )
   /* int64_t already defined; do nothing */

#elif defined( HAVE___INT64 )
   /* Use the compiler's __int64 type for int64_t (Win32) */
   typedef __int64 int64_t;
   typedef unsigned __int64 uint64_t;
#  define HAVE_INT64_T 1

#elif defined( HAVE_LONG_LONG )
   /* Use the compiler's long long type for int64_t (GCC) */
   typedef long long int64_t;
   typedef unsigned long long uint64_t;
#  define HAVE_INT64_T 1

#endif


#endif /* CONDOR_SYS_TYPES_H */
