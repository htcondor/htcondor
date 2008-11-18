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

// $Id: condor_userlog_job_counter.C,v 1.3 2007-11-07 23:21:24 nleroy Exp $
//
// given a userlog, returns the number of queued (e.g., submitted but
// not yet terminated or aborted) jobs found from 0 to 254, or 255 to
// indicate an error (e.g., a userlog reading/parsing error, no events
// found, a job count <0 or >254, improper usage, etc.)

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "read_user_log.h"

int main( int argc, char** argv )
{
	if( argc != 2 ) {
		fprintf( stderr, "usage: %s file\n", argv[0] );
		return 255;
	}

	bool done = false;
	bool seenJob = false;
	int numJobs = 0;
	ReadUserLog ru( argv[1] );
    ULogEvent* e = NULL;

	while( !done ) {

        ULogEventOutcome outcome = ru.readEvent( e );
		const char *eventName = NULL;

        switch (outcome) {

        case ULOG_NO_EVENT:
        case ULOG_RD_ERROR:
        case ULOG_UNK_ERROR:

			printf( "Log outcome: %s\n", ULogEventOutcomeNames[outcome] );
			done = true;
			break;
 
        case ULOG_OK:

            eventName = ULogEventNumberNames[e->eventNumber];

			switch( e->eventNumber ) {

			case ULOG_SUBMIT:
				seenJob = true;
				printf( "Log event: %s\n", eventName );
				numJobs++;
				printf( "Queued Jobs: %d\n", numJobs );
				break;

			case ULOG_JOB_TERMINATED:
			case ULOG_JOB_ABORTED:
				printf( "Log event: %s\n", eventName );
				numJobs--;
				printf( "Queued Jobs: %d\n", numJobs );
				break;

			default:
				break;
			}

			break;

		default:

			return 255;
        }
	}
	return (seenJob && numJobs >= 0 && numJobs < 255) ? numJobs : 255;
}
