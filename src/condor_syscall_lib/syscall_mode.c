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

#if defined(AIX32)	/* Just to test linking */
int syscall( int num, ... )
{
	return 0;
}
#endif
