
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"

// Globus include files
#include "globus_common.h"
#include "globus_gram_client.h"
#include "globus_gass_server_ez.h"

#include "gm_common.h"
#include "gridmanager.h"


char *gramCallbackContact = NULL;
char *gassServerUrl = NULL;
globus_gass_transfer_listener_t gassServerListener;

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

	err = globus_module_activate( GLOBUS_GRAM_CLIENT_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error initializing GRAM, err=%d - %s\n", 
			err, globus_gram_client_error_string(err) );
		DC_Exit( 1 );
	}

	err = globus_gram_client_callback_allow( GridManager::gramCallbackHandler,
											 &gridmanager,
											 &gramCallbackContact );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GRAM callback, err=%d - %s\n", 
			err, globus_gram_client_error_string(err) );
		globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
		DC_Exit( 1 );
	}

	err = globus_module_activate( GLOBUS_GASS_SERVER_EZ_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error initializing GASS server, err=%d\n", err );
		globus_gram_client_callback_disallow( gramCallbackContact );
		globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
		DC_Exit( 1 );
	}

	err = globus_gass_server_ez_init( &gassServerListener, NULL, NULL, NULL,
									  GLOBUS_GASS_SERVER_EZ_READ_ENABLE |
									  GLOBUS_GASS_SERVER_EZ_WRITE_ENABLE,
									  NULL );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GASS server, err=%d\n", err );
		globus_gram_client_callback_disallow( gramCallbackContact );
		globus_module_deactivate_all();
		DC_Exit( 1 );
	}

	gassServerUrl = globus_gass_transfer_listener_get_base_url(gassServerListener);

	gridmanager.Init();
	gridmanager.Register();

	// Trigger a check of the schedd's jobs here to make certain we start
	// managing any globus universe jobs already in the queue at the time we're born
	daemonCore->Send_Signal( getpid(), GRIDMAN_ADD_JOBS );

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
	globus_gram_client_callback_disallow( gramCallbackContact );
	globus_gass_server_ez_shutdown( gassServerListener );
	globus_module_deactivate_all();
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	globus_gram_client_callback_disallow( gramCallbackContact );
	globus_gass_server_ez_shutdown( gassServerListener );
	globus_module_deactivate_all();
	DC_Exit(0);
	return TRUE;	// to satify c++
}
