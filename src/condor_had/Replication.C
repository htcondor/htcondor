#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "ReplicatorStateMachine.h"

/* daemon core needs this variable to associate its entries
 * inside $CONDOR_CONFIG file
 */
char* mySubSystem  = "REPLICATION";
// replication daemon single object
ReplicatorStateMachine* stateMachine = NULL;

int
main_init( int , char *[] )
{
    dprintf(D_ALWAYS, "main_init replication daemon started\n");
//    int oldflags = fcntl(1, F_GETFL, 0);
//    dprintf(D_ALWAYS, "Is non-blocking file behaviour set: %d\n", O_NONBLOCK 
//            & oldflags);

    try {
        stateMachine = new ReplicatorStateMachine( );
        stateMachine->initialize( );

        return TRUE;
    }
    catch(char* exceptionString) {
        dprintf( D_FAILURE, "main_init exception thrown %s\n",
                   exceptionString );

        return FALSE;
    }
}

int
main_shutdown_graceful( )
{
    dprintf( D_ALWAYS, "main_shutdown_graceful started\n" );

    delete stateMachine;
    DC_Exit( 0 );

    return 0;
}

int
main_shutdown_fast( )
{
    delete stateMachine;
    DC_Exit( 0 );

    return 0;
}

int
main_config( bool isFull )
{
    // NOTE: restart functionality instead of reconfig
	stateMachine->reinitialize( );
    
    return 0;
}

void
main_pre_dc_init( int argc, char* argv[] )
{
}

void
main_pre_command_sock_init( )
{
}

