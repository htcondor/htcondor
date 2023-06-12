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

#ifdef WIN32
#include "condor_header_features.h"
#include "condor_sys_nt.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "read_user_log.h"

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

            eventName = e->eventName();

			printf( "Log event: %s", eventName );

			if( e->eventNumber == ULOG_SUBMIT ) {
				SubmitEvent* ee = (SubmitEvent*) e;
				printf( " (\"%s\")", ee->submitEventLogNotes.c_str() );
			}
			
			if( e->eventNumber == ULOG_JOB_HELD ) {
				JobHeldEvent* ee = (JobHeldEvent*) e;
				printf( " (code=%d subcode=%d)",ee->getReasonCode(),ee->getReasonSubCode());
			}

			if( e->eventNumber == ULOG_JOB_DISCONNECTED ) {
				JobDisconnectedEvent* ee = (JobDisconnectedEvent*) e;
				printf( " (can_reconnect=True addr=%s name=%s "
						"reason=\"%s\")", ee->getStartdAddr(),
						ee->getStartdName(), ee->getDisconnectReason() );
			}
			if( e->eventNumber == ULOG_JOB_RECONNECTED ) {
				JobReconnectedEvent* ee = (JobReconnectedEvent*) e;
				printf( " (name=%s startd=%s starter=%s)",
						ee->getStartdName(), ee->getStartdAddr(),
						ee->getStarterAddr() );
			}
			if( e->eventNumber == ULOG_JOB_RECONNECT_FAILED ) {
				JobReconnectFailedEvent* ee = (JobReconnectFailedEvent*) e;
				printf( " (name=%s reason=\"%s\")", ee->getStartdName(),
						ee->getReason() );
			}

			printf( "\n" );

			break;

		default:

			assert( false );
			break;
        }
	}
}
