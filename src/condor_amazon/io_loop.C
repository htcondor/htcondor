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
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "string_list.h"
#include "MyString.h"
#include "HashTable.h"
#include "PipeBuffer.h"
#include "io_loop.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

#define AMAZON_GAHP_VERSION	"0.0.1"

const char * version = "$GahpVersion " AMAZON_GAHP_VERSION " Feb 15 2008 Condor\\ AMAZONGAHP $";

char *mySubSystem = "AMAZON_GAHP";	// used by Daemon Core

static IOProcess *ioprocess = NULL;

// this appears at the bottom of this file
extern "C" int display_dprintf_header(FILE *fp);

// forwarding declaration
static void gahp_output_return_error();
static void gahp_output_return_success();
static void gahp_output_return (const char ** results, const int count);
static int verify_gahp_command(char ** argv, int argc);

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

static void io_process_exit(int exit_num)
{
	// kill all child processes
	ioprocess->killWorker(0, true);
	DC_Exit( exit_num );
}

void Init() {}

void Register() {}

void Reconfig() {
	MyString tmp_string;
	if( find_amazon_lib(tmp_string) == false ) {
		io_process_exit(1);
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
	// Kill all workers
	if( ioprocess ) {
		ioprocess->killWorker(0, false);
		ioprocess->is_shutdowning = true;
	}
	return TRUE;	// to satisfy c++
}

int
main_shutdown_graceful()
{
	daemonCore->Cancel_And_Close_All_Pipes();

	// Kill all workers
	if( ioprocess ) {
		ioprocess->killWorker(0, true);
		ioprocess->is_shutdowning = true;
	}
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

void
usage()
{
	dprintf( D_ALWAYS, "Usage: amazon-gahp\n");
	DC_Exit( 1 );
}

int
main_init( int argc, char ** const argv )
{
	dprintf(D_FULLDEBUG, "AMAZON-GAHP IO thread\n");

	// Setup dprintf to display pid
	DebugId = display_dprintf_header;

	Init();
	Register();
	Reconfig();

	// Find the name of worker thread
	MyString worker_exec_name;
	char * amazon_gahp_worker_thread = param("AMAZON_GAHP_WORKER");
	if (amazon_gahp_worker_thread) {
		worker_exec_name = amazon_gahp_worker_thread;
		free(amazon_gahp_worker_thread);
	}
	else {
		char * amazon_gahp_name = param("AMAZON_GAHP");
		ASSERT(amazon_gahp_name);
		worker_exec_name.sprintf ("%s_worker_thread", amazon_gahp_name);
		free (amazon_gahp_name);
	}

	if( check_read_access_file(worker_exec_name.Value()) == false ) {
		dprintf(D_ALWAYS, "Can't read worker thread(%s)\n", worker_exec_name.Value());
		DC_Exit( 1 );
	}
	
	// Find the name of amazon library (e.g. perl script)
	MyString tmp_string;
	if( find_amazon_lib(tmp_string) == false ) {
		DC_Exit( 1 );
	}

	// Register all amazon commands
	if( registerAllAmazonCommands() == false ) {
		dprintf(D_ALWAYS, "Can't register Amazon Commands\n");
		DC_Exit( 1 );
	}
	
	int stdin_pipe = -1;
#if defined(WIN32)
	// if our parent is not DaemonCore, then our assumption that
	// the pipe we were passed in via stdin is overlapped-mode
	// is probably wrong. we therefore create a new pipe with the
	// read end overlapped and start a "forwarding thread"
	char* tmp;
	if ((tmp = daemonCore->InfoCommandSinfulString(daemonCore->getppid())) == NULL) {

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

	int min_workers = MIN_NUMBER_WORKERS;
	int max_workers = -1;

	char* num_workers = param("AMAZON_GAHP_WORKER_MIN_NUM");
	if (num_workers) {
		min_workers = atoi(num_workers);
		free(num_workers);
	}

	num_workers = param("AMAZON_GAHP_WORKER_MAX_NUM");
	if (num_workers) {
		max_workers = atoi(num_workers);
		free(num_workers);
	}

	// Create IOProcess class
	ioprocess = new IOProcess;
	ASSERT(ioprocess);

	if( ioprocess->startUp(stdin_pipe, worker_exec_name.Value(), 
				min_workers, max_workers) == false ) {
		dprintf(D_ALWAYS, "Failed to start IO Process\n");
		delete ioprocess;
		DC_Exit(1);
	}

	// Print out the GAHP version to the screen
	// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);

	dprintf (D_FULLDEBUG, "AMAZON-GAHP IO initialized\n");
	return TRUE;
}

// Check the parameters to make sure the command
// is syntactically correct
static int
verify_gahp_command(char ** argv, int argc) {
	// Special Commands First
	if (strcasecmp (argv[0], GAHP_COMMAND_RESULTS) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_VERSION) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_COMMANDS) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_QUIT) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0 ||
			strcasecmp (argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
		// These are no-arg commands
		return verify_number_args (argc, 1);
	}

	return executeIOCheckFunc(argv[0], argv, argc); 
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

static void
gahp_output_return_success() {
	const char* result[] = {GAHP_RESULT_SUCCESS};
	gahp_output_return (result, 1);
}

static void
gahp_output_return_error() {
	const char* result[] = {GAHP_RESULT_ERROR};
	gahp_output_return (result, 1);
}

int
Worker::resultHandler() 
{
	MyString* line = NULL;
	int req_id = 0;
	while ((line = m_result_buffer.GetNextLine()) != NULL) {

		dprintf (D_FULLDEBUG, "Received from worker[%d]:\"%s\"\n", 
				m_pid, line->Value());

		Gahp_Args args;
		if( !( parse_gahp_command(line->GetCStr(), &args) && 
					verify_request_id(args.argv[0]) ) ) {
			dprintf (D_ALWAYS, "Invalid result format from worker[%d]:\"%s\"\n",
					m_pid, line->Value());
			continue;
		}

		req_id = atoi(args.argv[0]);

		// remove this from worker request queue
		removeRequest(req_id);

		//remove this from request queue
		ioprocess->removeRequest(req_id);

		// Add this to the result list
		ioprocess->addResult(line->Value());

		if (ioprocess->m_async_mode) {
			if (!ioprocess->m_new_results_signaled) {
				printf ("R\n");
				fflush (stdout);
			}
			ioprocess->m_new_results_signaled = TRUE;	// So that we only do it once
		}

		delete line;
	}

	if (m_result_buffer.IsError() || m_result_buffer.IsEOF()) {

		int pipe_end = m_result_buffer.getPipeEnd();

		if( pipe_end > 0 ) {
			// Close pipe
			daemonCore->Close_Pipe( m_result_buffer.getPipeEnd() );
			m_result_buffer.setPipeEnd(-1);
		}

		m_need_kill = true;
		ioprocess->resetWorkerManagerTimer();

		if( ioprocess->numOfWorkers() == 1 ) { 
			// This worker is the last one 
			dprintf (D_ALWAYS, "Result buffer for the last worker[%d] "
					"has error, Exiting..\n", m_pid);
			io_process_exit(1);
		}
	}

	return TRUE;
}

bool
Worker::removeRequest(int req_id)
{
	Request *request = NULL;
	m_request_list.Rewind();
	while( m_request_list.Next(request) ) {

		if( request->m_reqid == req_id ) {
			// remove this request from worker request queue
			m_request_list.DeleteCurrent();
			return true;
		}	
	}

	return false;
}

// Functions for IOProcess class
IOProcess::IOProcess(void) 
	: m_workers_list(20, &hashFuncInt), m_pending_req_list(20, &hashFuncInt)
{
	m_min_workers = MIN_NUMBER_WORKERS;
	m_max_workers = -1;
	m_async_mode = 0;
	m_new_results_signaled = 0;
	m_flush_request_tid = -1;
	m_worker_manager_tid = -1;
	m_reaper_id = -1;
	is_shutdowning = false;
}

IOProcess::~IOProcess()
{
}

bool
IOProcess::startUp(int stdin_pipe, const char* worker_prog, int min_workers, int max_workers)
{
	m_min_workers = min_workers;
	m_max_workers = max_workers;
	m_worker_exec_name = worker_prog;
	m_stdin_buffer.setPipeEnd(stdin_pipe);

	int result = -1;
	result = daemonCore->Register_Pipe(stdin_pipe,
			"stdin pipe",
			(PipeHandlercpp)&IOProcess::stdinPipeHandler,
			"IOProcess::stdinPipeHandler", this);
	if( result == -1 ) {
		// failed to register pipe
		dprintf(D_ALWAYS, "Failed to register stdin pipe\n");
		return false;
	}

	// Register the reaper for the child process
	m_reaper_id = daemonCore->Register_Reaper(
				"workerThreadReaper",
				(ReaperHandlercpp)&IOProcess::workerThreadReaper,
				"IOProcess::workerThreadReaper", this);

	m_flush_request_tid = 
		daemonCore->Register_Timer (1,
				1,
				(TimerHandlercpp)&IOProcess::flushPendingRequests,
				"IOProcess::flushPendingRequests", 
				this);
	if( m_flush_request_tid == -1 ) {
		dprintf(D_ALWAYS, "Failed to register timer to send requests to worker\n");
		return false;
	}

	m_worker_manager_tid = 
		daemonCore->Register_Timer (WORKER_MANAGER_TIMER_INTERVAL,
				WORKER_MANAGER_TIMER_INTERVAL,
				(TimerHandlercpp)&IOProcess::workerManager,
				"IOProcess::workerManager", 
				this);
	if( m_worker_manager_tid == -1 ) {
		dprintf(D_ALWAYS, "Failed to register timer for worker manager\n");
		return false;
	}

	// Create initial worker processes
	int i = 0;
	for( i = 0; i < m_min_workers; i++ ) {
		createNewWorker();
	}

	return true;
}

int
IOProcess::stdinPipeHandler() 
{
	MyString* line;
	while ((line = m_stdin_buffer.GetNextLine()) != NULL) {

		const char *command = line->Value();

		dprintf (D_ALWAYS, "got stdin: %s\n", command);

		Gahp_Args args;

		if (parse_gahp_command (command, &args) &&
			verify_gahp_command (args.argv, args.argc)) {

				// Catch "special commands first
			if (strcasecmp (args.argv[0], GAHP_COMMAND_RESULTS) == 0) {
					// Print number of results
				MyString rn_buff;
				rn_buff += numOfResult();
				const char * commands [] = {
					GAHP_RESULT_SUCCESS,
					rn_buff.Value() };
				gahp_output_return (commands, 2);

					// Print each result line
				char* next = NULL;
				startResultIteration();
				while ((next = NextResult()) != NULL) {
					printf ("%s\n", next);
					fflush(stdout);
					deleteCurrentResult();
				}

				m_new_results_signaled = FALSE;
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_VERSION) == 0) {
				printf ("S %s\n", version);
				fflush (stdout);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				io_process_exit(0);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
				m_async_mode = TRUE;
				m_new_results_signaled = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
				m_async_mode = FALSE;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_COMMANDS) == 0) {
				StringList amazon_commands;
				int num_commands = 0;

				num_commands = allAmazonCommands(amazon_commands);
				num_commands += 7;

				const char *commands[num_commands];
				int i = 0;
				commands[i++] = GAHP_RESULT_SUCCESS;
				commands[i++] = GAHP_COMMAND_ASYNC_MODE_ON;
				commands[i++] = GAHP_COMMAND_ASYNC_MODE_OFF;
				commands[i++] = GAHP_COMMAND_RESULTS;
				commands[i++] = GAHP_COMMAND_QUIT;
				commands[i++] = GAHP_COMMAND_VERSION;
				commands[i++] = GAHP_COMMAND_COMMANDS;

				amazon_commands.rewind();
				char *one_amazon_command = NULL;

				while( (one_amazon_command = amazon_commands.next() ) != NULL ) {
					commands[i++] = one_amazon_command;
				}

				gahp_output_return (commands, i);
			} else {
				// got new command
				Request *new_req = addNewRequest(command);
				if( new_req ) {
					flushRequest(new_req);
				}
				gahp_output_return_success(); 
			}
			
		} else {
			gahp_output_return_error();
		}

		delete line;
	}

	// check if GetNextLine() returned NULL because of an error or EOF
	if (m_stdin_buffer.IsError() || m_stdin_buffer.IsEOF()) {
		dprintf (D_ALWAYS, "stdin buffer closed, exiting\n");
		io_process_exit(1);
	}

	return TRUE;
}

