
#include "condor_common.h"
#include "condor_file_local.h"
#include "condor_error.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

/* A CondorFileLocal is just like a CondorFileBasic, but it does its operations in local and unmapped mode. */

CondorFileLocal::CondorFileLocal()
	: CondorFileBasic( SYS_LOCAL|SYS_UNMAPPED )
{
}

/* Read requires a seek and a read.  This could be optimized to use pread on the platforms that support it. */

int CondorFileLocal::read(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(syscall_mode);
	::lseek(fd,pos,SEEK_SET);
	result = ::read(fd,data,length);
	SetSyscalls(scm);

	return result;
}

/* Write requires a seek and a write.  This could be optimized to use pwrite on the platforms that support it. */

int CondorFileLocal::write(int pos, char *data, int length) {
	int result, scm;

	scm = SetSyscalls(syscall_mode);
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
*/

int CondorFileLocal::fcntl( int cmd, int arg )
{
	int result, scm;

	scm = SetSyscalls(syscall_mode);
	result = ::fcntl(fd,cmd,arg);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::ioctl( int cmd, int arg )
{
	int result, scm;

	scm = SetSyscalls(syscall_mode);
	result = ::ioctl(fd,cmd,arg);
	SetSyscalls(scm);

	return result;
}

int CondorFileLocal::local_access_hack()
{
	return 1;
}

char * CondorFileLocal::get_kind()
{
	return "local file";
}
