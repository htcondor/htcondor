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

/* ============================================================================
 * DaP_Server.C - Data Placement Scheduler Server
 * Reliable File Transfer through SRB-GsiFTP-NeST-SRM.. and other services
 * by Tevfik Kosar <kosart@cs.wisc.edu>
 * University of Wisconsin - Madison
 * February 2002 - ...
 * ==========================================================================*/

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "env.h"
#include "daemon.h"
#include "condor_config.h"
#include "internet.h"
#include "dap_server.h"
#include "dap_server_interface.h"
#include "dap_classad_reader.h"
#include "dap_logger.h"
#include "dap_utility.h"
#include "dap_error.h"
#include "dap_scheduler.h"
#include "stork-lm.h"

#ifndef WANT_CLASSAD_NAMESPACE
#define WANT_CLASSAD_NAMESPACE
#endif
#include "classad/classad_distribution.h"
#define DAP_CATALOG_NAMESPACE	"stork."
#define DYNAMIC_XFER_DEST_HOST	"$DYNAMIC"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * These are the default values for some global Stork parameters.
 * They will be overwritten by the values in the Stork Config file.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
unsigned long Max_num_jobs;       //max number of concurrent jobs running
unsigned long Max_retry;          //max number of times a job will be retried
unsigned long Max_delayInMinutes; //max time a job is allowed to finish
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

extern char *logfilename;
extern char *xmllogfilename;
extern char *userlogfilename;
MyString historyfilename;
char *Cred_tmp_dir = NULL;			// temporary credential storage directory
char *Module_dir = NULL;			// Module directory (DAP Catalog)
char *Log_dir = NULL;				// LOG directory
StorkLeaseManager					*LeaseManager = NULL;

int  transfer_dap_reaper_id, reserve_dap_reaper_id, release_dap_reaper_id;
int  requestpath_dap_reaper_id;

unsigned long last_dap = 0;  //changed only on new request

classad::ClassAdCollection      *dapcollection = NULL;
Scheduler dap_queue;

int listenfd_submit;
int listenfd_status;
int listenfd_remove;

// Timers
extern int IdleJobMonitorInterval;
extern int IdleJobMonitorTid;
extern int HungJobMonitorInterval;
extern int HungJobMonitorTid;
extern int RescheduledJobMonitorInterval;
extern int RescheduledJobMonitorTid;

// Module standard I/O file descriptor type and indices.
typedef int module_stdio_t[3];
#define MODULE_STDIN_INDEX		0
#define MODULE_STDOUT_INDEX		1
#define MODULE_STDERR_INDEX		2

// Prototypes
void clean_job_queue();
void remove_job(const char *dap_id);


/* ==========================================================================
 * read the config file and set some global parameters
 * ==========================================================================*/
int
read_config_file(void)
{
	//get value for Max_num_jobs
	Max_num_jobs =
		param_integer(
			"STORK_MAX_NUM_JOBS",		// name
			10,							// default
			1							// minimum value
		);
	dprintf(D_ALWAYS, "STORK_MAX_NUM_JOBS = %ld\n", Max_num_jobs);

	//get value for Max_retry
	Max_retry =
		param_integer(
			"STORK_MAX_RETRY",		// name
			10,						// default
			1						// minimum value
		);
	dprintf(D_ALWAYS, "STORK_MAX_RETRY = %ld\n", Max_retry);

	//get value for Max_delayInMinutes
	Max_delayInMinutes =
		param_integer(
			"STORK_MAXDELAY_INMINUTES",	// name
			10,						// default
			1						// minimum value
		);
	dprintf(D_ALWAYS, "STORK_MAXDELAY_INMINUTES = %ld\n", Max_delayInMinutes);

	//get value for STORK_TMP_CRED_DIR
	if (Cred_tmp_dir) free(Cred_tmp_dir);
	Cred_tmp_dir = param("STORK_TMP_CRED_DIR");
	if ( ! Cred_tmp_dir ) Cred_tmp_dir = strdup("/tmp");	// default
	dprintf(D_ALWAYS, "STORK_TMP_CRED_DIR = %s\n", Cred_tmp_dir);

	// get value for STORK_MODULE_DIR
	if (Module_dir) free(Module_dir);
	Module_dir = param("STORK_MODULE_DIR");
	if ( ! Module_dir ) {
		Module_dir = param("LIBEXEC");
		if ( ! Module_dir ) {
			dprintf (D_ALWAYS, "ERROR: STORK_MODULE_DIR not defined in config file!\n");
			DC_Exit (1);
		}
	}
	dprintf(D_ALWAYS, "STORK_MODULE_DIR = %s\n", Module_dir);

	// get value for LOG
	if (Log_dir) free(Log_dir);
	Log_dir = param("LOG");
	if ( ! Log_dir ) Log_dir = strdup("/tmp");	// default
	dprintf(D_ALWAYS, "modules will execute in LOG directory %s\n", Log_dir);

    // Preset the job queue clean timer only upon cold start, or if the timer
    // value has been changed.  If the timer period has not changed, leave the
    // timer alone.  This will avoid undesirable behavior whereby timer is
    // preset upon every reconfig, and job queue is not cleaned often enough.
#define CLEAN_Q_INTERVAL_COLDSTART	(-1)
#define CLEAN_Q_INTERVAL_DEFAULT	(60*60*24)
#define CLEAN_Q_INTERVAL_MIN		1

	static int old_job_q_clean_interval = CLEAN_Q_INTERVAL_MIN - 1;
	static int cleanid = -1;
	int job_q_clean_interval =
		param_integer(
				"STORK_QUEUE_CLEAN_INTERVAL",	// parameter name
				CLEAN_Q_INTERVAL_DEFAULT,
				CLEAN_Q_INTERVAL_MIN
		);

	if (job_q_clean_interval != old_job_q_clean_interval) {
		if (cleanid >= 0) {
			daemonCore->Cancel_Timer(cleanid);
		}
		cleanid =
			daemonCore->Register_Timer(
					job_q_clean_interval,	// deltawhen
					job_q_clean_interval,	// period
					clean_job_queue,	// event
					"clean_job_queue"		// description
			);

		old_job_q_clean_interval = job_q_clean_interval;
	}

	return TRUE;
}

/* ==========================================================================
 * open module stdio file descriptors
 * ==========================================================================*/
bool
open_module_stdio( classad::ClassAd *ad,
				   module_stdio_t    module_stdio)
{
	std::string path;
	int dap_id;
	priv_state priv;

	module_stdio[ MODULE_STDIN_INDEX ] = -1;
	module_stdio[ MODULE_STDOUT_INDEX ] = -1;
	module_stdio[ MODULE_STDERR_INDEX ] = -1;

	if (! ad->EvaluateAttrInt("dap_id", dap_id) ) {
		dprintf(D_ALWAYS, "open_module_stdio: error, no dap_id: %s\n",
				classad::CondorErrMsg.c_str() );
	}
	if ( ! ad->EvaluateAttrString("input", path) ) {
		path = NULL_FILE;
	}
	if ( path.empty() ) {
		dprintf(D_ALWAYS, "error: job %d empty input file path\n", dap_id);
		return false;
	}

	priv = set_user_priv();
	module_stdio[ MODULE_STDIN_INDEX ] =
		safe_open_wrapper( path.c_str(), O_RDONLY);
	set_priv( priv );

	if ( module_stdio[ MODULE_STDIN_INDEX ] < 0 ) {
		dprintf(D_ALWAYS, "error: job %d open input file %s: %s\n",
				dap_id, path.c_str(), strerror(errno) );
		return false;
	}

	if ( ! ad->EvaluateAttrString("output", path) ) {
		path = NULL_FILE;
	}
	if ( path.empty() ) {
		dprintf(D_ALWAYS, "error: job %d empty output file path\n",
				dap_id);
		return false;
	}

	priv = set_user_priv();
	module_stdio[ MODULE_STDOUT_INDEX ] =
		safe_open_wrapper( path.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
	set_priv( priv );

	if ( module_stdio[ MODULE_STDOUT_INDEX ] < 0 ) {
		dprintf(D_ALWAYS, "error: job %d open output file %s: %s\n",
				dap_id, path.c_str(), strerror(errno) );
		return false;
	}

	if ( ! ad->EvaluateAttrString("err", path) ) {
		path = NULL_FILE;
	}
	if ( path.empty() ) {
		dprintf(D_ALWAYS, "error: job %d empty error file path\n",
				dap_id);
		return false;
	}

	priv = set_user_priv();
	module_stdio[ MODULE_STDERR_INDEX ] =
		safe_open_wrapper( path.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0644);
	set_priv( priv );

	if ( module_stdio[ MODULE_STDERR_INDEX ] < 0 ) {
		dprintf(D_ALWAYS, "error: job %d open error file %s: %s\n",
				dap_id, path.c_str(), strerror(errno) );
		return false;
	}

	return true;
}

/* ==========================================================================
 * close module stdio file descriptors
 * ==========================================================================*/
void
close_module_stdio( module_stdio_t module_stdio)
{
	if ( module_stdio[ MODULE_STDIN_INDEX ] >= 0) {
		close( module_stdio[ MODULE_STDIN_INDEX ] );
		module_stdio[ MODULE_STDIN_INDEX ] = -1;
	}

	if ( module_stdio[ MODULE_STDOUT_INDEX ] >= 0) {
		close( module_stdio[ MODULE_STDOUT_INDEX ] );
		module_stdio[ MODULE_STDOUT_INDEX ] = -1;
	}

	if ( module_stdio[ MODULE_STDERR_INDEX ] >= 0) {
		close( module_stdio[ MODULE_STDERR_INDEX ] );
		module_stdio[ MODULE_STDERR_INDEX ] = -1;
	}

	return;
}

/* ============================================================================
 * check whether the status of a dap job is already logged or not
 * ==========================================================================*/
int
already_logged(char *dap_id, char *status)
{
	classad::ClassAd       *job_ad;
	char                    current_status[MAXSTR];
	std::string             key;
	char unstripped[MAXSTR];

	key = "key = ";
	key += dap_id;

	job_ad = dapcollection->GetClassAd(key);
	if (!job_ad) {
		return 0; //no such job!
	}
	else{
		getValue(job_ad, "status", unstripped);
		strncpy(current_status, strip_str(unstripped), MAXSTR);

		if (!strcmp(current_status, status)){
			return 1; //already logged
		}
		else {
			return 0; //not logged
		}
	}//else
}

/* ============================================================================
// Note: this function should really have a job ad passed to it.
 * ==========================================================================*/
bool
dynamicOK(classad::ClassAd *job_ad, time_t now)
{
	bool is_dynamic = false;
	bool ret_value;
	static time_t keep_giving_false_until = 0;


	std::string transfer_url;
	if	(	job_ad->EvaluateAttrString(
				"dest_url",
				transfer_url
			) && transfer_url.length() > 0)
	{
		if (strstr( transfer_url.c_str(), DYNAMIC_XFER_DEST_HOST) ) {
			is_dynamic = true;
		}
	}

	if (is_dynamic && (now < keep_giving_false_until)) {
		return false;
	}
	
	if (is_dynamic) {
		ret_value = LeaseManager->areMatchesAvailable();
	} else {
		ret_value = true;
	}

	if ( ret_value == false ) {
		// if the lease manager gave us nothing, don't bother it for another
		// several 2 minutes.
		keep_giving_false_until = now + 120;
	}

	return ret_value;
}

/* ============================================================================
 * get the last dap_id from the dap-jobs-to-process
 * ==========================================================================*/
void
get_last_dapid(void)
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	unsigned long           max_dapid = 0;
	char                    dap_id[MAXSTR];
	std::string             key;
	char unstripped[MAXSTR];

	query.Bind(dapcollection);
	query.Query("root", NULL);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) {
				break; //no matching classad
			}
			else{
				getValue(job_ad, "dap_id", unstripped);
				strncpy(dap_id, strip_str(unstripped), MAXSTR);
	
				if ((unsigned long)atol(dap_id) > max_dapid) {
					max_dapid = atol(dap_id);
				}
			}
		}while (query.Next(key));
	}

		//check also the history log file for max dapid
	ClassAd_Reader adreader(historyfilename.Value());
	while(1) {
		if ( !adreader.readAd()) {
			break;
		}
		adreader.getValue("dap_id", unstripped);
		strncpy(dap_id, strip_str(unstripped), MAXSTR);

		if ((unsigned long)atol(dap_id) > max_dapid) {
			max_dapid = atol(dap_id);
		}
	}
		//end of while

	last_dap = max_dapid;
}


