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

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

void
usage( char *name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-b] [-t] [-p <port>] [-s <schedd addr>]\n"
					   "   [-x <x509_user_proxy>] [-j <JobClusterID.JobProcID>]\n",
		condor_basename( name ) );
	DC_Exit( 1 );
}

bool
main_activate_globus()
{
	int err;
	static int first_time = true;
	char *proxyString;
	
	if(gridmanager.X509Proxy && first_time) {
		 proxyString = (char *)malloc(strlen("X509_USER_PROXY=") +
                strlen(gridmanager.X509Proxy)+1);
        sprintf(proxyString, "X509_USER_PROXY=%s",
            gridmanager.X509Proxy);
        if(putenv(proxyString) < 0) {
            dprintf(D_ALWAYS, "putenv(\"%s\") failed!\n",
                proxyString);
		}
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

	err = globus_gram_client_callback_allow( GridManager::gramCallbackHandler,
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
	char *ptr1, *ptr2;

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
			// Use a different user_proxy;
			if ( argc <= i + 1 )
				usage( argv[0] );
			gridmanager.X509Proxy = strdup( argv[i + 1] );
			gridmanager.useDefaultProxy = false;
			i++;
			break;
		case 'j':
			// Specify a specific job to manage
			if ( argc <= i + 1 ) {
				usage( argv[0] );
			}
			ptr1 = strdup( argv[i + 1] );
			ptr2 = strchr( ptr1, '.' );
			if ( ptr2 == NULL ) {
				usage( argv[0] );
			}
			ptr2++;
			if ( *ptr2 == '\0' ) {
				usage( argv[0] );
			}
			gridmanager.m_cluster = atoi(ptr1);
			gridmanager.m_proc = atoi(ptr2);
			if ( gridmanager.m_cluster == 0 ) {
				usage( argv[0] );
			}
			free(ptr1);		// since we strduped it above
			i++;
			break;
		default:
			usage( argv[0] );
			break;
		}

		i++;
	}

	// Setup dprintf to display pid and/or job id
	DebugId = display_dprintf_header;

	// Activate Globus libraries
	if ( main_activate_globus() == false ) {
		dprintf(D_ALWAYS,"Failed to activate Globus Libraries\n");
		DC_Exit(1);
	}

	gridmanager.Init();
	gridmanager.Register();

	// Trigger a check of the schedd's jobs here to make certain we start
	// managing any globus universe jobs already in the queue at the time we're born
	daemonCore->Send_Signal( getpid(), GRIDMAN_ADD_JOBS );

	return TRUE;
}

int
main_config( bool is_full )
{
	gridmanager.reconfig();
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


// This function is called by dprintf - always display our job, proc,
// and pid in our log entries. 
extern "C" 
int
display_dprintf_header(FILE *fp)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	if ( gridmanager.m_cluster ) {
		fprintf( fp, "(%d.%d) [%ld]: ", gridmanager.m_cluster,
			gridmanager.m_proc,mypid );
	} else {
		fprintf( fp, "[%ld]: ", mypid );
	}	

	return TRUE;
}
