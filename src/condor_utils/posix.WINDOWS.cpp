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
  This file implements some POSIX functions we rely on that don't
  exist on WIN32.  Many of these things are actually macros on most
  UNIX platforms, but for us, we have to implement our own versions. 
*/

#include "condor_common.h"
#include "filename_tools.h"

extern "C" 
{

#define EXCEPTION( x ) case EXCEPTION_##x: return 0;

int 
WIFEXITED(DWORD stat) 
{
	switch (stat) {
		EXCEPTION( ACCESS_VIOLATION )
        EXCEPTION( DATATYPE_MISALIGNMENT )        
		EXCEPTION( BREAKPOINT )
        EXCEPTION( SINGLE_STEP )        
		EXCEPTION( ARRAY_BOUNDS_EXCEEDED )
        EXCEPTION( FLT_DENORMAL_OPERAND )        
		EXCEPTION( FLT_DIVIDE_BY_ZERO )
        EXCEPTION( FLT_INEXACT_RESULT )
        EXCEPTION( FLT_INVALID_OPERATION )        
		EXCEPTION( FLT_OVERFLOW )
        EXCEPTION( FLT_STACK_CHECK )        
		EXCEPTION( FLT_UNDERFLOW )
        EXCEPTION( INT_DIVIDE_BY_ZERO )        
		EXCEPTION( INT_OVERFLOW )
        EXCEPTION( PRIV_INSTRUCTION )        
		EXCEPTION( IN_PAGE_ERROR )
        EXCEPTION( ILLEGAL_INSTRUCTION )
        EXCEPTION( NONCONTINUABLE_EXCEPTION )        
		EXCEPTION( STACK_OVERFLOW )
        EXCEPTION( INVALID_DISPOSITION )        
		EXCEPTION( GUARD_PAGE )
        EXCEPTION( INVALID_HANDLE )
	}
	return 1;
}


int 
WIFSIGNALED(DWORD stat) 
{
	if ( WIFEXITED(stat) ) {
		return 0;
	} else {
		return 1;
	}
}


char*
index( const char *s, int c )
{
	return (char *)strchr( s, c );
}

//#pragma warning(disable: 4273) // inconsistent dll linkage

int
__access_(const char *path, int mode)
{
	int result, len;
	char *buf;

	result = _access(path,mode);

		/* If we are testing for executable (X_OK), and failed because file
		   not found (ENOENT), then try again after making certain directories
		   delimiters are backslahes and adding a file extention of .exe if no 
		   extention exists.  Note this is the same algorithm used by 
		   DaemonCore::Create_Process().
	    */		
	if ( (result == -1) && (errno == ENOENT) && 
		 (mode & X_OK) && path && path[0] ) 
	{
		buf = alternate_exec_pathname( path );
		if ( buf ) {
			result = _access(buf,mode);
			free(buf);
		}
	}

	return result;
}
//#pragma warning(default: 4996) // inconsistent dll linkage

}
