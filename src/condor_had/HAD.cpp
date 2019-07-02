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


/*
  HAD.cpp : Defines the entry point for the console application.
*/

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "subsystem_info.h"

#define  USE_REPLICATION    (0)

#include "StateMachine.h"

#if USE_REPLICATION
	#include "ReplicaStateMachine.h"
#endif // USE_REPLICATION

#include <iostream>
using namespace std;

extern char* myName;

HADStateMachine* stateMachine = NULL;

void
main_init (int, char *[])
{
    dprintf(D_ALWAYS,"Starting HAD ....\n");
    try {
        stateMachine = new HADStateMachine();

        stateMachine->initialize();
    } catch (char* rr) {
        cout << rr << endl;
        dprintf(D_ALWAYS, "Exception in main_init %s \n", rr);
    }
}

void
main_shutdown_graceful()
{

    dprintf(D_ALWAYS, "main_shutdown_graceful \n");
    if(stateMachine!=NULL){
        delete  stateMachine;
    }
    DC_Exit(0);
}


void
main_shutdown_fast()
{
    if(stateMachine!=NULL){
        delete  stateMachine;
    }

    DC_Exit(0);
}
/**
 * we employ the following terminology inside the function:
 * hard reconfiguration - for starting the HAD state machine from the scratch
 *                        and gracefully killing the negotiator
 * soft reconfiguration - for rereading configuration values from
 *                        $CONDOR_CONFIG without changing the state machine
 **/
void
main_config()
{
	if( stateMachine->isHardConfigurationNeeded( ) ) {
    	dprintf( D_ALWAYS, "main_config hard configuration started\n" );
		stateMachine->reinitialize( );
	} else {
		dprintf( D_ALWAYS, "main_config soft configuration started\n" );
		stateMachine->softReconfigure( );
	}
}


int
main( int argc, char **argv )
{
	set_mySubSystem( "HAD", SUBSYSTEM_TYPE_DAEMON );		// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}
