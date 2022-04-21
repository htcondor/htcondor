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
#include "schedd_client.h"
#include "PipeBuffer.h"
#include "globus_utils.h"
#include "subsystem_info.h"


const char * version = "$GahpVersion 2.0.1 Jun 27 2005 Condor\\ GAHP $";

static int verify_filename(const char *);

int async_mode = 0;
int new_results_signaled = 0;

int flush_request_tid = -1;

// The list of results ready to be output to IO
StringList result_list;

// pipe buffers
PipeBuffer stdin_buffer; 

// Each of these represent a thread that does the
// actual work of communicating with the SchedD
//
// Currently workers[1] handles STAGE_IN requests,
// workers[0] does everything else
//
// This is done so that the GAHP is not tied up
// while doing file staging. The SchedD also
// forks a file transfer thread, so it's not tied
// up either.
#define NUMBER_WORKERS 2
Worker workers[NUMBER_WORKERS];

// this appears at the bottom of this file
int display_dprintf_header(char **buf,int *bufpos,int *buflen);

#ifdef WIN32
int STDIN_FILENO = fileno(stdin);
#endif

#if defined(WIN32)
int forwarding_pipe = -1;
unsigned __stdcall pipe_forward_thread(void *)
{
	const int FORWARD_BUFFER_SIZE = 4096;
	char buf[FORWARD_BUFFER_SIZE];
	
	// just copy everything from stdin to the forwarding pipe
	while (true) {

		// read from stdin
		int bytes = read(0, buf, FORWARD_BUFFER_SIZE);
		if (bytes == -1) {
			dprintf(D_ALWAYS, "pipe_forward_thread: error reading from stdin\n");
			daemonCore->Close_Pipe(forwarding_pipe);
			return 1;
		}

		// close forwarding pipe and exit on EOF
		if (bytes == 0) {
			daemonCore->Close_Pipe(forwarding_pipe);
			return 0;
		}

		// write to the forwarding pipe
		char* ptr = buf;
		while (bytes) {
			int bytes_written = daemonCore->Write_Pipe(forwarding_pipe, ptr, bytes);
			if (bytes_written == -1) {
				dprintf(D_ALWAYS, "pipe_forward_thread: error writing to forwarding pipe\n");
				daemonCore->Close_Pipe(forwarding_pipe);
				return 1;
			}
			ptr += bytes_written;
			bytes -= bytes_written;
		}
	}

}
#endif

void
usage()
{
	dprintf( D_ALWAYS,
		"Usage: condor_gahp -s <schedd name> [-P <pool name>]\n"
			 );
	DC_Exit( 1 );
}

