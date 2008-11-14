/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_file_basic.h"
#include "condor_error.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

CondorFileBasic::CondorFileBasic( int mode )
{
	fd = -1;
	url = NULL;
	readable = 0;
	writeable = 0;
	size = 0;
	syscall_mode = mode;
}

CondorFileBasic::~CondorFileBasic( )
{
	free( url );
}

int CondorFileBasic::open(const char *url_in, int flags, int mode)
{
	char *junk = (char *)malloc(strlen(url_in)+1);
	char *path = (char *)malloc(strlen(url_in)+1);

	free( url );
	url = strdup(url_in);

	/* linux glibc 2.1 and presumeably 2.0 had a bug where the range didn't
		work with 8bit numbers */
	#if defined(LINUX) && (defined(GLIBC20) || defined(GLIBC21))
	sscanf(url,"%[^:]:%[\x1-\x7F]",junk,path);
	#else
	sscanf(url,"%[^:]:%[\x1-\xFF]",junk,path);
	#endif

	free( junk );
	junk = NULL;

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
			free(path);
			path = NULL;
			return -1;
	}

	// Open the file

	int scm = SetSyscalls(syscall_mode);
	fd = ::open(path,flags,mode);
	free( path );
	path = NULL;
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

int CondorFileBasic::ioctl( int cmd, long arg )
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

char const *CondorFileBasic::get_url()
{
	return url ? url : "";
}

int CondorFileBasic::get_unmapped_fd()
{
	return fd;
}
