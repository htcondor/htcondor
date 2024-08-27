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
#include "stl_string_utils.h"
#include "read_multiple_logs.h"
#include "check_events.h"
#include "condor_config.h"

int main(int argc, char **argv)
{
	int		result = 0;

	if ( argc <= 1 || (argc >= 2 && !strcmp("-usage", argv[1])) ) {
		printf("Usage: condor_check_userlogs <log file 1> "
				"[log file 2] ... [log file n]\n");
		exit(0);
	}

		// Set up dprintf.
	dprintf_set_tool_debug("condor_check_userlogs", 0);
	set_debug_flags(NULL, D_ALWAYS);

	std::vector<std::string> logFiles {&argv[1], &argv[argc]};

	ReadMultipleUserLogs	ru;
	for (const auto &filename : logFiles) {
		CondorError errstack;
		if ( !ru.monitorLogFile( filename, false, errstack ) ) {
			fprintf( stderr, "Error monitoring log file %s: %s\n", filename.c_str(),
						errstack.getFullText().c_str() );
			result = 1;
		}
	}

	bool logsMissing = false;

	CheckEvents		ce;
	int totalSubmitted = 0;
	int netSubmitted = 0;
	bool done = false;
	while( !done ) {

    	ULogEvent* e = NULL;
		std::string errorMsg;

        ULogEventOutcome outcome = ru.readEvent( e );

        switch (outcome) {

        case ULOG_RD_ERROR:
        case ULOG_UNK_ERROR:
			logsMissing = true;
			// Fall through
			//@fallthrough@
        case ULOG_NO_EVENT:

			printf( "Log outcome: %s\n", ULogEventOutcomeNames[outcome] );
			done = true;
			break;
 
        case ULOG_OK:

			printf( "Log event: %s (%d.%d.%d)",
						e->eventName(),
						e->cluster, e->proc, e->subproc );

			if ( ce.CheckAnEvent(e, errorMsg) != CheckEvents::EVENT_OKAY ) {
				fprintf(stderr, "%s\n", errorMsg.c_str());
				result = 1;
			}

			if( e->eventNumber == ULOG_SUBMIT ) {
				SubmitEvent* ee = (SubmitEvent*) e;
				printf( " (\"%s\")", ee->submitEventLogNotes.c_str() );
				++totalSubmitted;
				++netSubmitted;
				printf( "\n Total submitted: %d; net submitted: %d\n",
						totalSubmitted, netSubmitted );
			}
			
			if( e->eventNumber == ULOG_JOB_HELD ) {
				JobHeldEvent* ee = (JobHeldEvent*) e;
				printf( " (code=%d subcode=%d)", ee->code,
						ee->subcode);
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

	for (auto& filename: logFiles) {
		CondorError errstack;
		if ( !ru.unmonitorLogFile( filename, errstack ) ) {
			fprintf( stderr, "Error unmonitoring log file %s: %s\n", filename.c_str(),
						errstack.getFullText().c_str() );
			result = 1;
		}
	}

	std::string errorMsg;
	CheckEvents::check_event_result_t checkAllResult =
				ce.CheckAllJobs(errorMsg);
	if ( checkAllResult != CheckEvents::EVENT_OKAY ) {
		fprintf(stderr, "%s\n", errorMsg.c_str());
		fprintf(stderr, "CheckAllJobs() result: %s\n",
					CheckEvents::ResultToString(checkAllResult));
		result = 1;
	}

	if ( result == 0 ) {
		if ( !logsMissing ) {
			printf("Log(s) are okay\n");
		} else {
			printf("Log(s) may be okay\n");
			printf(  "Some logs cannot be read\n");
		}
	} else {
		printf("Log(s) have error(s)\n");
	}
	return result;
}
