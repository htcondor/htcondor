/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _CONDOR_SYSCALLS_H
#define _CONDOR_SYSCALLS_H

#if defined( AIX32)
#	include "syscall.aix.h"
#elif defined(Solaris)
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
