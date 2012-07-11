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
#include "io_loop.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_daemon_core.h"
#include "PipeBuffer.h"
#include "globus_utils.h"
#include "subsystem_info.h"

const char * version = "$GahpVersion 2.0.1 Jul 30 2012 Condor_FT_GAHP $";


int async_mode = 0;
int new_results_signaled = 0;

int flush_request_tid = -1;

// The list of results ready to be output to IO
StringList result_list;

// pipe buffers
PipeBuffer stdin_buffer; 

// this appears at the bottom of this file
extern "C" int display_dprintf_header(char **buf,int *bufpos,int *buflen);

#ifdef WIN32
int STDIN_FILENO = fileno(stdin);
#endif


void
usage()
{
	dprintf( D_ALWAYS,
		"Usage: condor_ft-gahp\n"
			 );
	DC_Exit( 1 );
}

void
main_init( int, char ** const)
{
	dprintf(D_FULLDEBUG, "FT-GAHP IO thread\n");

	int stdin_pipe = -1;
#if defined(WIN32)
	// if our parent is not DaemonCore, then our assumption that
	// the pipe we were passed in via stdin is overlapped-mode
	// is probably wrong. we therefore create a new pipe with the
	// read end overlapped and start a "forwarding thread"
	if (daemonCore->InfoCommandSinfulString(daemonCore->getppid()) == NULL) {

		dprintf(D_FULLDEBUG, "parent is not DaemonCore; creating a forwarding thread\n");

		int pipe_ends[2];
		if (daemonCore->Create_Pipe(pipe_ends, true) == FALSE) {
			EXCEPT("failed to create forwarding pipe");
		}
		forwarding_pipe = pipe_ends[1];
		HANDLE thread_handle = (HANDLE)_beginthreadex(NULL,                   // default security
		                                              0,                      // default stack size
		                                              pipe_forward_thread,    // start function
		                                              NULL,                   // arg: write end of pipe
		                                              0,                      // not suspended
													  NULL);                  // don't care about the ID
		if (thread_handle == NULL) {
			EXCEPT("failed to create forwarding thread");
		}
		CloseHandle(thread_handle);
		stdin_pipe = pipe_ends[0];
	}
#endif

	if (stdin_pipe == -1) {
		// create a DaemonCore pipe from our stdin
		stdin_pipe = daemonCore->Inherit_Pipe(fileno(stdin),
		                                      false,    // read pipe
		                                      true,     // registerable
		                                      false);   // blocking
	}

	stdin_buffer.setPipeEnd(stdin_pipe);

	(void)daemonCore->Register_Pipe (stdin_buffer.getPipeEnd(),
					"stdin pipe",
					(PipeHandler)&stdin_pipe_handler,
					"stdin_pipe_handler");

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

		// Print out the GAHP version to the screen
		// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);

	dprintf (D_FULLDEBUG, "FT-GAHP IO initialized\n");
}


