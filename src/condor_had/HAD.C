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
