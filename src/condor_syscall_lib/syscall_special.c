/*******************************************************************
  System call stubs which need special treatment and cannot be generated
  automatically go here...
*******************************************************************/
#define _POSIX_SOURCE

	/* Temporary - need to get real PSEUDO definitions brought in... */
#define PSEUDO_getwd	1

#include "condor_syscall_mode.h"
#include "condor_constants.h"
#include "file_table_interf.h"
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/uio.h>

/*
  In remote mode the process should send the exit status to the shadow,
  then do a local exit() - so it really goes away.
*/
void
_exit( status )
int status;
{
	if( LocalSysCalls() ) {
		(void) syscall( SYS_exit, status );
	} else {
		(void)REMOTE_syscall( SYS_exit, status );
		(void) syscall( SYS_exit, status );
	}
}

/*
  The Open Files Table module needs to keep the current working
  directory in memory so that it can remember pathnames of files that
  are opened, even if they are opened by simple file names.  We first
  try to do the indicated change of directory, and then if that works,
  we record the new current working directory.
*/
int
chdir( const char *path )
{
	int rval, status;
	char	tbuf[ _POSIX_PATH_MAX ];

		/* Try the system call */
	if( LocalSysCalls() ) {
		rval = syscall( SYS_chdir, path );
	} else {
		rval = REMOTE_syscall( SYS_chdir, path );
	}

		/* If it fails we can stop here */
	if( rval < 0 ) {
		return rval;
	}

		/* Need to keep Condor's version of the CWD up to date */
	if( MappingFileDescriptors ) {
			/* Get the information */
		if( LocalSysCalls() ) {
			status = getwd( tbuf );
		} else {
			status = REMOTE_syscall( PSEUDO_getwd, tbuf );
		}

			/* These routines return 0 on error! */
		if( status == 0 ) {
			fprintf( stderr, tbuf );
			abort();
		}

			/* Ok - everything worked */
		Set_CWD( tbuf );
	}

	return rval;
}

/*
  Condor doesn't support the fork() system call, so by definition the
  resource usage of all our child processes is zero.  We must support
  this, since those users in the POSIX world will call utimes() which
  probably checks resource ruage for children - even though there are
  none.

  In the remote case things are a bit more complicated.  The rusage
  should he the sum of what the user process has accumulated on the
  current machine, and the usages it accumulated on all the machines
  where it has run in the past.
*/
int
getrusage( int who, struct rusage *rusage )
{
	int rval;
	struct rusage accum_rusage;

	if( LocalSysCalls() ) {
		rval = syscall( SYS_getrusage, who, rusage );
	} else {

			/* Condor user processes don't have children - yet */
		if( who != RUSAGE_SELF ) {
			memset( (char *)rusage, '\0', sizeof(struct rusage) );
			return 0;
		}

			/* Get current rusage for the running job. */
		syscall( SYS_getrusage, who, rusage);

			/* Get accumulated rusage from previous runs */
		rval = REMOTE_syscall( SYS_getrusage, who, &accum_rusage );

			/* Sum the two. */
		update_rusage(rusage, &accum_rusage);
	}

	return( rval );
}

/*
  This routine which is normally provided in the C library determines
  whether a given file descriptor refers to a tty.  The underlying
  mechanism is an ioctl, but Condor does not support ioctl.  We
  therefore provide a "quick and dirty" substitute here.  This may
  not always be correct, but it will get you by in most cases.
*/
isatty( filedes )
int		filedes;
{
		/* Stdin, stdout, and stderr are redirected to normal
		** files for all condor jobs.
		*/
	if( RemoteSysCalls() ) {
		return FALSE;
	}

		/* Assume stdin, stdout, and stderr are the only ttys */
	switch( filedes ) {
		case 0:
		case 1:
		case 2:
				return TRUE;
		default:
			return FALSE;
	}
}

/*
  We don't handle readv directly in remote system calls.  Instead we
  break the readv up into a series of individual reads.
*/
int
readv( int fd, struct iovec *iov, int iovcnt )
{
	int rval;
	int user_fd;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return -1;
	}

	if( LocalSysCalls() ) {
		rval = syscall( SYS_readv, user_fd, iov, iovcnt );
	} else {
		rval = fake_readv( user_fd, iov, iovcnt );
	}

	return rval;
}

/*
  Handle a call to readv which must be done remotely, (not NFS).  Note
  the user file descriptor has already been mapped by the calling routine,
  and we already know this is a remote system call.  We handle the call
  by breaking it up into a series of remote reads.
*/
static int
fake_readv( int fd, struct iovec *iov, int iovcnt )
{
	register int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = REMOTE_syscall( SYS_read, fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) {
			return cc;
		}

		rval += cc;
		if( cc != iov->iov_len ) {
			return rval;
		}

		iov++;
	}

	return rval;
}

/*
  We don't handle writev directly in remote system calls.  Instead we
  break the writev up into a series of individual writes.
*/
int
writev( int fd, struct iovec *iov, int iovcnt )
{
	int rval;
	int user_fd;

	if( (user_fd=MapFd(fd)) < 0 ) {
		return -1;
	}

	if( LocalSysCalls() ) {
		rval = syscall( SYS_writev, user_fd, iov, iovcnt );
	} else {
		rval = fake_writev( user_fd, iov, iovcnt );
	}

	return rval;
}

/*
  Handle a call to writev which must be done remotely, (not NFS).  Note
  the user file descriptor has already been mapped by the calling routine,
  and we already know this is a remote system call.  We handle the call
  by breaking it up into a series of remote writes.
*/
static int
fake_writev( int fd, struct iovec *iov, int iovcnt )
{
	register int i, rval = 0, cc;

	for( i = 0; i < iovcnt; i++ ) {
		cc = REMOTE_syscall( SYS_write, fd, iov->iov_base, iov->iov_len );
		if( cc < 0 ) {
			return cc;
		}

		rval += cc;
		if( cc != iov->iov_len ) {
			return rval;
		}

		iov++;
	}

	return rval;
}
