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

 
/* 
   Shared functions that are used both for the real dprintf(), used by
   our daemons, and the mirco-version used by the Condor code we link
   with the user job in the libcondorsyscall.a or libcondorckpt.a. 
*/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"
#include "condor_sys.h"


FILE*	_condor_DebugFP		= NULL;

/* 
   This should default to 0 so we only get dprintf() messages if we
   actually request them somewhere, either in dprintf_config(), or the
   equivalent inside the user job.
*/
int		DebugFlags			= 0;		


char *_condor_DebugFlagNames[] = {
	"D_ALWAYS", "D_SYSCALLS", "D_CKPT", "D_XDR", "D_MALLOC", "D_LOAD",
	"D_EXPR", "D_PROC", "D_JOB", "D_MACHINE", "D_FULLDEBUG", "D_NFS",
	"D_UPDOWN", "D_AFS", "D_PREEMPT", "D_PROTOCOL",	"D_PRIV",
	"D_TAPENET", "D_DAEMONCORE", "D_COMMAND", "D_BANDWIDTH", "D_NETWORK",
	"D_KEYBOARD", "D_PROCFAMILY", "D_IDLE", "D_MATCH", "D_UNDEF26",
	"D_UNDEF27", "D_UNDEF28", "D_FDS", "D_SECONDS", "D_NOHEADER",
};


void
dprintf(int flags, char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, fmt, args );
    va_end( args );
}


#if !defined(WIN32)	// Need to port this to WIN32.  It is used when logging to a socket.
/*
** Initialize the _condor_DebugFP to a specific file number.  */
void
_condor_dprintf_init( int fd )
{
	int scm;
	FILE *fp;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

	errno = 0;
	fp = fdopen( fd, "a" );

	if( fp != NULL ) {
		_condor_DebugFP = fp;
	} else {
		fprintf(stderr, "dprintf_init: failed to fdopen(%d)\n", fd );
		_condor_dprintf_exit();
	}

	(void) SetSyscalls( scm );
}
#endif /* ! LOOSE32 */


void
_condor_set_debug_flags( char *strflags )
{
	char tmp[ BUFSIZ ];
	char *argv[ BUFSIZ ], *flag;
	int argc, notflag, bit, i;

		// Always set D_ALWAYS
	DebugFlags |= D_ALWAYS;

	(void)strncpy(tmp, strflags, sizeof(tmp));
	_condor_mkargv(&argc, argv, tmp);

	while( --argc >= 0 ) {
		flag = argv[argc];
		if( *flag == '-' ) {
			flag += 1;
			notflag = 1;
		} else {
			notflag = 0;
		}

		bit = 0;
		if( stricmp(flag, "D_ALL") == 0 ) {
			bit = D_ALL;
		} else for( i = 0; i < D_MAXFLAGS; i++ ) {
			if( stricmp(flag, _condor_DebugFlagNames[i]) == 0 ) {
				bit = (1 << i);
				break;
			}
		}

		if( notflag ) {
			DebugFlags &= ~bit;
		} else {
			DebugFlags |= bit;
		}
	}
}

