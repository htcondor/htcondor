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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "basename.h"
#include "globus_utils.h"
#include "condor_config.h"
#include "PipeBuffer.h"
#include "io_loop.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

char *mySubSystem = "AMAZON_GAHP_WORKER_THREAD";	// used by Daemon Core

char * myUserName = NULL;

// Buffer for reading requests from the IO thread
PipeBuffer request_buffer;

int RESULT_OUTBOX = -1;
int REQUEST_INBOX = -1;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

void
usage()
{
	dprintf( D_ALWAYS, "Usage: amazon-gahp_worker_thread\n");
	DC_Exit( 1 );
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
	MyString tmp_string;
	if( find_amazon_lib(tmp_string) == false ) {
		DC_Exit(1);
	}

	set_amazon_lib_path(tmp_string.Value());
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

int init_pipes();

static void
enqueue_result(const char *buffer, int length)
{
	if(!buffer || !length ) {
		return;
	}

	daemonCore->Write_Pipe( RESULT_OUTBOX, buffer, length);
}

static int
handle_gahp_command(char ** argv, int argc) 
{
	// Assume it's been verified
	if( argc < 2 ) {
		dprintf (D_ALWAYS, "Invalid request\n");
		return FALSE;
	}

	if( !verify_request_id(argv[1]) ) {
		dprintf (D_ALWAYS, "Invalid request ID\n");
		return FALSE;
	}

	MyString output_string;
	bool result = executeWorkerFunc(argv[0], argv, argc, output_string);

	if( output_string.IsEmpty() == false ) {
		enqueue_result(output_string.Value(), output_string.Length());
	}

	return result;
}

static int
request_pipe_handler(Service*, int) 
{

	MyString* next_line = NULL;
	while ((next_line = request_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "got work request: %s\n", next_line->Value());

		Gahp_Args args;

			// Parse the command...
		if (!( parse_gahp_command (next_line->Value(), &args) &&
			  handle_gahp_command (args.argv, args.argc) )) {
			dprintf (D_ALWAYS, "ERROR processing %s\n", next_line->Value());
		}

			// Clean up...
		delete  next_line;
	}

	// check for an error in GetNextLine
	if (request_buffer.IsError() || request_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "Request pipe closed. Exiting...\n");
		DC_Exit (1);
	}

	return TRUE;
}

int
main_init( int argc, char ** const argv )
{
	dprintf(D_FULLDEBUG, "Welcome to the AMAZON-GAHP\n");

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	Init();
	Register();
	Reconfig();

	// Find the path of amazon library (e.g. perl script)
	MyString tmp_string;
	if( find_amazon_lib(tmp_string) == false ) {
		DC_Exit(1);
	}
	set_amazon_lib_path(tmp_string.Value());

	// Save current working_dir;
	char tmpCwd[_POSIX_PATH_MAX];
	if( !getcwd(tmpCwd, _POSIX_PATH_MAX) ) {
		dprintf(D_ALWAYS, "Failed to getcwd\n");
		DC_Exit(1);
	}
	set_working_dir(tmpCwd);

	// Register all amazon commands
	if( registerAllAmazonCommands() == false ) {
		dprintf(D_ALWAYS, "Can't register Amazon Commands\n");
		DC_Exit( 1 );
	}

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