/* ============================================================================
 * Remove a job from the queue.
 * ==========================================================================*/
void
remove_job(const char *dap_id)
{
	std::string                  key;
	key = "key = ";
	key += dap_id;
	dprintf(D_ALWAYS, "remove job %s from queue\n", dap_id);
	bool status = dapcollection->RemoveClassAd(key);
	if ( ! status ) {
		dprintf(D_ALWAYS, "Failed to remove job %s from queue\n", dap_id);
	}
	return;
}

/* ============================================================================
 * dap transfer function
 * ==========================================================================*/
bool
transfer_dap(char *dap_id,
			 char *src_url,
			 char *_dest_url,
			 const ArgList &arguments,
			 char * cred_file_name,
			 classad::ClassAd *job_ad)
{

    // FIXME: This is an awful hack.  Stork should pass around strings, not
    // classad expressions.
	std::string dest_url = strip_str( _dest_url );
	char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	int pid;
	char unstripped[MAXSTR];
	bool dynamic_xfer_dest = false;

	parse_url(src_url, src_protocol, src_host, src_file);
	parse_url(dest_url.c_str(), dest_protocol, dest_host, dest_file);

#if 0
dprintf(D_ALWAYS, "DEBUG: dest_url: '%s'\n", dest_url.c_str() );
dprintf(D_ALWAYS, "DEBUG: dest_protocol: '%s'\n", dest_protocol);
dprintf(D_ALWAYS, "DEBUG: dest_host: '%s'\n", dest_host);
dprintf(D_ALWAYS, "DEBUG: dest_file: '%s'\n", dest_file);
#endif

	strncpy(unstripped, src_url, MAXSTR-1 );
	strncpy(src_url, strip_str(unstripped), MAXSTR-1 );

	// For dynamic destinations ...
	// FIXME: This is an awful detection algorithm
	//if (! strcmp(dest_host, DYNAMIC_XFER_DEST_HOST) )
	if (strstr( _dest_url, DYNAMIC_XFER_DEST_HOST) ) {

		dynamic_xfer_dest = true;

		// See if this job is already associated with a dynamic destination
		// URL.
		std::string prev_dest_transfer_url;
		if	(	job_ad->EvaluateAttrString(
					"dest_transfer_url",
					prev_dest_transfer_url
				) && prev_dest_transfer_url.length() > 0)
		{
			// Re-use previous destination URL from lease manager.
			dest_url = prev_dest_transfer_url;

		} else {
			// This job needs a dynamic transfer URL from the Stork LeaseManager
			// Lite.
			std::string dest_transfer_url;
			const char *tmp =
				LeaseManager->getTransferDirectory(dest_protocol);
			if (tmp == NULL) {
				// No dynamic destination URLs are available for this protocol.
				write_collection_log(dapcollection, dap_id,
									 "status = \"request_rescheduled\";"
									 "error_code = \"no match found\";"
									 );
				dprintf(D_FULLDEBUG,
						"reschedule source URL %s: "
						"no dynamic destinations for protocol %s\n",
						src_url, dest_protocol);
				return false;	// no transfer
			} else {
				dest_transfer_url = tmp;

				// required by lease manager lite interface
				free( const_cast<char*>(tmp) );
			}
				
			// Save the dest_transfer_url in the job queue classad collection.
			std::string modify_s =  "dest_transfer_url = \"";
			modify_s += dest_transfer_url;
			modify_s += "\";";
			write_collection_log(dapcollection, dap_id, modify_s.c_str() );
			dprintf(D_FULLDEBUG, "matched source URL %s to dynamic URL %s\n",
					src_url, dest_transfer_url.c_str() );

			// Update destination URL to that from lease manager.
			dest_url = dest_transfer_url;
		}
	}


	//create a new process to transfer the files
	MyString	commandbody;
	ArgList		new_args;
	commandbody.sprintf( "%stransfer.%s-%s",
						 DAP_CATALOG_NAMESPACE, src_protocol, dest_protocol);
	new_args.AppendArg(commandbody);

	// "command" is the command is exec'ed by daemon core
	MyString	command;
	command.sprintf( "%s/%s", Module_dir, commandbody.Value() );

	// Append the src and dest URLs
	new_args.AppendArg( strip_str(src_url) );
	new_args.AppendArg( dest_url.c_str() );

	// Add the dynamic option
	if ( dynamic_xfer_dest ) {
		new_args.AppendArg( "-dynamic" );
	}

	// Finally, any other args
	new_args.AppendArgsFromArgList(arguments);


		// If using GSI proxy set up the environment to point to it
	Env myEnv;
	if (!cred_file_name) {
		cred_file_name = "";
	}

	dprintf (D_FULLDEBUG, "Using user credential %s\n", cred_file_name);
	MyString newenv_buff;


	// child process will need additional environments
	myEnv.SetEnv("STORK_JOBID", dap_id);
	myEnv.SetEnv("X509_USER_PROXY", cred_file_name);
	myEnv.SetEnv("X509_USER_KEY", cred_file_name);
	myEnv.SetEnv("X509_USER_CERT", cred_file_name);

	// This block is used only for writing the user log
	{
		MyString args_string;
		arguments.GetArgsStringForDisplay(&args_string);

		MyString src_url_value, dest_url_value;
		src_url_value.sprintf("\"%s\"", src_url);
		dest_url_value.sprintf("\"%s\"", dest_url.c_str() );

		write_xml_user_log(userlogfilename, "MyType", "\"GenericEvent\"",
						   "EventTypeNumber", "8",
						   "Cluster", dap_id,
						   "Proc", "-1",
						   "Subproc", "-1",
						   "Type", "transfer",
						   "SrcUrl", src_url_value.Value(),
						   "DestUrl", dest_url_value.Value(),
						   "Arguments", args_string.Value(),
						   "CredFile", cred_file_name);
	}

	// Open file descriptors for child module.
	module_stdio_t module_stdio;
	if (!  open_module_stdio( job_ad, module_stdio) ) {
		return false;
	}

	// Create child process via daemoncore
	pid =
		daemonCore->Create_Process(
			command.Value(),			// command path
			new_args,					// arguments
			PRIV_USER_FINAL,			// privilege state
			transfer_dap_reaper_id,		// reaper id
			FALSE,						// do not want a command port
			&myEnv,  		            // environment
			Log_dir,					// current working directory
			FALSE,						// do not create a new process group
			NULL,						// list of socks to inherit
			module_stdio				// child stdio file descriptors
		 								// nice increment = 0
		 			 					// job_opt_mask = 0
			);

	// Close module file descriptors in parent process.
	close_module_stdio( module_stdio);

	if (pid > 0) {
		dap_queue.insert(dap_id, pid);
		dprintf(D_ALWAYS,"GUC STARTED dapid=%s pid=%d src=%s dest=%s \n",
			dap_id,pid,src_url,dest_url.c_str());
		return true;
	}
	else{
		transfer_dap_reaper(NULL, 0 ,111); //executable not found!
		dprintf(D_ALWAYS,"ERROR: GUC fork failed dapid=%s src=%s dest=%s\n",
			dap_id,src_url,dest_url.c_str());
		return false;                  //--> Find a better soln!
	}

}

/* ============================================================================
 * dap reserve function
 * ==========================================================================*/
void
reserve_dap(char *dap_id, char *reserve_id, char *reserve_size,
			char *duration, char *dest_url, char *output_file)
{
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	char command[MAXSTR], commandbody[MAXSTR];
	int pid;
	ArgList args;

	(void) reserve_id;		// quiet warning

	parse_url(dest_url, dest_protocol, dest_host, dest_file);

		//dynamically create a shell script to execute
	snprintf(commandbody, MAXSTR, "%sreserve.%s",
		DAP_CATALOG_NAMESPACE, dest_protocol);

		//create a new process to transfer the files
	snprintf(command, MAXSTR, "%s/%s", Module_dir, commandbody);

	args.AppendArg(commandbody);
	args.AppendArg(dest_host);
	args.AppendArg(output_file);
	args.AppendArg(reserve_size);
	args.AppendArg(duration);

	// Open file descriptors for child module.
    classad::ClassAd                *job_ad;
	std::string key;
    key = "key = ";
    key += dap_id;
    job_ad = dapcollection->GetClassAd(key);
	module_stdio_t module_stdio;
	if (!  open_module_stdio( job_ad, module_stdio) ) {
		return; //  FIXME: must return failure! DAP_SUCCESS;
	}

	// Create child process via daemoncore
	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 args,							// args string
		 PRIV_USER_FINAL,				// privilege state
		 reserve_dap_reaper_id,			// reaper id
		 FALSE,							// do not want a command port
		 NULL,  		              	// colon seperated environment string
		 Log_dir,						// current working directory
		 NULL,							// do not register a process family
		 NULL,							// list of socks to inherit
		 module_stdio					// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	// Close module file descriptors in parent process.
	close_module_stdio( module_stdio);

	dap_queue.insert(dap_id, pid);
}

/* ============================================================================
 * dap release function
 * ==========================================================================*/