int
IOProcess::workerThreadReaper(int pid, int exit_status) 
{
	dprintf (D_FULLDEBUG, "Worker process pid=%d exited with status %d\n", 
			 pid, exit_status);

	// If there are some requests given to this worker, 
	// just reply errors for the requests

	Worker* worker = findWorker(pid);
	if( worker ) {
		if( !is_shutdowning ) {
			removeAllRequestsFromWorker(worker);
		}

		// remove this worker from worker list
		m_workers_list.remove(pid);

		delete worker;
		worker = NULL;
	}

	if( numOfWorkers() == 0 ) { 
		// this worker is the last one
		dprintf (D_ALWAYS, "There is no available worker, Exiting..\n");
		io_process_exit(1);
	}

	return TRUE;
}

void
IOProcess::flushRequest(Request* request) 
{
	if( !request ) {
		return;
	}

	dprintf (D_FULLDEBUG, "Sending %s to worker %d\n", 
			 request->m_raw_cmd.Value(), request->m_worker->m_pid);

	MyString strRequest = request->m_raw_cmd;
	strRequest += "\n";

	request->m_worker->m_request_buffer.Write(strRequest.Value());

	daemonCore->Reset_Timer(m_flush_request_tid, 0, 1);
}

int 
IOProcess::flushPendingRequests() {

	int currentkey = 0;
	Worker *worker = NULL;

	m_workers_list.startIterations();

	while( m_workers_list.iterate(currentkey, worker) != 0 ) {
		worker->m_request_buffer.Write();

		if (worker->m_request_buffer.IsError()) {

			dprintf (D_ALWAYS, "Worker[%d] request buffer error.\n", 
					worker->m_pid);

			worker->m_need_kill = true;
			resetWorkerManagerTimer();
		}
	}

	return TRUE;
}

