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



/* Avoid linking with util lib functions for standalone checkpointing. */


#include "condor_common.h"
#include "condor_sys.h"
#include "exit.h"

int		_EXCEPT_Line;
int		_EXCEPT_Errno;
char	*_EXCEPT_File;
int		(*_EXCEPT_Cleanup)(int,int,char*);

extern	int		DebugFlags;
extern int SetSyscalls(int sysnum);
extern int _condor_vfprintf_va( int, const char*, va_list );

void _condor_dprintf_va( int flags, const char* fmt, va_list args );
void _EXCEPT_(char* fmt, ...);

void
_condor_dprintf_va( int flags, const char* fmt, va_list args )
{
	int scm;

		/* See if this is one of the messages we are logging */
	if( !(flags&DebugFlags) ) {
		return;
	}

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		/* Actually print the message */
	_condor_vfprintf_va( 2, fmt, args );

	(void) SetSyscalls( scm );
}


void
_EXCEPT_(char* fmt, ...)
{
	if (DebugFlags) {
		va_list args;
		va_start( args, fmt );
		_condor_vfprintf_va( 2, fmt, args );
		va_end( args );
	}
	abort();
}

