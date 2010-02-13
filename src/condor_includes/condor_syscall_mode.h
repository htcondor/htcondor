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

#ifndef _CONDOR_SYSCALLS_H
#define _CONDOR_SYSCALLS_H

#if defined( AIX )
#	include "syscall.aix.h"
#elif defined(Solaris) || defined(Darwin) || defined(CONDOR_FREEBSD)
#	include <sys/syscall.h>
#elif defined(WIN32)
#else
#	include <syscall.h>
#endif

#ifndef WIN32

#include "syscall_numbers.h"

typedef int BOOL;
#endif

static const int SYS_LOCAL = 1;
static const int 	SYS_REMOTE = 0;
static const int	SYS_RECORDED = 2;
static const int	SYS_MAPPED = 2;

static const int	SYS_UNRECORDED = 0;
static const int	SYS_UNMAPPED = 0;

#if defined(__cplusplus)
extern "C" {
#endif

int SetSyscalls( int mode );
int GetSyscallMode( void );
BOOL LocalSysCalls( void );
BOOL RemoteSysCalls( void );
BOOL MappingFileDescriptors( void );

#if defined(HPUX) || defined(Solaris)
	int syscall( int, ... );
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_SYSCALLS_H */
