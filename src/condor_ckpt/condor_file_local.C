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

CondorFileLocal::~CondorFileLocal()
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
			size = pos+result;
		}
	}

	return result;
}

int CondorFileLocal::is_file_local()
{
	return 1;
}
