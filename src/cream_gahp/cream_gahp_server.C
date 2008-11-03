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


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "glite/ce/cream-client-api-c/CreamProxy.h"
#include "glite/ce/cream-client-api-c/creamApiLogger.h"
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <ctime>
#include <queue>
#include <map>

using namespace std;

#define WORKER 6 // Worker threads

// Shortcuts
#define CREAMPROXY glite::ce::cream_client_api::soap_proxy::CreamProxy
#define CREAM_SERVICE "/ce-cream/services/CREAM"
#define CREAM_DELG_SERVICE "/ce-cream/services/CREAMDelegation"

// Macro for handling commands 
#define HANDLE( x ) \
if (strcasecmp(#x, input_line[0]) == 0) \
	handle_##x(input_line)

// Macro for bad syntax
#define HANDLE_SYNTAX_ERROR() \
{ \
 gahp_printf("E\n"); \
 free_args(input_line); \
 return 0; \
}	

#define HANDLE_INVALID_REQ() \
{ \
 gahp_printf("E\n"); \
 free_args(input_line); \
}	

void free_args( char ** );

struct WorkerData {
	CREAMPROXY *creamProxy;
};

struct Request {
	Request() { input_line = NULL; }
	~Request() { if ( input_line ) free_args( input_line ); }

	char **input_line;
	string proxy;
	int (*handler)(WorkerData *, Request *);
};

static char *commands_list =
"COMMANDS "
"VERSION "
"INITIALIZE_FROM_FILE "
"CREAM_DELEGATE "
"CREAM_JOB_REGISTER "
"CREAM_JOB_START "
"CREAM_JOB_PURGE "
"CREAM_JOB_CANCEL "
"CREAM_JOB_SUSPEND "
"CREAM_JOB_RESUME "
"CREAM_JOB_STATUS "
"CREAM_JOB_LIST "
"CREAM_PING "
"CREAM_DOES_ACCEPT_JOB_SUBMISSIONS "
"CREAM_PROXY_RENEW "
"CREAM_GET_CEMON_URL "
"RESPONSE_PREFIX "
"ASYNC_MODE_ON "
"ASYNC_MODE_OFF "
"CACHE_PROXY_FROM_FILE "
"USE_CACHED_PROXY "
"UNCACHE_PROXY "
"RESULTS "
"QUIT";
/* The last command in the list should NOT have a space after it */

// GLOBAL VARIABLES
char *VersionString ="$GahpVersion: CREAM " __DATE__ " UW\\ Gahp $";
pthread_t *threads;
string active_proxy;
bool initialized = false; // Indicates if PROXY INIT has been called
map<string, string> proxies;
string sp = " "; // CHANGE THIS

// Mutex requestQueueLock controlls access to requestQueue
queue<Request *> requestQueue;
pthread_cond_t requestQueueEmpty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t requestQueueLock = PTHREAD_MUTEX_INITIALIZER;

// Mutex resultQueueLock controlls access to resultQueue, async, and
// results_pending
queue<string> resultQueue;
pthread_mutex_t resultQueueLock = PTHREAD_MUTEX_INITIALIZER;
bool async = false;
bool results_pending = false; // Default, asynchronous mode is off

// Mutex outputLock controlls access to standard output and response_prefix
pthread_mutex_t outputLock = PTHREAD_MUTEX_INITIALIZER;
char *response_prefix = NULL; 

int thread_cream_delegate( WorkerData *wd, Request *req );
int thread_cream_job_register( WorkerData *wd, Request *req );
int thread_cream_job_start( WorkerData *wd, Request *req );
int thread_cream_job_purge( WorkerData *wd, Request *req );
int thread_cream_job_cancel( WorkerData *wd, Request *req );
int thread_cream_job_suspend( WorkerData *wd, Request *req );
int thread_cream_job_resume( WorkerData *wd, Request *req );
int thread_cream_job_lease( WorkerData *wd, Request *req );
int thread_cream_job_status( WorkerData *wd, Request *req );
int thread_cream_job_info( WorkerData *wd, Request *req );
int thread_cream_job_list( WorkerData *wd, Request *req );
int thread_cream_ping( WorkerData *wd, Request *req );
int thread_cream_does_accept_job_submissions( WorkerData *wd, Request *req );
int thread_cream_proxy_renew( WorkerData *wd, Request *req );
int thread_cream_get_CEMon_url( WorkerData *wd, Request *req );

/* =========================================================================
   GAHP_PRINTF: prints to stdout. If response prefix is specified, prints it as well 
   ========================================================================= */

int gahp_printf(const char *format, ...)
{
	int ret_val;
	va_list ap;
	char buf[10000];
  
	va_start(ap, format);
	vsprintf(buf, format, ap);
  
	pthread_mutex_lock( &outputLock );

	if (response_prefix)
		ret_val = printf("%s%s", response_prefix, buf);
	else
		ret_val = printf("%s",buf);
	
	fflush(stdout);

	pthread_mutex_unlock( &outputLock );
	
	return ret_val;
}


/* =========================================================================
   PROCESS_STRING_ARG: determines if input_line is valid
   ========================================================================= */

void process_string_arg(char *input_line, char **output)
{
		// some commands are allowed to pass in "NULL" as their option. If this
		// is the case, set output to point to a NULL string
	if(!strcasecmp(input_line, "NULL")) 
		*output = NULL;  
	else
		*output = input_line; // give back what they gave us
}

int 
process_int_arg( char *input, int *output)
{
	if( input && strlen(input)) {
		if ( output ) {
			*output = atoi(input);
		}
		return true;
	}
	return false;
}

int count_args( char **input_line )
{
	int count = 0;
	for ( count = 0; input_line[count]; count++ );
	return count;
}

/* =========================================================================
   ESCAPE_SPACES: replaces ' ' with '\ '
   The caller is responsible for freeing the memory returned by this func 
   ========================================================================= */
char *escape_spaces( const char *input_line) 
{
	int i;
	char *temp;
	char *output_line;

	// first, count up the spaces
	temp = (char *)input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if( *temp == ' ' || *temp == '\r' || *temp =='\n')  i++;
	}

	// get enough space to store it.  	
	output_line = (char *)malloc(strlen(input_line) + i + 200);

	// now, blast across it
	temp = (char *)input_line;
	for(i = 0; *temp != '\0'; temp++) {
		if( *temp == ' ') {
			output_line[i] = '\\'; 
			i++;
		}
		if( *temp == '\r' || *temp == '\n') {
			output_line[i] = '\\'; 
			i++;
			*temp = ' ';
		}
		output_line[i] = *temp;
		i++;
	}
	output_line[i] = '\0';
	// the caller is responsible for freeing this memory, not us
	return output_line;	
}


/* =========================================================================
   FREE_ARGS: frees the input line arguments 
   ========================================================================= */

void free_args( char **input_line )
{
	if(input_line == NULL)
		return;

	for ( int i = 0; input_line[i]; i++ ) {
		free( input_line[i] );
	}

	free(input_line);
}

void enqueue_request( char **input_line,
					  int(* handler)(WorkerData *, Request *) )
{
	Request *req = new Request();
	req->input_line = input_line;
	req->proxy = active_proxy;
	req->handler = handler;

	pthread_mutex_lock( &requestQueueLock );
	requestQueue.push( req );
	pthread_cond_signal( &requestQueueEmpty );
	pthread_mutex_unlock( &requestQueueLock );
}

/* =========================================================================
   ENQUEUE_RESULT: enqueues the result line into the queue. Also notifies
   to stdout if asynchronous mode is on.
   ========================================================================= */

void enqueue_result(string result_line)
{
	pthread_mutex_lock( &resultQueueLock );

	if (async && !results_pending) {
		gahp_printf("R\n");
		results_pending = true;
	}
	
		// Need to add locks here to make sure of exact order
	resultQueue.push(result_line);

	pthread_mutex_unlock( &resultQueueLock );
	
	return;
}


/* =========================================================================
   INITIALIZE_FROM_FILE: provides the GAHP server with a
   GSI Proxy certificate used for all subsequent authentication
   ========================================================================= */

int handle_initialize_from_file(char **input_line)
{
	char *cert;
	
	if ( count_args( input_line) != 2 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[1], &cert );

	active_proxy = cert;

	try {
			// Check whether CreamProxy likes the proxy
		CREAMPROXY cream_proxy( false );
		cream_proxy.Authenticate(active_proxy.c_str()); 
	} catch(std::exception& ex) {
		gahp_printf("E\n");
		free_args(input_line);
		
		return 1;
	}

	initialized = true;    	
	gahp_printf("S\n");
	
	free_args(input_line);
	
	return 0;
}  

/* =========================================================================
   CREAM_DELEGATE: delegates a proxy to the CREAM Service.
   ========================================================================= */

int handle_cream_delegate( char **input_line )
{
	if ( count_args( input_line ) != 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_delegate );

	gahp_printf("S\n");

	return 0;
}

int thread_cream_delegate( WorkerData *wd, Request *req )
{
	char *reqid, *delgservice, *delgid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &delgid );
	process_string_arg( req->input_line[3], &delgservice);

	try {
		wd->creamProxy->Delegate(delgid, delgservice, req->proxy.c_str());
	}  catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Delegate\\ Error\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);

		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_REGISTER: registers a job in the CREAM service
   ========================================================================= */

