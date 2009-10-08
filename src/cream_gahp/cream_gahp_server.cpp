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
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <ctime>
#include <queue>
#include <map>
#include "glite/ce/cream-client-api-c/CreamProxyFactory.h"
#include "glite/ce/cream-client-api-c/JobDescriptionWrapper.h"
#include "glite/ce/cream-client-api-c/creamApiLogger.h"

using namespace std;
using namespace glite::ce::cream_client_api::soap_proxy;

#define WORKER 6 // Worker threads

#define DEFAULT_TIMEOUT 60

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

static void
check_for_factory_error(AbsCreamProxy* cp)
{
	if (cp == NULL) {
		throw runtime_error("CreamProxyFactory failed to make a cream proxy");
	}
}

static void
check_result_wrapper(ResultWrapper& rw)
{
	list<pair<JobIdWrapper, string> > job_list;
	rw.getNotExistingJobs(job_list);
	if (!job_list.empty()) {
		throw runtime_error("job does not exist");
	}
	rw.getNotMatchingStatusJobs(job_list);
	if (!job_list.empty()) {
		throw runtime_error("job status does not match");
	}
	rw.getNotMatchingDateJobs(job_list);
	if (!job_list.empty()) {
		throw runtime_error("job date does not match");
	}
	rw.getNotMatchingProxyDelegationIdJobs(job_list);
	if (!job_list.empty()) {
		throw runtime_error("job proxy delegation ID does not match");
	}
	rw.getNotMatchingLeaseIdJobs(job_list);
	if (!job_list.empty()) {
		throw runtime_error("job lease ID does not match");
	}
}

void free_args( char ** );

struct Request {
	Request() { input_line = NULL; handler = NULL; }
	~Request() { if ( input_line ) free_args( input_line ); }

	char **input_line;
	string proxy;
	int (*handler)(Request *);
};

static char *commands_list =
"COMMANDS "
"VERSION "
"INITIALIZE_FROM_FILE "
"CREAM_DELEGATE "
"CREAM_SET_LEASE "
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

