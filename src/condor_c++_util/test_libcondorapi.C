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

			if( e->eventNumber == ULOG_JOB_DISCONNECTED ) {
				JobDisconnectedEvent* ee = (JobDisconnectedEvent*) e;
				if( ee->canReconnect() ) {
					printf( " (can_reconnect=True addr=%s name=%s "
							"reason=\"%s\")", ee->getStartdAddr(),
							ee->getStartdName(), ee->getDisconnectReason() );
				} else {
					printf( " (can_reconnect=False addr=%s name=%s "
							"dis_reason=\"%s\" no_reconnect=\"%s\")",
							ee->getStartdAddr(), ee->getStartdName(),
							ee->getDisconnectReason(),
							ee->getNoReconnectReason() );
				}
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
