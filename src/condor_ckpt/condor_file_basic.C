#include "condor_file_basic.h"
#include "condor_error.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFileBasic::CondorFileBasic( int mode )
{
	fd = -1;
	name[0] = 0;
	readable = 0;
	writeable = 0;
	size = 0;
	attached = 0;
	ioctl_sig = 0;
	fcntl_fl = 0;
	fcntl_fd = 0;
	syscall_mode = mode;
	resume_count = 0;
	open_flags = 0;
}

int CondorFileBasic::open(const char *path, int flags, int mode)
{
	strncpy(name,path,_POSIX_PATH_MAX);

	// Store the open flags we will use on a restart
	open_flags = flags;

	// APPEND must be simulated for checkpoint safety
	open_flags &= ~O_APPEND;

	// Summarize read and write for convenience

	switch( flags & O_ACCMODE ) {
		case O_RDONLY:
			readable = 1;
			writeable = 0;
			break;
		case O_WRONLY:
			readable = 0;
			writeable = 1;
			break;
		case O_RDWR:
			readable = 1;
			writeable = 1;
			break;
		default:
			return -1;
	}

	// Open the file

	int scm = SetSyscalls(syscall_mode);
	fd = ::open(path,open_flags,mode);
	SetSyscalls(scm);

	if(fd<0) return fd;

	// CREAT and TRUNC must be applied the first time, but not afterwards
	open_flags &= ~O_CREAT;
	open_flags &= ~O_TRUNC;

	// Figure out the size of the file
	scm = SetSyscalls(syscall_mode);
	size = ::lseek(fd,0,SEEK_END);
	SetSyscalls(scm);

	if(size==-1) {
		size=0;
	} else {
		// If opened for append, leave it there
		// Otherwise, seek back to zero

		if( flags & O_APPEND ) {
			/* do nothing */
		} else {
			scm = SetSyscalls(syscall_mode);
			::lseek(fd,0,SEEK_SET);
			SetSyscalls(scm);
		}
	}

	return fd;
}


int CondorFileBasic::close()
{
	int rval;

	if( fd!=-1 ) {
		int scm = SetSyscalls(syscall_mode);
		rval = ::close(fd);
		SetSyscalls(scm);
	}

	fd = -1;
	return rval;
}

void CondorFileBasic::checkpoint()
{
	int scm = SetSyscalls(syscall_mode);

	fcntl_fl = ::fcntl( fd, F_GETFL, 0 );
	fcntl_fd = ::fcntl( fd, F_GETFD, 0 );

	SetSyscalls(scm);
}

void CondorFileBasic::suspend()
{
	// don't suspend an attached file
	if( attached ) return;

	// has this file already been suspended?
	if(fd==-1) return;

	// do everything that would happen at a periodic checkpoint
	checkpoint();

	// and then close the file
	int scm = SetSyscalls(syscall_mode);
	::close(fd);
	SetSyscalls(scm);

	// and mark it as suspended
	fd = -1;
}

void CondorFileBasic::resume( int count )
{
	// don't try to resume an attached file
	if( attached ) return;

	// has this file already been resumed?
	if( resume_count==count ) return;

	// now re-open it
	int scm = SetSyscalls(syscall_mode);
	fd = ::open(name,open_flags,0);
	SetSyscalls(scm);

	if(fd<0) {
		_condor_error_retry("Couldn't re-open %s (%s).  Please fix it.\n",name,strerror(errno));
	}

	// Restore the fcntl and ioctl flags

	scm = SetSyscalls(syscall_mode);

	::fcntl( fd, F_SETFL, fcntl_fl );
	::fcntl( fd, F_SETFD, fcntl_fd );

	SetSyscalls(scm);

	resume_count = count;
}

int CondorFileBasic::ioctl( int cmd, int arg )
{
	int scm, result;

	scm = SetSyscalls( syscall_mode );
	result = ::ioctl( fd, cmd, arg );
	SetSyscalls( scm );

	return result;
}

int CondorFileBasic::fcntl( int cmd, int arg )
{
	int scm, result;

	scm = SetSyscalls( syscall_mode );
	result = ::fcntl( fd, cmd, arg );
	SetSyscalls( scm );

	return result;
}

int CondorFileBasic::ftruncate( size_t length )
{
	int scm,result;

	set_size(length);
	scm = SetSyscalls(syscall_mode);
	result = ::ftruncate(fd,length);
	SetSyscalls(scm);

	return result;
}

int CondorFileBasic::fsync()
{
	int scm,result;

	scm = SetSyscalls(syscall_mode);
	result = ::fsync(fd);
	SetSyscalls(scm);

	return result;
}

int CondorFileBasic::attach( int f, char *n, int r, int w )
{
	fd = f;
	strcpy(name,n);
	readable = r;
	writeable = w;
	attached = 1;
}


int CondorFileBasic::is_readable()
{
	return readable;
}

int CondorFileBasic::is_writeable()
{
	return writeable;
}

void CondorFileBasic::set_size( size_t s )
{
	size = s;
}

int CondorFileBasic::get_size()
{
	return size;
}

char *CondorFileBasic::get_name()
{
	return name;
}

int CondorFileBasic::map_fd_hack()
{
	return fd;
}