void
release_dap(char *dap_id, char *reserve_id, char *dest_url)
{
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];
	char command[MAXSTR], commandbody[MAXSTR];
	int pid;
	ArgList args;

	parse_url(dest_url, dest_protocol, dest_host, dest_file);

		//get the corresponding lot_id
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	char                    lot_id[MAXSTR] = "", current_reserve_id[MAXSTR];
	std::string             key, constraint;
	classad::ClassAdParser  parser;
	char unstripped[MAXSTR];

	constraint = "other.dap_type == \"reserve\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,
				"Error in parsing constraint: %s\n", constraint.c_str());
	}

	query.Bind(dapcollection);
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) {
				dprintf(D_ALWAYS,"No matching add!\n");
				return; //no matching classad
			}
			else{
				getValue(job_ad, "reserve_id", unstripped);
				strncpy(current_reserve_id, strip_str(unstripped), MAXSTR);
	
				if (!strcmp(current_reserve_id, reserve_id)) {
					getValue(job_ad, "lot_id", unstripped);
					strncpy(lot_id, strip_str(unstripped), MAXSTR);
				}
			}
		}while (query.Next(key));
	}

		//dynamically create a shell script to execute
	snprintf(commandbody, MAXSTR, "%srelease.%s",
		DAP_CATALOG_NAMESPACE, dest_protocol);

		//dynamically create a shell script to execute
	snprintf(command, MAXSTR, "%s/%s",  Module_dir, commandbody);

		//create a new process to transfer the files
	args.AppendArg(commandbody);
	args.AppendArg(dest_host);
	args.AppendArg(lot_id);

	// Open file descriptors for child module.
	module_stdio_t module_stdio;
	if (!  open_module_stdio( job_ad, module_stdio) ) {
		return; //  FIXME: must return failure! DAP_SUCCESS;
	}

	// Create child process via daemoncore
	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 args,							// args string
		 PRIV_USER_FINAL,				// privilege state
		 release_dap_reaper_id,			// reaper id
		 FALSE,							// do not want a command port
		 NULL, 			               	// colon seperated environment string
		 Log_dir,						// current working directory
		 NULL,							// do not register a process family
		 NULL,							// list of socks to inherit
		 module_stdio					// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	// Close module file descriptors in parent process.
	close_module_stdio( module_stdio);

	dap_queue.insert(dap_id, pid);
	if (constraint_tree != NULL) delete constraint_tree;
}

/* ============================================================================
 * dap reserve function
 * ==========================================================================*/
void
requestpath_dap(char *dap_id, char *src_url, char *dest_url)
{

	char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
	char dest_protocol[MAXSTR], dest_host[MAXSTR], dest_file[MAXSTR];

	char command[MAXSTR], commandbody[MAXSTR];
	int pid;
	ArgList args;

	parse_url(src_url, src_protocol, src_host, src_file);
	parse_url(dest_url, dest_protocol, dest_host, dest_file);

		//dynamically create a shell script to execute
	snprintf(commandbody, MAXSTR, "%srequestpath.%s",
		DAP_CATALOG_NAMESPACE, src_protocol);

		//create a new process to transfer the files
	snprintf(command, MAXSTR, "%s/%s", Module_dir, commandbody);

	args.AppendArg(commandbody);
	args.AppendArg(src_host);
	args.AppendArg(dest_host);

	// Open file descriptors for child module.
    classad::ClassAd                *job_ad;
	std::string key;
    key = "key = ";
    key += dap_id;
    job_ad = dapcollection->GetClassAd(key);
	module_stdio_t module_stdio;
	if (!  open_module_stdio( job_ad, module_stdio) ) {
		return; //  FIXME: must return failure! DAP_SUCCESS;
	}

	// Create child process via daemoncore
	pid =
	daemonCore->Create_Process(
		 command,						// command path
		 args,							// args string
		 PRIV_USER_FINAL,				// privilege state
		 requestpath_dap_reaper_id,		// reaper id
		 FALSE,							// do not want a command port
		 NULL,  		              	// colon seperated environment string
		 Log_dir,						// current working directory
		 NULL,							// do not register a process family
		 NULL,							// list of socks to inherit
		 module_stdio					// child stdio file descriptors
		 								// nice increment = 0
		 								// job_opt_mask = 0
	);

	// Close module file descriptors in parent process.
	close_module_stdio( module_stdio);

	dap_queue.insert(dap_id, pid);
}

/* ============================================================================
 * process the request last read..
 * - if the request is already logged as being in progress, don't log it again
 * ==========================================================================*/
void
process_request(classad::ClassAd *currentAd)
{
	char dap_id[MAXSTR], dap_type[MAXSTR];
	char unstripped[MAXSTR];
	bool status;

		//get the dap_id of the request
	getValue(currentAd, "dap_id", unstripped);
	strncpy(dap_id, strip_str(unstripped), MAXSTR);

		//log the new status of the request
	write_collection_log(dapcollection, dap_id,
						 "status = \"processing_request\"");

	user_log(currentAd, ULOG_EXECUTE);

	char lognotes[MAXSTR];
	getValue(currentAd, "LogNotes", lognotes);

	// FIXME delete this old userlog writer
	write_xml_user_log(userlogfilename, "MyType", "\"ExecuteEvent\"",
					   "EventTypeNumber", "1",
					   "Cluster", dap_id,
					   "Proc", "-1",
					   "Subproc", "-1",
					   "LogNotes", lognotes);


		//write_xml_log(xmllogfilename, currentAd, "\"processing_request\"");


		//get the dap_type
	getValue(currentAd, "dap_type", unstripped);
	strncpy(dap_type, strip_str(unstripped), MAXSTR);


		// Init user id for the right user
	std::string owner;
	std::string key = "owner";
	if (! currentAd->EvaluateAttrString(key, owner) || owner.empty() ) {
		dprintf (D_ALWAYS, "Unable to extract owner for job %s\n", dap_id);
		return;
	}
#ifdef WIN32
#error FIX init_user_ids( domain ) for Windows
#endif
	if ( ! init_user_ids(owner.c_str(), NULL) ) {
		dprintf (D_ALWAYS, "Unable to find local user for owner \"%s\"\n",
				owner.c_str() );
		return;
	}

		// Check for GSI proxy
	char * cred_file_name = NULL;
	key="cred_name";
	std::string cred_name;
	if (currentAd->EvaluateAttrString(key, cred_name)) {

			// If "cred_name" attribute is present, ask credd for the
			// credential We should do this each time, since credd may refresh
			// credentials, etc

		void * buff = NULL;
		int size = 0;
		if (get_cred_from_credd (cred_name.c_str(), buff, size)) {
			cred_file_name = get_credential_filename (dap_id);

				// Save to file

			// Create file as user
			//priv_state old_priv = set_root_priv();
			std::string job_owner;
			if	(	! currentAd->EvaluateAttrString("owner", job_owner) ||
					job_owner.empty()
				)
			{
				dprintf (D_ALWAYS, "Unable to extract owner for job %s\n",
						dap_id);
				return;
			}
			if ( ! init_user_ids( job_owner.c_str(), NULL) ) {
				dprintf (D_ALWAYS, "Unable to switch to owner %s for job %s\n",
						job_owner.c_str(), dap_id);
				return;
			}
			priv_state old_priv = set_user_priv();
			int fd = safe_open_wrapper(cred_file_name, O_WRONLY | O_CREAT, 0600 );
			if (fd > -1) {
				if ( fchmod (fd, S_IRUSR | S_IWUSR) < 0 ) {
					dprintf( D_ALWAYS,
							"%s:%d: cred file fchmod(%d,%#o): (%d)%s\n",
							__FILE__, __LINE__,
							fd, S_IRUSR | S_IWUSR,
							errno, strerror(errno)
					);
				}
				if ( fchown (fd, get_user_uid(), get_user_gid() ) < 0 ) {
					dprintf( D_ALWAYS,
							"%s:%d: cred file fchown(%d,%d,%d): (%d)%s\n",
							__FILE__, __LINE__,
							fd,  get_user_uid(), get_user_gid(),
							errno, strerror(errno)
					);
				}
			}
			set_priv(old_priv);

			if (fd > -1) {
				int nbytes = write (fd, buff, size);
				if ( nbytes < size ) {
					dprintf( D_ALWAYS,
							"%s:%d: cred short write: %d out of %d (%d)%s\n",
							__FILE__, __LINE__,
							nbytes, size,
							errno, strerror(errno)
					);
				}
				close (fd);
			}
			else {
				dprintf(
					D_ALWAYS,
					"Unable to create credential storage file %s !\n%d(%s)\n",
					cred_file_name,
					errno,
					strerror(errno)
				);
			}
			free (buff);
		} else {
			dprintf (D_ALWAYS,
					 "Unable to receive credential %s from CREDD\n",
					 cred_name.c_str());
		}
	}

		// If proxy was submitted directly to stork, use it
	if (cred_file_name == NULL) {
		std::string cred_file_name_str;
		if (currentAd->EvaluateAttrString("x509proxy", cred_file_name_str))
			cred_file_name=strdup(cred_file_name_str.c_str());
	}

		//if dap_type == transfer
	if (!strcmp(dap_type, "transfer")){

		char src_url[MAXSTR], dest_url[MAXSTR], arguments[MAXSTR];
		char use_protocol[MAXSTR], alt_protocols[MAXSTR];
		char src_alt_protocol[MAXSTR], dest_alt_protocol[MAXSTR];
		char next_protocol[MAXSTR];
		int protocol_num;
		ArgList args;

		getValue(currentAd, "src_url", src_url);
		getValue(currentAd, "dest_url", dest_url);

		//getValue(currentAd, "arguments", arguments);
		// awful, ugly hack to import arguments strings from ad, without
		// embedded double quotes.
		std::string arguments_string;
		currentAd->EvaluateAttrString("arguments", arguments_string);
		strncpy(arguments, arguments_string.c_str(), sizeof(arguments) );
		arguments[ sizeof(arguments) - 1 ] = '\0';	// ensure null termination

		MyString args_error;
		if(!args.AppendArgsV2Raw(arguments,&args_error)) {
			dprintf(D_ALWAYS,"Failed to read arguments: %s\n",
					args_error.Value());

			dapcollection->RemoveClassAd(dap_id);
			write_dap_log( historyfilename.Value(), "\"request_failed\"",
						   "dap_id",dap_id,
						   "error_code","\"bad arguments\"" );
			write_xml_user_log( userlogfilename,
								"MyType", "\"JobCompletedEvent\"",
								"EventTypeNumber", "5",
								"TerminatedNormally", "0",
								"ReturnValue", "1",
								"Cluster", dap_id,
								"Proc", "-1",
								"Subproc", "-1",
								"LogNotes", lognotes );
			remove_credential(dap_id);

			return;
		}

			//select which protocol to use
		getValue(currentAd, "use_protocol", use_protocol);
		protocol_num = atoi(use_protocol);

		if (!strcmp(use_protocol, "0")){
				//dprintf(D_ALWAYS, "use_protocol: %s\n", use_protocol);
			status =
				transfer_dap(dap_id, src_url, dest_url, args, cred_file_name,
							 currentAd);
			if (! status) {
				currentAd->InsertAttr("generic_event",
									  "server error: job removed");
				user_log(currentAd, ULOG_GENERIC);
				currentAd->InsertAttr("termination_type", "server_error");
				user_log(currentAd, ULOG_JOB_TERMINATED);
				write_dap_log(historyfilename.Value(), "\"request_removed\"",
							  "dap_id", dap_id, "error_code", "\"REMOVED!\"");
				remove_job( dap_id);
			}
		}
		else{
			getValue(currentAd, "alt_protocols", alt_protocols);
			dprintf(D_ALWAYS, "alt. protocols = %s\n", alt_protocols);

			if (!strcmp(alt_protocols,"")) { //if no alt. protocol defined
				status = transfer_dap(dap_id, src_url, dest_url, args,
									  cred_file_name, currentAd);
				if (! status) {
					currentAd->InsertAttr("generic_event",
							"server error: job removed");
					user_log(currentAd, ULOG_GENERIC);
					currentAd->InsertAttr("termination_type", "server_error");
					user_log(currentAd, ULOG_JOB_TERMINATED);
					write_dap_log(historyfilename.Value(), "\"request_removed\"",
						  "dap_id", dap_id, "error_code", "\"REMOVED!\"");
					remove_job( dap_id);
				}
			}
			else{ //use alt. protocols
				strcpy(next_protocol, strtok(alt_protocols, ",") );
	
				for(int i=1;i<protocol_num;i++){
					strcpy(next_protocol, strtok(NULL, ",") );
				}

				dprintf(D_FULLDEBUG, "next protocol = %s\n", next_protocol);
	
				if (strcmp(next_protocol,"")) {
					strcpy(src_alt_protocol, strtok(next_protocol, "-") );
					strcpy(dest_alt_protocol, strtok(NULL, "") );
				}
	
				char src_alt_url[MAXSTR], dest_alt_url[MAXSTR];
				char src_protocol[MAXSTR], src_host[MAXSTR], src_file[MAXSTR];
				char dest_protocol[MAXSTR],dest_host[MAXSTR],dest_file[MAXSTR];
	
				parse_url(src_url, src_protocol, src_host, src_file);
				parse_url(dest_url, dest_protocol, dest_host, dest_file);
	
				if (!strcmp(strip_str(src_alt_protocol), "file"))
					snprintf(src_alt_url, MAXSTR, "file:%s", src_file);
				else
					snprintf(src_alt_url, MAXSTR, "%s://%s/%s",
							 strip_str(src_alt_protocol), src_host, src_file);
	
				if (!strcmp(strip_str(dest_alt_protocol), "file"))
					snprintf(dest_alt_url, MAXSTR, "file:%s", dest_file);
				else
					snprintf(dest_alt_url, MAXSTR, "%s://%s/%s",
							 strip_str(dest_alt_protocol),dest_host,dest_file);
	
	
					//--> These "arguments" may need to be removed or chaged !!
				status = transfer_dap(dap_id, src_alt_url, dest_alt_url,
									  args, cred_file_name, currentAd);
				if (! status) {
					currentAd->InsertAttr("generic_event",
							"server error: job removed");
					user_log(currentAd, ULOG_GENERIC);
					currentAd->InsertAttr("termination_type", "server_error");
					user_log(currentAd, ULOG_JOB_TERMINATED);
					write_dap_log(historyfilename.Value(),
								  "\"request_removed\"",
								  "dap_id", dap_id,
								  "error_code", "\"REMOVED!\"");
					remove_job( dap_id);
				}
			}// end use alt. protocols
		}
	}

		//if dap_type == reserve
	else if(!strcmp(dap_type, "reserve")){

		char dest_host[MAXSTR], reserve_size[MAXSTR];
		char duration[MAXSTR], reserve_id[MAXSTR];
		char output_file[MAXSTR];

		getValue(currentAd, "dest_host", dest_host);
		strncpy(dest_host, strip_str(dest_host), MAXSTR);

		getValue(currentAd, "output_file", output_file);
		strncpy(output_file, strip_str(output_file), MAXSTR);

		getValue(currentAd, "reserve_size", reserve_size);
		strncpy(reserve_size, strip_str(reserve_size), MAXSTR);

		getValue(currentAd, "duration", duration);
		strncpy(duration, strip_str(duration), MAXSTR);

		getValue(currentAd, "reserve_id", reserve_id);
		strncpy(reserve_id, strip_str(reserve_id), MAXSTR);

		reserve_dap(dap_id, reserve_id, reserve_size, duration, dest_host, output_file);
	}

		//if dap_type == release
	else if(!strcmp(dap_type, "release")){

		char dest_host[MAXSTR], reserve_id[MAXSTR];

		getValue(currentAd, "dest_host", dest_host);
		strncpy(dest_host, strip_str(dest_host), MAXSTR);

		getValue(currentAd, "reserve_id", reserve_id);
		strncpy(reserve_id, strip_str(reserve_id), MAXSTR);

		release_dap(dap_id, reserve_id, dest_host);
	}

		//if dap_type == requestpath
	else if(!strcmp(dap_type, "requestpath")){

		char src_host[MAXSTR], dest_host[MAXSTR];

		getValue(currentAd, "src_host", src_host);
		strncpy(src_host, strip_str(src_host), MAXSTR);

		getValue(currentAd, "dest_host", dest_host);
		strncpy(dest_host, strip_str(dest_host), MAXSTR);

			//getValue(currentAd, "path_id", path_id);
			//strncpy(path_id, strip_str(path_id), MAXSTR);

		requestpath_dap(dap_id, src_host, dest_host);
	}

	if (cred_file_name)
		free (cred_file_name);

}