int thread_cream_delegate( Request *req );
int thread_cream_job_register( Request *req );
int thread_cream_job_start( Request *req );
int thread_cream_job_purge( Request *req );
int thread_cream_job_cancel( Request *req );
int thread_cream_job_suspend( Request *req );
int thread_cream_job_resume( Request *req );
int thread_cream_set_lease( Request *req );
int thread_cream_job_status( Request *req );
int thread_cream_job_info( Request *req );
int thread_cream_job_list( Request *req );
int thread_cream_ping( Request *req );
int thread_cream_does_accept_job_submissions( Request *req );
int thread_cream_proxy_renew( Request *req );
int thread_cream_get_CEMon_url( Request *req );

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
	va_end(ap);
  
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
					  int(* handler)(Request *) )
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
			// Check whether CreamProxy likes the proxy (we only use
			// setCredential() for this - we DON'T want to execute())
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyDelegate("XXX", DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(cert);
		delete cp;
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

int thread_cream_delegate( Request *req )
{
	char *reqid, *delgservice, *delgid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &delgid );
	process_string_arg( req->input_line[3], &delgservice);

	try {
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyDelegate(delgid,
			                                           DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(delgservice);
		delete cp;
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Delegate\\ Error:\\ " + escape_spaces(ex.what());
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
	if ( count_args( input_line ) != 7 ) {
		HANDLE_SYNTAX_ERROR();
	}
	
	enqueue_request( input_line, thread_cream_job_register );

	gahp_printf("S\n");  

	return 0;
}

int thread_cream_job_register( Request *req )
{
	// FIXME: The new API does not allow for the proxy to be
	// delegated as part of the JobRegister operation. As such,
	// the delgservice parameter is no longer needed and delgid
	// must not be an empty string. The GridManager already obeys
	// this new model, but we should still modify the stubs in
	// gahp-client.C to reflect this change

	// FIXME: The new API returns a lot more information about
	// the job than just its ID and upload URL. Should we make
	// more of this available via our GAHP protocol?

	char *reqid, *service, *delgservice, *delgid, *jdl, *lease_id;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &delgservice );
	process_string_arg( req->input_line[4], &delgid );
	process_string_arg( req->input_line[5], &jdl );
	process_string_arg( req->input_line[6], &lease_id );
	
	AbsCreamProxy::RegisterArrayResult resp;
	try {
		JobDescriptionWrapper jdw(jdl,
		                          delgid,
		                          "",      // delegation proxy
		                          lease_id,// lease id
		                          false,   // autostart
		                          "JDI");  // job description ID
		AbsCreamProxy::RegisterArrayRequest reqs;
		reqs.push_back(&jdw);
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyRegister(&reqs,
			                                           &resp,
			                                           DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		if (resp["JDI"].get<0>() != JobIdWrapper::OK) {
			throw runtime_error(resp["JDI"].get<2>());
		}
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Register\\ Error:\\ "+ escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}

	map<string, string> props;
	resp["JDI"].get<1>().getProperties(props);
	result_line = (string)reqid +
	              " NULL " +
	              resp["JDI"].get<1>().getCreamJobID() +
	              sp +
	              props["CREAMInputSandboxURI"];
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

int thread_cream_job_start( Request *req )
{
	char *reqid, *service, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobid );
	
	try {
		JobIdWrapper jiw(jobid, service, vector<JobPropertyWrapper>());
		vector<JobIdWrapper> jv;
		jv.push_back(jiw);
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		ResultWrapper rw;
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyStart(&jfw,
			                                        &rw,
			                                        DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		check_result_wrapper(rw);
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Start\\ Error:\\ " + escape_spaces(ex.what());
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

int thread_cream_job_purge( Request *req )
{
	// FIXME: It doesn't appear that the new API supports
	// operating on all jobs. It also doesn't look like the
	// GridManager uses this capability. We should disable
	// the syntax for asking for operating on all jobs

	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		if (jobnum_str == NULL) {
			throw runtime_error("purge of all jobs not supported");
		}
		vector<JobIdWrapper> jv;
		int jobnum = atoi( jobnum_str );
		for ( int i = 0; i < jobnum; i++) {
			process_string_arg( req->input_line[i+4], &jobid );
			jv.push_back(JobIdWrapper(jobid,
			                          service,
			                          vector<JobPropertyWrapper>()));
		}
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		ResultWrapper rw;
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyPurge(&jfw,
			                                        &rw,
			                                        DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		check_result_wrapper(rw);
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Purge\\ Error:\\ " + escape_spaces(ex.what());
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

int thread_cream_job_cancel( Request *req )
{
	// FIXME: It doesn't appear that the new API supports
	// operating on all jobs. It also doesn't look like the
	// GridManager uses this capability. We should disable
	// the syntax for asking for operating on all jobs

	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		if (jobnum_str == NULL) {
			throw runtime_error("cancel of all jobs not supported");
		}
		vector<JobIdWrapper> jv;
		int jobnum = atoi( jobnum_str );
		for ( int i = 0; i < jobnum; i++) {
			process_string_arg( req->input_line[i+4], &jobid );
			jv.push_back(JobIdWrapper(jobid,
			                          service,
			                          vector<JobPropertyWrapper>()));
		}
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		ResultWrapper rw;
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyCancel(&jfw,
			                                         &rw,
			                                         DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		check_result_wrapper(rw);
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Cancel\\ Error:\\ " + escape_spaces(ex.what());
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

int thread_cream_job_suspend( Request *req )
{
	// FIXME: It doesn't appear that the new API supports
	// operating on all jobs. It also doesn't look like the
	// GridManager uses this capability. We should disable
	// the syntax for asking for operating on all jobs

	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		if (jobnum_str == NULL) {
			throw runtime_error("suspend of all jobs not supported");
		}
		vector<JobIdWrapper> jv;
		int jobnum = atoi( jobnum_str );
		for ( int i = 0; i < jobnum; i++) {
			process_string_arg( req->input_line[i+4], &jobid );
			jv.push_back(JobIdWrapper(jobid,
			                          service,
			                          vector<JobPropertyWrapper>()));
		}
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		ResultWrapper rw;
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxySuspend(&jfw,
			                                          &rw,
			                                          DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		check_result_wrapper(rw);
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Suspend\\ Error:\\ " + escape_spaces(ex.what());
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

int thread_cream_job_resume( Request *req )
{
	// FIXME: It doesn't appear that the new API supports
	// operating on all jobs. It also doesn't look like the
	// GridManager uses this capability. We should disable
	// the syntax for asking for operating on all jobs

	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	try {
		if (jobnum_str == NULL) {
			throw runtime_error("resume of all jobs not supported");
		}
		vector<JobIdWrapper> jv;
		int jobnum = atoi( jobnum_str );
		for ( int i = 0; i < jobnum; i++) {
			process_string_arg( req->input_line[i+4], &jobid );
			jv.push_back(JobIdWrapper(jobid,
			                          service,
			                          vector<JobPropertyWrapper>()));
		}
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		ResultWrapper rw;
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyResume(&jfw,
			                                         &rw,
			                                         DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		check_result_wrapper(rw);
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Resume\\ Error:\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	enqueue_result(result_line);
	
	return 0;
}


/* =========================================================================
   CREAM_SET_LEASE:
   ========================================================================= */

int handle_cream_set_lease( char **input_line )
{
	int arg_cnt = count_args( input_line );

	if ( arg_cnt != 5 ) {
		HANDLE_SYNTAX_ERROR();
	}

	enqueue_request( input_line, thread_cream_set_lease );

	gahp_printf("S\n");
	
	return 0;
}

int thread_cream_set_lease( Request *req )
{
	// FIXME: The new API does not appear to have an interface
	// for managing lease times. This command is currently
	// stubbed out: we always return success

	char *reqid, *service, *lease_id;
	int lease_expiry;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &lease_id );
	process_int_arg( req->input_line[4], &lease_expiry );
	
	try {
		pair<string, time_t> cmd_input;
		pair<string, time_t> cmd_output;
		if ( lease_id ) {
			cmd_input.first = lease_id;
		} else {
			cmd_input.first = "";
		}
		cmd_input.second = lease_expiry;
		AbsCreamProxy *cp =
			CreamProxyFactory::make_CreamProxyLease(cmd_input, &cmd_output,
													DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		lease_expiry = cmd_output.second;
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Set_Lease\\ Error:\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	char buf[100];
	sprintf( buf, "%d", lease_expiry );
	result_line = (string)reqid + " NULL " + buf;
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

int thread_cream_job_status( Request *req )
{
	// FIXME: The new API does not appear to have an interface
	// for managing lease times. This command is currently
	// stubbed out: we always return success

	char *reqid, *service, *jobnum_str, *jobid;
	string result_line, temp;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	AbsCreamProxy::StatusArrayResult sar;
	try {
		if (jobnum_str == NULL) {
			throw runtime_error("status of all jobs not supported");
		}
		vector<JobIdWrapper> jv;
		int jobnum = atoi( jobnum_str );
		for ( int i = 0; i < jobnum; i++) {
			process_string_arg( req->input_line[i+4], &jobid );
			jv.push_back(JobIdWrapper(jobid,
			                          service,
			                          vector<JobPropertyWrapper>()));
		}
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyStatus(&jfw,
			                                         &sar,
			                                         DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		for (AbsCreamProxy::StatusArrayResult::iterator i = sar.begin();
		     i != sar.end();
		     i++)
		{
			if (i->second.get<0>() != JobStatusWrapper::OK) {
				throw runtime_error(i->second.get<2>());
			}
		}
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Status\\ Error:\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);

		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	
	int cnt = 0;
	char *failure_reason = NULL;
	string exit_code;

	for (AbsCreamProxy::StatusArrayResult::iterator i = sar.begin();
	     i != sar.end();
	     i++)
	{
		if (i->second.get<1>().getExitCode().size() == 0) 
			exit_code = "NULL";
		else
			exit_code = i->second.get<1>().getExitCode();

		temp += sp +
		        i->second.get<1>().getCreamJobID() +
		        sp +
		        i->second.get<1>().getStatusName() +
		        sp +
		        exit_code;

		if (i->second.get<1>().getFailureReason().size() == 0) {
			temp += " NULL";
		}
		else {
			failure_reason =
				escape_spaces(i->second.get<1>().getFailureReason().c_str());
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

int thread_cream_job_info( Request *req )
{
	// FIXME: The new API does not appear to have an interface
	// for managing lease times. This command is currently
	// stubbed out: we always return success

	char *reqid, *service, *jobnum_str, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &jobnum_str );
	
	AbsCreamProxy::InfoArrayResult iar;
	try {
		if (jobnum_str == NULL) {
			throw runtime_error("info of all jobs not supported");
		}
		vector<JobIdWrapper> jv;
		int jobnum = atoi( jobnum_str );
		for ( int i = 0; i < jobnum; i++) {
			process_string_arg( req->input_line[i+4], &jobid );
			jv.push_back(JobIdWrapper(jobid,
			                          service,
			                          vector<JobPropertyWrapper>()));
		}
		vector<string> sv;
		JobFilterWrapper jfw(jv,
		                     sv,  // status contraint: none 
		                     -1,  // from date: none 
		                     -1,  // to date: none
		                     "",  // delegation ID constraint: none
		                     ""); // lease ID constraint: none
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyInfo(&jfw,
			                                       &iar,
			                                       DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
		for (AbsCreamProxy::InfoArrayResult::iterator i = iar.begin();
		     i != iar.end();
		     i++)
		{
			if (i->second.get<0>() != JobInfoWrapper::OK) {
				throw runtime_error(i->second.get<2>());
			}
		}
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Job_Info\\ Error:\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
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

int thread_cream_job_list( Request *req )
{
	char *reqid, *service;
	string result_line, temp;

	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );

	vector<JobIdWrapper> jv;
	try {
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyList(&jv,
			                                       DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
	}
	catch(std::exception& ex) {

		result_line = (string)reqid + " CREAM_Job_List\\ Error:\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);
				
		return 1;
	}
	
	result_line = (string)reqid + " NULL";
	
	int cnt = 0;
	for (vector<JobIdWrapper>::iterator i = jv.begin();
	     i != jv.end();
	     i++)
	{
		temp += sp + i->getCreamJobID();
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

int thread_cream_ping( Request *req )
{
	// FIXME: The new API does not have a ping command. We use
	// the "service info" command to detect whether CREAM is up

	char *reqid, *service;
	bool ret;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	
	ServiceInfoWrapper siw;
	try {
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyServiceInfo(&siw,
			                                              DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
	}
	catch(std::exception& ex) {
		
		enqueue_result((string)reqid + " " + escape_spaces(ex.what()));
		
		return 1;
	}
	
	enqueue_result((string)reqid + " NULL true"); // Service is available
	
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

int thread_cream_does_accept_job_submissions( Request *req )
{
	char *reqid, *service;
	bool ret;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );

	ServiceInfoWrapper siw;
	try {
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyServiceInfo(&siw,
			                                              DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
	}
	catch(std::exception& ex) {
		
		enqueue_result((string)reqid + " " + escape_spaces(ex.what()));
		
		return 1;
	}
	
	if (siw.getAcceptJobSubmission()) {
		enqueue_result((string)reqid + " NULL true");
	}
	else {
		enqueue_result((string)reqid + " NULL false");
	}
	
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

int thread_cream_proxy_renew( Request *req )
{
	// FIXME: In the new API, the CREAM URL is no longer needed -
	// only the URL to the delegation service. Also, a list of
	// job IDs is no longer needed. The renewal simply happens
	// for the given delegation ID.

	int jobnum;
	char *reqid, *service, *delgservice, *delgid, *jobid;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	process_string_arg( req->input_line[3], &delgservice );
	process_string_arg( req->input_line[4], &delgid );
	process_int_arg( req->input_line[5], &jobnum );
	
	try {
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxy_ProxyRenew(delgid,
			                                              DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(delgservice);
		delete cp;
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Proxy_Renew\\ Error:\\ " + escape_spaces(ex.what());
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

int thread_cream_get_CEMon_url( Request *req )
{
	char *reqid, *service;
	string result_line;
	
	process_string_arg( req->input_line[1], &reqid );
	process_string_arg( req->input_line[2], &service );
	
	ServiceInfoWrapper siw;
	try {
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyServiceInfo(&siw,
			                                              DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(req->proxy.c_str());
		cp->execute(service);
		delete cp;
	}
	catch(std::exception& ex) {
		
		result_line = (string)reqid + " CREAM_Get_CEMon_URL\\ Error:\\ " + escape_spaces(ex.what());
		enqueue_result(result_line);
		
		return 1;
	}
	
	result_line = (string)reqid + " NULL " + siw.getCEMonURL();
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
			// Check whether CreamProxy likes the proxy (we only use
			// setCredential() for this - we DON'T want to execute())
		AbsCreamProxy* cp =
			CreamProxyFactory::make_CreamProxyDelegate("XXX", DEFAULT_TIMEOUT);
		check_for_factory_error(cp);
		cp->setCredential(filename);
		delete cp;
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
		else HANDLE(cream_set_lease); 
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
	Request *req;

	while ( 1 ) {
		pthread_mutex_lock( &requestQueueLock );
		while ( requestQueue.empty() ) {
			pthread_cond_wait( &requestQueueEmpty, &requestQueueLock );
		}
		req = requestQueue.front();
		requestQueue.pop();
		pthread_mutex_unlock( &requestQueueLock );

		try {
			req->handler( req );
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
