// $Id: condor_userlog_job_counter.C,v 1.1.2.2 2006-03-28 23:17:39 pfc Exp $
//
// given a userlog, returns the number of queued (e.g., submitted but
// not yet terminated or aborted) jobs found from 0 to 254, or 255 to
// indicate an error (e.g., a userlog reading/parsing error, no events
// found, a job count <0 or >254, improper usage, etc.)

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "user_log.c++.h"

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
