/**************************************************************
**
** Stubs for all those system calls which don't affect the state
** of the open file table.
**
**************************************************************/

#define _POSIX_SOURCE
#include "condor_syscalls.h"
#include "file_table_interf.h"
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>

#if defined( SYS_read )
int
read( int fd, void *buf, size_t nbytes )
{
	int		rval;

	fd = MapFd( fd );

	if( fd < 0 ) {
		return -1;
	}

	if( LocalSysCalls() ) {
		rval = syscall( SYS_read, fd, buf, nbytes );
	} else {
		rval = REMOTE_syscall( SYS_read, fd, buf, nbytes );
	}

	return rval;
}
#endif

#if defined( SYS_write )
int
write( int fd, void *buf, size_t nbytes )
{
	fd = MapFd( fd );

	if( fd < 0 ) {
		return -1;
	}

	if( LocalSysCalls() ) {
		return syscall( SYS_write, fd, buf, nbytes );
	} else {
		return REMOTE_syscall( SYS_write, fd, buf, nbytes );
	}

}
#endif