// SC2005 Hackery
#if SC2005_DEMO

// Following declarations and two functions are SC05 hackery
int low_water_reaper_id;
bool low_water_action_running = false;

int
low_water_reaper(Service *,int pid,int exit_status)
{
	dprintf(D_ALWAYS,
			"low water action process pid %d terminated with status %d\n",
			pid, exit_status);

	low_water_action_running = false;
	return DAP_SUCCESS;
}

// count total jobs in ClassAdCollection job queue, in all states.  Note that
// Scheduler::get_numjobs() only returns the count of running jobs.  For the
// purposes of the demo, it would be better to count the number of idle jobs.
// However, this would require performing a query, and walking the returned
// query list.  This is too slow for the purposes of the demo.
int
total_job_count(void)
{
	const classad::View *rootView = dapcollection->GetView("root");
	return rootView->Size() ;
}

void
low_water_timer(Service *)
{
	int low_water = param_integer("STORK_LOW_WATER_VALUE", 0, 0);

	if (low_water == 0) {
		low_water_action_running = false;	// force to known state
		return;
	}

	if (low_water_action_running) {
		return;
	}

	//int cur_jobs  = dap_queue.get_numjobs(); // running jobs only
	int cur_jobs  = total_job_count();
	dprintf(D_FULLDEBUG,
			"Running low water mark monitor.  Total jobs in queue: %d\n",
			cur_jobs);
	char *low_water_action = param("STORK_LOW_WATER_ACTION");

	if (low_water_action == NULL) {
		dprintf(D_ALWAYS,
				"STORK_LOW_WATER_VALUE is set to non-zero, "
				"but there is no STORK_LOW_WATER_ACTION\n");
		return;
	}

	if (cur_jobs < low_water) {
		dprintf(D_ALWAYS,
				"Number of jobs in queue (%d) "
				"is less than low-water mark (%d)\n",
				cur_jobs, low_water);
		dprintf(D_ALWAYS, "running %s\n", low_water_action);

		module_stdio_t		module_stdio;
		classad::ClassAd	ad;
		if (!  open_module_stdio( &ad, module_stdio) ) {
			return;
		}

		ArgList		args;
		args.AppendArg(low_water_action);
		int pid =
			daemonCore->Create_Process(
				low_water_action,		// command path
				args,					// args string
				PRIV_USER_FINAL,		// privilege state
				low_water_reaper_id,	// reaper id
				FALSE,					// do not want a command port
				NULL,  		            // colon seperated environment string
				Log_dir,				// current working directory
				FALSE,					// do not create a new process group
				NULL,					// list of socks to inherit
				module_stdio			// child stdio file descriptors
										// nice increment = 0
										// job_opt_mask = 0
				);
		close_module_stdio( module_stdio);

		if (pid != 0) {
			low_water_action_running = true;
		}
	}
	free(low_water_action);
}
#endif /*SC2005_DEMO (hackery for SC-2005) */


/* ============================================================================
 * regularly check for requests which are in the state of being processed but
 * not completed yet
 * ==========================================================================*/
