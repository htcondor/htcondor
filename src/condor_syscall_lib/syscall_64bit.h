/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