void
main_init( int argc, char ** const argv )
{
	dprintf(D_FULLDEBUG, "C-GAHP IO thread\n");

	std::string schedd_addr;
	std::string schedd_pool;

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
			schedd_addr = argv[i + 1];
			i++;
			break;
		case 'P':
			// specify what pool (i.e. collector) to lookup the schedd name
			if ( argc <= i + 1 )
				usage();
			schedd_pool = argv[i + 1];
			i++;
			break;
		default:
			usage();
			break;
		}

		i++;
	}

	Init();
	Register();
	Reconfig();

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
					&stdin_pipe_handler,
					"stdin_pipe_handler");

	for (i=0; i<NUMBER_WORKERS; i++) {

		workers[i].Init(i);

			// The IO (this) process cannot block, otherwise it's poosible
			// to create deadlock between these two pipes
		if (!daemonCore->Create_Pipe (workers[i].request_pipe,
					      true,	// read end registerable
					      false,	// write end not registerable
					      false,	// read end blocking
					      true	// write end blocking
					     ) ||
		    !daemonCore->Create_Pipe (workers[i].result_pipe,
					      true	// read end registerable
					     ) )
		{
			return;
		}

		workers[i].request_buffer.setPipeEnd(workers[i].request_pipe[1]);
		workers[i].result_buffer.setPipeEnd(workers[i].result_pipe[0]);

		(void)daemonCore->Register_Pipe (workers[i].result_buffer.getPipeEnd(),
										 "result pipe",
										 static_cast<PipeHandlercpp>(&Worker::result_handler),
										 "Worker::result_handler",
										 (Service*)&workers[i]);
	}

	// Create child process
	// Register the reaper for the child process
	int reaper_id =
		daemonCore->Register_Reaper(
							"worker_thread_reaper",
							&worker_thread_reaper,
							"worker_thread_reaper");



	flush_request_tid = 
		daemonCore->Register_Timer (1,
									1,
									flush_pending_requests,
									"flush_pending_requests");


	std::string exec_name;
	char * c_gahp_worker_thread = param("CONDOR_GAHP_WORKER");
	if (c_gahp_worker_thread) {
		exec_name = c_gahp_worker_thread;
		free(c_gahp_worker_thread);
	}
	else {
		char * c_gahp_name = param ("CONDOR_GAHP");
		ASSERT (c_gahp_name);
		formatstr(exec_name, "%s_worker_thread", c_gahp_name);
		free (c_gahp_name);
	}

	for (i=0; i<NUMBER_WORKERS; i++) {
		ArgList args;

		args.AppendArg(exec_name.c_str());
		args.AppendArg("-f");

		if (schedd_addr.length()) {
			args.AppendArg("-s");
			args.AppendArg(schedd_addr.c_str());
		}

		if (schedd_pool.length()) {
			args.AppendArg("-P");
			args.AppendArg(schedd_pool.c_str());
		}

		std::string args_string;
		args.GetArgsStringForDisplay(args_string);
		dprintf (D_FULLDEBUG, "Staring worker # %d: %s\n", i, args_string.c_str());

			// We want IO thread to inherit these ends of pipes
		int std_fds[3];
		std_fds[0] = workers[i].request_pipe[0];
		std_fds[1] = workers[i].result_pipe[1];
		std_fds[2] = fileno(stderr);

		workers[i].pid = 
			daemonCore->Create_Process (
										exec_name.c_str(),
										args,
										PRIV_UNKNOWN,
										reaper_id,
										FALSE,			// no command port
										FALSE,			// no command port
										NULL,
										NULL,
										NULL,
										NULL,
										std_fds);


		if (workers[i].pid > 0) {
			daemonCore->Close_Pipe(workers[i].request_pipe[0]);
			daemonCore->Close_Pipe(workers[i].result_pipe[1]);
		}
	}
			
	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

		// Print out the GAHP version to the screen
		// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);

	dprintf (D_FULLDEBUG, "C-GAHP IO initialized\n");
}


