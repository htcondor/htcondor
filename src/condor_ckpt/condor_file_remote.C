
#include "condor_common.h"
#include "condor_error.h"
#include "condor_file_remote.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

/* A CondorFileRemote is a CondorFileBasic that does syscalls in remote and unmapped mode. */

CondorFileRemote::CondorFileRemote()
	: CondorFileBasic( SYS_REMOTE|SYS_UNMAPPED )
{
}

/* A read results in a CONDOR_lseekread */

int CondorFileRemote::read(int pos, char *data, int length) 
{
       	return REMOTE_syscall( CONDOR_lseekread, fd, pos, SEEK_SET, data, length );
}

/* A write results in a CONDOR_lseekwrite */

int CondorFileRemote::write(int pos, char *data, int length)
{
	int result;
	result = REMOTE_syscall( CONDOR_lseekwrite, fd, pos, SEEK_SET, data, length );

	if(result>0) {
		if((pos+result)>get_size()) {
			set_size(pos+result);
		}
	}

	return result;
}

/*
In remote mode, we can only support fcntl and ioctl
commands that have a single integer argument.  Others
are a lost cause...
*/

int CondorFileRemote::fcntl( int cmd, int arg )
{
	int result, scm;

	struct flock *f;

	switch(cmd) {
		#ifdef F_GETFD
		case F_GETFD:
		#endif

		#ifdef F_GETFL
		case F_GETFL:
		#endif

		#ifdef F_SETFD
		case F_SETFD:
		#endif
		
		#ifdef F_SETFL
		case F_SETFL:
		#endif
			scm = SetSyscalls(syscall_mode);
			result = ::fcntl(fd,cmd,arg);
			SetSyscalls(scm);
			return result;

		#ifdef F_FREESP
		case F_FREESP:
		#endif

		#ifdef F_FREESP64
		case F_FREESP64:
		#endif

			/* When all fields of the lockarg are zero,
			   this is the same as truncate, and we know
			   how to do that already. */

			f = (struct flock *)arg;
			if( (f->l_whence==0) && (f->l_start==0) && (f->l_len==0) ) {
				return ftruncate(0);
			}

			/* Otherwise, fall through here. */

		default:

			_condor_warning("fcntl(%d,%d,...) is not supported for remote files.",fd,cmd);
			errno = EINVAL;
			return -1;
	}
}

int CondorFileRemote::ioctl( int cmd, int arg )
{
	#ifdef I_SETSIG
	if(cmd==I_SETSIG) {
		int scm = SetSyscalls(syscall_mode);
		int result = ::ioctl( fd, cmd, arg );
		SetSyscalls(scm);
		return result;
	}
	#endif
	
	_condor_warning("ioctl(%d,%d,...) is not supported for remote files.",fd,cmd);
	errno = EINVAL;
	return -1;
}

/* This file cannot be accessed locally */

int CondorFileRemote::local_access_hack()
{
	return 0;
}

char * CondorFileRemote::get_kind()
{
	return "remote file";
}
