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

/*
  This file implements some POSIX functions we rely on that don't
  exist on WIN32.  Many of these things are actually macros on most
  UNIX platforms, but for us, we have to implement our own versions. 
*/

#include "condor_common.h"

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
	return strchr( s, c );
}