Worker*
IOProcess::createNewWorker(void)
{
	Worker *new_worker = NULL;
	new_worker = new Worker;
	ASSERT(new_worker);

	int result_pipe[2];
	int request_pipe[2];

	if (!daemonCore->Create_Pipe(request_pipe,
				true,	// read end registerable
				false,	// write end not registerable
				false,	// read end blocking
				true	// write end blocking
				) ||
			!daemonCore->Create_Pipe (result_pipe,
				true	// read end registerable
				) )
	{
		dprintf (D_ALWAYS, "Failed to create pipe for a new worker\n");
		delete new_worker;
		return NULL;
	}

	new_worker->m_request_buffer.setPipeEnd(request_pipe[1]);
	new_worker->m_result_buffer.setPipeEnd(result_pipe[0]);

	(void)daemonCore->Register_Pipe(new_worker->m_result_buffer.getPipeEnd(),
			"result pipe",
			(PipeHandlercpp)&Worker::resultHandler,
			"Worker::resultHandler",
			(Service*)new_worker);

	ArgList args;

	args.AppendArg(m_worker_exec_name.Value());
	args.AppendArg("-f");

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string);
	dprintf (D_FULLDEBUG, "About to start a new worker %s\n", 
			args_string.Value());

	// We want IO thread to inherit these ends of pipes
	int std_fds[3];
	std_fds[0] = request_pipe[0];
	std_fds[1] = result_pipe[1];
	std_fds[2] = fileno(stderr);

	// set up a FamilyInfo structure to register a family 
	//FamilyInfo fi;

	// take snapshots at no more than 15 seconds in between, by default
	//fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	new_worker->m_pid = daemonCore->Create_Process (
				m_worker_exec_name.Value(),
				args,
				PRIV_UNKNOWN,
				m_reaper_id,
				FALSE,			// no command port
				NULL,			// env
				NULL,			// cwd	
				// &fi,			// family_info
				NULL,			
				NULL,			// network scokets to inherit
				std_fds);

	char const *create_process_error = NULL;
	if( new_worker->m_pid == FALSE && errno ) {
		create_process_error = strerror(errno);
	}

	daemonCore->Close_Pipe( std_fds[0] );
	daemonCore->Close_Pipe( std_fds[1] );

	if( new_worker->m_pid == FALSE ) {
		dprintf(D_ALWAYS, "Failed to start a new worker : %s\n", 
			create_process_error? create_process_error: "");
		delete new_worker;
		return NULL;
	}

	// Insert a new worker to HashTable
	m_workers_list.insert(new_worker->m_pid, new_worker);

	dprintf(D_FULLDEBUG, "Worker[%d] is created.\n", new_worker->m_pid);
	return new_worker;
}

