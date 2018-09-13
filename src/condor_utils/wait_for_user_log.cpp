/******************************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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
 ******************************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

#include "read_user_log.h"
#include "file_modified_trigger.h"
#include "wait_for_user_log.h"
#include "utc_time.h"

WaitForUserLog::WaitForUserLog( const std::string & f ) :
	filename( f ), reader( f.c_str() ), trigger( f ) { };

WaitForUserLog::~WaitForUserLog() { }

// This is what ReadUserLog::readEvent() does, but I'm not sure it's right.
// So, a layer of indirection.
#define ULOG_INVALID ULOG_RD_ERROR

ULogEventOutcome
WaitForUserLog::readEvent( ULogEvent * & event, int timeout, bool following ) {
	if(! isInitialized()) {
		return ULOG_INVALID;
	}

	struct timeval then; condor_gettimestamp( then );
	ULogEventOutcome outcome = reader.readEvent( event );
	if( outcome != ULOG_NO_EVENT ) {
		return outcome;
	} else {
		if(! following) { return ULOG_NO_EVENT; }

		int result = trigger.wait( timeout );
		switch( result ) {
			case -1:
				return ULOG_INVALID;
			case  0:
				return ULOG_NO_EVENT;
			case  1: {
				// If the job event log event was not written atomically, it's
				// possible for reader.readEvent() to return ULOG_NO_EVENT
				// here, so if we just return that, we might break our promise
				// about how long we waited for a new event.
				struct timeval now; condor_gettimestamp( now );
				int elapsedMilliseconds = timersub_usec( now, then ) / 1000;
				ULogEventOutcome o = readEvent( event, timeout - elapsedMilliseconds, following );
dprintf( D_ALWAYS, "%d = readEvent( , %d )\n", o, timeout - elapsedMilliseconds );
				return o;
				}
			default:
				EXCEPT( "Unknown return value from FileModifiedTrigger::wait(): %d, aborting.\n", result );
		}
	}
}
