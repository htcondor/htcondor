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

#ifndef CONDOR_SYSCALL_64BIT_H
#define CONDOR_SYSCALL_64BIT_H

#if HAS_64BIT_STRUCTS

/* Since we include this in switches.c, which doesn't use
   condor_common.h, we want to include this file to get our shared
   macros defined, etc */
#include "condor_header_features.h"

#if defined(HPUX)
#	include "condor_hpux_64bit_types.h"
#else 
#if defined(Solaris26) && !defined(_CONDOR_OFF64_T)
    typedef long long off64_t;
#endif
    struct stat64;
    struct rlimit64;
#endif /* HPUX */

BEGIN_C_DECLS

off64_t lseek64( int, off64_t, int );
int fstat64( int, struct stat64 * );
int stat64( const char *, struct stat64 * );
int lstat64( const char *, struct stat64 * );
int getdents64( int, char *, int );
int setrlimit64( int, const  struct rlimit64 * );
int getrlimit64( int, struct rlimit64 * );
caddr_t mmap64( caddr_t, size_t, int, int, int, off64_t );

END_C_DECLS

#endif /* HAS_64BIT_STRUCTS */

#endif /* CONDOR_SYSCALL_64BIT_H */
