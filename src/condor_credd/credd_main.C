/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"
#include "credd.h"

void Init();
void Register();

char *mySubSystem = "CREDD";	// used by Daemon Core

char * myUserName = NULL;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

int CheckCredentials_tid = -1;
extern int CheckCredentials_interval;

int myproxyGetDelegationReaperId = -1;

void
usage( char *name )
{
	dprintf( D_ALWAYS,
		"Usage: condor_gahp -s <schedd name>\n",
		condor_basename( name ) );
	DC_Exit( 1 );
}

int
main_init( int argc, char ** const argv )
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
			//ScheddAddr = strdup( argv[i + 1] );
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

	Init();
	Register();
	if (!LoadCredentialList()) {
	  dprintf (D_ALWAYS, "Unable to Load Credentials\n");
	  return FALSE;
	}

	return TRUE;
}


void
Register() {
  daemonCore->Register_Command(CREDD_STORE_CRED, "CREDD_STORE_CRED",
                               (CommandHandler)&store_cred_handler,
                               "store_cred_handler", NULL,WRITE);

  daemonCore->Register_Command(CREDD_GET_CRED, "CREDD_GET_CRED",
                               (CommandHandler)&get_cred_handler,
                               "get_cred_handler", NULL,WRITE);

  daemonCore->Register_Command(CREDD_REMOVE_CRED, "CREDD_REMOVE_CRED",
                               (CommandHandler)&rm_cred_handler,
                               "rm_cred_handler", NULL,WRITE);

  daemonCore->Register_Command(CREDD_QUERY_CRED, "CREDD_QUERY_CRED",
                               (CommandHandler)&query_cred_handler,
                               "query_cred_handler", NULL,WRITE);

  CheckCredentials_tid = daemonCore->Register_Timer( 1, 
						     CheckCredentials_interval,
                                                     (TimerHandler)&CheckCredentials,
                                                      "CheckCredentials", 
						     NULL );

  myproxyGetDelegationReaperId = daemonCore->Register_Reaper(
					   "GetDelegationReaper",
					   (ReaperHandler) &MyProxyGetDelegationReaper,
					   "GetDelegation Reaper");

}

void
Reconfig()
{
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
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
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

	fprintf( fp, "[%ld] ", (long)mypid );

	return TRUE;
}
