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
