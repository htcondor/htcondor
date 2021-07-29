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

#include "debug.h"
#include "condor_daemon_core.h"
#include "MyString.h"
#include "dagman_main.h"
#include "../condor_utils/dagman_utils.h"

debug_level_t debug_level    = DEBUG_NORMAL;
const char *        debug_progname = NULL;

// sometimes we want to keep a cache for our log message when DAGMan is
// emitting thousands of them quickly and we could be writing the log to
// and NFS server.
static bool cache_enabled = false;		// do I honor start/stop requests?
static bool cache_is_caching = false;	// have I started caching or stopped it?
static MyString cache;

// NOTE: if this isn't changed via a config file or something, then all lines
// are immediately written as they enter the cache.
static int cache_size = 0;


// from condor_util_lib/dprintf.c
// from condor_util_lib/dprintf_common.c
//extern "C" {
#ifdef D_CATEGORY_MASK
extern unsigned int DebugHeaderOptions;	// for D_FID, D_PID, D_CAT, D_NOHEADER & D_TIMESTAMP
#else
extern int DebugFlags;
extern int DebugUseTimestamps;
#endif
//}

static void debug_cache_insert(int flags, const char *fmt, va_list args);


/*--------------------------------------------------------------------------*/
void
debug_printf( debug_level_t level, const char *fmt, ... ) {
	const DPF_IDENT ident = 0; // REMIND: maybe have something useful here?
	if( DEBUG_LEVEL( level ) ) {
		if (cache_enabled == false ||
			(cache_enabled == true && cache_is_caching == false))
		{
			// let it go through right away
			va_list args;
			va_start( args, fmt );
			_condor_dprintf_va( D_ALWAYS, ident, fmt, args );
			va_end( args );
		} else {
			// store it for later flushing
			va_list args;
			va_start( args, fmt );
			debug_cache_insert( D_ALWAYS, fmt, args );
			va_end( args );
		}
	}
}

/*--------------------------------------------------------------------------*/
void
debug_dprintf( int flags, debug_level_t level, const char *fmt, ... ) {
	const DPF_IDENT ident = 0; // REMIND: maybe have something useful here?

	if( DEBUG_LEVEL( level ) ) {

		if (cache_enabled == true && cache_is_caching == true) {
			// This call is uncachable because I haven't implemented how we
			// can cache a debug_dprintf() because it specifies the flags
			// parameter that _condor_dprintf_va needs. So for now, we'll
			// just flush the cache and this isn't so bad because the place
			// where we need the caching doesn't call debug_dprintf() within
			// the critical section of the caching.

			// Note: After thinking about it, maybe I can
			// check the flag against DebugFlags and see if
			// it should pass, if so, we can shove it into
			// the cache. I'll attempt it later.

			dprintf(D_ALWAYS, "Uncachable dprintf forcing log line flush.\n");
			debug_cache_flush();
		}

		va_list args;
		va_start( args, fmt );
		_condor_dprintf_va( flags, ident, fmt, args );
		va_end( args );
			
	}
}

/*--------------------------------------------------------------------------*/
void
debug_error( int error, debug_level_t level, const char *fmt, ... ) {
	const DPF_IDENT ident = 0;
	// make sure these come out before emitting the error
	debug_cache_flush();
	debug_cache_stop_caching();
	debug_cache_disable();

    if( DEBUG_LEVEL( level ) ) {
        va_list args;
        va_start( args, fmt );
        _condor_dprintf_va( D_ALWAYS | D_FAILURE, ident, fmt, args );
        va_end( args );
    }
	DC_Exit( error );
}

/*--------------------------------------------------------------------------*/
void
debug_cache_enable(void)
{
	cache_enabled = true;
	dprintf(D_ALWAYS, 
		"Enabling log line cache for increased NFS performance.\n");
}

/*--------------------------------------------------------------------------*/
void
debug_cache_disable(void)
{
	debug_cache_flush();
	cache_enabled = false;
	dprintf(D_ALWAYS, "Disabling log line cache.\n");
}

/*--------------------------------------------------------------------------*/
void
debug_cache_start_caching(void)
{
	if (cache_enabled) {
		cache_is_caching = true;
		dprintf(D_ALWAYS, "Starting to cache log lines.\n");
	}
}

/*--------------------------------------------------------------------------*/
void
debug_cache_stop_caching(void)
{
	if (cache_enabled) {
		debug_cache_flush();
		cache_is_caching = false;
		dprintf(D_ALWAYS, "Stopping the caching of log lines.\n");
	}
}