Worker* 
IOProcess::findFreeWorker(void)
{
	int currentkey = 0;
	Worker *worker = NULL;
	
	m_workers_list.startIterations();
	while( m_workers_list.iterate(currentkey, worker) != 0 ) {
		if( worker->canUse() == false ) {
			continue;
		}

		if( worker->numOfRequest() == 0 ) {
			return worker;
		}
	}

	return NULL;
}

Worker* 
IOProcess::findWorkerWithFewestRequest(void)
{
	int currentkey = 0;
	Worker *worker = NULL;
	Worker *min_worker = NULL;
	
	m_workers_list.startIterations();
	while( m_workers_list.iterate(currentkey, worker) != 0 ) {
		if( worker->canUse() == false ) {
			continue;
		}

		if( !min_worker ) {
			min_worker = worker;
		}else {
			if( worker->numOfRequest() < min_worker->numOfRequest() ) {
				min_worker = worker;
			}
		}
	}

	return min_worker;
}

Worker* 
IOProcess::findWorker(int pid)
{
	int currentkey = 0;
	Worker *worker = NULL;

	m_workers_list.startIterations();
	while( m_workers_list.iterate(currentkey, worker) != 0 ) {
		if( worker->m_pid == pid ) {
			return worker;
		}
	}

	return NULL;
}

