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
#include "condor_debug.h"
#include "condor_syscall_mode.h"
#include "condor_sys.h"
#include "condor_io.h"


ReliSock *syscall_sock;

static int RSCSock;
static int ErrSock;

int InDebugMode;

extern "C" {

ReliSock *
RSC_Init( int rscsock, int errsock )
{
	RSCSock     = rscsock;
	syscall_sock = new ReliSock();
	syscall_sock->attach_to_file_desc(rscsock);

	ErrSock = errsock;

	return( syscall_sock );
}

} /* extern "C" */
