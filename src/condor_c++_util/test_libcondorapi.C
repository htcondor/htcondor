#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "user_log.c++.h"

int main()
{
	int done;
	ReadUserLog ru("test_condor.log");

    ULogEvent* e = NULL;

	done = 0;
	while( !done ) {

        ULogEventOutcome outcome = ru.readEvent( e );
		const char *eventName = NULL;

        switch (outcome) {

        case ULOG_NO_EVENT:
        case ULOG_RD_ERROR:
        case ULOG_UNK_ERROR:

			printf( "Log outcome: %s\n", ULogEventOutcomeNames[outcome] );
			done = 1;
			break;
 
        case ULOG_OK:

            eventName = ULogEventNumberNames[e->eventNumber];

			printf( "Log event: %s", eventName );

			if( e->eventNumber == ULOG_SUBMIT ) {
				SubmitEvent* ee = (SubmitEvent*) e;
				printf( " (\"%s\")", ee->submitEventLogNotes );
			}
			
			if( e->eventNumber == ULOG_JOB_HELD ) {
				JobHeldEvent* ee = (JobHeldEvent*) e;
				printf( " (code=%d subcode=%d)",ee->getReasonCode(),ee->getReasonSubCode());
			}

			printf( "\n" );

			break;

		default:

			assert( false );
			break;
        }
	}
}
