/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


/* Avoid linking with util lib functions for standalone checkpointing. */


#include "condor_common.h"
#include "condor_sys.h"
#include "exit.h"

int		_EXCEPT_Line;
int		_EXCEPT_Errno;
char	*_EXCEPT_File;
int		(*_EXCEPT_Cleanup)(int,int,char*);

extern	int		DebugFlags;


void
_condor_dprintf_va( int flags, char* fmt, va_list args )
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

