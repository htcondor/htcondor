/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "condor_config.h"
#include "condor_debug.h"
#include "PipeBuffer.h"
#include "my_getopt.h"
#include "io_loop_pthread.h"
#include "openstackgahp_common.h"
#include "openstackCommands.h"
#include "subsystem_info.h"
#include <algorithm>

#define OPENSTACK_GAHP_VERSION	"0.1"

int RESULT_OUTBOX = 1;	// stdout
int REQUEST_INBOX = 0; // stdin

const char * version = "$GahpVersion " OPENSTACK_GAHP_VERSION " Sep 25 2015 Condor\\ Openstack\\ GAHP $";

static IOProcess ioprocess;

// forwarding declaration
static void gahp_output_return_error();
static void gahp_output_return_success();
static void gahp_output_return (const char ** results, size_t count);
static int verify_gahp_command(char ** argv, int argc);
static void *worker_function( void *ptr );

/* We use a big mutex to make certain only one thread is running at a time,
 * except when that thread would be blocked on I/O or a signal.  We do
 * this becase many data structures and methods used from utility libraries
 * are not inheriently thread safe.  So we error on the side of correctness,
 * and this isn't a big deal since we are only really concerned with the gahp
 * blocking when we do network communication and SOAP/WS_SECURITY processing.
 */
pthread_mutex_t global_big_mutex = PTHREAD_MUTEX_INITIALIZER;
#include "thread_control.h"

static void io_process_exit(int exit_num)
{
	exit( exit_num );
}

void
usage()
{
	dprintf( D_ALWAYS, "Usage: openstack_gahp -d debuglevel -w min_worker_nums -m max_worker_nums\n");
	fprintf( stderr, "Usage: openstack_gahp -d debuglevel -w min_worker_nums -m max_worker_nums\n");
	exit(1);
}

static bool
registerAllOpenstackCommands(void)
{
	if( numofOpenstackCommands() > 0 ) {
		dprintf(D_ALWAYS, "There are already registered commands\n");
		return false;
	}

	// Keystone commands
	registerOpenstackGahpCommand( KEYSTONE_COMMAND_SERVICE,
		KeystoneService::ioCheck, KeystoneService::workerFunction );

	// Nova commands
	registerOpenstackGahpCommand( NOVA_COMMAND_PING,
			NovaPing::ioCheck, NovaPing::workerFunction );

	registerOpenstackGahpCommand( NOVA_COMMAND_SERVER_CREATE,
			NovaServerCreate::ioCheck, NovaServerCreate::workerFunction );
	registerOpenstackGahpCommand( NOVA_COMMAND_SERVER_DELETE,
			NovaServerDelete::ioCheck, NovaServerDelete::workerFunction );
	registerOpenstackGahpCommand( NOVA_COMMAND_SERVER_LIST,
			NovaServerList::ioCheck, NovaServerList::workerFunction );
	registerOpenstackGahpCommand( NOVA_COMMAND_SERVER_DETAIL,
			NovaServerListDetail::ioCheck, NovaServerListDetail::workerFunction );

	return true;
}

void
quit_on_signal(int sig)
{
	/* Posix says exit() is not signal safe, so call _exit() */
	_exit(sig);
}