int handle_cream_job_register( char **input_line )
{
	int lease_time;
	if ( count_args( input_line ) != 7 ||
		 !process_int_arg( input_line[6], &lease_time ) ||
		 lease_time <= 0 ) {
		HANDLE_SYNTAX_ERROR();
	}
	
	enqueue_request( input_line, thread_cream_job_register );

	gahp_printf("S\n");  

	return 0;
}

int thread_cream_job_register( WorkerData *wd, Request *req )
{
	char *reqid, *service, *delgservice, *delgid, *jdl;
	string result_line;
	int lease_time = 0;
	
	std::vector<std::string> uploadURL_and_jobID;
	std::string JDLBuffer, JDLBufferTemp;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &delgservice );
	process_string_arg( req->input_line[4], &delgid );
	process_string_arg( req->input_line[5], &jdl );
	process_int_arg( req->input_line[6], &lease_time );
	
	try {
		
		string delgID = delgid;

		wd->creamProxy->Register(service, delgservice, delgID, jdl, active_proxy,
							   uploadURL_and_jobID, lease_time, false); // No auto start
		
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Register\\ Error\\ "+ escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL " + uploadURL_and_jobID[1] + sp + uploadURL_and_jobID[0];
	enqueue_result(result_line);
	
	return 0;
}

