
#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"

#include "sslutils.h"	// for proxy_get_filenames

#include "gridmanager.h"


char *mySubSystem = "GRIDMANAGER";	// used by Daemon Core

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

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

	// Find the location of our proxy file, if we don't already
	// know (from the command line)
	if (X509Proxy == NULL) {
		proxy_get_filenames(NULL, 1, NULL, NULL, &X509Proxy, NULL, NULL);
		if ( X509Proxy == NULL ) {
			dprintf(D_ALWAYS,"Error finding X509 proxy filename. "
					"Proxy file probably doesn't exist. Aborting.\n");
			return false;
		}
	}

	if(first_time) {
		char buf[1024];
		snprintf(buf,1024,"X509_USER_PROXY=%s",X509Proxy);
		putenv(buf);
		first_time = false;
	}		

	if ( GahpMain.Initialize( X509Proxy )  == false ) {
		dprintf( D_ALWAYS, "Error initializing GAHP\n" );
		return false;
	}

	GahpMain.setMode( GahpClient::blocking );

	err = GahpMain.globus_gram_client_callback_allow( gramCallbackHandler,
													  NULL,
													  &gramCallbackContact );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GRAM callback, err=%d - %s\n", 
			err, GahpMain.globus_gram_client_error_string(err) );
		return false;
	}

	err = GahpMain.globus_gass_server_superez_init( &gassServerUrl, 0 );
	if ( err != GLOBUS_SUCCESS ) {
		dprintf( D_ALWAYS, "Error enabling GASS server, err=%d\n", err );
		return false;
	}
	dprintf( D_FULLDEBUG, "GASS server URL: %s\n", gassServerUrl );

	return true;
}


bool
main_deactivate_globus()
{
	return true;
}


int
main_init( int argc, char **argv )
{

	dprintf(D_FULLDEBUG,
		"Welcome to the all-singing, all dancing, \"amazing\" GridManager!\n");

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

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	// Activate Globus libraries
	if ( main_activate_globus() == false ) {
		dprintf(D_ALWAYS,"Failed to activate Globus Libraries\n");
		DC_Exit(1);
	}

	Init();
	Register();

	return TRUE;
}

int
main_config( bool is_full )
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

void
main_pre_dc_init( int argc, char* argv[] )
{
}

void
main_pre_command_sock_init( )
{
}

// This function is called by dprintf - always display our pid in our
// log entries. 
extern "C" 
int
display_dprintf_header(FILE *fp)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	fprintf( fp, "[%ld] ", mypid );

	return TRUE;
}
