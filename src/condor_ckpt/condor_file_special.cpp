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
#include "condor_file_special.h"
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "signals_control.h"

CondorFileSpecial::CondorFileSpecial() : CondorFileLocal()
{
	readable = writeable = 1;
	_condor_ckpt_disable();
}

CondorFileSpecial::~CondorFileSpecial()
{
	_condor_ckpt_enable();
}

int CondorFileSpecial::attach( int fd_in )
{
	fd = fd_in;
	readable = writeable = 1;
	size = -1;
	return 0;
}

