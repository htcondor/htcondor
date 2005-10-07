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
   Shared functions that are used both for the real dprintf(), used by
   our daemons, and the mirco-version used by the Condor code we link
   with the user job in the libcondorsyscall.a or libcondorckpt.a. 
*/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"

int _condor_mkargv( int* argc, char* argv[], char* line );


/* 
   This should default to 0 so we only get dprintf() messages if we
   actually request them somewhere, either in dprintf_config(), or the
   equivalent inside the user job.
*/
int		DebugFlags			= 0;		

/*
   This is a global flag that tells us if we've successfully ran
   dprintf_config() or otherwise setup dprintf() to print where we
   want it to go.  This is used by EXCEPT() to know if it can safely
   call dprintf(), or if it should just use printf(), instead.
*/
int		_condor_dprintf_works = 0;

char *_condor_DebugFlagNames[] = {
	"D_ALWAYS", "D_SYSCALLS", "D_CKPT", "D_HOSTNAME", "D_MALLOC", "D_LOAD",
	"D_EXPR", "D_PROC", "D_JOB", "D_MACHINE", "D_FULLDEBUG", "D_NFS",
	"D_UPDOWN", "D_NET_REMAP", "D_PREEMPT", "D_PROTOCOL",	"D_PRIV",
	"D_SECURITY", "D_DAEMONCORE", "D_COMMAND", "D_BANDWIDTH", "D_NETWORK",
	"D_KEYBOARD", "D_PROCFAMILY", "D_IDLE", "D_MATCH", "D_ACCOUNTANT",
	"D_FAILURE", "D_PID", "D_FDS", "D_SECONDS", "D_NOHEADER",
};


/*
   The real dprintf(), called by both the user job and all the daemons
   and tools.  To actually log the message, we call
   _condor_dprintf_va(), which is implemented differently for the user
   job and everything else.  If dprintf() has been configured (with
   dprintf_config() or it's equivalent in the user job), this will
   show up where we want it.  If not, we'll just drop the message in
   the bit bucket.  Someday, when we clean up dprintf() more
   effectively, we'll want to call _condor_sprintf_va() (defined
   below) if dprintf hasn't been configured so we'll still see
   the messages going to stderr, instead of being dropped.
*/
void
dprintf(int flags, char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, fmt, args );
    va_end( args );
}


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


#if 0
/*
   Until we know the difference between D_ALWAYS and D_ERROR, we don't
   really want to do this stuff below, since there are lots of
   D_ALWAYS messages we really don't want to see in the tools.  For
   now, all we really care about is the dprintf() from EXCEPT(), which
   we handle in except.c, anyway.
*/

/* 
   This method is called by dprintf() if we haven't already configured
   dprintf() to tell it where and what to log.  It this prints all
   debug messages we care about to the given fp, usually stderr.  In
   addition, there's no date/time header printed in this case (it's
   equivalent to always having D_NOHEADER defined), to avoid clutter. 
   Derek Wright <wright@cs.wisc.edu>
*/
void
_condor_sprintf_va( int flags, FILE* fp, char* fmt, va_list args )
{
		/* 
		   For now, only log D_ALWAYS if we're dumping to stderr.
		   Once we have ToolCore and can easily set the debug flags for
		   all command-line tools, and *everything* is just using
		   dprintf() again, we should compare against DebugFlags.
		   Derek Wright <wright@cs.wisc.edu>
		*/
    if( ! (flags & D_ALWAYS) ) {
        return;
    }
	vfprintf( fp, fmt, args );
}
#endif /* 0 */

