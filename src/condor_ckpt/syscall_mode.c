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

 

/*******************************************************************
**
** Manage system call mode and do remote system calls.
**
*******************************************************************/

#define _POSIX_SOURCE
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"

static int		SyscallMode = 1; /* LOCAL and UNMAPPED */

int
SetSyscalls( int mode )
{
	int	answer;
	answer = SyscallMode;
	SyscallMode = mode;
	return answer;
}

int
GetSyscallMode()
{
	return SyscallMode;
}

BOOL
LocalSysCalls()
{
	return SyscallMode & SYS_LOCAL;
}

BOOL
RemoteSysCalls()
{
	return (SyscallMode & SYS_LOCAL) == 0;
}

BOOL
MappingFileDescriptors()
{
	return (SyscallMode & SYS_MAPPED);
}

void
DisplaySyscallMode()
{
	dprintf( D_ALWAYS,
		"Syscall Mode is: %s %s\n",
		RemoteSysCalls() ? "REMOTE" : "LOCAL",
		MappingFileDescriptors() ? "MAPPED" : "UNMAPPED"
	);
}
