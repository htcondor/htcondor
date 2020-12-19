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


#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "debug.h"
#include "util.h"
#include "util_lib_proto.h"
#include "condor_arglist.h"
#include "my_popen.h"
#include "../condor_procapi/processid.h"
#include "../condor_procapi/procapi.h"
#include "dagman_main.h"

# if 0 // Moved to dagman_utils
//-----------------------------------------------------------------------------
int util_popen (ArgList &args) {
	MyString cmd; // for debug output
	args.GetArgsStringForDisplay( &cmd );
    debug_printf( DEBUG_VERBOSE, "Running: %s\n", cmd.Value() );

	FILE *fp = my_popen( args, "r", MY_POPEN_OPT_WANT_STDERR );

    int r = 0;
    if (fp == NULL || (r = my_pclose(fp) & 0xff) != 0) {
		debug_printf( DEBUG_QUIET, "Warning: failure: %s\n", cmd.Value() );
		if( fp != NULL ) {
			debug_printf ( DEBUG_QUIET,
						"\t(my_pclose() returned %d (errno %d, %s))\n",
						r, errno, strerror( errno ) );
		} else {
			debug_printf ( DEBUG_QUIET,
						"\t(my_popen() returned NULL (errno %d, %s))\n",
						errno, strerror( errno ) );
			r = -1;
		}
		check_warning_strictness( DAG_STRICT_1 );
    }
    return r;
}

//-----------------------------------------------------------------------------

int util_create_lock_file(const char *lockFileName, bool abortDuplicates) {
	int result = 0;

	FILE *fp = safe_fopen_wrapper_follow( lockFileName, "w" );
	if ( fp == NULL ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: could not open lock file %s for writing.\n",
					lockFileName);
		result = -1;
	}

		//
		// Create the ProcessId object.
		//
	ProcessId *procId = NULL;
	if ( result == 0 && abortDuplicates ) {
		int status;
		int precision_range = 1;
		if ( ProcAPI::createProcessId( daemonCore->getpid(), procId,
					status, &precision_range ) != PROCAPI_SUCCESS ) {
			debug_printf( DEBUG_QUIET, "ERROR: ProcAPI::createProcessId() "
						"failed; %d\n", status );
			result = -1;
		}
	}

		//
		// Write out the ProcessId object.
		//
	if ( result == 0 && abortDuplicates ) {
		if ( procId->write( fp ) != ProcessId::SUCCESS ) {
			debug_printf( DEBUG_QUIET, "ERROR: ProcessId::write() failed\n");
			result = -1;
		}
	}

		//
		// Confirm the ProcessId object's uniqueness.
		//
	if ( result == 0 && abortDuplicates ) {
		int status;
		if ( ProcAPI::confirmProcessId( *procId, status ) !=
					PROCAPI_SUCCESS ) {
			debug_printf( DEBUG_QUIET, "Warning: ProcAPI::"
						"confirmProcessId() failed; %d\n", status );
			check_warning_strictness( DAG_STRICT_3 );
		} else {
			if ( !procId->isConfirmed() ) {
				debug_printf( DEBUG_QUIET, "Warning: ProcessId not "
							"confirmed unique\n" );
				check_warning_strictness( DAG_STRICT_3 );
			} else {

					//
					// Write out the confirmation.
					//
				if ( procId->writeConfirmationOnly( fp ) !=
							ProcessId::SUCCESS ) {
					debug_printf( DEBUG_QUIET, "ERROR: ProcessId::"
								"writeConfirmationOnly() failed\n");
					result = -1;
				}
			}
		}
	}

	delete procId;

	if ( fp != NULL ) {
		if ( fclose( fp ) != 0 ) {
			debug_printf( DEBUG_QUIET, "ERROR: closing lock "
						"file failed with errno %d (%s)\n", errno,
						strerror( errno ) );
		}
	}

	return result;
}

//-----------------------------------------------------------------------------

int util_check_lock_file(const char *lockFileName) {
	int result = 0;

	FILE *fp = safe_fopen_wrapper_follow( lockFileName, "r" );
	if ( fp == NULL ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: could not open lock file %s for reading.\n",
					lockFileName );
		result = -1;
	}

	ProcessId *procId = NULL;
	if ( result != -1 ) {
		int status;
		procId = new ProcessId( fp, status );
		if ( status != ProcessId::SUCCESS ) {
			debug_printf( DEBUG_QUIET, "ERROR: unable to create ProcessId "
						"object from lock file %s\n", lockFileName );
			result = -1;
		}
	}

	if ( result != -1 ) {
		int status;
		int aliveResult = ProcAPI::isAlive( *procId, status );
		if ( aliveResult != PROCAPI_SUCCESS ) {
			debug_printf( DEBUG_QUIET, "ERROR: failed to determine "
						"whether DAGMan that wrote lock file is alive\n" );
			result = -1;
		} else {

			if ( status == PROCAPI_ALIVE ) {
				debug_printf( DEBUG_NORMAL,
						"Duplicate DAGMan PID %d is alive; this DAGMan "
						"should abort.\n", procId->getPid() );
				result = 1;

			} else if ( status == PROCAPI_DEAD ) {
				debug_printf( DEBUG_NORMAL,
						"Duplicate DAGMan PID %d is no longer alive; "
						"this DAGMan should continue.\n",
						procId->getPid() );
				result = 0;

			} else if ( status == PROCAPI_UNCERTAIN ) {
				debug_printf( DEBUG_NORMAL,
						"Duplicate DAGMan PID %d *may* be alive; this "
						"DAGMan is continuing, but this will cause "
						"problems if the duplicate DAGMan is alive.\n",
						procId->getPid() );
				result = 0;

			} else {
				EXCEPT( "Illegal ProcAPI::isAlive() status value: %d",
							status );
			}
		}
	}

	delete procId;

	if ( fp != NULL ) {
		if ( fclose( fp ) != 0 ) {
			debug_printf( DEBUG_QUIET, "ERROR: closing lock "
						"file failed with errno %d (%s)\n", errno,
						strerror( errno ) );
		}
	}

	return result;
}
#endif