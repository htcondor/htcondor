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
   Shared functions that are used both for the real dprintf(), used by
   our daemons, and the mirco-version used by the Condor code we link
   with the user job in the libcondorsyscall.a or libcondorckpt.a. 
*/


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_uid.h"


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
	"D_ALWAYS", "D_SYSCALLS", "D_CKPT", "D_HOSTNAME", "D_PERF_TRACE", "D_LOAD",
	"D_EXPR", "D_PROC", "D_JOB", "D_MACHINE", "D_FULLDEBUG", "D_NFS",
	"D_CONFIG", "D_UNUSED2", "D_PREEMPT", "D_PROTOCOL",	"D_PRIV",
	"D_SECURITY", "D_DAEMONCORE", "D_COMMAND", "D_MATCH", "D_NETWORK",
	"D_KEYBOARD", "D_PROCFAMILY", "D_IDLE", "D_THREADS", "D_ACCOUNTANT",
	"D_FAILURE", "D_PID", "D_FDS", "D_UNUSED3", "D_NOHEADER",
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
dprintf(int flags, const char* fmt, ...)
{
    va_list args;
    va_start( args, fmt );
    _condor_dprintf_va( flags, fmt, args );
    va_end( args );
}


/* If this were C++ code, we could use StringList instead of strtok().
 * We don't use strtok_r() because it's not available on Windows.
 */
void
_condor_set_debug_flags( const char *strflags )
{
	char *tmp;
	char *flag;
	int notflag, bit, i;

		// Always set D_ALWAYS
	DebugFlags |= D_ALWAYS;

	tmp = strdup( strflags );
	if ( tmp == NULL ) {
		return;
	}

	flag = strtok( tmp, ", " );

	while ( flag != NULL ) {
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

		flag = strtok( NULL, ", " );
	}

	free( tmp );
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