/* =========================================================================
   CREAM_START: triggers registered jobs.
   ========================================================================= */

int handle_cream_job_start( char **input_line )
{
	if ( count_args( input_line ) != 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_start );

	gahp_printf("S\n");  

	return 0;
}

int thread_cream_job_start( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobid );
	
	try {
		wd->creamProxy->Start( service, jobid );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Start\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_PURGE: purges jobs in 
   (DONE_OK, DONE_FAILED, CANCELLED, ABORTED state)
   ========================================================================= */

int handle_cream_job_purge( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[3], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 4 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_purge );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_purge( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, purge ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+4], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		wd->creamProxy->Purge( service, jobs );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Purge\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_CANCEL: cancels job(s) in 
   (PENDING, IDLE, RUNNING, REALLY_RUNNING or HELD state)
   ========================================================================= */

int handle_cream_job_cancel( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[3], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 4 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_cancel );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_cancel( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, cancel ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+4], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		wd->creamProxy->Cancel( service, jobs );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Cancel\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}

/* =========================================================================
   CREAM_SUSPEND: suspend job(s) in 
   (PENDING, IDLE, RUNNING, REALLY_RUNNING state)
   ========================================================================= */

int handle_cream_job_suspend( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[3], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 4 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_suspend );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_suspend( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, suspend ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+4], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		wd->creamProxy->Suspend( service, jobs );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Suspend\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_JOB_RESUME: resume job(s) in HELD state
   ========================================================================= */

int handle_cream_job_resume( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[3], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 4 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_resume );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_resume( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, resume ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+4], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		wd->creamProxy->Resume( service, jobs );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Resume\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_JOB_LEASE:
   ========================================================================= */

int handle_cream_job_lease( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 5 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[4], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 5 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_lease );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_lease( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid, *lease_incr;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &lease_incr );
	process_string_arg( req->input_line[4], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, Lease ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+5], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		map<string, time_t> lease_times;

		wd->creamProxy->Lease( service, jobs, atoi(lease_incr), lease_times );
		
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Lease\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL"; // todo
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_STATUS
   ========================================================================= */

