
#include "condor_common.h"
#include "condor_file_local.h"
#include "condor_error.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFileLocal::CondorFileLocal()
{
	init();
	kind = "local file";
}

int CondorFileLocal::open(const char *path, int flags, int mode ) {

	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = CondorFile::open(path,flags,mode);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::close() {
	int result, scm;

	if( fd!=-1 ) {
		scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
		CondorFile::close();
		SetSyscalls(scm);
	}

	return result;
}

int CondorFileLocal::read(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::lseek(fd,pos,SEEK_SET);
	result = ::read(fd,data,length);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::write(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::lseek(fd,pos,SEEK_SET);
	result = ::write(fd,data,length);
	SetSyscalls(scm);

	if(result>0) {
		if((pos+result)>get_size()) {
			set_size(pos+result);
		}
	}

	return result;
}

/*
We can happily support any fcntl or ioctl command in local mode.
Remote mode is a different story, see below.
*/

int CondorFileLocal::fcntl( int cmd, int arg )
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::fcntl(fd,cmd,arg);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::ioctl( int cmd, int arg )
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::ioctl(fd,cmd,arg);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::ftruncate( size_t length )
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::ftruncate(fd,length);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::fsync()
{
	int result, scm;

	scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	result = ::fsync(fd);
	SetSyscalls(scm);

	return result;
}

void CondorFileLocal::checkpoint()
{
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	#ifdef I_GETSIG
	ioctl_sig = ioctl( I_GETSIG, 0 );
	#endif

	fcntl_fl = fcntl( F_GETFL, 0 );
	fcntl_fd = fcntl( F_GETFD, 0 );

	SetSyscalls(scm);
}

void CondorFileLocal::suspend()
{
	if(forced) return;
	if(fd==-1) return;
	checkpoint();
	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
	::close(fd);
	SetSyscalls(scm);
	fd=-1;
}

void CondorFileLocal::resume( int count )
{
	if(count==resume_count) return;
	resume_count = count;

	if(forced) return;

	int flags;

	if( readable&&writeable )	flags = O_RDWR;
	else if( writeable )		flags = O_WRONLY;
	else				flags = O_RDONLY;

	int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);

	fd = ::open(name,flags);
	if( fd==-1 ) {
		_condor_error_retry("Unable to reopen local file %s after a checkpoint!\n",name);
	}

	#ifdef I_SETSIG
	ioctl( I_SETSIG, ioctl_sig );
	#endif

	fcntl( F_SETFL, fcntl_fl );
	fcntl( F_SETFD, fcntl_fd );

	SetSyscalls(scm);
}

int CondorFileLocal::local_access_hack()
{
	return 1;
}

int CondorFileLocal::map_fd_hack()
{
	return fd;
}