int
stdin_pipe_handler(Service*, int) {

	std::string* line;
	while ((line = stdin_buffer.GetNextLine()) != NULL) {

		const char * command = line->c_str();

		dprintf (D_ALWAYS, "got stdin: %s\n", command);

		Gahp_Args args;

		if (parse_gahp_command (command, &args) &&
			verify_gahp_command (args.argv, args.argc)) {

				// Catch "special commands first
			if (strcasecmp (args.argv[0], GAHP_COMMAND_RESULTS) == 0) {
					// Print number of results
				std::string rn_buff;
				sprintf( rn_buff, "%d", result_list.number() );
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					rn_buff.c_str() };
				gahp_output_return (commands, 2);

					// Print each result line
				char * next;
				result_list.rewind();
				while ((next = result_list.next()) != NULL) {
					printf ("%s\n", next);
					fflush(stdout);
					result_list.deleteCurrent();
				}

				new_results_signaled = FALSE;
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_VERSION) == 0) {
				printf ("S %s\n", version);
				fflush (stdout);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				DC_Exit(0);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
				async_mode = TRUE;
				new_results_signaled = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
				async_mode = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				return 0; // exit
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_COMMANDS) == 0) {
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					GAHP_COMMAND_DOWNLOAD_SANDBOX,
					GAHP_COMMAND_UPLOAD_SANDBOX,
					GAHP_COMMAND_DESTROY_SANDBOX,
					GAHP_COMMAND_ASYNC_MODE_ON,
					GAHP_COMMAND_ASYNC_MODE_OFF,
					GAHP_COMMAND_RESULTS,
					GAHP_COMMAND_QUIT,
					GAHP_COMMAND_VERSION,
					GAHP_COMMAND_COMMANDS};
				gahp_output_return (commands, 10);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_DOWNLOAD_SANDBOX) == 0) {
				// TODO: implement
				dprintf(D_ALWAYS, "FTGAHP: download sandbox\n");
				gahp_output_return_error();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_UPLOAD_SANDBOX) == 0) {
				// TODO: implement
				dprintf(D_ALWAYS, "FTGAHP: upload sandbox\n");
				gahp_output_return_error();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_DESTROY_SANDBOX) == 0) {
				// TODO: implement
				dprintf(D_ALWAYS, "FTGAHP: destroy sandbox\n");
				gahp_output_return_error();
			} else {
				// should never get here if verify does its job
				gahp_output_return_error();
			}
			
		} else {
			gahp_output_return_error();
		}

		delete line;
	}

	// check if GetNextLine() returned NULL because of an error or EOF
	if (stdin_buffer.IsError() || stdin_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "stdin buffer closed, exiting\n");
		DC_Exit (1);
	}

	return TRUE;
}


void
handle_results( std::string line ) {
		// Add this to the list
	result_list.append (line.c_str());

	if (async_mode) {
		if (!new_results_signaled) {
			printf ("R\n");
			fflush (stdout);
		}
		new_results_signaled = TRUE;	// So that we only do it once
	}
}


// Check the parameters to make sure the command
// is syntactically correct
int
verify_gahp_command(char ** argv, int argc) {

	if (strcasecmp (argv[0], GAHP_COMMAND_DOWNLOAD_SANDBOX) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_UPLOAD_SANDBOX) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_DESTROY_SANDBOX) ==0) {
		// Expecting:GAHP_COMMAND_*_SANDBOX <req_id> <sandbox_id> <job_ad>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]);
	} else if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0 ||
				strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
	    // These are no-arg commands
	    return verify_number_args (argc, 1);
	}

	dprintf (D_ALWAYS, "Unknown command\n");

	return FALSE;
}

int
verify_number_args (const int is, const int should_be) {
	if (is != should_be) {
		dprintf (D_ALWAYS, "Wrong # of args %d, should be %d\n", is, should_be);
		return FALSE;
	}
	return TRUE;
}

int
verify_request_id (const char * s) {
    unsigned int i;
	for (i=0; i<strlen(s); i++) {
		if (!isdigit(s[i])) {
			dprintf (D_ALWAYS, "Bad request id %s\n", s);
			return FALSE;
		}
	}

	return TRUE;
}

int
verify_number (const char * s) {
	if (!s || !(*s))
		return FALSE;
	
	int i=0;
   
	do {
		if (s[i]<'0' || s[i]>'9')
			return FALSE;
	} while (s[++i]);

	return TRUE;
}
		

void
gahp_output_return (const char ** results, const int count) {
	int i=0;

	for (i=0; i<count; i++) {
		printf ("%s", results[i]);
		if (i < (count - 1 )) {
			printf (" ");
		}
	}


	printf ("\n");
	fflush(stdout);
}

void
gahp_output_return_success() {
	const char* result[] = {GAHP_RESULT_SUCCESS};
	gahp_output_return (result, 1);
}

void
gahp_output_return_error() {
	const char* result[] = {GAHP_RESULT_ERROR};
	gahp_output_return (result, 1);
}


void
main_config()
{
}

void
main_shutdown_fast()
{
}

void
main_shutdown_graceful()
{
	daemonCore->Cancel_And_Close_All_Pipes();
}

void
main_pre_command_sock_init( )
{
	daemonCore->WantSendChildAlive( false );
}

int
main( int argc, char **argv )
{
	set_mySubSystem("FT_GAHP", SUBSYSTEM_TYPE_GAHP);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;
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
