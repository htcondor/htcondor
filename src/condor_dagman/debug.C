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
#include "../condor_daemon_core.V6/condor_daemon_core.h"

debug_level_t debug_level    = DEBUG_NORMAL;
const char *        debug_progname = NULL;

/*--------------------------------------------------------------------------*/
void
debug_printf( debug_level_t level, char *fmt, ... ) {
	if( DEBUG_LEVEL( level ) ) {
		va_list args;
		va_start( args, fmt );
		_condor_dprintf_va( D_ALWAYS, fmt, args );
		va_end( args );
	}
}

/*--------------------------------------------------------------------------*/
void
debug_dprintf( int flags, debug_level_t level, char *fmt, ... ) {
	if( DEBUG_LEVEL( level ) ) {
		va_list args;
		va_start( args, fmt );
		_condor_dprintf_va( flags, fmt, args );
		va_end( args );
	}
}

/*--------------------------------------------------------------------------*/
void
debug_error( int error, debug_level_t level, char *fmt, ... ) {
    if( DEBUG_LEVEL( level ) ) {
        va_list args;
        va_start( args, fmt );
        _condor_dprintf_va( D_ALWAYS, fmt, args );
        va_end( args );
    }
	DC_Exit( error );
}
