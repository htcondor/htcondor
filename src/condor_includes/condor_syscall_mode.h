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
#ifndef _CONDOR_SYSCALLS_H
#define _CONDOR_SYSCALLS_H

#if defined( AIX )
#	include "syscall.aix.h"
#elif defined(Solaris) || defined(Darwin)
#	include <sys/syscall.h>
#elif defined(IRIX)
#	undef SYSVoffset
#	undef __SYS_S__
#	include <sys.s>
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
int GetSyscallMode();
BOOL LocalSysCalls();
BOOL RemoteSysCalls();
BOOL MappingFileDescriptors();

#if defined(OSF1) || defined(HPUX) || defined(IRIX) || defined(Solaris)
	int syscall( int, ... );
#endif

#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_SYSCALLS_H */