int handle_cream_job_status( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[3], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 4 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_status );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_status( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid;
	string result_line, temp;
	std::vector<glite::ce::cream_client_api::soap_proxy::Status> job_info;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, query ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+4], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		std::vector<std::string> status_filter; // for now, no filter

		wd->creamProxy->Status( service, jobs, status_filter, job_info, -1, -1 );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Status\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);

		return 1;
	}
	
	std::vector<glite::ce::cream_client_api::soap_proxy::Status>::const_iterator jobStatusIt;
	
	result_line = (string)reqid + " NULL";
	
	int cnt = 0;
	char *failure_reason = NULL;
	string exit_code;

	for (jobStatusIt = job_info.begin(); jobStatusIt != job_info.end(); ++jobStatusIt) {

		if (((*jobStatusIt).getExitCode()).size() == 0) 
			exit_code = "NULL";
		else
			exit_code = (*jobStatusIt).getExitCode();
		
		temp +=  sp + (*jobStatusIt).getJobID() + sp + (*jobStatusIt).getStatusName() + sp + exit_code;

		if (((*jobStatusIt).getFailureReason()).size() == 0) 
			temp += " NULL";

		else {
			failure_reason = escape_spaces(((*jobStatusIt).getFailureReason()).c_str());
			temp += sp + failure_reason;
			free(failure_reason);
		}

		cnt++;
	}

	char buf[10000];
	
	sprintf(buf, "%d", cnt);
	result_line += sp + string(buf) + temp;

	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_INFO: prints out job's information. For debugging.
   ========================================================================= */

int handle_cream_job_info( char **input_line )
{
	int arg_cnt = count_args( input_line );
	char *jobnum = NULL;

	if ( arg_cnt < 4 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[3], &jobnum );
	if ( jobnum && ( atoi( jobnum ) + 4 != arg_cnt ) ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_info );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_job_info( WorkerData *wd, Request *req )
{
	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	std::vector<glite::ce::cream_client_api::soap_proxy::JobInfo> job_info;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		std::vector<std::string> jobs;
		
			// if jobnum_str is NULL, query ALL available jobs
		if ( jobnum_str != NULL ) {
			int jobnum = atoi( jobnum_str );
			for ( int i = 0; i < jobnum; i++) {
				process_string_arg( req->input_line[i+4], &jobid );
				jobs.push_back(jobid);
			}
		}
		
		std::vector<std::string> status_filter; // for now, no filter

		wd->creamProxy->Info( service, jobs, status_filter, job_info, -1, -1 );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Info\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	std::vector<glite::ce::cream_client_api::soap_proxy::JobInfo>::const_iterator jobStatusIt;
	
	for (jobStatusIt = job_info.begin(); jobStatusIt != job_info.end(); ++jobStatusIt) {
		(*jobStatusIt).print(2);
	}

		// TODO What aren't we returning anything!!!!?????
	
	return 0;
}


/* =========================================================================
   CREAM_JOB_LIST: prints out a list of job ids
   ========================================================================= */

int handle_cream_job_list( char ** input_line )
{
	if ( count_args( input_line ) != 3 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_job_list );

	gahp_printf("S\n");  

	return 0;
}

int thread_cream_job_list( WorkerData *wd, Request *req )
{
	char *reqid, *service;
	std::vector<std::string> jobs;	
	string result_line, temp;

	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	
	try {
		wd->creamProxy->List( service, jobs );
	} catch(std::exception& ex) {

		result_line = (string)reqid + " CREAM_Job_List\\ Error\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);
				
		return 1;
	}
	
	std::vector<std::string>::const_iterator jobIt;

	result_line = (string)reqid + " NULL";
	
	int cnt = 0;
	for (jobIt = jobs.begin(); jobIt != jobs.end(); ++jobIt) {
		temp += sp + *jobIt;
		cnt++;
	}
	
	char buf[10000];
	
	sprintf(buf, "%d", cnt);  
	result_line += sp + string(buf) + temp;

	enqueue_result(result_line);
		
	return 0;
}


/* =========================================================================
   CREAM_PING: pings a CREAM service.
   ========================================================================= */

int handle_cream_ping( char **input_line )
{
	if ( count_args( input_line ) != 3 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_ping );
	
	gahp_printf("S\n");

	return 0;
}

int thread_cream_ping( WorkerData *wd, Request *req )
{
	char *reqid, *service;
	bool ret;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	
	try {
		ret = wd->creamProxy->Ping(service);
	} catch(std::exception& ex) {
		
		enqueue_result((string)reqid + " " + escape_spaces(ex.what()));
		
		return 1;
	}
	
	if (ret) 
		enqueue_result((string)reqid + " NULL true"); // Service is available
	else 
		enqueue_result((string)reqid + " NULL false"); // Service is not available
	
	return 0;
}


/* =========================================================================
   CREAM_DOES_ACCEPT_JOB_SUBMISSIONS: asks if a CREAM service has job submission enabled
   ========================================================================= */

int handle_cream_does_accept_job_submissions( char **input_line )
{
	if ( count_args( input_line ) != 3 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_does_accept_job_submissions );

	gahp_printf("S\n");

	return 0;
}

int thread_cream_does_accept_job_submissions( WorkerData *wd, Request *req )
{
	char *reqid, *service;
	bool ret;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	
	try {
		ret = wd->creamProxy->DoesAcceptJobSubmissions(service);
	} catch(std::exception& ex) {
		
		enqueue_result((string)reqid + " " + escape_spaces(ex.what()));
		
		return 1;
	}
	
	if (ret)
		enqueue_result((string)reqid + " NULL true"); // Accepts job submissions
	
	else 
		enqueue_result((string)reqid + " NULL false"); // Does not accept job submissions
	
	return 0;
}


/* =========================================================================
   CREAM_PROXY_RENEW: renews proxy of jobs in 
   <PENDING, IDLE, RUNNING, REALLY-RUNNING, HELD> status
   ========================================================================= */

int handle_cream_proxy_renew( char **input_line )
{
	int arg_cnt = count_args( input_line );
	int jobnum;

	if ( arg_cnt < 6 || !process_int_arg( input_line[5], &jobnum ) ||
		 jobnum <= 0 || jobnum + 6 != arg_cnt ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_proxy_renew );

	gahp_printf("S\n");

	return 0;
}

int thread_cream_proxy_renew( WorkerData *wd, Request *req )
{
	int jobnum;
	char *reqid, *service, *delgservice, *delgid, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &delgservice );
	process_string_arg( req->input_line[4], &delgid );
	process_int_arg( req->input_line[5], &jobnum );
	
	try {
		std::vector<std::string> jobs;
		
		for ( int i = 0; i < jobnum; i++ ) {
			process_string_arg( req->input_line[i+6], &jobid );
			
			jobs.push_back(jobid);
		}
		
		wd->creamProxy->renewProxy( delgid, service, delgservice,
								   req->proxy.c_str(), jobs);
	}  catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Proxy_Renew\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);  
	
	return 0;
}