void
regular_check_for_requests_in_process()
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;
	char   dap_id[MAXSTR];
	int timediff_inseconds, maxdelay_inseconds;
	int num_attempts = 0;
		
		//  printf("regular check for requests in process..\n");

	maxdelay_inseconds = Max_delayInMinutes * 60;

		//set the constraint for the query
	constraint = "other.status == \"processing_request\"";
		//constraint = "other.status == \"request_completed\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,
				"Error in parsing constraint: %s\n", constraint.c_str());
	}

	query.Bind(dapcollection);
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			} else {
				// determine elapsed run time for this job.
				classad::Value timediff_value;
				std::string time_expr = "int(currentTime() - timestamp)";
				if (! job_ad->EvaluateExpr(time_expr, timediff_value ) ) {
					dprintf(D_ALWAYS, "%s:%d EvaluateExpr() timediff failed\n",
							__FILE__, __LINE__);
					continue;
				}
				if (! timediff_value.IsIntegerValue(timediff_inseconds)) {
					dprintf(D_ALWAYS,"%s:%d IsIntegerValue() timediff failed\n",
							__FILE__, __LINE__);
					continue;
				}
	
				// if the elapsed time is greater than the maximum delay
				// allowed, then reprocess the request..
				if (timediff_inseconds > maxdelay_inseconds){
					getValue(job_ad, "dap_id", dap_id);
					dprintf(D_ALWAYS,
							"job id: %s may be hung.  "
							"Elapsed time: %d min, limit: %lu min .  "
							"Reprocessing ...\n",
							dap_id,
							timediff_inseconds/60,
							Max_delayInMinutes
					   );
	
					unsigned int pid;
					if (dap_queue.get_pid(pid, dap_id) == DAP_SUCCESS){

						if (dap_queue.remove(dap_id) == DAP_SUCCESS){
							dprintf(D_ALWAYS,
									"Killing process %d and removing dap %s\n",
									pid, dap_id);

							priv_state priv;
							priv = set_user_priv( );
							int kill_status = kill( pid, SIGKILL );
							set_priv( priv );

							if ( kill_status < 0 ) {
								dprintf( D_ALWAYS,
										"%s:%d:kill pid %d error: (%d)%s\n",
										__FILE__, __LINE__,
										pid,
										errno, strerror(errno)
								);
							}
							

                            // If this job has a dynamic transfer destination,
                            // return this to the lease manager.
                            std::string dest_transfer_url;
                            if  (   job_ad->EvaluateAttrString(
                                        "dest_transfer_url",
                                        dest_transfer_url
                                    ) && dest_transfer_url.length() > 0
                                )
                            {
                                dprintf(D_FULLDEBUG,
                                    "returning dynamic transfer destination"
                                    " %s to lease manager\n",
                                    dest_transfer_url.c_str() );
                                LeaseManager->returnTransferDestination(
                                        dest_transfer_url.c_str() );
                            }

							char attempts[MAXSTR], tempstr[MAXSTR];
							std::string modify_s = "";

								//get num_attempts
							if (getValue(job_ad, "num_attempts", attempts) != DAP_SUCCESS)
								num_attempts = 0;
							else
								num_attempts = atoi(attempts);
		
							dprintf(D_ALWAYS, "dap id: %s, attempt # %d\n", dap_id, num_attempts+2);
		
								//		string sstatus = "";
		
							if (num_attempts + 1 < (int)Max_retry){
									//		  sstatus = "\"request_rescheduled\"";
								modify_s += "status = \"request_rescheduled\";";
							}
		
							else{
									//		  sstatus = "\"request_failed\"";
								modify_s += "status = \"request_failed\";";
							}
		
							snprintf(tempstr, MAXSTR,
									 "num_attempts = %d;",num_attempts + 1);
		
							modify_s +=  tempstr;
							modify_s += "error_code = \"\"";
		
							write_collection_log(dapcollection, dap_id, modify_s.c_str());
		
								//when request is failed for good, remove it from the collection
								//and log it to the history log file..



							char lognotes[MAXSTR] ;
							getValue(job_ad, "LogNotes", lognotes);
		
							if (num_attempts + 1 >=  (int)Max_retry){

								remove_credential (dap_id);

								//write user log
								job_ad->InsertAttr("termination_type",
										"job_retry_limit");
								user_log(job_ad, ULOG_JOB_TERMINATED);


								dapcollection->RemoveClassAd(key);
								write_dap_log(historyfilename.Value(),
										"\"request_failed\"",
											  "dap_id", dap_id, "error_code", "\"ABORTED!\"");
		
								//write XML user logs
								// FIXME delete this old userlog writer
								char exit_status_str[MAXSTR];
								snprintf(exit_status_str, MAXSTR, "%d", 9);
								write_xml_user_log(userlogfilename, "MyType", "\"JobCompletedEvent\"",
												   "EventTypeNumber", "5",
												   "TerminatedNormally", "1",
												   "ReturnValue", exit_status_str,
												   "Cluster", dap_id,
												   "Proc", "-1",
												   "Subproc", "-1",
												   "LogNotes", lognotes);
		
								dprintf(D_ALWAYS, "Request aborted!\n");
								continue;
							}
		
						} // remove PID from dap_queue
						else{
							dprintf(D_ALWAYS, "Error in Removing process %d\n", pid);
						}
	
					}

					char use_protocol[MAXSTR];

					getValue(job_ad, "use_protocol", use_protocol);

					if (dap_queue.get_numjobs() < Max_num_jobs){
		
						if (num_attempts + 2 < (int)Max_retry){
							process_request(job_ad);
						}

					}

				} // hung job
			}//else
		}while (query.Next(key));
	}//if
	if (constraint_tree != NULL) delete constraint_tree;

	// Reset timer for this function, if period has changed.
	int period = param_integer(
		"STORK_HUNG_JOB_MONITOR",
		STORK_HUNG_JOB_MONITOR_DEFAULT,
		STORK_HUNG_JOB_MONITOR_MIN);

	if ( period == HungJobMonitorInterval && HungJobMonitorTid != -1 ) {
        // we are already done, since we already
        // have a timer set with the desired interval
        return;
    }

	if (HungJobMonitorTid != -1) {
		// destroy pre-existing timer
        daemonCore->Cancel_Timer(HungJobMonitorTid);
    }

	HungJobMonitorTid =
		daemonCore->Register_Timer(
			HungJobMonitorInterval,		// deltawhen
			HungJobMonitorInterval,		// period
			regular_check_for_requests_in_process,
			"check_for_requests_in_process");

	return;
}

/* ============================================================================
 * check at the startup for requests which are in the state of being processed
 * but not completed yet
 * ==========================================================================*/
void
startup_check_for_requests_in_process(void)
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;
	time_t	right_now = time(NULL);

		//set the constraint for the query
	constraint = "other.status == \"processing_request\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,"Error in parsing constraint!\n");
	}

	query.Bind(dapcollection);
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			}
			else{
				if ( (dap_queue.get_numjobs() < Max_num_jobs) &&
					 dynamicOK(job_ad,right_now) ) {
					process_request(job_ad);
				} else {
					break;
				}
			}
		}while (query.Next(key));
	}
	if (constraint_tree != NULL) delete constraint_tree;
}

/* ============================================================================
 * check for requests which are rescheduled for execution
 * ==========================================================================*/
void
regular_check_for_rescheduled_requests()
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;
	time_t right_now = time(NULL);

		//set the constraint for the query
	constraint = "other.status == \"request_rescheduled\"";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,"Error in parsing constraint!\n");
	}

	query.Bind(dapcollection);
	query.Query("root", constraint_tree);
	query.ToFirst();

	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
				break;
			}
			else{
				if ( (dap_queue.get_numjobs() < Max_num_jobs) &&
					 dynamicOK(job_ad,right_now) ) {
					process_request(job_ad);
				} else {
					break;
				}
			}
		}while (query.Next(key));
	}
	if (constraint_tree != NULL) delete constraint_tree;

	// Reset timer for this function, if period has changed.
	int period =
		param_integer(
			"STORK_RESCHEDULED_JOB_MONITOR",
			STORK_RESCHEDULED_JOB_MONITOR_DEFAULT,
			STORK_RESCHEDULED_JOB_MONITOR_MIN);

	if ( (period == RescheduledJobMonitorInterval) &&
		 (RescheduledJobMonitorTid != -1) ) {
        // we are already done, since we already
        // have a timer set with the desired interval
        return;
    }

	if (RescheduledJobMonitorTid != -1) {
		// destroy pre-existing timer
        daemonCore->Cancel_Timer(RescheduledJobMonitorTid);
    }

	RescheduledJobMonitorTid =
		daemonCore->Register_Timer(
			RescheduledJobMonitorInterval,		// deltawhen
			RescheduledJobMonitorInterval,		// period
			regular_check_for_rescheduled_requests,
			"regular_check_for_rescheduled_requests");

	return;
}

/* ============================================================================
 * initialize dapcollection
 * ==========================================================================*/
void
initialize_dapcollection(void)
{
	dapcollection = new classad::ClassAdCollection();

	if (dapcollection == NULL ){
		dprintf(D_ALWAYS,"Error: Failed to construct dap collection!\n");
	}

#if SC2005_DEMO
	if (param_boolean("STORK_JOB_QUEUE_PERSISTENCE", true)) {
		dprintf(D_ALWAYS,"using persistent job queue file %s\n",
				logfilename);
		if (!dapcollection->InitializeFromLog(logfilename)) {
			dprintf(D_ALWAYS,
					"Couldn't recover ClassAdCollection from log: %s!\n",
					logfilename);
		}
	} else {
		dprintf(D_ALWAYS,"NOT using persistent job queue file %s\n",
				logfilename);
	}
#endif /*SC2005_DEMO*/
}


/* ============================================================================
 * make some initializations before calling main()
 * ==========================================================================*/
int
initializations(void)
{
		//read the config file and set values for some global parameters
	if (!read_config_file()) {
		return FALSE;
	}

	// Module_dir must be a valid directory path.
	struct stat stat_buff;
	if (stat (Module_dir, &stat_buff) != 0) {
		dprintf (D_ALWAYS, "Invalid value for STORK_MODULE_DIR %s: %s\n",
				Module_dir,
				strerror(errno)
				);
		return FALSE;
	}
	if ( ! S_ISDIR(stat_buff.st_mode) ) {
		dprintf (D_ALWAYS, "STORK_MODULE_DIR %s is not a directory\n",
				Module_dir
				);
		return FALSE;
	}

		//initialize dapcollection
	initialize_dapcollection();

		// instantiate lease manager
	dprintf(D_FULLDEBUG, "Creating new lease manager object\n");
	LeaseManager = new StorkLeaseManager();
	dprintf(D_FULLDEBUG, "Done creating new lease manager object\n");
	dprintf(D_FULLDEBUG, "It's %p, size %d\n",
			LeaseManager, sizeof(*LeaseManager) );
	ASSERT(LeaseManager);

		//init history file name
	historyfilename.sprintf( "%s.history", logfilename);

		//init xml log file name
		//  sprintf(xmllogfilename, "%s.xml", logfilename);<-- change this

	if (!userlogfilename){
		userlogfilename = (char*) malloc(MAXSTR * sizeof(char));
		if ( userlogfilename ) {
			snprintf(userlogfilename, MAXSTR, "%s.user_log", logfilename);
		} else {
			dprintf( D_ALWAYS,
					"%s:%d: malloc userlogfilename %s.user_log: (%d)%s\n",
					__FILE__, __LINE__,
					logfilename,
					errno, strerror(errno)
			);
		}
	}

		//get the last dap_id from dap-jobs-to-process
	get_last_dapid();

		//check the logfile for noncompleted requests at startup
	startup_check_for_requests_in_process();

	return TRUE;
}

/* ============================================================================
 * Clean (compress) the job queue.
 * ==========================================================================*/
void
clean_job_queue()
{
	dprintf(D_ALWAYS, "Compressing job log %s\n", logfilename);
	dapcollection->TruncateLog();
}

/* ============================================================================
 * Return dynamic destination matches to lease manager.
 * ==========================================================================*/
void
returnDynamicMatches(void)
{
	classad::LocalCollectionQuery query;
	classad::ClassAd       *job_ad;
	classad::ClassAdParser  parser;
	std::string             key, constraint;

    // set the constraint for the query
	//constraint = "other.status == \"processing_request\"";
	constraint = "dest_transfer_url is defined";
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,
				"Error in parsing constraint: %s\n", constraint.c_str());
        return;
	}

	query.Bind(dapcollection);
	query.Query("root", constraint_tree);
	query.ToFirst();

    dprintf(D_FULLDEBUG,
            "returning dynamic transfer destinations to lease manager\n");
	if ( query.Current(key) ){
		do{
			job_ad = dapcollection->GetClassAd(key);
			if (!job_ad) { //no matching classad
                dprintf(D_ALWAYS, "%s:%d: no matching job ad for query %s",
                        __FILE__, __LINE__, constraint.c_str());
				break;
			}

            // If this job has a dynamic transfer destination, return this to
            // the lease manager.
            std::string dest_transfer_url;
            if  (   job_ad->EvaluateAttrString(
                        "dest_transfer_url",
                        dest_transfer_url
                    ) && dest_transfer_url.length() > 0
                )
            {
                dprintf(D_FULLDEBUG, "returning dynamic transfer destination "
                        "%s to lease manager\n", dest_transfer_url.c_str() );
                LeaseManager->returnTransferDestination(
                        dest_transfer_url.c_str() );
            }
		} while (query.Next(key));
    }

	if (constraint_tree != NULL) delete constraint_tree;
}

