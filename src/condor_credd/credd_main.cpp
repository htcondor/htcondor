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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "basename.h"
#include "condor_config.h"
#include "credd.h"
#include "subsystem_info.h"

void Init();
void Register();

char * myUserName = NULL;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(char **buf,int *bufpos,int *buflen);

int CheckCredentials_tid = -1;
extern int CheckCredentials_interval;

int myproxyGetDelegationReaperId = -1;

void
usage( char *name )
{
	dprintf( D_ALWAYS,
		"Usage: %s -s <schedd name>\n",
		condor_basename( name ) );
	DC_Exit( 1 );
}

void
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
	}
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
                                                     CheckCredentials,
                                                      "CheckCredentials" );

  myproxyGetDelegationReaperId = daemonCore->Register_Reaper(
					   "GetDelegationReaper",
					   (ReaperHandler) &MyProxyGetDelegationReaper,
					   "GetDelegation Reaper");

}

void
Reconfig()
{
}


void
main_config()
{
	Reconfig();
}

void
main_shutdown_fast()
{
	DC_Exit(0);
}

void
main_shutdown_graceful()
{
	DC_Exit(0);
}

int
main( int argc, char **argv )
{
	set_mySubSystem( "CREDD", SUBSYSTEM_TYPE_DAEMON );	// used by Daemon Core

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}

// This function is called by dprintf - always display our pid in our
// log entries.
extern "C"
int
display_dprintf_header(char **buf,int *bufpos,int *buflen)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	return sprintf_realloc( buf, bufpos, buflen, "[%ld] ", (long)mypid );
}
