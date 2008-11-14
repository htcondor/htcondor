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


#include "condor_common.h"
#include "condor_error.h"
#include "condor_file_remote.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "image.h"

/* Here are the remote system calls we use in this file */
extern "C" int REMOTE_CONDOR_lseekread(int fd, off_t offset, int whence, 
	void *buf, size_t len);
extern "C" int REMOTE_CONDOR_lseekwrite(int fd, off_t offset, int whence, 
	void *buf, size_t len);
extern "C" int REMOTE_CONDOR_ftruncate(int fd, off_t length);


/* A CondorFileRemote is a CondorFileBasic that does syscalls in remote and unmapped mode. */

CondorFileRemote::CondorFileRemote()
	: CondorFileBasic( SYS_REMOTE|SYS_UNMAPPED )
{
}

CondorFileRemote::~CondorFileRemote()
{
}

/* A read results in a CONDOR_lseekread */

int CondorFileRemote::read(int pos, char *data, int length) 
{
	return REMOTE_CONDOR_lseekread( fd, pos, SEEK_SET, data, length );
}

/* A write results in a CONDOR_lseekwrite */

int CondorFileRemote::write(int pos, char *data, int length)
{
	int result;
	result = REMOTE_CONDOR_lseekwrite( fd, pos, SEEK_SET, data, length );

	if(result>0) {
		if((pos+result)>get_size()) {
			size = pos+result;
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

		default:

			_condor_warning(CONDOR_WARNING_KIND_UNSUP,"fcntl(%d,%d,...) is not supported for remote files.",fd,cmd);
			errno = EINVAL;
			return -1;
	}
}

int CondorFileRemote::ioctl( int cmd, long /* arg */ )
{
	_condor_warning(CONDOR_WARNING_KIND_UNSUP,"ioctl(%d,%d,...) is not supported for remote files.",fd,cmd);
	errno = EINVAL;
	return -1;
}

/*
This needs to make a remote system call directly, because ftruncate is often
implemented in terms of fcntl, which (see CondorFileTable) can arrive here, resulting
in an infinite loop.
*/

int CondorFileRemote::ftruncate( size_t s )
{
	size = s;
	return REMOTE_CONDOR_ftruncate( fd, s );
}

/* This file cannot be accessed locally */

int CondorFileRemote::is_file_local()
{
	return 0;
}
