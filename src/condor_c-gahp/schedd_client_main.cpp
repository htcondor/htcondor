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
#include "globus_utils.h"
#include "condor_config.h"
#include "subsystem_info.h"

#include "schedd_client.h"
#include "io_loop.h"
#include "PipeBuffer.h"

DECL_SUBSYSTEM( "C_GAHP_WORKER_THREAD", SUBSYSTEM_TYPE_GAHP );

char * myUserName = NULL;

extern char * ScheddAddr;

extern int RESULT_OUTBOX;
extern int REQUEST_INBOX;

extern PipeBuffer request_buffer;

int io_loop_pid = -1;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

int my_fork();
extern int schedd_loop( void* arg, Stream * s);


void
usage()
{
	dprintf( D_ALWAYS, "Usage: c-gahp_worker_thread -s <schedd> [-P <pool>] -I <fd> -O <fd> \n");
	DC_Exit( 1 );
}

int init_pipes();

int
main_init( int argc, char ** const argv )
{

	dprintf(D_FULLDEBUG, "Welcome to the C-GAHP\n");

	// handle specific command line args
	int i = 1;
	while ( i < argc ) {
		if ( argv[i][0] != '-' )
			usage();

		switch( argv[i][1] ) {
		case 's':
			// don't check parent for schedd addr. use this one instead
			if ( argc <= i + 1 )
				usage();
			ScheddAddr = strdup( argv[i + 1] );
			i++;
			break;
		case 'P':
			// specify what pool (i.e. collector) to lookup the schedd name
			if ( argc <= i + 1 )
				usage();
			ScheddPool = strdup( argv[i + 1] );
			i++;
			break;
		default:
			usage();
			break;
		}

		i++;
	}

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	Init();
	Register();
	Reconfig();

	// inherit the DaemonCore pipes that our parent created
	REQUEST_INBOX = daemonCore->Inherit_Pipe(fileno(stdin),
						 false,		// read pipe
						 true,		// registerable
						 false);	// blocking
	RESULT_OUTBOX = daemonCore->Inherit_Pipe(fileno(stdout),
						 true,		// write pipe
						 false,		// nonregistrable
						 false);	// blocking

	request_buffer.setPipeEnd( REQUEST_INBOX );


	daemonCore->Register_Timer(0, 
		  (TimerHandler)&init_pipes,
		  "init_pipes", NULL );


    // Just set up timers....
    contactScheddTid = daemonCore->Register_Timer( 
		   contact_schedd_interval,
		  (TimerHandler)&doContactSchedd,
		  "doContactSchedD", NULL );


	
	return TRUE;
}

int 
init_pipes() {
	dprintf (D_FULLDEBUG, "PRE Request pipe initialized\n");

	(void)daemonCore->Register_Pipe (
									 request_buffer.getPipeEnd(),
									 "request pipe",
									 (PipeHandler)&request_pipe_handler,
									 "request_pipe_handler");


	dprintf (D_FULLDEBUG, "Request pipe initialized\n");
	return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Misc daemoncore crap

void
Init() {
}

void
Register() {
}

void
Reconfig()
{
	contact_schedd_interval = 
		param_integer ("C_GAHP_CONTACT_SCHEDD_DELAY", 5);

	useXMLClassads = param_boolean( "GAHP_USE_XML_CLASSADS", false );

		// When GSI authentication is used, we're willing to trust schedds
		// which have the same credential as the job
	if ( proxySubjectName ) {
		char *daemon_subjects = param( "GSI_DAEMON_NAME" );
		if ( daemon_subjects ) {
			MyString buff;
			buff.sprintf( "%s,%s", daemon_subjects, proxySubjectName );
			dprintf( D_ALWAYS, "Setting %s=%s\n", "GSI_DAEMON_NAME",
					 buff.Value() );
				// We must use our daemon subsystem prefix in case the
				// admin used it in the config file.
			config_insert( "C_GAHP_WORKER_THREAD.GSI_DAEMON_NAME",
						   buff.Value() );
			free( daemon_subjects );
		}
	}
}


int
main_config( bool )
{
	Reconfig();
	return TRUE;
}

int
main_shutdown_fast()
{
#ifndef WIN32
	if (io_loop_pid != -1)
		kill(io_loop_pid, SIGKILL);
#endif
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
#ifndef WIN32
	if (io_loop_pid != -1)
		kill(io_loop_pid, SIGTERM);
#endif
	DC_Exit(0);
	return TRUE;	// to satify c++
}

void
main_pre_dc_init( int, char*[] )
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
