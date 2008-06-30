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

#define DEFAULT_CACHE_SIZE ((1024 * 1024) * 5)

debug_level_t debug_level    = DEBUG_NORMAL;
const char *        debug_progname = NULL;

// sometimes we want to keep a cache for our log message when DAGMan is
// emitting thousands of them quickly and we could be writing the log to
// and NFS server.
static bool cache_enabled = false;
static MyString cache;
static unsigned int cache_size = DEFAULT_CACHE_SIZE;

void debug_cache_disable(void);
void debug_cache_enable(void);
void debug_cache_flush(void);
void debug_cache_insert(int flags, const char *fmt, va_list args);

/*--------------------------------------------------------------------------*/
void
debug_printf( debug_level_t level, char *fmt, ... ) {
	if( DEBUG_LEVEL( level ) ) {
		if (cache_enabled == false) {
			va_list args;
			va_start( args, fmt );
			_condor_dprintf_va( D_ALWAYS, fmt, args );
			va_end( args );
		} else {
			va_list args;
			va_start( args, fmt );
			debug_cache_insert( D_ALWAYS, fmt, args );
			va_end( args );
		}
	}
}

/*--------------------------------------------------------------------------*/
void
debug_dprintf( int flags, debug_level_t level, char *fmt, ... ) {
	if( DEBUG_LEVEL( level ) ) {
		if (cache_enabled == false) {
			va_list args;
			va_start( args, fmt );
			_condor_dprintf_va( flags, fmt, args );
			va_end( args );
		} else {
			
		}
	}
}

/*--------------------------------------------------------------------------*/
void
debug_error( int error, debug_level_t level, char *fmt, ... ) {

	// make sure these come out before emitting the error
	debug_cache_flush();
	debug_cache_disable();

    if( DEBUG_LEVEL( level ) ) {
        va_list args;
        va_start( args, fmt );
        _condor_dprintf_va( D_ALWAYS, fmt, args );
        va_end( args );
    }
	DC_Exit( error );
}

/*--------------------------------------------------------------------------*/
void
debug_cache_enable(void)
{
	cache_enabled = true;
}

/*--------------------------------------------------------------------------*/
void
debug_cache_disable(void)
{
	debug_cache_flush();
	cache_enabled = false;
}

/*--------------------------------------------------------------------------*/
void
debug_cache_insert(int flags, const char *fmt, va_list args)
{
	// from condor_util_lib/dprintf.c
	extern int DebugUseTimestamps;

	/* create the header */

	// XXX TODO
	// Make the header, add it first to the string, then cache.vsnprintf the
	// args and crap into itself.

	// if the cache has surpassed the highwater mark, then flush it.
	if (cache.Length() > cache_size) {
		debug_cache_flush();
	}
}

/*--------------------------------------------------------------------------*/
void
debug_cache_flush(void)
{
	// This emits ONE dprintf call which could write out up to the maximum
	// size of the cache. The lines in the cache are newline delimited.

	if (cache != "") {
		dprintf(D_ALWAYS, "%s", cache.Value());
		cache = "";
	}
}




