/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "string_list.h"
#include "read_multiple_logs.h"
#include "check_events.h"

MULTI_LOG_HASH_INSTANCE; // For the multi-log-file code...
CHECK_EVENTS_HASH_INSTANCE; // For the event checking code...

int main(int argc, char **argv)
{
	int		result = 0;

	if ( argc <= 1 || (argc >= 2 && !strcmp("-usage", argv[1])) ) {
		printf("Usage: condor_check_userlogs <log file 1> "
				"[log file 2] ... [log file n]\n");
		exit(0);
	}


	StringList	logFiles;
	for ( int argnum = 1; argnum < argc; ++argnum ) {
		logFiles.append(argv[argnum]);
	}
	logFiles.rewind();

	ReadMultipleUserLogs	ru(logFiles);

	CheckEvents		ce;
	int totalSubmitted = 0;
	int netSubmitted = 0;
	bool done = false;
	while( !done ) {

    	ULogEvent* e = NULL;
		MyString errorMsg;

        ULogEventOutcome outcome = ru.readEvent( e );

        switch (outcome) {

        case ULOG_NO_EVENT:
        case ULOG_RD_ERROR:
        case ULOG_UNK_ERROR:

			printf( "Log outcome: %s\n", ULogEventOutcomeNames[outcome] );
			done = true;
			break;
 
        case ULOG_OK:

			printf( "Log event: %s", ULogEventNumberNames[e->eventNumber] );

			if ( ce.CheckAnEvent(e, errorMsg) != CheckEvents::EVENT_OKAY ) {
				fprintf(stderr, "%s\n", errorMsg.Value());
				result = 1;
			}

			if( e->eventNumber == ULOG_SUBMIT ) {
				SubmitEvent* ee = (SubmitEvent*) e;
				printf( " (\"%s\")", ee->submitEventLogNotes );
				++totalSubmitted;
				++netSubmitted;
				printf( "\n Total submitted: %d; net submitted: %d\n",
						totalSubmitted, netSubmitted );
			}
			
			if( e->eventNumber == ULOG_JOB_HELD ) {
				JobHeldEvent* ee = (JobHeldEvent*) e;
				printf( " (code=%d subcode=%d)", ee->getReasonCode(),
						ee->getReasonSubCode());
			}

			if( e->eventNumber == ULOG_JOB_TERMINATED ) {
				--netSubmitted;
				printf( "\n Total submitted: %d; net submitted: %d\n",
						totalSubmitted, netSubmitted );
			}

			if( e->eventNumber == ULOG_JOB_ABORTED ) {
				--netSubmitted;
				printf( "\n Total submitted: %d; net submitted: %d\n",
						totalSubmitted, netSubmitted );
			}

			if( e->eventNumber == ULOG_EXECUTABLE_ERROR ) {
				--netSubmitted;
				printf( "\n Total submitted: %d; net submitted: %d\n",
						totalSubmitted, netSubmitted );
			}

			printf( "\n" );
			break;

		default:

			fprintf(stderr, "Unexpected read event outcome!\n");
			result = 1;
			break;
        }
	}

	MyString errorMsg;
	if ( !ce.CheckAllJobs(errorMsg) ) {
		fprintf(stderr, "%s\n", errorMsg.Value());
		result = 1;
	}

	if ( result == 0 ) {
		printf("Log is okay\n");
	} else {
		printf("Log has error(s)\n");
	}
	return result;
}