/*--------------------------------------------------------------------------*/
void
debug_cache_insert(int flags, const char *fmt, va_list args)
{
	time_t clock_now;

	MyString tstamp, fds, line, pid, cid;
	pid_t my_pid;

#ifdef D_CATEGORY_MASK
	int HdrFlags = (DebugHeaderOptions|flags) & ~D_CATEGORY_RESERVED_MASK;
#else
	int HdrFlags = DebugFlags|flags;
#endif
	// XXX TODO
	// handle flags...
	// For now, always assume D_ALWAYS since the caller assumes it as well.

	clock_now = time( NULL );
	time_to_str( clock_now, tstamp );

	if ((HdrFlags & D_NOHEADER) == 0) {
		if (HdrFlags & D_FDS) {
				// Because of Z's dprintf changes, we no longer have
				// access to the dprintf FP.  For now we're just going
				// to skip figuring out the FD *while caching*.
				// wenger 2011-05-18
			fds.formatstr("(fd:?) " );
		}

		if (HdrFlags & D_PID) {
#ifdef WIN32
			my_pid = (int) GetCurrentProcessId();
#else
			my_pid = (int) getpid();
#endif
			pid.formatstr("(pid:%d) ", my_pid );

		}

#if 0  // not currently used by dagman.
		if (HdrFlags & D_IDENT) {
			const void * ident = 0;
			cid.formatstr("(ident:%p) ", ident);
		}
#endif
		// We skip running of the DebugId function, since it needs to
		// emit to a FILE* and we can't store it in the cache. It, as of this
		// time, isn't used in condor_dagman.
	}

	// figure out the line the user needs to emit.
	line.vformatstr(fmt, args);

	// build the cached line and add it to the cache
	cache += (tstamp + fds + line + cid);

	// if the cache has surpassed the highwater mark, then flush it.
	if (cache.length() > cache_size) {
		debug_cache_flush();
	}
}

/*--------------------------------------------------------------------------*/
void
debug_cache_flush(void)
{
	// This emits a dprintf call which could write out up to the maximum
	// size of the cache. The lines in the cache are newline delimited.
	// We bracket the output a bit so people know what happened.

	if (cache != "") {
		dprintf(D_ALWAYS, "LOG LINE CACHE: Begin Flush\n");
		dprintf(D_ALWAYS | D_NOHEADER, "%s", cache.c_str());
		dprintf(D_ALWAYS, "LOG LINE CACHE: End Flush\n");
		cache = "";
	}
}

/*--------------------------------------------------------------------------*/
void debug_cache_set_size(int size)
{
	// Get rid of what is there, if anything.
	if (cache_enabled && cache_is_caching) {
		debug_cache_flush();
	}

	if (size < 0) {
		cache_size = 0;
	}

	cache_size = size;
}

/*--------------------------------------------------------------------------*/
bool check_warning_strictness( strict_level_t strictness, bool quit_if_error )
{
	if ( Dagman::_strict >= strictness ) {
		debug_printf( DEBUG_QUIET, "ERROR: Warning is fatal "
					"error because of DAGMAN_USE_STRICT setting\n" );
		if ( quit_if_error ) {
			main_shutdown_rescue( EXIT_ERROR, DagStatus::DAG_STATUS_ERROR );
		}

		return true;
	}

	return false;
}

/*--------------------------------------------------------------------------*/

void
time_to_str( time_t timestamp, MyString &tstr )
{
#ifdef D_CATEGORY_MASK
	bool UseTimestamps = (DebugHeaderOptions & D_TIMESTAMP) != 0;
#else
	int UseTimestamps = DebugUseTimestamps;
#endif

	// HACK
	// Note: This nasty bit of code is copied in spirit from dprintf.c
	// It needs abstracting out a little bit from there into its own
	// function, but this is a quick hack for LIGO. I'll come back to it
	// and do it better later when I have time.
	if ( UseTimestamps ) {
		tstr.formatstr( "(%d) ", (int)timestamp );
	} else {
		struct tm *tm = NULL;
		tm = localtime( &timestamp );
		time_to_str( tm, tstr );
	}
}

/*--------------------------------------------------------------------------*/
void
time_to_str( const struct tm *tm, MyString &tstr )
{
	tstr.formatstr( "%02d/%02d/%02d %02d:%02d:%02d ",
		tm->tm_mon + 1, tm->tm_mday, tm->tm_year - 100,
		tm->tm_hour, tm->tm_min, tm->tm_sec );
}
