/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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
debug_error( int error, debug_level_t level, char *fmt, ... ) {
    if( DEBUG_LEVEL( level ) ) {
        va_list args;
        va_start( args, fmt );
        _condor_dprintf_va( D_ALWAYS, fmt, args );
        va_end( args );
    }
	DC_Exit( error );
}
