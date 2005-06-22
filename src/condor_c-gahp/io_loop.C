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

#include "io_loop.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "schedd_client.h"
#include "FdBuffer.h"
#include "globus_utils.h"


const char * version = "$GahpVersion 2.0.0 Jan 21 2004 Condor\\ GAHP $";


char *mySubSystem = "C_GAHP";	// used by Daemon Core
int result_pipe_in_fd = 0;
int request_pipe_out_fd = 0;


int async_mode = 0;
int new_results_signaled = 0;

int flush_request_tid = -1;

// The list of results ready to be output to IO
StringList result_list;

// pipe buffers
FdBuffer stdin_buffer; 
FdBuffer result_buffer;

FdBuffer request_buffer;

// true iff worker thread is ready to receive more requests
bool worker_thread_ready = false;

// Queue for pending commands to worker thread
//template class SimpleList<char *>;
StringList pending_request_list;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);



void
usage( char *name )
{
	dprintf( D_ALWAYS,
		"Usage: condor_gahp -s <schedd name> [-P <pool name>]\n"
			 );
	DC_Exit( 1 );
}

int
main_init( int argc, char ** const argv )
{

	dprintf(D_FULLDEBUG, "C-GAHP IO thread\n");

	MyString ScheddAddr;
	MyString ScheddPool;

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
			ScheddAddr = argv[i + 1];
			i++;
			break;
		case 'P':
			// specify what pool (i.e. collector) to lookup the schedd name
			if ( argc <= i + 1 )
				usage( argv[0] );
			ScheddPool = argv[i + 1];
			i++;
			break;
		default:
			usage( argv[0] );
			break;
		}

		i++;
	}

	Init();
	Register();
	Reconfig();

		// Create inter-process pipes
	int request_pipe[2];
	int result_pipe[2];

		// The IO (this) process cannot block, otherwise it's poosible
		// to create deadlock between these two pipes
	if (daemonCore->Create_Pipe (request_pipe, true, true) == FALSE ||
		daemonCore->Create_Pipe (result_pipe, true) == FALSE) {
		return -1;
	}

	request_pipe_out_fd = request_pipe[1];
	result_pipe_in_fd = result_pipe[0];

	request_buffer.setFd (request_pipe_out_fd);
	stdin_buffer.setFd (dup(STDIN_FILENO));
	result_buffer.setFd (result_pipe_in_fd);

		// Register pipes
	(void)daemonCore->Register_Pipe (stdin_buffer.getFd(),
									 "stdin pipe",
									 (PipeHandler)&stdin_pipe_handler,
									 "stdin_pipe_handler");

	(void)daemonCore->Register_Pipe (result_buffer.getFd(),
									 "result pipe",
									 (PipeHandler)&result_pipe_handler,
									 "result_pipe_handler");


	flush_request_tid = 
		daemonCore->Register_Timer (1,
									1,
									(TimerHandler)&flush_next_request,
									"flush_next_request",
									NULL);
									
									  

		// Create child process
		// Register the reaper for the child process
	int reaper_id =
		daemonCore->Register_Reaper(
				"worker_thread_reaper",
				(ReaperHandler)worker_thread_reaper,
				"worker_thread_reaper");

	char * c_gahp_name = param ("CONDOR_GAHP");
	ASSERT (c_gahp_name);
	MyString exec_name;
	exec_name.sprintf ("%s_worker_thread", c_gahp_name);
	free (c_gahp_name);

	MyString args;
	args += exec_name;
	args += " -f";

	if (ScheddAddr.Length()) {
		args += " -s ";
		args += ScheddAddr;
	}

	if (ScheddPool.Length()) {
		args += " -P ";
		args += ScheddPool;
	}

	MyString _fds;
	_fds.sprintf (" -I %d -O %d",
				  request_pipe[0],
				  result_pipe[1]);

	args += _fds;

	dprintf (D_FULLDEBUG, "Staring worker: %s\n", args.Value());

	// We want IO thread to inherit these ends of pipes
	int inherit_fds[3];
	inherit_fds[0] = request_pipe[0];
	inherit_fds[1] = result_pipe[1];
	inherit_fds[2] = 0;

	int child_pid = 
		daemonCore->Create_Process (
								exec_name.Value(),
								args.Value(),
								PRIV_UNKNOWN,
								reaper_id,
								FALSE,			// no command port
								NULL,
								NULL,
								FALSE,
								NULL,
								NULL,
								0,				// nice inc
								0,				// job opt mask
								inherit_fds);

	if (child_pid > 0) {
		close (STDIN_FILENO);
		close (request_pipe[0]);
		close (result_pipe[1]);
	}
			
	worker_thread_ready = false;

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

		// Print out the GAHP version to the screen
		// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);

	dprintf (D_FULLDEBUG, "C-GAHP IO initialized\n");

	return TRUE;
}


