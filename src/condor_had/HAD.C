/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 *
 * The Condor High Availability Daemon (HAD) software was developed by
 * by the Distributed Systems Laboratory at the Technion Israel
 * Institute of Technology, and contributed to to the Condor Project.
 *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/*
  HAD.cpp : Defines the entry point for the console application.
*/

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_io.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_config.h"

#define  USE_REPLICATION    (0)

#include "StateMachine.h"

#if USE_REPLICATION
	#include "ReplicaStateMachine.h"
#endif // USE_REPLICATION


extern "C" int SetSyscalls(int val){return val;}
extern char* myName;

char *mySubSystem = "HAD";  // for daemon core

HADStateMachine* stateMachine = NULL;

int
main_init (int, char *[])
{
    dprintf(D_ALWAYS,"Starting HAD ....\n");	
    try {
#if USE_REPLICATION
        stateMachine = new ReplicaStateMachine();
#else
        stateMachine = new HADStateMachine();
#endif // USE_REPLICATION
	
        stateMachine->initialize();
        return TRUE;
    } catch (char* rr) {
        cout << rr << endl;
        dprintf(D_ALWAYS, "Exception in main_init %s \n", rr);
        return FALSE;
    }
}

int
main_shutdown_graceful()
{

    dprintf(D_ALWAYS, "main_shutdown_graceful \n");
    if(stateMachine!=NULL){
        delete  stateMachine;
    }
    DC_Exit(0);
    return 0;
}


int
main_shutdown_fast()
{
    if(stateMachine!=NULL){
        delete  stateMachine;
    }

    DC_Exit(0);
    return 0;
}

int
main_config( bool is_full )
{
    //Why not reinilize everything even if one of then can't reinilialize?
    bool ret = stateMachine->reinitialize();
    return ret;
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}