/* =========================================================================
   CREAM_GET_CEMON_URL: returns the URL of the CEMon service
   coupled to the given CREAM service.
   ========================================================================= */

int handle_cream_get_CEMon_url( char **input_line )
{
	if ( count_args( input_line ) != 3 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_get_CEMon_url );
	
	gahp_printf("S\n"); 

	return 0;
}

int thread_cream_get_CEMon_url( WorkerData *wd, Request *req )
{
	char *reqid, *service;
	string CEMon, result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	
	try {
		wd->creamProxy->GetCEMonURL( service, CEMon );
	} catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Get_CEMon_URL\\ Error\\" + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL " + CEMon;
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   RESULTS: prints out content of result line queue
   ========================================================================= */

int handle_results( char **input_line )
{
	pthread_mutex_lock( &resultQueueLock );

	results_pending = false; // Reset results pending
	
	gahp_printf( "S %d\n", resultQueue.size( ));
	
	while ( !resultQueue.empty( )) {
		gahp_printf( "%s\n", (resultQueue.front()).c_str( ));
		resultQueue.pop();
	}
	
	pthread_mutex_unlock( &resultQueueLock );

	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   VERSION: prints out Gahp Server's Version
   ========================================================================= */

int handle_version( char **input_line )
{
	gahp_printf("S %s\n", VersionString);
	
	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   RESPONSE_PREFIX: specifies prefix to prepend every
   subsequent line of output.
   ========================================================================= */

int handle_response_prefix( char **input_line )
{
	char *prefix;
	
	if ( count_args( input_line ) != 2 ) {
		for(int i=0;input_line[i];i++){fprintf(stderr,"input_line[%d]=\"%s\" (len=%d)\n",i,input_line[i],strlen(input_line[i]));}
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[1], &prefix );

	gahp_printf("S\n");

	pthread_mutex_lock( &outputLock );

	if (response_prefix)
		free(response_prefix);

	if (prefix) {
		response_prefix = strdup( prefix );
	}

	pthread_mutex_unlock( &outputLock );
	
	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   ASYNC_MODE_ON: enables asynchronous notification
   ========================================================================= */

int handle_async_mode_on( char **input_line )
{
	pthread_mutex_lock( &resultQueueLock );
	async = true;
	results_pending = false;
	pthread_mutex_unlock( &resultQueueLock );
	
	gahp_printf("S\n");
	
	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   ASYNC_MODE_OFF: disables asynchronous notification
   ========================================================================= */

int handle_async_mode_off( char **input_line )
{
	pthread_mutex_lock( &resultQueueLock );
	async = false;
	results_pending = false;
	pthread_mutex_unlock( &resultQueueLock );
	
	gahp_printf("S\n");
	
	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   COMMANDS: prints out command list
   ========================================================================= */

int handle_commands( char **input_line )
{
	gahp_printf("S %s\n", commands_list);
	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   QUIT: quits and frees allocated memory
   ========================================================================= */

int handle_quit( char **input_line )
{
	gahp_printf("S\n");

	free_args( input_line );
	
	if (response_prefix != NULL)
		free(response_prefix);
	
	exit(0);
}


/* =========================================================================
   CACHE_PROXY_FROM_FILE: read proxy and store under a symbolic name
   ========================================================================= */

int handle_cache_proxy_from_file( char **input_line)
{
	char *reqid, *filename;

	if ( count_args( input_line ) != 3 ) {
		HANDLE_SYNTAX_ERROR();
	}
	
	process_string_arg( input_line[1], &reqid );
	process_string_arg( input_line[2], &filename );
	if ( reqid == NULL || filename == NULL ) {
		HANDLE_SYNTAX_ERROR();
	}

	try {
			// Check whether CreamProxy likes the proxy
		CREAMPROXY cream_proxy( false );
		cream_proxy.Authenticate( filename ); 
	} catch ( std::exception& ex ) {
		gahp_printf("E\n");
		free_args( input_line );

		return 1;
	}

	gahp_printf("S\n");

	proxies[string(reqid)] = string(filename);

	free_args( input_line );

	return 0;
}


/* =========================================================================
   USE_CACHED_PROXY: sets previously cached proxy as active proxy
   ========================================================================= */

int handle_use_cached_proxy( char **input_line )
{
	char *reqid;
	map<string, string>::const_iterator it;

	if ( count_args( input_line ) != 2 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[1], &reqid );
	if ( reqid == NULL ) {
		HANDLE_SYNTAX_ERROR();
	}
	
		// What the hell is this sleep for?
	sleep(1);
	it = proxies.find(string(reqid));

	if (it != proxies.end()) {
		gahp_printf("S\n");
		active_proxy = it->second;
	}
	else {
		gahp_printf("F unable\\ to\\ locate\\ proxy\\ cached\\ under\\ key:\\ %s\n", reqid);
	}

	free_args( input_line );
	
	return 0;
}


/* =========================================================================
   UNCACHE_PROXY: removes proxy from list of cached proxies
   ========================================================================= */

int handle_uncache_proxy( char **input_line )
{
	char *reqid;
	map<string, string>::const_iterator it;

	if ( count_args( input_line ) != 2 ) {
		HANDLE_SYNTAX_ERROR();
	}

	process_string_arg( input_line[1], &reqid );
	if ( reqid == NULL ) {
		HANDLE_SYNTAX_ERROR();
	}

		// What the hell is this sleep for?
	sleep(1);
	it = proxies.find(string(reqid));

	if (it != proxies.end()) {
		gahp_printf("S\n");
		proxies.erase(string(reqid));
	} else {
		gahp_printf("F unable\\ to\\ locate\\ proxy\\ cached\\ under\\ key:\\ %s\n", reqid);
	}

	free_args( input_line );

	return 0;
}


/* =========================================================================
   READ_COMMAND: parses command line input from stdin.
   Using read() system call which blocks just the thread.
   ========================================================================= */

char **read_command()
{
	static char* buf = NULL;
	char** command_argv;
	int ibuf = 0;
	int iargv = 0;
	int result = 0;
	
	if ( buf == NULL ) {
		buf = (char *) malloc(1024 * 500);
	}
	
	command_argv = (char **)calloc(500, sizeof(char*));
	
	while(1) {
		result = read(0, &(buf[ibuf]), 1 );
		
		if (result < 0) // Error
			exit(2);	
		
		if (result == 0) // EOF
			exit(3);
		
			// If the last character was a backslash, this character has
			// no special meaning and the backslash is dropped.
		if ( ibuf > 0 && buf[ibuf-1] == '\\' ) {
			buf[ibuf-1] = buf[ibuf];
			continue;
		}
			// Unescaped backslashes are ignored.
		if ( buf[ibuf] == '\r' ) {
			continue;
		}
			// An unescaped space seperates arguments
		if ( buf[ibuf] == ' ' ) {

			buf[ibuf] = '\0';
			command_argv[iargv] = (char *)malloc(ibuf + 5);
			strcpy(command_argv[iargv],buf);
			ibuf = 0;
			iargv++;
			continue;
		}
		
			// If character was a newline, copy into argv and return
		if (buf[ibuf] == '\n') { 
			buf[ibuf] = '\0';
			command_argv[iargv] = (char *)malloc(ibuf + 5);
			strcpy(command_argv[iargv],buf);
			
			return command_argv;
		}
		
		ibuf++;
	}	
}

/* =========================================================================
   PROCESS_REQUEST: handles requests read from bufferpool.
   ========================================================================= */

void process_request( char **input_line )
{
	HANDLE(initialize_from_file);
	else HANDLE(commands); 
	else HANDLE(version);
	else HANDLE(quit);
	else HANDLE(cache_proxy_from_file);
	else HANDLE(use_cached_proxy);
	else HANDLE(uncache_proxy);
	else HANDLE(response_prefix);
	else HANDLE(async_mode_on);
	else HANDLE(async_mode_off);
	else HANDLE(results);

		// Only after initialize_from_file is called, can the following commands be issued 
	else if (initialized == true) {
		
		HANDLE(cream_job_status);
		else HANDLE(cream_job_info);
		else HANDLE(cream_job_start);
		else HANDLE(cream_job_register); 
		else HANDLE(cream_job_purge);
		else HANDLE(cream_job_suspend); 
		else HANDLE(cream_job_cancel);
		else HANDLE(cream_job_resume);
		else HANDLE(cream_job_list); 
		else HANDLE(cream_job_lease); 
		else HANDLE(cream_delegate);
		else HANDLE(cream_proxy_renew);
		else HANDLE(cream_get_CEMon_url);
		else HANDLE(cream_ping);
		else HANDLE(cream_does_accept_job_submissions);
		else HANDLE_INVALID_REQ();
	} // End if initialized
  
	else HANDLE_INVALID_REQ();
}


/* =========================================================================
   WORKER_MAIN: called by every worker threads.
   ========================================================================= */

void *worker_main(void *ignored)
{
	CREAMPROXY cream_proxy( false );
	WorkerData wd;
	Request *req;

	wd.creamProxy = &cream_proxy;

	while ( 1 ) {
		pthread_mutex_lock( &requestQueueLock );
		while ( requestQueue.empty() ) {
			pthread_cond_wait( &requestQueueEmpty, &requestQueueLock );
		}
		req = requestQueue.front();
		requestQueue.pop();
		pthread_mutex_unlock( &requestQueueLock );

		try {
			cream_proxy.Authenticate( req->proxy );

			req->handler( &wd, req );
		} catch ( std::exception& ex ) {
			enqueue_result( (string)req->input_line[0] + " " +
							escape_spaces( ex.what() ) );
		}

		delete req;
	}

	return NULL;
}  


/* =========================================================================
   MAIN
   ========================================================================= */

int main(int argc, char **argv)
{
	int i;

		// Enable logging to stderr. In the future, we may want make
		// this configurable and/or turn down the priority level.
	glite::ce::cream_client_api::util::creamApiLogger* logger_instance =
		glite::ce::cream_client_api::util::creamApiLogger::instance();
	log4cpp::Category* log_dev = logger_instance->getLogger();
	log_dev->setPriority( log4cpp::Priority::INFO );
	//logger_instance->setLogFile( string( "/tmp/cream.log" ) );
	logger_instance->setConsoleEnabled( true );

	gahp_printf("%s\n", VersionString);
	
	threads = (pthread_t *)malloc(sizeof(pthread_t) * WORKER);
	
		//create & detach worker threads
	for (i = 0; i < WORKER; i++){
		
		pthread_create(&threads[i], NULL, worker_main, NULL);
		pthread_detach(threads[i]);
	}
	
	while (1) {
		char **in = read_command(); // Read commands from stdin
		
		process_request( in );
	}
	
	return 0;
}
