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
#include "globus_utils.h"
#include "condor_config.h"

#include "schedd_client.h"
#include "io_loop.h"
#include "FdBuffer.h"

// How often we contact the schedd (secs)
extern int contact_schedd_interval;

// How often we check results in the pipe (secs)
extern int check_requests_interval;


char *mySubSystem = "C_GAHP_WORKER_THREAD";	// used by Daemon Core

char * myUserName = NULL;

extern char * ScheddAddr;

extern int RESULT_OUTBOX;
extern int REQUEST_INBOX;


extern FdBuffer request_buffer;

int io_loop_pid = -1;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

int my_fork();
extern int schedd_loop( void* arg, Stream * s);


void
usage( char *name )
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
			usage( argv[0] );

		switch( argv[i][1] ) {
		case 's':
			// don't check parent for schedd addr. use this one instead
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddAddr = strdup( argv[i + 1] );
			i++;
			break;
		case 'P':
			// specify what pool (i.e. collector) to lookup the schedd name
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddPool = strdup( argv[i + 1] );
			i++;
			break;
		case 'I':
				// specify fd of the read-end of request pipe
		   if ( argc <= i + 1 )
				usage( argv[0] );
			REQUEST_INBOX = atoi( argv[i + 1] );
			i++;
			break;
		case 'O':
				// specify fd of the write-end of result pipe
		   if ( argc <= i + 1 )
				usage( argv[0] );
			RESULT_OUTBOX = atoi( argv[i + 1] );
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
	Reconfig();


	request_buffer.setFd( REQUEST_INBOX );


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
									 request_buffer.getFd(),
									 "request pipe",
									 (PipeHandler)&request_pipe_handler,
									 "request_pipe_handler");


	dprintf (D_FULLDEBUG, "Request pipe initialized\n");
}

/////////////////////////////////////////////////////////////////////////////
// Misc daemoncore crap

void
Init() {
	contact_schedd_interval = 
		param_integer ("C_GAHP_CONTACT_SCHEDD_DELAY", 20);
	check_requests_interval = 
		param_integer ("C_GAHP_CHECK_NEW_REQUESTS_DELAY", 5);

}

void
Register() {
}

void
Reconfig()
{
	useXMLClassads = param_boolean( "GAHP_USE_XML_CLASSADS", false );
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
	if (io_loop_pid != -1)
		kill(io_loop_pid, SIGKILL);
	DC_Exit(0);
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	if (io_loop_pid != -1)
		kill(io_loop_pid, SIGTERM);
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
