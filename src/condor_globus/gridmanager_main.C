
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"

// Globus include files
#include "globus_common.h"
#include "globus_gram_client.h"

#include "gridmanager.h"


char *mySubSystem = "GRIDMANAGER";	// used by Daemon Core

GridManager gridmanager;

void
usage( char *name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-b] [-t] [-p <port>] [-s <schedd addr>] [-x]\n",
		basename( name ) );
	DC_Exit( 1 );
}

int
main_init( int argc, char **argv )
{
	int err;

	// handle specific command line args
	int i = 1;
	while ( i < argc ) {
		if ( argv[i][0] != '-' )
			usage( argv[0] );

		switch( argv[i][1] ) {
		case 's':
			// don't check parent for schedd addr. use this one instead
			if ( argc <= i + 1 )
				usage( argv[0] );
			gridmanager.ScheddAddr = strdup( argv[i + 1] );
			i++;
			break;
		case 'x':
			// test mode
			test_mode = 1;
			break;
		default:
			usage( argv[0] );
			break;
		}

		i++;
	}

	err = globus_module_activate( GLOBUS_COMMON_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error initializing globus, err=%d\n", err );
	}

	err = globus_module_activate( GLOBUS_GRAM_CLIENT_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error initializing globus GRAM, err=%d\n", err );
		DC_Exit( 1 );
	}

	err = globus_gram_client_callback_allow( GridManager::gramCallbackHandler,
											 &gridmanager,
											 &gridmanager.gramCallbackContact );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GRAM callback, err=%d\n", err );
		globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
		DC_Exit( 1 );
	}

	gridmanager.Init();
	gridmanager.Register();

	// Maybe trigger a check of the schedd's jobs here
	if ( test_mode ) {
		daemonCore->Send_Signal( getpid(), ADD_JOBS );
	}

	return TRUE;
}

int
main_config()
{
	gridmanager.reconfig();
	return TRUE;
}

int
main_shutdown_fast()
{
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	DC_Exit(0);
	return TRUE;	// to satify c++
}