int
stdin_pipe_handler(int pipe) {
		// Don't make this a while() loop b/c
		// stdin read is blocking
	MyString * line = NULL;
	char ** argv;
	int argc;

	if ((line = stdin_buffer.GetNextLine()) != NULL) {
		const char * command = line->Value();

		dprintf (D_ALWAYS, "got stdin: %s\n", command);

		if (parse_gahp_command (command, &argv, &argc) &&
			verify_gahp_command (argv, argc)) {

				// Catch "special commands first
			if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0) {
					// Print number of results
				MyString rn_buff;
				rn_buff+=result_list.number();
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					rn_buff.Value() };
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
			} else if (strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0) {
				printf ("S %s\n", version);
				fflush (stdout);
			} else if (strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				DC_Exit(0);
			} else if (strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
				async_mode = TRUE;
				new_results_signaled = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
				async_mode = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				return 0; // exit
			} else if (strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0) {
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					GAHP_COMMAND_JOB_SUBMIT,
					GAHP_COMMAND_JOB_REMOVE,
					GAHP_COMMAND_JOB_STATUS_CONSTRAINED,
					GAHP_COMMAND_JOB_UPDATE_CONSTRAINED,
					GAHP_COMMAND_JOB_UPDATE,
					GAHP_COMMAND_JOB_HOLD,
					GAHP_COMMAND_JOB_RELEASE,
					GAHP_COMMAND_JOB_STAGE_IN,
					GAHP_COMMAND_JOB_STAGE_OUT,
					GAHP_COMMAND_JOB_REFRESH_PROXY,
					GAHP_COMMAND_ASYNC_MODE_ON,
					GAHP_COMMAND_ASYNC_MODE_OFF,
					GAHP_COMMAND_RESULTS,
					GAHP_COMMAND_QUIT,
					GAHP_COMMAND_VERSION,
					GAHP_COMMAND_COMMANDS};
				gahp_output_return (commands, 15);
			} else if (strcasecmp (argv[0], GAHP_COMMAND_REFRESH_PROXY_FROM_FILE) == 0) {
					// For now, just return success. This will work if
					// the file is the same as that given to
					// INITIALIZE_FROM_FILE (since our worker reads from
					// the file on every use.
				gahp_output_return_success();
			} else {
					// Pass it on to the worker thread
					// Actually buffer it, until the worker says it's ready
				gahp_output_return_success();
				queue_request (command);
			}
			
			delete [] argv;
		} else {
			gahp_output_return_error();
		}
		delete line;

	}

	return TRUE;
}

int 
result_pipe_handler(int pipe) {
	MyString * line = NULL;

		// Check that we get a full line
		// if not, intermediate results will be stored in buffer
	if ((line = result_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "Master received:\"%s\"\n", line->Value());

			// Add this to the list
		result_list.append (line->Value());

		if (async_mode) {
			if (!new_results_signaled) {
				printf ("R\n");
				fflush (stdout);
			}
			new_results_signaled = TRUE;	// So that we only do it once
		}

		delete line;
	}

	if (result_buffer.IsError()) {
		DC_Exit(1);
	}

	return TRUE;
}

/*
void
process_next_request() {
	if (worker_thread_ready) {
		if (flush_next_request(request_pipe_out_fd)) {
			worker_thread_ready = false;
		}
	}
}
*/

int
worker_thread_reaper (Service*, int pid, int exit_status) {

	dprintf (D_ALWAYS, "Worker process pid=%d exited with status %d\n", 
			 pid, 
			 exit_status);

	// Our child exited (presumably b/c we got a QUIT command),
	// so should we
	// If we're in this function there shouldn't be much cleanup to be done,
	// except the command queue

	DC_Exit(1);
	return TRUE;
}