int
IOProcess::workerManager()
{
	int currentkey = 0;
	Worker *worker = NULL;

	int num_extra_workers = 0;
	int num_real_workers = numOfRealWorkers();
	if( num_real_workers > m_min_workers ) {
		// Too much workers
		// We need to kill extra ones
		num_extra_workers = num_real_workers - m_min_workers;
	}

	m_workers_list.startIterations();
	while( m_workers_list.iterate(currentkey, worker) != 0 ) {

		if( worker->m_need_kill ) {
			killWorker(worker, true);
		}

		if( worker->m_can_use && (worker->numOfRequest() == 0) ) {
			if( num_extra_workers > 0 ) {
				dprintf(D_FULLDEBUG, "Worker[%d] will be killed "
						"because it is extra one\n", worker->m_pid);
				killWorker(worker, true);

				num_extra_workers--;
			}
		}
	}

	return true;
}

void 
IOProcess::resetWorkerManagerTimer(void)
{
	if( m_worker_manager_tid > 0 ) {
		daemonCore->Reset_Timer(m_worker_manager_tid, 0, WORKER_MANAGER_TIMER_INTERVAL);
	}
}

void 
IOProcess::killWorker(int pid, bool graceful)
{
	int currentkey = 0;
	Worker *worker = NULL;

	// pid == 0 means killing all workers
	m_workers_list.startIterations();
	while( m_workers_list.iterate(currentkey, worker) != 0 ) {
		if( !pid || (worker->m_pid == pid) ) {
			
			killWorker(worker, graceful);

			if( pid > 0 ) {
				// we wanted just one
				return;
			}
		}
	}
}