int
main( int argc, char ** const argv )
{
#ifndef WIN32
	/* Add the signals we want unblocked into sigSet */
	sigset_t sigSet;
	struct sigaction act;

	sigemptyset( &sigSet );
	sigaddset(&sigSet,SIGTERM);
	sigaddset(&sigSet,SIGQUIT);
	sigaddset(&sigSet,SIGPIPE);

	/* Set signal handlers */
	sigemptyset(&act.sa_mask);  /* do not block anything in handler */
	act.sa_flags = 0;

	/* Signals which should cause us to exit with status = signum */
	act.sa_handler = quit_on_signal;
	sigaction(SIGTERM,&act,0);
	sigaction(SIGQUIT,&act,0);
	sigaction(SIGPIPE,&act,0);

	/* Unblock signals in our set */
	sigprocmask( SIG_UNBLOCK, &sigSet, NULL );
#endif

	set_mySubSystem("OPENSTACK_GAHP", false, SUBSYSTEM_TYPE_GAHP);

	config();
	dprintf_config( "OPENSTACK_GAHP" );
	const char * debug_string = getenv( "DebugLevel" );
	if( debug_string && * debug_string ) {
		set_debug_flags( debug_string, 0 );
	}

	int min_workers = MIN_NUMBER_WORKERS;
	int max_workers = MAX_NUMBER_WORKERS;

	int c = 0;
	while ( (c = my_getopt(argc, argv, "f:d:w:m:" )) != -1 ) {
		switch(c) {
			case 'f':
				break;
			case 'd':
				// Debug Level
				if( my_optarg && *my_optarg ) {
					set_debug_flags(my_optarg, 0);
				}
				break;
			case 'w':
				// Minimum number of worker pools
				min_workers = atoi(my_optarg);
				if( min_workers < MIN_NUMBER_WORKERS ) {
					min_workers = MIN_NUMBER_WORKERS;
				}
				break;
			case 'm':
				// Maximum number of worker pools
				max_workers = atoi(my_optarg);
				if( max_workers <= 0 ) {
					max_workers = -1;
				}
				break;
			default:
				usage();
		}
	}

	dprintf(D_FULLDEBUG, "Welcome to the Openstack GAHP\n");

	const char *buff;

	//Try to read env for openstack_http_proxy
	buff = getenv(OPENSTACK_HTTP_PROXY);
	if( buff && *buff ) {
		set_openstack_proxy_server(buff);
		dprintf(D_ALWAYS, "Using http proxy = %s\n", buff);
	}

	// Register all Openstack commands
	if( registerAllOpenstackCommands() == false ) {
		dprintf(D_ALWAYS, "Can't register Openstack Commands\n");
		exit(1);
	}

	if( ioprocess.startUp(REQUEST_INBOX, min_workers, max_workers) == false ) {
		dprintf(D_ALWAYS, "Failed to start IO Process\n");
		exit(1);
	}

	// Print out the GAHP version to the screen
	// now we're ready to roll
	printf ("%s\n", version);
	fflush(stdout);

	dprintf (D_FULLDEBUG, "Openstack GAHP initialized\n");

		/* Our main thread should grab the mutex first.  We will then
		 * release it and let other threads run when we would otherwise
		 * block.
		 */
	openstack_gahp_grab_big_mutex();

	for(;;) {
		ioprocess.stdinPipeHandler();
	}

	return 0;
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
gahp_output_return (const char ** results, size_t count) {
	size_t i=0;
	for (i=0; i<count; i++) {
		if (i > 0) {
			printf (" ");
		}
		printf ("%s", results[i]);
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

Worker::Worker(int worker_id)
{
	m_id = worker_id;
	m_is_doing = false;
	m_is_waiting = false;

	pthread_cond_init(&m_cond, NULL);
}

Worker::~Worker()
{
	for (auto request : m_request_list) {
		delete request;
	}
	m_request_list.clear();

	pthread_cond_destroy(&m_cond);
}

bool
Worker::removeRequest(int req_id)
{
	auto delete_matched = [req_id](const Request *r) {
		if (r->m_reqid == req_id) {
			delete r;
			return true;
		} else {
			return false;
		}
	};

	// Assume only one matches
	auto it = std::remove_if(m_request_list.begin(), m_request_list.end(),
			delete_matched);

	if (it == m_request_list.end()) {
		return false; // not found
	} else {
		m_request_list.erase(it);
		return true;
	}
}


Request::Request (const char *cmd)
{
	m_worker = NULL;
	m_raw_cmd = cmd;

	if ( parse_gahp_command(cmd, &m_args) )
		m_reqid = (int)strtol(m_args.argv[1], (char **)NULL, 10);
	else
		m_reqid = -1;
}


// Functions for IOProcess class
IOProcess::IOProcess()
{
	m_async_mode = false;
	m_new_results_signaled = false;

	m_min_workers = MIN_NUMBER_WORKERS;
	m_max_workers = MAX_NUMBER_WORKERS;

	m_next_worker_id = 0;
	m_rotated_worker_ids = false;

	m_avail_workers_num = 0;

}

IOProcess::~IOProcess()
{
}

bool
IOProcess::startUp(int stdin_pipe, int min_workers, int max_workers)
{
	m_min_workers = min_workers;
	m_max_workers = max_workers;
	m_stdin_buffer.setPipeEnd(stdin_pipe);

	// Create initial worker processes
	int i = 0;
	for( i = 0; i < m_min_workers; i++ ) {
		if( createNewWorker() == NULL ) {
			dprintf(D_ALWAYS, "Failed to create initial workers\n");
			dprintf(D_ALWAYS, "Exiting....\n");
			io_process_exit(1);
		}
	}

	return true;
}

int
IOProcess::stdinPipeHandler()
{
	std::string* line;
	while ((line = m_stdin_buffer.GetNextLine()) != NULL) {

		const char *command = line->c_str();

		dprintf (D_FULLDEBUG, "got stdin: %s\n", command);

		Gahp_Args args;

		if (parse_gahp_command (command, &args) &&
			verify_gahp_command (args.argv, args.argc)) {

				// Catch "special commands first
			if (strcasecmp (args.argv[0], GAHP_COMMAND_RESULTS) == 0) {
				// Print each result line

				// Print number of results
				printf("%s %zu\n", GAHP_RESULT_SUCCESS, m_result_list.size());
				fflush(stdout);

				for (const auto& next : m_result_list) {
					printf ("%s", next.c_str());
					fflush(stdout);
				}
				m_result_list.clear();
				m_new_results_signaled = false;

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_VERSION) == 0) {

				printf ("S %s\n", version);
				fflush (stdout);

			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_QUIT) == 0) {
				gahp_output_return_success();
				io_process_exit(0);
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_ON) == 0) {
				m_async_mode = true;
				m_new_results_signaled = false;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_ASYNC_MODE_OFF) == 0) {
				m_async_mode = false;
				gahp_output_return_success();
			} else if (strcasecmp (args.argv[0], GAHP_COMMAND_COMMANDS) == 0) {
				std::vector<std::string> openstack_commands;
				size_t num_commands = 0;

				num_commands = allOpenstackCommands(openstack_commands);
				num_commands += 7;

				const char *commands[num_commands];
				size_t i = 0;
				commands[i++] = GAHP_RESULT_SUCCESS;
				commands[i++] = GAHP_COMMAND_ASYNC_MODE_ON;
				commands[i++] = GAHP_COMMAND_ASYNC_MODE_OFF;
				commands[i++] = GAHP_COMMAND_RESULTS;
				commands[i++] = GAHP_COMMAND_QUIT;
				commands[i++] = GAHP_COMMAND_VERSION;
				commands[i++] = GAHP_COMMAND_COMMANDS;

				for (const auto& one_command : openstack_commands) {
					commands[i++] = one_command.c_str();
				}

				gahp_output_return (commands, i);
			} else {
				// got new command
				if( !addNewRequest(command) ) {
					gahp_output_return_error();
				}else {
					gahp_output_return_success();
				}
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

Worker*
IOProcess::createNewWorker(void)
{
	int new_id = newWorkerId();
	auto [it, success] = m_workers_list.insert({new_id, Worker(new_id)});
	ASSERT(success);
	Worker& new_worker = it->second;

	dprintf (D_FULLDEBUG, "About to start a new thread\n");

	// Create pthread
	pthread_t thread;
	if( pthread_create(&thread, NULL,
				worker_function, (void *)&new_worker) !=  0 ) {
		dprintf(D_ALWAYS, "Failed to create a new thread\n");

		m_workers_list.erase(it);
		return nullptr;
	}

	// Deatch this thread
	pthread_detach(thread);

	m_avail_workers_num++;

	dprintf(D_FULLDEBUG, "New Worker[id=%d] is created!\n", new_worker.m_id);
	return &new_worker;
}

Worker*
IOProcess::findFreeWorker(void)
{
	for (auto& [key, worker]: m_workers_list) {
		if (!worker.m_is_doing) {
			return &worker;
		}
	}
	return nullptr;
}

bool
IOProcess::removeWorkerFromWorkerList(int id)
{
	return (m_workers_list.erase(id) > 0);
}

Request*
IOProcess::addNewRequest(const char *cmd)
{
	if( !cmd ) {
		return NULL;
	}

	Request* new_req = new Request(cmd);
	ASSERT(new_req);

	// check if there is available worker process
	Worker *worker = findFreeWorker();

	if( !worker ) {
		// There is no available worker

		if( m_max_workers == -1 || m_avail_workers_num < m_max_workers ) {
			worker = createNewWorker();
		}
	}

	addRequestToWorker(new_req, worker);
	return new_req;
}

void
IOProcess::addResult(const char *result)
{
	if( !result ) {
		return;
	}

	// Put this result into result buffer
	m_result_list.emplace_back(result);

	if (m_async_mode) {
		if (!m_new_results_signaled) {
			printf ("R\n");
			fflush (stdout);
		}
		m_new_results_signaled = true;	// So that we only do it once
	}
}

int
IOProcess::newWorkerId(void)
{
	int starting_worker_id = m_next_worker_id++;

	while( starting_worker_id != m_next_worker_id ) {
		if( m_next_worker_id > 990'000'000 ) {
			m_next_worker_id = 1;
			m_rotated_worker_ids = true;
		}

		if( !m_rotated_worker_ids ) {
			return m_next_worker_id;
		}

		// Make certain this worker_id is not already in use
		if (m_workers_list.find(m_next_worker_id) == m_workers_list.end()) {
			// not in use, we are done
			return m_next_worker_id;
		}

		m_next_worker_id++;
	}

	// If we reached here, we are out of worker ids
	dprintf(D_ALWAYS, "Gahp Server Error - out of worker ids !!!?!?!?\n");
	return -1;
}

void
IOProcess::addRequestToWorker(Request* request, Worker* worker)
{
	if( !request ) {
		return;
	}

	if( worker ) {
		dprintf (D_FULLDEBUG, "Sending %s to worker %d\n",
				request->m_raw_cmd.c_str(), worker->m_id);

		request->m_worker = worker;
		worker->m_request_list.push_back(request);
		worker->m_is_doing = true;

		if( worker->m_is_waiting ) {
			pthread_cond_signal(&worker->m_cond);
		}

	}else {
		// There is no available worker.
		// So we will insert this request to global pending request list
		dprintf (D_FULLDEBUG, "Appending %s to global pending request list\n",
				request->m_raw_cmd.c_str());

		m_pending_req_list.push_back(request);
	}
}

int
IOProcess::numOfPendingRequest(void)
{
	int num = 0;
	num = (int) m_pending_req_list.size();

	return num;
}

Request*
IOProcess::popPendingRequest(void)
{
	if (m_pending_req_list.empty()) {
		return nullptr;
	} else {
		Request *new_request = m_pending_req_list.front();
		m_pending_req_list.erase(m_pending_req_list.begin());
		return new_request;
	}
}

Request* popRequest(Worker* worker)
{
	Request *new_request = nullptr;
	if( !worker ) {
		return nullptr;
	}

	if (!worker->m_request_list.empty()) {
		// Remove this request from worker request queue
		new_request = worker->m_request_list.front();
		worker->m_request_list.erase(worker->m_request_list.begin());
	} else {
		new_request = ioprocess.popPendingRequest();

		if( new_request ) {
			new_request->m_worker = worker;

			dprintf (D_FULLDEBUG, "Assigning %s to worker %d\n",
					 new_request->m_raw_cmd.c_str(), worker->m_id);
		}
	}

	return new_request;
}

static void
enqueue_result(Request* request)
{
	if( !request || request->m_result.empty() ) {
		return;
	}

	ioprocess.addResult(request->m_result.c_str());
}

static bool
handle_gahp_command(Request* request)
{
	if( !request ) {
		return false;
	}
	char ** argv = request->m_args.argv;
	int argc = request->m_args.argc;

	// Assume it's been verified
	if( argc < 2 ) {
		dprintf (D_ALWAYS, "Invalid request\n");
		return false;
	}

	bool result = executeWorkerFunc(argv[0], argv, argc, request->m_result);

	if( request->m_result.empty() == false ) {
		enqueue_result(request);
	}

	return result;
}

static void worker_exit(Worker *worker, bool force)
{
	if( !worker ) {
		return;
	}

	int worker_id = worker->m_id;

	bool need_remove = force;

	if( need_remove == false ) {
		if( ioprocess.m_avail_workers_num > ioprocess.m_min_workers ) {
			need_remove = true;
		}
	}

	if( need_remove ) {
		// worker will be deleted inside removeWorkerFromWorkerList
		ioprocess.removeWorkerFromWorkerList(worker->m_id);
		worker = NULL;
		ioprocess.m_avail_workers_num--;

		dprintf(D_FULLDEBUG, "Thread(%d) is exiting...\n", worker_id);

		int retval = 0;
		openstack_gahp_release_big_mutex();
		pthread_exit(&retval);
	}else {
		//dprintf(D_FULLDEBUG, "Thread(%d) is going to be used again\n",
		//		worker_id);

		// We need to keep this thread running
		worker->m_is_doing = false;
		worker->m_is_waiting = false;
	}
}

static void *worker_function( void *ptr )
{
	Worker *worker = (Worker *)ptr;

		/* Our new thread should grab the big mutex before starting.
		 * We will then
		 * release it and let other threads run when we would otherwise
		 * block.
		 */
	openstack_gahp_grab_big_mutex();

	if( !worker ) {
		dprintf (D_ALWAYS, "Ooops!! No input Data in worker thread\n");
		openstack_gahp_release_big_mutex();
		return NULL;
	}

	// Pop Request
	Request *new_request = NULL;
	struct timespec ts;
	struct timeval tp;

	while(1) {

		worker->m_is_doing = false;
		worker->m_is_waiting = false;

		while( (new_request = popRequest(worker)) == NULL ) {

			worker->m_is_waiting = true;

			// Get Current Time
			gettimeofday(&tp, NULL);

			/* Convert from timeval to timespec */
			ts.tv_sec = tp.tv_sec;
			ts.tv_nsec = tp.tv_usec * 1000;
			ts.tv_sec += WORKER_MANAGER_TIMER_INTERVAL;

			//dprintf(D_FULLDEBUG, "Thread(%d) is calling cond_wait\n",
			//		worker->m_id);

			// The pthread_cond_timedwait will block until signalled
			// with more work from our main thread; so we MUST release
			// the big fat mutex here or we will deadlock.
			int retval = pthread_cond_timedwait(&worker->m_cond,
					&global_big_mutex, &ts);

			worker->m_is_waiting = false;

			if( retval == ETIMEDOUT ) {
				//dprintf(D_FULLDEBUG, "Thread(%d) Wait timed out !\n",
				//		worker->m_id);

				if( ioprocess.numOfPendingRequest() > 0 ) {
					continue;
				}

				worker_exit(worker, false);
			}
		}

		worker->m_is_doing = true;
		worker->m_is_waiting = false;

		if(!handle_gahp_command(new_request) ) {
			dprintf(D_ALWAYS, "ERROR (io_loop) processing %s\n",
					new_request->m_raw_cmd.c_str());
		} else {
			if(! new_request->m_result.empty()) {
				dprintf(D_FULLDEBUG, "CMD(\"%s\") is done with result %s",
						new_request->m_raw_cmd.c_str(),
						new_request->m_result.c_str());
			} else {
				dprintf(D_FULLDEBUG, "CMD(\"%s\") is done with no result\n",
						new_request->m_raw_cmd.c_str());
			}
		}

		// Now we processed one request
		delete new_request;
		new_request = NULL;

	}

	openstack_gahp_release_big_mutex();
	return NULL;
}