/* ============================================================================
 * Terminate lease manager interface
 * ==========================================================================*/
void
terminateLeaseManager(void)
{
	if ( LeaseManager ) {

        // Return all active matches from lease manager.
        returnDynamicMatches();

		dprintf(D_FULLDEBUG, "Deleting lease manager interface\n");
		delete LeaseManager;
		LeaseManager = NULL;
	}
}

/* ============================================================================
 * Terminate server.
 * ==========================================================================*/
int
terminate(terminate_t terminate_type)
{
	if ( LeaseManager ) {
		terminateLeaseManager();
    }

	if (terminate_type != TERMINATE_FAST) {
		// Compress the job queue upon exit.  This will enable a faster startup
		// using the same job queue.
		clean_job_queue();
	}

	if ( dapcollection ) {
		dprintf(D_FULLDEBUG, "Deleting RAM job queue\n");
		delete dapcollection;
		dapcollection = NULL;
	}

	return TRUE;
}

/* ============================================================================
 * main body of the condor_srb_reqex
 * ==========================================================================*/
void
call_main()
{

	classad::ClassAd       *job_ad;
	classad::LocalCollectionQuery query;
	classad::ClassAdParser       parser;
	std::string                  key, constraint;
	int period;
	time_t right_now = time(NULL);

	// Avoid query if possible.
	if (dap_queue.get_numjobs() < Max_num_jobs ) {

			//setup constraints for the query
		constraint = "other.status == \"request_received\"";
		classad::ExprTree *constraint_tree =
			parser.ParseExpression( constraint );

		if (!constraint_tree) {
			dprintf(D_ALWAYS, "Error in parsing constraint!\n");
		}

	
		query.Bind(dapcollection);
		query.Query("root", constraint_tree);
		query.ToFirst();

		if ( query.Current(key) ){
			do{
				job_ad = dapcollection->GetClassAd(key);
				if (!job_ad) { //no matching classad
					break;
				}
				else{
					if (dap_queue.get_numjobs() < Max_num_jobs && dynamicOK(job_ad,right_now))
						process_request(job_ad);
					else
						break;
				}
			}while (query.Next(key));
		}

		if (constraint_tree != NULL) delete constraint_tree;

	}

	// Reset timer for this function, if period has changed.
	period =
		param_integer(
			"STORK_IDLE_JOB_MONITOR",
			STORK_IDLE_JOB_MONITOR_DEFAULT,
			STORK_IDLE_JOB_MONITOR_MIN);

	if ( period == IdleJobMonitorInterval && IdleJobMonitorTid != -1 ) {
        // we are already done, since we already
        // have a timer set with the desired interval
        return;
    }

	if (IdleJobMonitorTid != -1) {
		// destroy pre-existing timer
        daemonCore->Cancel_Timer(IdleJobMonitorTid);
    }

	IdleJobMonitorTid =
		daemonCore->Register_Timer(
			IdleJobMonitorInterval,		// deltawhen
			IdleJobMonitorInterval,		// period
			call_main,	// event
			"call_main");				// description

	return;
}


/* ============================================================================
 * write incoming dap requests to file: <dap-jobs-to-process>
 * ==========================================================================*/
int
write_requests_to_file(ReliSock * sock)
{
		// Require authentication
    if (!sock->triedAuthentication()) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
    }



    sock->decode();

    MyString	buf;
    if (!sock->code(buf)) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: recv error (request)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

	if( buf.IsEmpty() ) {
		// This is the final "goodbye" from the client.
		// TODO: when transactional submits are supported,
		// this message should be treated as a "commit".
		return TRUE; // do not keep stream, client is done
		// TODO: Add transaction processing, so that either all of, or none of
		// the submit ads are added to the job queue.  The current
		// implementation can fail after a partial submit, and not inform the
		// user.
	}
    int cred_size;


    if (!sock->code(cred_size)) {
		dprintf(D_ALWAYS, "%s:%d Server: recv error (cred size)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }


    void*cred_buff = NULL;
    if (cred_size > 0) {
		cred_buff = malloc (cred_size);
		if (!sock->code_bytes(cred_buff, cred_size)) {
			dprintf(D_ALWAYS, "%s:%d Server: recv error (cred)!: (%d)%s\n",
					__FILE__, __LINE__,
					errno, strerror(errno)
					);
			return FALSE;
		}
    }

	if(!sock->eom()) {
		dprintf(D_ALWAYS, "%s:%d Server: failed to read eom!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno));
		return FALSE;
	}

		//convert request into a classad
    classad::ClassAdParser parser;
    classad::ClassAd *requestAd = NULL;
    classad::PrettyPrint unparser;
	std::string adbuffer = "";
    char last_dapstr[MAXSTR];
    classad::ExprTree *expr = NULL;

		//check the validity of the request
    requestAd = parser.ParseClassAd( buf.Value() );
    if (requestAd == NULL) {
		dprintf(D_ALWAYS, "Invalid input format! Not a valid classad!\n");
		return FALSE;
    }

	// Insert "owner" attribute
	std::string owner = sock->getOwner();
	if (! requestAd->InsertAttr("owner", owner) ) {
		// Must perform this error check for security reasons!
		dprintf(D_ALWAYS, "error inserting owner into job ad: %s\n",
				classad::CondorErrMsg.c_str() );
		return FALSE;
	}

	// Insert "remote_user" attribute
	requestAd->InsertAttr("remote_user", sock->getFullyQualifiedUser() );

	// Get remote submit host.
	std::string submit_host = sin_to_string(sock->peer_addr() );
	requestAd->InsertAttr("submit_host", submit_host);

	// Get this execute host.
	struct sockaddr_in sin;
	sock->my_addr(&sin);
	std::string execute_host = sin_to_string( &sin );
	requestAd->InsertAttr("execute_host", execute_host);

		//add the dap_id to the request
	unsigned long this_dap_id = last_dap + 1;
    snprintf(last_dapstr, MAXSTR, "%lu", this_dap_id);
    if ( !parser.ParseExpression(last_dapstr, expr) ) {
		dprintf(D_ALWAYS,"Parse error\n");
    }
    requestAd->Insert("dap_id", expr);
	requestAd->InsertAttr("cluster_id", (int)this_dap_id);
	requestAd->InsertAttr("proc_id", -1);

		//add the status to the request
    if ( !parser.ParseExpression("\"request_received\"", expr) ) {
		dprintf(D_ALWAYS,"Parse error\n");
    }
    requestAd->Insert("status", expr);
    requestAd->Insert("timestamp",classad::Literal::MakeAbsTime());


		//use the default protocol first
    if ( !parser.ParseExpression("0", expr) ) {
		dprintf(D_ALWAYS,"Parse error\n");
    }
    requestAd->Insert("use_protocol", expr);

    if (cred_buff) {
		char _dap_id_buff[10];
		sprintf (_dap_id_buff, "%ld", this_dap_id);
		char * cred_file_name = get_credential_filename (_dap_id_buff);

		// Switch to job user
		if (! init_user_ids(owner.c_str(), NULL) ) {
			dprintf(D_ALWAYS, "%s:%d init_user_ids(%s,NULL) failed\n",
					__FILE__, __LINE__, owner.c_str() );
			if (cred_buff) free(cred_buff);
			return FALSE;
		}
		priv_state old_priv = set_user_priv();
		int fd = safe_open_wrapper(cred_file_name, O_WRONLY | O_CREAT, 0600);
		if (fd < 0) {
			set_priv(old_priv);
			dprintf(D_ALWAYS,
					"error opening credential file %s for write: %s\n",
					cred_file_name, strerror(errno) );
			if (cred_buff) free(cred_buff);
			if (cred_file_name) free(cred_file_name);
			return FALSE;
		}
		int nbytes = full_write (fd, cred_buff, cred_size);
		if (cred_buff) free(cred_buff);
		if (nbytes != cred_size) {
			close(fd);
			unlink(cred_file_name);
			set_priv(old_priv);
			dprintf(D_ALWAYS,
					"error writing credential file %s: %s\n",
					cred_file_name, strerror(errno) );
			if (cred_file_name) free(cred_file_name);
			return FALSE;
		}

		close(fd);
		set_priv(old_priv);

		requestAd->InsertAttr("x509proxy", cred_file_name);
		if (cred_file_name) free (cred_file_name);
    } // if cred_buff

		//insert the request to the dap collection
    char keystr[MAXSTR];
    snprintf(keystr, MAXSTR, "key = %ld", this_dap_id);

		//add the rquest classad to dap collection
    dapcollection->AddClassAd(keystr, requestAd);

		//increment last dap_id
    last_dap ++;

    unparser.Unparse(adbuffer, requestAd);
    dprintf(D_ALWAYS, "New Request => %s\n",adbuffer.c_str());


		//---> XML Stuff
		//ClassAdXMLUnParser  xmlunparser;
		//string xmlstring;
		//xmlunparser.SetCompactSpacing(false);
		//xmlunparser.Unparse(xmlstring, requestAd);
		//printf("new Request in XML format=> %s\n",xmlstring.c_str());
		//write_xml_log(xmllogfilename, requestAd, "\"request_received\"");


	//write user log
	user_log(requestAd, ULOG_SUBMIT);

	// FIXME delete this old userlog writer
    char lognotes[MAXSTR];
    getValue(requestAd, "LogNotes", lognotes);
    write_xml_user_log(userlogfilename, "MyType", "\"SubmitEvent\"",
					   "EventTypeNumber", "0",
					   "Cluster", last_dapstr,
					   "Proc", "-1",
					   "Subproc", "-1",
					   "LogNotes", lognotes);




		//send response to the client
    char response[MAXSTR];

    snprintf(response, MAXSTR, "%s", last_dapstr);

    sock->encode();

    if ( !sock->put ( response )){
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }
    if ( !sock->eom() ) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error (eom))!: (%d)%s\n",
				__FILE__, __LINE__,
				 errno, strerror(errno));
		return FALSE;
	}

    return KEEP_STREAM;
}

int
list_queue(ReliSock * sock)
{
	std::string adbuffer = "";
	classad::ClassAdUnParser unparser;


		// Require authentication
	if (!sock->triedAuthentication()) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
	}

		// Determine the remote user of the request
	const char * remote_user = sock->getFullyQualifiedUser();

		// Create a query
	std::string constraint;
	constraint = "other.remote_user == ";
	constraint += "\"";
	constraint += remote_user;
	constraint += "\"";

	classad::ClassAdParser parser;
	classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
	if (!constraint_tree) {
		dprintf(D_ALWAYS,
				"Error in parsing constraint: %s\n", constraint.c_str());
		return FALSE;
	}

	classad::LocalCollectionQuery query;
	query.Bind(dapcollection);
	query.Query("root", constraint_tree);
	query.ToFirst();

		// Get the list of results
	StringList result_list;
	std::string key;
	if (query.Current (key)) {
		do {
			classad::ClassAd * job_ad = dapcollection->GetClassAd (key);
			if (job_ad) {
				std::string ad_buffer = "";
				unparser.Unparse(ad_buffer, job_ad);
				result_list.append (ad_buffer.c_str());
			}
		} while (query.Next(key));
	}
	if (constraint_tree != NULL) delete constraint_tree;

	int result_size = result_list.number();


		// Send back the results
	sock->encode();
	
	if (!(sock->code(result_size))) {
		return FALSE;
	}

	result_list.rewind();
	char * job_ad_str = NULL;

	while ((job_ad_str = result_list.next()) != NULL) {

		if (!sock->code (job_ad_str)) {
			return FALSE;
		}
	}

	sock->eom();
	return TRUE;
}

