/*******************************************************************
**
** Manage system call mode and do remote system calls.
**
*******************************************************************/

#define _POSIX_SOURCE
#include <stdio.h>
#include "condor_syscalls.h"

static int		SyscallMode;

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

int
REMOTE_syscall( int syscall_num, ... )
{
	fprintf(
		stderr,
		"Don't know how to do system call %d remotely - yet\n",
		syscall_num
	);
	abort();
	return -1;
}

#if defined(AIX32)	/* Just to test linking */
int syscall( int num, ... )
{
	return 0;
}
#endif
