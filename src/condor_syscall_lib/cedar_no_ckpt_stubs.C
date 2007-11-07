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


/* 
   This file contains special stubs from the CEDAR library (ReliSock,
   SafeSock, etc) that are needed to make things link correctly within
   a user job that can't use their normal functionality.
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_io.h"


int
ReliSock::get_file_with_permissions( filesize_t*, const char *, bool )
{
	EXCEPT( "ReliSock::get_file_with_permissions() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}


int
ReliSock::put_file_with_permissions( filesize_t *, const char * )
{
	EXCEPT( "ReliSock::put_file_with_permissions() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

int
ReliSock::get_x509_delegation( filesize_t*, const char *, bool )
{
	EXCEPT( "ReliSock::get_x509_delegation() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}


int
ReliSock::put_x509_delegation( filesize_t *, const char * )
{
	EXCEPT( "ReliSock::put_x509_delegation() should never be "
			"called within the Condor syscall library" );
	return FALSE;
}

