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
#include "condor_file_basic.h"
#include "condor_error.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFileBasic::CondorFileBasic( int mode )
{
	fd = -1;
	url[0] = 0;
	readable = 0;
	writeable = 0;
	size = 0;
	syscall_mode = mode;
}

CondorFileBasic::~CondorFileBasic( )
{
}

int CondorFileBasic::open(const char *url_in, int flags, int mode)
{
	char junk[_POSIX_PATH_MAX];
	char path[_POSIX_PATH_MAX];

	strcpy(url,url_in);

	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf(url,"%[^:]:%[\x1-\x7F]",junk,path);
	#else
	sscanf(url,"%[^:]:%[\x1-\xFF]",junk,path);
	#endif

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
	fd = ::open(path,flags,mode);
	SetSyscalls(scm);

	if(fd<0) return fd;

	// Figure out the size of the file
	scm = SetSyscalls(syscall_mode);
	size = ::lseek(fd,0,SEEK_END);
	SetSyscalls(scm);

	if(size==-1) {
		size=0;
	} else {
		scm = SetSyscalls(syscall_mode);
		::lseek(fd,0,SEEK_SET);
		SetSyscalls(scm);
	}

	return fd;
}


int CondorFileBasic::close()
{
       	int scm = SetSyscalls(syscall_mode);
	int rval = ::close(fd);
	SetSyscalls(scm);
	fd = -1;
	return rval;
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

	size = length;
	scm = SetSyscalls(syscall_mode);
	result = ::ftruncate(fd,length);
	SetSyscalls(scm);

	return result;
}

int CondorFileBasic::fstat(struct stat *buf)
{
	int scm,result;

	scm = SetSyscalls(syscall_mode);
	result = ::fstat(fd, buf);
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

int CondorFileBasic::flush()
{
	/* nothing to flush */
	return 0;
}

int CondorFileBasic::is_readable()
{
	return readable;
}

int CondorFileBasic::is_writeable()
{
	return writeable;
}

int CondorFileBasic::is_seekable()
{
	return 1;
}

int CondorFileBasic::get_size()
{
	return size;
}

char *CondorFileBasic::get_url()
{
	return url;
}

int CondorFileBasic::get_unmapped_fd()
{
	return fd;
}