int
stdin_pipe_handler(int) {

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
				formatstr( rn_buff, "%d", result_list.number() );
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
					GAHP_COMMAND_JOB_SUBMIT,
					GAHP_COMMAND_JOB_REMOVE,
					GAHP_COMMAND_JOB_STATUS_CONSTRAINED,
					GAHP_COMMAND_JOB_UPDATE_CONSTRAINED,
					GAHP_COMMAND_JOB_UPDATE,
					GAHP_COMMAND_JOB_HOLD,
					GAHP_COMMAND_JOB_RELEASE,
					GAHP_COMMAND_JOB_STAGE_IN,
					GAHP_COMMAND_JOB_STAGE_OUT,
					GAHP_COMMAND_JOB_UPDATE_LEASE,
					GAHP_COMMAND_JOB_REFRESH_PROXY,
					GAHP_COMMAND_ASYNC_MODE_ON,
					GAHP_COMMAND_ASYNC_MODE_OFF,
					GAHP_COMMAND_RESULTS,
					GAHP_COMMAND_QUIT,
					GAHP_COMMAND_VERSION,
					GAHP_COMMAND_COMMANDS,
					GAHP_COMMAND_INITIALIZE_FROM_FILE,
					GAHP_COMMAND_UPDATE_TOKEN_FROM_FILE,
					GAHP_COMMAND_REFRESH_PROXY_FROM_FILE};
				gahp_output_return (commands, 20);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_REFRESH_PROXY_FROM_FILE) == 0) {
					// For now, just return success. This will work if
					// the file is the same as that given to
					// INITIALIZE_FROM_FILE (since our worker reads from
					// the file on every use.
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_INITIALIZE_FROM_FILE) == 0) {
					// Forward this request to both workers, since both
					// need to know which proxy to use.
				flush_request (0,
							   command);
				flush_request (1,
							   command);
				gahp_output_return_success();
			} else if (strcasecmp(args.argv[0], GAHP_COMMAND_UPDATE_TOKEN_FROM_FILE) == 0) {
					// As above, both workers need to know which token
					// file to use.
				flush_request(0, command);
				flush_request(1, command);
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_JOB_STAGE_IN) == 0 ||
					   strcasecmp (args.argv[0], GAHP_COMMAND_JOB_STAGE_OUT) == 0 ||
					   strcasecmp (args.argv[0], GAHP_COMMAND_JOB_REFRESH_PROXY) == 0) {
				flush_request (1, 	// worker for staging requests
							   command);
				gahp_output_return_success(); 
			} else {
				flush_request (0,	// general worker 
							   command);
				gahp_output_return_success(); 
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

int 
result_pipe_handler(int id) {

	std::string* line;
	while ((line = workers[id].result_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "Received from worker:\"%s\"\n", line->c_str());

			// Add this to the list
		result_list.append (line->c_str());

		if (async_mode) {
			if (!new_results_signaled) {
				printf ("R\n");
				fflush (stdout);
			}
			new_results_signaled = TRUE;	// So that we only do it once
		}

		delete line;
	}

	if (workers[id].result_buffer.IsError() || workers[id].result_buffer.IsEOF()) {
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
worker_thread_reaper (int pid, int exit_status) {

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
		// Expecting:GAHP_COMMAND_JOB_REFRESH_PROXY <req_id> <schedd_name> <job_id> <proxy file> [<expiration time>]
		return (verify_number_args (argc, 5) || verify_number_args (argc, 6)) &&
				verify_request_id (argv[1]) &&
				verify_schedd_name (argv[2]) &&
				verify_job_id (argv[3]);

	} else if (strcasecmp (argv[0], GAHP_COMMAND_INITIALIZE_FROM_FILE) == 0) {
		// Expecting:GAHP_COMMAND_INITIALIZE_FROM_FILE <proxy file>
		return verify_number_args (argc, 2) &&
			 x509_proxy_expiration_time (argv[1]) > 0;
	} else if (strcasecmp(argv[0], GAHP_COMMAND_UPDATE_TOKEN_FROM_FILE) == 0) {
		// Expecting:GAHP_COMMAND_UPDATE_TOKEN_FROM_FILE <token file>
		return verify_number_args(argc, 2) &&
			verify_filename(argv[1]);
	} else if (strcasecmp (argv[0], GAHP_COMMAND_REFRESH_PROXY_FROM_FILE) == 0) {
		// Expecting:GAHP_COMMAND_REFRESH_PROXY_FROM_FILE <proxy file>
		return verify_number_args (argc, 2);

	} else if (strcasecmp (argv[0], GAHP_COMMAND_JOB_UPDATE_LEASE) == 0) {
			// Expecting CONDOR_JOB_UPDATE_LEASE <req id> <schedd name> <num  jobs> <job id> <expiration> <job id> <expiration> ...
		return (argc >= 4) &&
			   verify_number (argv[3]) &&
			   (argc == (atoi (argv[3]) * 2 + 4)) &&
			   verify_request_id (argv[1]) &&
			   verify_schedd_name (argv[2]);
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

static int
verify_filename(const char * fname) {
	return fname && strlen(fname);
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
flush_request (int worker_id, const char * request) {

	dprintf (D_FULLDEBUG, "Sending %s to worker %d\n", 
			 request,
			 worker_id);

	std::string strRequest = request;
	strRequest += "\n";

	workers[worker_id].request_buffer.Write(strRequest.c_str());

	daemonCore->Reset_Timer (flush_request_tid, 0, 1);
}


void flush_pending_requests() {
	for (int i=0; i<NUMBER_WORKERS; i++) {
		workers[i].request_buffer.Write();

		if (workers[i].request_buffer.IsError()) {
			dprintf (D_ALWAYS, "Worker %d request buffer error, exiting...\n", i);
			DC_Exit (1);
		}
	}
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

void Init() {}

void Register() {}

void Reconfig() {}


void
main_config()
{
	Reconfig();
}

void
main_shutdown_fast()
{
}

void
main_shutdown_graceful()
{
	daemonCore->Cancel_And_Close_All_Pipes();

	for (int i=0; i<NUMBER_WORKERS; i++) {
		daemonCore->Send_Signal (workers[i].pid, SIGKILL);
	}
}

void
main_pre_command_sock_init( )
{
	daemonCore->WantSendChildAlive( false );
	daemonCore->m_create_family_session = false;
}

int
main( int argc, char **argv )
{
	set_mySubSystem("C_GAHP", false, SUBSYSTEM_TYPE_GAHP);

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_pre_command_sock_init = main_pre_command_sock_init;
	return dc_main( argc, argv );
}

// This function is called by dprintf - always display our pid in our
// log entries.
int
display_dprintf_header(char **buf,int *bufpos,int *buflen)
{
	static pid_t mypid = 0;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	return sprintf_realloc( buf, bufpos, buflen, "[%ld] ", (long)mypid );
}