// Check the parameters to make sure the command
// is syntactically correct
int
verify_gahp_command(char ** argv, int argc) {

	if (strcasecmp (argv[0], GAHP_COMMAND_JOB_REMOVE) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_JOB_HOLD) ==0 ||
		strcasecmp (argv[0], GAHP_COMMAND_JOB_RELEASE) ==0) {
		// Expecting:GAHP_COMMAND_JOB_REMOVE <req_id> <schedd_name> <job_id> <reason>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STATUS_CONSTRAINED) ==0) {
		// Expected: CONDOR_JOB_STATUS_CONSTRAINED <req id> <schedd> <constraint>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_constraint (argv[3]);
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE_CONSTRAINED) == 0) {
		// Expected: CONDOR_JOB_UPDATE_CONSTRAINED <req id> <schedd name> <constraint> <update ad>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_constraint (argv[3]) &&
				verify_class_ad (argv[4]);

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE) == 0) {
		// Expected: CONDOR_JOB_UPDATE <req id> <schedd name> <job id> <update ad>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]) &&
				verify_class_ad (argv[4]);

		return TRUE;
	} else if ((strcasecmp (argv[0], GAHP_COMMAND_JOB_SUBMIT) == 0) ||
			   (strcasecmp (argv[0], GAHP_COMMAND_JOB_STAGE_IN) == 0) ) {
		// Expected: CONDOR_JOB_SUBMIT <req id> <schedd name> <job ad>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_class_ad (argv[3]);

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_STAGE_OUT) == 0) {
		// Expected: CONDOR_JOB_STAGE_OUT <req id> <schedd name> <job id>
		return verify_number_args (argc, 4) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

		return TRUE;
	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_REFRESH_PROXY) == 0) {
		// Expecting:GAHP_COMMAND_JOB_REFRESH_PROXY <req_id> <schedd_name> <job_id> <proxy file>
		return verify_number_args (argc, 5) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

	} else if (strcasecmp (argv[0], GAHP_COMMAND_INITIALIZE_FROM_FILE) == 0) {
		// Expecting:GAHP_COMMAND_INITIALIZE_FROM_FILE <proxy file>
		return verify_number_args (argc, 2) &&
			 x509_proxy_expiration_time (argv[1]) > 0;

	} else if (strcasecmp (argv[0], GAHP_COMMAND_REFRESH_PROXY_FROM_FILE) == 0) {
		// Expecting:GAHP_COMMAND_REFRESH_PROXY_FROM_FILE <proxy file>
		return verify_number_args (argc, 2);

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
verify_schedd_name (const char * s) {
	// TODO: Check against our schedd name, we can only accept one schedd
    return (s != NULL) && (strlen(s) > 0);
}

int
verify_constraint (const char * s) {
	// TODO: How can we verify a constraint?
    return (s != NULL) && (strlen(s) > 0);
}
int
verify_class_ad (const char * s) {
	// TODO: How can we verify XML?
	return (s != NULL) && (strlen(s) > 0);
}

int
verify_job_id (const char * s) {
    int dot_count = 0;
    int ok = TRUE;
    unsigned int i;
    for (i=0; i<strlen (s); i++) {
		if (s[i] == '.') {
			dot_count++;
			if ((dot_count > 1) || (i==0) || (s[i+1] == '\0')) {
				ok = FALSE;
				break;
			}
		} else if (!isdigit(s[i])) {
			ok = FALSE;
			break;
		}
	}

	if ((!ok) || (dot_count != 1)) {
		dprintf (D_ALWAYS, "Bad job id %s\n", s);
		return FALSE;
	}

	return TRUE;
}


void
queue_request (const char * request) {
	MyString strRequest = request;
	strRequest += "\n";

	pending_request_list.append (strRequest.Value());
	daemonCore->Reset_Timer (flush_request_tid, 0, 1);
}


int 
flush_next_request() {
	if (request_buffer.IsEmpty() &&
		pending_request_list.isEmpty()) {
		return TRUE; //nothing to do
	}

	dprintf (D_FULLDEBUG, "flush_next_request()\n");

	int wrote = 0;

	if (!pending_request_list.isEmpty()) {
		char * command;
		pending_request_list.rewind();
		while ((command = pending_request_list.next ())) {

			dprintf (D_FULLDEBUG, "Sending to worker: %s\n", command);
			request_buffer.Write (command);

			pending_request_list.deleteCurrent();

			if (!request_buffer.IsEmpty())
				break;
		} 
	}
	else if (!request_buffer.IsEmpty()) {
		request_buffer.Write();
	}
	
	return TRUE;
}

/*
int
flush_next_request(int fd) {
	
	if (!request_buffer.IsEmpty()) {
		request_buffer.Write();
		return TRUE;
	}

	pending_request_list.Rewind();
	char * _command;
	if (pending_request_list.Next(_command)) {
		dprintf (D_FULLDEBUG, "Sending %s to worker\n", _command);
		MyString command = _command;
		command += "\n";

		int len = strlen(_command)+1;
		int numwritten = 
			write (  fd, command.Value(), strlen (_command)+1);
		
		if (len != numwritten) {
			dprintf (D_ALWAYS, "Warning! Wrote only %d out of %d\n",
					 numwritten,
					 len);
		}

		pending_request_list.DeleteCurrent();
		free (_command);
		return TRUE;
	}

	return FALSE;
}
*/
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

void Init() {}

void Register() {}

void Reconfig() {}


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