void 
IOProcess::killWorker(Worker *worker, bool graceful)
{
	if( !worker ) {
		return;
	}
	
	worker->m_need_kill = false;

	if( worker->m_can_use == false ) {
		// Already this worker has been killed
		return;
	}

	worker->m_can_use = false;

	if( worker->m_pid > 0 ) {
		// First, close pipe
		int pipe_end = worker->m_result_buffer.getPipeEnd();
		if( pipe_end > 0 ) {
			daemonCore->Close_Pipe( pipe_end );
			worker->m_result_buffer.setPipeEnd(-1);
		}

		//daemonCore->Kill_Family(worker->m_pid);
		if( graceful ) {
			dprintf (D_FULLDEBUG, "Sending SIGTERM to Worker[%d]...\n", 
					worker->m_pid);
			daemonCore->Send_Signal(worker->m_pid, SIGTERM);
		}else {
			dprintf (D_FULLDEBUG, "Sending SIGKILL to Worker[%d]...\n", 
					worker->m_pid);
			daemonCore->Send_Signal(worker->m_pid, SIGKILL);
		}
	}
}

void 
IOProcess::removeAllRequestsFromWorker(Worker *worker)
{
	if( !worker ) {
		return;
	}	

	// If there are pending requests given to this worker, 
	// just reply errors for the requests
	Request *request = NULL;
	worker->m_request_list.Rewind();
	while( worker->m_request_list.Next(request) ) {

		// remove this request from worker request queue
		worker->m_request_list.DeleteCurrent();

		int req_id = request->m_reqid;

		// remove this request from request queue
		if( removeRequest(req_id) ) {
			// this request was in request queue
			
			dprintf (D_ALWAYS, "Req(id=%d) is forcedly removed from worker[%d]\n",
					req_id, worker->m_pid);

			MyString output_result;
			output_result = create_failure_result( req_id, "Req is forcedly removed from worker");
			printf("%s", output_result.Value());
		}
	}
}

Request* 
IOProcess::addNewRequest(const char *cmd)
{
	if( !cmd ) {
		return NULL;
	}

	Request* new_req = new Request(cmd);
	ASSERT(new_req);

	// check if there is the pending request with the same req id
	Request* old_req = findRequest(new_req->m_reqid);

	if( old_req ) {
		dprintf (D_ALWAYS, "Request with the duplicated req_id\n"
				"\tOld Req:%s\n"
			   	"\tNew Req::%s\n", 
				old_req->m_raw_cmd.Value(), cmd);
		delete new_req;
		return NULL;
	}

	// check if there is available worker process
	Worker *worker = findFreeWorker();

	if( !worker ) {
		// There is no available worker

		if( m_max_workers <= 0 ||
				(numOfRealWorkers() < m_max_workers) ) {
			// If there is no limitation for number of workers or
			// current number of workers is less than max number
			// Create one worker
			worker = createNewWorker();
		}

		if( !worker ) {
			// Here, we will use the worker with the smallest number of req.
			worker = findWorkerWithFewestRequest();
			if( !worker ) {
				dprintf (D_ALWAYS, "Ooops!! There is no worker..exiting\n");
				io_process_exit(1);
			}
		}
	}
	
	new_req->m_worker = worker;
	worker->m_request_list.Append(new_req);

	// Finally put this request into request queue
	m_pending_req_list.insert(new_req->m_reqid, new_req);
	return new_req;
}

Request*
IOProcess::findRequest(int req_id)
{
	Request* request = NULL;
	m_pending_req_list.lookup(req_id, request);

	if( request ) {
		return request;
	}

	return NULL;
}

bool
IOProcess::removeRequest(int req_id)
{
	Request* request = NULL;
	m_pending_req_list.lookup(req_id, request);

	if( request ) {
		m_pending_req_list.remove(req_id);
		delete request;
		return true;
	}

	return false;
}

void 
IOProcess::addResult(const char *result)
{
	if( result ) {
		m_result_list.append(result);
	}
}

int
IOProcess::numOfRealWorkers(void)
{
	int currentkey = 0;
	Worker *worker = NULL;
	int real_workers = 0;

	m_workers_list.startIterations();
	while( m_workers_list.iterate(currentkey, worker) != 0 ) {
		if( worker->canUse() ) {
			real_workers++;
		}
	}

	return real_workers;
}
