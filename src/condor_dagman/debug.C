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

#include "condor_common.h"

#include "debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

debug_level_t debug_level    = DEBUG_NORMAL;
char *        debug_progname = NULL;

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