int
send_dap_status_to_client(ReliSock * sock)
{
	char nextdap[MAXSTR];
	char unstripped[MAXSTR];

		// Require authentication
	if (!sock->triedAuthentication()) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
	}

    sock->decode();

    char * dap_id = NULL;
    if (!sock->code (dap_id)) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: recv error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

    const classad::ClassAd                *job_ad;


	bool found_job = false;
	std::string adbuffer;
	classad::ClassAdUnParser unparser;

		// Try getting the job from the queue
	std::string key;
    key = "key = ";
    key += dap_id;
    job_ad = dapcollection->GetClassAd(key);


    if (job_ad) {
		found_job = true;
		unparser.Unparse (adbuffer, job_ad);
	} else {
		// If not, try getting it from the history file
		ClassAd_Reader adreader(historyfilename.Value());

		while(adreader.readAd()) {
			adreader.getValue("dap_id", unstripped);
			strncpy(nextdap, strip_str(unstripped), MAXSTR);

			if ( !strcmp(dap_id, nextdap) ){
				found_job = true;
				unparser.Unparse (adbuffer, adreader.getCurrentAd());
				break;
			}
		}
	}

	int rc = 1;


		//if the history for dap_id still not found:
    if (!found_job) {
		rc = 0;
		dprintf(D_ALWAYS,
				"Unable to get status for job with job id: %s\n",
				dap_id);
		adbuffer = "Couldn't find DaP job: ";
		adbuffer += dap_id;
    } else {
		rc = 1;
		dprintf(D_FULLDEBUG,
				"status report for the job with job id: %s ==>\n%s\n",
				dap_id,
				adbuffer.c_str());
	}
	if (dap_id) free (dap_id);

    sock->encode();

    char * pstr = strdup(adbuffer.c_str());
	if (pstr == NULL) {
		EXCEPT("Out of memory. Aborting.");
	}
    if (!(sock->code(rc) && sock->code (pstr))) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		if (pstr) free (pstr);
		return FALSE;
    }
    sock->eom();
	if (pstr) free (pstr);
    return TRUE;
}

/* ============================================================================
 * remove requests from queue (via DaemonCore)
 * ==========================================================================*/
int
remove_requests_from_queue(ReliSock * sock)
{
	std::string adbuffer = "";
	classad::ClassAdUnParser unparser;
	int rc = 0;
	
		// Require authentication
	if (!sock->triedAuthentication()) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, WRITE, &errstack) ) {
			dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
			return FALSE;
		}
	}


    char * dap_id = NULL;

    sock->decode();

    if (!sock->code (dap_id)) {
		dprintf( D_ALWAYS,
				"%s:%d: Server: recv error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }

    classad::ClassAd            *job_ad;
	std::string                  key;

		//search the collection for the classad
    key = "key = ";
    key += dap_id;

    job_ad = dapcollection->GetClassAd(key);
    if (job_ad) {

		unsigned int pid;

		if (dap_queue.get_pid(pid, dap_id) == DAP_SUCCESS){
			dprintf(D_ALWAYS, "Killing process: %d for job: %s\n", pid, dap_id);
			if ( kill(pid, SIGKILL) != 0 ) {
				dprintf(D_ALWAYS,
					"kill process %d for job %s error: %s\n",
					pid, dap_id, strerror(errno) );
			}
            // If this job has a dynamic transfer destination, return this to
            // the lease manager.
            std::string dest_transfer_url;
            if  ( job_ad->EvaluateAttrString(
					  "dest_transfer_url",
					  dest_transfer_url
                    ) && dest_transfer_url.length() > 0
                )
			{
                dprintf(D_FULLDEBUG, "returning dynamic transfer destination "
                        "%s to lease manager\n", dest_transfer_url.c_str() );
                LeaseManager->returnTransferDestination(
                        dest_transfer_url.c_str() );
            }
		}
		else{
			dprintf(D_ALWAYS, "Corresponding process for job: %s not found\n", dap_id);
		}


		if (dap_queue.remove(dap_id) == DAP_SUCCESS){
			// THIS IS A BUG.  dap_reaper() will attempt to retrieve the PID
			// for the job when it terminates, and fail.
			dprintf(D_ALWAYS, "Removed job: %s\n", dap_id);
		}
		else{
			dprintf(D_ALWAYS, "Error in removing job: %s from dap_queue\n", dap_id);
			dprintf(D_ALWAYS, "Still deleting job: %s from collection & logs\n", dap_id);
		}

		remove_credential (dap_id);

		std::string modify_s = "status = \"request_removed\"";
		write_collection_log(dapcollection, dap_id, modify_s.c_str());
	
		char lognotes[MAXSTR] ;
		getValue(job_ad, "LogNotes", lognotes);

		user_log(job_ad, ULOG_JOB_ABORTED);

		dapcollection->RemoveClassAd(key);

		write_dap_log(historyfilename.Value(), "\"request_removed\"",
					  "dap_id", dap_id, "error_code", "\"REMOVED!\"");

		// FIXME delete this old userlog writer
		write_xml_user_log(userlogfilename, "MyType", "\"JobAbortedEvent\"",
						   "EventTypeNumber", "9",
						   "TerminatedNormally", "0",
						   "Cluster", dap_id,
						   "Proc", "-1",
						   "Subproc", "-1",
						   "LogNotes", lognotes);


		adbuffer += "DaP job ";
		adbuffer += dap_id;
		adbuffer += " is removed from queue.";
		rc = 1;
    }//if
    else {
		dprintf(D_ALWAYS, "Couldn't find/remove job: %s\n", dap_id);
		adbuffer += "Couldn't find/remove DaP job: ";
		adbuffer += dap_id;
		rc = 0;
    }
	if (dap_id) free (dap_id);

		//send response to the client
    sock->encode();
    char * pstr = strdup(adbuffer.c_str());
	if (pstr == NULL) {
		EXCEPT("Out of memory. Aborting.");
	}
    if (!(sock->code(rc) && sock->code (pstr))) {
		if (pstr) free (pstr);
		dprintf( D_ALWAYS,
				"%s:%d: Server: send error)!: (%d)%s\n",
				__FILE__, __LINE__,
				errno, strerror(errno)
		);
		return FALSE;
    }
    if (pstr) free (pstr);

	return TRUE;
}

/* ============================================================================
 * shared dap reaper function
 * ==========================================================================*/
int
dap_reaper(std::string modify_s, int pid,int exit_status)
{
	char dap_id[MAXSTR], dap_type[MAXSTR], reserve_id[MAXSTR];
	char fname[MAXSTR], linebuf[MAXSTR]="", linebufstr[MAXSTR] = "\"";
	FILE *f;
	classad::ClassAd *job_ad;
	std::string key = "key = ";
	char unstripped[MAXSTR];

	dprintf(D_ALWAYS, "Process %d terminated with exit status %d \n",
			pid, exit_status);
	snprintf(fname, MAXSTR, "%s/out.%d", Log_dir, pid);

	if (dap_queue.get_dapid(pid, dap_id) != DAP_SUCCESS){
		dprintf(D_ALWAYS, "Process %d not in queue!\n", pid);
			//dap_queue.remove("0"); //just decrease num_jobs

		// THIS IS A BUG.  remove_requests_from_queue() removes the same entry
		// from the dap_queue when it removes the job.
		return DAP_ERROR;
	}

	dap_queue.remove(dap_id);

	key += dap_id;
	job_ad = dapcollection->GetClassAd(key);
	if (!job_ad) {
		return 0; //no such job!
	}

	getValue(job_ad, "dap_type", unstripped);
	strncpy(dap_type, strip_str(unstripped), MAXSTR);

	char lognotes[MAXSTR] ;
	getValue(job_ad, "LogNotes", lognotes);

		//---- if process completed succesfully ----
	if (exit_status == 0){

		job_ad->InsertAttr("exit_status", exit_status);
		job_ad->InsertAttr("termination_type", "exited");
		user_log(job_ad, ULOG_JOB_TERMINATED);

		remove_credential (dap_id);

		if (!strcmp(dap_type, "reserve")){
				//this part commented temporarily, required by nest_reserve
				/*
				  f = safe_fopen_wrapper(fname, "r");
				  fgets( linebuf, MAXSTR, f);

				  modify_s += "lot_id = ";
				  modify_s += linebuf;
				  modify_s += ";";
				*/
		} // dap_type == reserve

		modify_s += "status = \"request_completed\"";
		write_collection_log(dapcollection, dap_id, modify_s.c_str());

			//-- remove the corresponding reserve request from collection
		if (!strcmp(dap_type, "release")){

			getValue(job_ad, "reserve_id", reserve_id);
			strncpy(reserve_id, strip_str(reserve_id), MAXSTR);

			classad::LocalCollectionQuery query;
			classad::ClassAd       *job_ad2;
			char                    current_reserve_id[MAXSTR];
			char                    reserve_dap_id[MAXSTR];
			std::string             key2, constraint;
			classad::ClassAdParser  parser;

			constraint = "other.dap_type == \"reserve\"";
			classad::ExprTree *constraint_tree = parser.ParseExpression( constraint );
			if (!constraint_tree) {
				dprintf(D_ALWAYS, "Error in parsing constraint: %s\n", constraint.c_str());
			}

			query.Bind(dapcollection);
			query.Query("root", constraint_tree);
			query.ToFirst();

			if ( query.Current(key2) ){
				do{
					job_ad2 = dapcollection->GetClassAd(key2);
					if (!job_ad2) {
						dprintf(D_ALWAYS, "No matching add!\n");
						break; //no matching classad
					}
					else{
						getValue(job_ad2, "reserve_id", current_reserve_id);
						strncpy(current_reserve_id, strip_str(current_reserve_id), MAXSTR);
	
						if (!strcmp(current_reserve_id, reserve_id)) {
							getValue(job_ad2, "dap_id", reserve_dap_id);
							strncpy(reserve_dap_id, strip_str(reserve_dap_id), MAXSTR);
						}
					}
				}while (query.Next(key2));
			}
			if (constraint_tree != NULL) delete constraint_tree;

			key2 = "key = ";
			key2 += reserve_dap_id;
			dprintf(D_ALWAYS, "Reserve key: %s\n", reserve_dap_id);

			dapcollection->RemoveClassAd(key2);
		} // dap_type == release


		// If completing a transfer to a dynamic destination, return the
		// dynamic destination to the lease manager.
		if (!strcmp(dap_type, "transfer")){
			getValue(job_ad, "dest_transfer_url", unstripped);
			char dest_transfer_url[MAXSTR];
			strncpy(dest_transfer_url, strip_str(unstripped), MAXSTR);
			if (strlen(dest_transfer_url) > 0 ) {
				dprintf(D_FULLDEBUG, "successful transfer to dynamic URL %s, "
						"returning to lease manager\n", dest_transfer_url);
				LeaseManager->returnTransferDestination(dest_transfer_url);
				// Dynamic transfers to globus multi file xfers need to clean
				// up the rewritten multi file URL list.
				const char *dynamic_multi_file_xfer_file =
					job_filepath("", "urls", dap_id, pid);
				unlink( dynamic_multi_file_xfer_file );
			}
		}

			//when request is comleted, remove it from the collection and
			//log it to the history log file..
		if (!strcmp(dap_type, "reserve")){
				//this part commented temporarily, required by nest_reserve
				/*
				  fclose(f);
				  unlink(fname);
				*/
		}
		else{
			dapcollection->RemoveClassAd(key);
		}



		write_dap_log(historyfilename.Value(), "\"request_completed\"", "dap_id", dap_id);
			//    write_xml_log(xmllogfilename, job_ad, "\"request_completed\"");

		// FIXME delete this old userlog writer
		write_xml_user_log(userlogfilename, "MyType", "\"JobCompleteEvent\"",
						   "EventTypeNumber", "5",
						   "TerminatedNormally", "1",
						   "ReturnValue", "0",
						   "Cluster", dap_id,
						   "Proc", "-1",
						   "Subproc", "-1",
						   "LogNotes", lognotes);

	}

		//---- if process failed (exit_status != 0) ----
	else{
		int num_attempts;
		char attempts[MAXSTR], tempstr[MAXSTR];
		char use_protocol[MAXSTR], alt_protocols[MAXSTR];
		int protocol_num, max_protocols = 0;

			//get num_attempts
		if (getValue(job_ad, "num_attempts", attempts) != DAP_SUCCESS)
			num_attempts = 0;
		else
			num_attempts = atoi(attempts);

		dprintf(D_ALWAYS, "number of attempts = %d\n", num_attempts);


		if ( (f = safe_fopen_wrapper(fname, "r")) == NULL ){
			dprintf(
				D_ALWAYS,
				"%s:%d: No error details for pid %d(file %s): (%d)%s\n",
				__FILE__, __LINE__,
				pid, fname,
				errno, strerror(errno)
			);
		}
		else{
			fgets( linebuf, MAXSTR, f);
		}

			//    string sstatus = "";

		std::string next_action;
		if (num_attempts + 1 < (int)Max_retry){
				//sstatus = "\"request_rescheduled\"";
			modify_s += "status = \"request_rescheduled\";";
			next_action = "Rescheduling.";
		}

		else{
				//      sstatus = "\"request_failed\"";
			modify_s += "status = \"request_failed\";";
			next_action = "Max retry limit reached.";

			// If completing a transfer to a dynamic destination, notify the
			// lease manager that this destination failed.
			if ( !strcmp(dap_type, "transfer") ) {
				getValue(job_ad, "dest_transfer_url", unstripped);
				char dest_transfer_url[MAXSTR];
				strncpy(dest_transfer_url, strip_str(unstripped), MAXSTR);
				if (strlen(dest_transfer_url) > 0 ) {
					dprintf( D_FULLDEBUG,
							 "failed transfer to dynamic URL %s, "
							 "returning to lease manager\n",
							 dest_transfer_url );
					LeaseManager->failTransferDestination( dest_transfer_url );
				}
			}
		}

		MyString generic_event;
		if ( WIFSIGNALED( exit_status) ) {
			generic_event.sprintf(
				"Job attempt %d exited by signal %d",
				num_attempts+1, WTERMSIG( exit_status ) );
#ifdef WCOREDUMP
			if ( WCOREDUMP( exit_status) ) {
				MyString core;
				core.sprintf(
					"Core file for process %d in server directory %s",
					pid, Log_dir );
				job_ad->InsertAttr("generic_event", core.Value() );
				user_log(job_ad, ULOG_GENERIC);
			}
#endif // WCOREDUMP
		} else if ( WIFEXITED( exit_status) ) {
			generic_event.sprintf(
				"Job attempt %d exited with status %d",
				num_attempts+1, WEXITSTATUS( exit_status ) );
		} else {
			generic_event.sprintf(
				"job attempt %d terminated with unknown status %d",
				num_attempts+1, exit_status );
		}
		job_ad->InsertAttr("generic_event", generic_event.Value() );
		user_log(job_ad, ULOG_GENERIC);
		job_ad->InsertAttr("generic_event", next_action );
		user_log(job_ad, ULOG_GENERIC);

		snprintf(tempstr, MAXSTR,
				 "num_attempts = %d;",num_attempts + 1);

		modify_s +=  tempstr;
		modify_s += "error_code = \"";
		modify_s += linebuf;
		modify_s += "\";";

			//get which protocol to use
		getValue(job_ad, "use_protocol", use_protocol);
		protocol_num = atoi(use_protocol);

		getValue(job_ad, "alt_protocols", alt_protocols);

		if (strcmp(alt_protocols,"")) {
			strtok(alt_protocols, ",");
			max_protocols ++;

			while( strtok(NULL, ",") != NULL){
				max_protocols ++;
				dprintf(D_ALWAYS, "max protocols = %d\n", max_protocols);
			}
		}

			//set which protocol to use
		if (protocol_num < max_protocols){
			snprintf(tempstr, MAXSTR, "use_protocol = %d;",protocol_num + 1);
			modify_s +=  tempstr;
		}
		else{
			snprintf(tempstr, MAXSTR, "use_protocol = %d;",0);
			modify_s +=  tempstr;
		}

		write_collection_log(dapcollection, dap_id, modify_s.c_str());
			//    write_xml_log(xmllogfilename, job_ad, sstatus.c_str());

			//when request is failed for good, remove it from the collection and
			//log it to the history log file..
		if (num_attempts + 1 >=  (int)Max_retry){
			strncat(linebufstr, linebuf, MAXSTR-2);
			strcat(linebufstr, "\"");

			job_ad->InsertAttr("exit_status", exit_status);
			job_ad->InsertAttr("termination_type", "exited");
			user_log(job_ad, ULOG_JOB_TERMINATED);

			dapcollection->RemoveClassAd(key);

			write_dap_log(historyfilename.Value(), "\"request_failed\"",
						  "dap_id", dap_id, "error_code", linebufstr);

			char exit_status_str[MAXSTR];
			snprintf(exit_status_str, MAXSTR, "%d", exit_status);

			// FIXME delete this old userlog writer
			write_xml_user_log(userlogfilename, "MyType", "\"JobCompletedEvent\"",
							   "EventTypeNumber", "5",
							   "TerminatedNormally", "1",
							   "ReturnValue", exit_status_str,
							   "Cluster", dap_id,
							   "Proc", "-1",
							   "Subproc", "-1",
							   "LogNotes", lognotes);


			remove_credential (dap_id);
		}

		if (f != NULL){
			fclose(f);
			unlink(fname);
		}

	}//else
	return DAP_SUCCESS;
}

/* ============================================================================
 * transfer dap reaper function
 * ==========================================================================*/
int
transfer_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
}

