
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

void
usage( char *name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-b] [-t] [-p <port>] [-s <schedd addr>] -x <x509_user_proxy>]\n",
		basename( name ) );
	DC_Exit( 1 );
}

bool
main_activate_globus()
{
	int err;
	static int first_time = true;

	if(gridmanager.X509Proxy && first_time) {
		setenv("X509_USER_PROXY", gridmanager.X509Proxy, 1);
		first_time = false;
	}		

	err = globus_module_activate( GLOBUS_GRAM_CLIENT_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error initializing GRAM, err=%d - %s\n", 
			err, globus_gram_client_error_string(err) );
		return false;
	}

	if ( gramCallbackContact ) {
		globus_libc_free(gramCallbackContact);
		gramCallbackContact = NULL;
	}

	err = globus_gram_client_callback_allow( gramCallbackHandler,
											 &gridmanager,
											 &gramCallbackContact );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GRAM callback, err=%d - %s\n", 
			err, globus_gram_client_error_string(err) );
		globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
		return false;
	}

	err = globus_module_activate( GLOBUS_GASS_SERVER_EZ_MODULE );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error initializing GASS server, err=%d\n", err );
		globus_gram_client_callback_disallow( gramCallbackContact );
		globus_module_deactivate( GLOBUS_GRAM_CLIENT_MODULE );
		return false;
	}

	err = globus_gass_server_ez_init( &gassServerListener, NULL, NULL, NULL,
									  GLOBUS_GASS_SERVER_EZ_READ_ENABLE |
									  GLOBUS_GASS_SERVER_EZ_LINE_BUFFER |
									  GLOBUS_GASS_SERVER_EZ_WRITE_ENABLE,
									  NULL );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GASS server, err=%d\n", err );
		globus_gram_client_callback_disallow( gramCallbackContact );
		globus_module_deactivate_all();
		return false;
	}

	if ( gassServerUrl ) {
		globus_libc_free(gassServerUrl);
		gassServerUrl = NULL;
	}
	gassServerUrl = globus_gass_transfer_listener_get_base_url(gassServerListener);
	return true;
}


bool
main_deactivate_globus()
{
	globus_gram_client_callback_disallow( gramCallbackContact );
	globus_gass_server_ez_shutdown( gassServerListener );
	globus_module_deactivate_all();
	return true;
}


int
main_init( int argc, char **argv )
{
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
			ScheddAddr = strdup( argv[i + 1] );
			i++;
			break;
		case 'x':
			// Use a different user_proxy;
			if ( argc <= i + 1 )
				usage( argv[0] );
			X509Proxy = strdup( argv[i + 1] );
			useDefaultProxy = false;
			i++;
			break;
		default:
			usage( argv[0] );
			break;
		}

		i++;
	}

	// Activate Globus libraries
	if ( main_activate_globus() == false ) {
		dprintf(D_ALWAYS,"Failed to activate Globus Libraries\n");
		DC_Exit(1);
	}

	Init();
	Register();

	// Trigger a check of the schedd's jobs here to make certain we start
	// managing any globus universe jobs already in the queue at the time we're born
	daemonCore->Send_Signal( getpid(), GRIDMAN_ADD_JOBS );

	return TRUE;
}

int
main_config()
{
	Reconfig();
	return TRUE;
}

int
main_shutdown_fast()
{
	main_deactivate_globus();
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	main_deactivate_globus();
	DC_Exit(0);
	return TRUE;	// to satify c++
}