/* ============================================================================
 * reserve dap reaper function
 * ==========================================================================*/
int
reserve_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);

}

/* ============================================================================
 * release dap reaper function
 * ==========================================================================*/
int
release_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
}

/* ============================================================================
 * requestpath dap reaper function
 * ==========================================================================*/
int
requestpath_dap_reaper(Service *,int pid,int exit_status)
{
	std::string modify_s = "";
	return dap_reaper(modify_s, pid, exit_status);
}

void
remove_credential (char * dap_id)
{
	char * filename = get_credential_filename (dap_id);

		// Switch to root
	priv_state old_priv = set_root_priv();

	struct stat stat_buff;

	if (stat ((const char*)filename, &stat_buff) == 0) {
		int rc = unlink (filename);

		set_priv(old_priv);

		dprintf (D_FULLDEBUG, "Removing credential %s (rc=%d)\n", filename, rc);
	} else {
		set_priv(old_priv);
	}

	if (filename)
		free (filename);
}

char *
get_credential_filename (char * dap_id)
{
	std::string buff;
	buff += Cred_tmp_dir;
	buff += "/";
	buff += "cred-";
	buff += dap_id;
	return strdup (buff.c_str());
}

int
get_cred_from_credd (const char * request, void *& buff, int & size)
{
	dprintf (D_FULLDEBUG, "Requesting credential %s\n", request);

	priv_state priv = set_user_priv();

	Daemon my_credd (DT_CREDD, NULL, NULL);

	Sock * socket =
		my_credd.startCommand (CREDD_GET_CRED, Stream::reli_sock, 0);
	ReliSock * sock = (ReliSock*)socket;

	if (!sock) {
		set_priv (priv);
		dprintf (D_ALWAYS,
			"%s:%d: Unable to start CREDD_GET_CRED command (%d)%s\n",
			__FILE__, __LINE__,
			errno, strerror(errno)
		);
		return FALSE;
	}

	
	// Authenticate
	if (!sock->triedAuthentication()) {
		CondorError errstack;
		if( ! SecMan::authenticate_sock(sock, CLIENT_PERM, &errstack) ) {
		  dprintf (D_ALWAYS, "Unable to authenticate, qutting\n");
		  return FALSE;
		}
	}

	sock->encode();

	char * _request=strdup(request);
	if (!sock->code (_request) ) {
		dprintf(
			D_ALWAYS,
			"%s:%d: put to credd:  (%d)%s\n",
			__FILE__, __LINE__,
			errno, strerror(errno)
		);
	}
	free (_request);

	if (!sock->eom() ) {
		dprintf(
			D_ALWAYS,
			"%s:%d: put eom to credd:  (%d)%s\n",
			__FILE__, __LINE__,
			errno, strerror(errno)
		);
	}

	sock->decode();

	int rc = 0;
	if (!sock->code (size)) {
		dprintf(
			D_ALWAYS,
			"%s:%d: get cred size %d of %d bytes from credd:  (%d)%s\n",
			__FILE__, __LINE__,
			size, sizeof( size),
			errno, strerror(errno)
		);
	} else {
		int nbytes;
		buff = malloc (size);
		nbytes = sock->code_bytes (buff, size);
		if ( nbytes == size ) {
			dprintf (D_FULLDEBUG, "Received credential\n");
			rc = 1;
		}
		else {
			dprintf (
				D_ALWAYS,
				"%s:%d: ERROR recv'ing credential %s, %d of %d bytes (%d)%s\n",
				__FILE__, __LINE__,
				request,
				nbytes, size,
				errno, strerror(errno)
			);
		}
	}

	sock->close();
	delete sock;
	set_priv (priv);
	return (rc==1)?TRUE:FALSE;
}

