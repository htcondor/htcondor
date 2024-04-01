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
#include "condor_attributes.h"
#include "my_popen.h"
#include "submit_utils.h"
#include "condor_version.h"
#include "my_username.h"
#include "../condor_utils/dagman_utils.h"

#include <sstream>

//
// Local DAGMan includes
//
#include "dagman_main.h"
#include "dag.h"
#include "dagman_submit.h"
#include "debug.h"
#include "tmp_dir.h"
#include "write_user_log.h"

namespace deep = DagmanDeepOptions;

typedef bool (* parse_submit_fnc)( const char *buffer, int &jobProcCount,
			int &cluster );

	// Get the event mask for the workflow/default log file.
const char *getEventMask();

//-------------------------------------------------------------------------
/** Parse output from condor_submit, determine the number of job procs
    and the cluster.
	@param buffer containing the line to be parsed
	@param integer to be filled in with the number of job procs generated
	       by the condor_submit
	@param integer to be filled in with the cluster ID
	@return true iff the line was correctly parsed
*/
static bool
parse_condor_submit( const char *buffer, int &jobProcCount, int &cluster )
{
  // The initial space is to make the sscanf match zero or more leading
  // whitespace that may exist in the buffer.
  if ( 2 != sscanf( buffer, " %d job(s) submitted to cluster %d",
  			&jobProcCount, &cluster) ) {
	debug_printf( DEBUG_QUIET, "ERROR: parse_condor_submit failed:\n\t%s\n",
				buffer );
    return false;
  }
  
  return true;
}

static bool
submit_try( ArgList &args, CondorID &condorID, bool prohibitMultiJobs )
{
  std::string cmd; // for debug output
  args.GetArgsStringForDisplay( cmd );

  FILE * fp = my_popen( args, "r", MY_POPEN_OPT_WANT_STDERR );
  if (fp == NULL) {
    debug_printf( DEBUG_NORMAL, 
		  "ERROR: my_popen(%s) in submit_try() failed!\n",
		  cmd.c_str() );
    return false;
  }
  
  //----------------------------------------------------------------------
  // Parse submit command output for a HTCondor job ID.  This
  // desperately needs to be replaced by HTCondor submit APIs.
  //
  // Typical condor_submit output for HTCondor v6 looks like:
  //
  //   Submitting job(s).
  //   Logging submit event(s).
  //   1 job(s) submitted to cluster 2267.
  //----------------------------------------------------------------------

  char buffer[UTIL_MAX_LINE_LENGTH];
  buffer[0] = '\0';

  	// Configure what we look for in the command output according to
	// which type of job we have.
  const char *marker = NULL;
  parse_submit_fnc parseFnc = NULL;

  marker = " submitted to cluster ";

  // Note: we *could* check how many jobs got submitted here, and
  // correlate that with how many submit events we see later on.
  // I'm not worrying about that for now...  wenger 2006-02-07.
  // We also have to check the number of jobs to get an accurate
  // count of submitted jobs to report in the dagman.out file.

  // We should also check whether we got more than one cluster, and
  // either deal with it correctly or generate an error message.
  parseFnc = parse_condor_submit;
  
  // Take all of the output (both stdout and stderr) from condor_submit,
  // and echo it to the dagman.out file.  Look for
  // the line (if any) containing the word "cluster" (HTCondor).
  // If we don't find such a line, something
  // went wrong with the submit, so we return false.  The caller of this
  // function can retry the submit by repeatedly calling this function.

  std::string command_output = "";
  std::string keyLine = "";
  while (fgets(buffer, UTIL_MAX_LINE_LENGTH, fp)) {
	std::string buf_line = buffer;
	chomp(buf_line);
	debug_printf(DEBUG_VERBOSE, "From submit: %s\n", buf_line.c_str());
	command_output += buf_line;
    if (strstr(buffer, marker) != NULL) {
	  keyLine = buf_line;
	}
  }

  { // Relocated this curly bracket to its previous position to hopefully
    // fix Coverity warning.  Not sure why these curly brackets are here
	// at all...  wenger 2013-06-12
    int status = my_pclose(fp) & 0xff;

    if (keyLine == "") {
      debug_printf(DEBUG_NORMAL, "failed while reading from pipe.\n");
      debug_printf(DEBUG_NORMAL, "Read so far: %s\n", command_output.c_str());
      return false;
    }

    if (status != 0) {
		debug_printf(DEBUG_NORMAL, "Read from pipe: %s\n", 
					 command_output.c_str());
		debug_printf( DEBUG_QUIET, "ERROR while running \"%s\": "
					  "my_pclose() failed with status %d (errno %d, %s)!\n",
					  cmd.c_str(), status, errno, strerror( errno ) );
		return false;
    }
  }

  int	jobProcCount;
  if ( !parseFnc( keyLine.c_str(), jobProcCount, condorID._cluster) ) {
		// We are going forward (do not return false here)
		// Expectation is that higher levels will catch that we
		// did not get a cluster initialized properly here, fail,
		// and write a rescue DAG. gt3658 2013-06-03
		//
		// This is better than the old failure that would submit
		// DAGMAN_MAX_SUBMIT_ATTEMPT copies of the same job.
      debug_printf( DEBUG_NORMAL, "WARNING: submit returned 0, but "
        "parsing submit output failed!\n" );
		// Returning here so we don't try to process invalid values
		// below.  (This should really return something like "submit failed
		// don't retry" -- see gittrac #3685.)
  	  return true;
  }

  	// Check for multiple job procs if configured to disallow that.
  if ( prohibitMultiJobs && (jobProcCount > 1) ) {
	debug_printf( DEBUG_NORMAL, "Submit generated %d job procs; "
				"disallowed by DAGMAN_PROHIBIT_MULTI_JOBS setting\n",
				jobProcCount );
	main_shutdown_rescue( EXIT_ERROR, DagStatus::DAG_STATUS_ERROR );
  }
  
  return true;
}

//-------------------------------------------------------------------------
// NOTE: this and submit_try should probably be merged into a single
// method -- submit_batch_job or something like that.  wenger/pfc 2006-04-05.
static bool
do_submit( ArgList &args, CondorID &condorID, bool prohibitMultiJobs )
{
	std::string cmd; // for debug output
	args.GetArgsStringForDisplay( cmd );
	debug_printf( DEBUG_VERBOSE, "submitting: %s\n", cmd.c_str() );
  
	bool success = false;

	success = submit_try( args, condorID, prohibitMultiJobs );

	if( !success ) {
	    debug_printf( DEBUG_QUIET, "ERROR: submit attempt failed\n" );
		debug_printf( DEBUG_QUIET, "submit command was: %s\n", cmd.c_str() );
	}

	return success;
}


//-------------------------------------------------------------------------
bool
condor_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName, const char *DAGParentNodeNames,
			   Job * node, int priority, int retry,
			   const char* directory, const char *workflowLogFile,
			   bool hold_claim, const std::string &batchName,
			   std::string &batchId )
{
	TmpDir		tmpDir;
	std::string	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.c_str() );
		return false;
	}
	bool success = false;

	ArgList args;

	// construct arguments to condor_submit to add attributes to the
	// job classad which identify the job's node name in the DAG, the
	// node names of its parents in the DAG, and the job ID of DAGMan
	// itself; then, define submit_event_notes to print the job's node
	// name inside the submit event in the userlog

	// NOTE: we specify the job ID of DAGMan using only its cluster ID
	// so that it may be referenced by jobs in their priority
	// attribute (which needs an int, not a string).  Doing so allows
	// users to effectively "batch" jobs by DAG so that when they
	// submit many DAGs to the same schedd, all the ready jobs from
	// one DAG complete before any jobs from another begin.

	args.AppendArg( dm.condorSubmitExe );

		//****Always append this variable to avoid overwriting
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	std::string nodeName = std::string(ATTR_DAG_NODE_NAME_ALT) + "=" + DAGNodeName;
	args.AppendArg( nodeName.c_str() );

		// append a line adding the parent DAGMan's cluster ID to the job ad
		//****Always append these variables to avoid overwriting
	if ( dm.DAGManJobId._cluster > 0 ) { 
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		std::string dagJobId = std::string( "My." ) + ATTR_DAGMAN_JOB_ID + "=" +
					std::to_string( dm.DAGManJobId._cluster );
		args.AppendArg( dagJobId.c_str() );

		// now we append a line setting the same thing as a submit-file macro
		// (this is necessary so the user can reference it in the priority)
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		std::string dagJobIdMacro = std::string( "" ) + ATTR_DAGMAN_JOB_ID + "=" +
					std::to_string( dm.DAGManJobId._cluster );
		args.AppendArg( dagJobIdMacro.c_str() );
	}

		// Pass the batch name to lower levels.
	if ( batchName != "" ) {
		args.AppendArg( "-batch-name" );
		args.AppendArg( batchName.c_str() );
	}
		// Pass the batch ID to lower levels.
	if ( batchId != "" ) {
		args.AppendArg( "-batch-id" );
		args.AppendArg( batchId.c_str() );
	}

		//****Always append this variable to avoid overwriting
	std::string submitEventNotes = std::string(
				"submit_event_notes = DAG Node: " ) + DAGNodeName;
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	args.AppendArg( submitEventNotes.c_str() );

	ASSERT( workflowLogFile );

		// We need to append the DAGman default log file to
		// the log file list
		//****Always append this variable to avoid overwriting
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	std::string dlog( "dagman_log=" );
	dlog += workflowLogFile;
	args.AppendArg( dlog.c_str() );
	debug_printf( DEBUG_VERBOSE, "Adding a DAGMan workflow log %s\n",
				workflowLogFile );

		// Now append the mask
		//****Always append this variable to avoid overwriting
	debug_printf( DEBUG_VERBOSE, "Masking the events recorded in the DAGMAN workflow log\n" );
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	std::string dmask("My.");
	dmask += ATTR_DAGMAN_WORKFLOW_MASK;
	dmask += "=\"";
	const char *eventMask = getEventMask();
	debug_printf( DEBUG_VERBOSE, "Mask for workflow log is %s\n",
				eventMask );
	dmask += eventMask;
	dmask += "\"";
	args.AppendArg( dmask.c_str() );

		// Append the priority, if we have one.
	if ( priority != 0 ) {
		std::string prioStr = "priority=";
		prioStr += std::to_string( priority );
		args.AppendArg( prioStr.c_str() );
	}


		// Suppress the job's log file if that option is enabled.
		//****Always append this variable to avoid overwriting
	if ( dm._suppressJobLogs ) {
		debug_printf( DEBUG_VERBOSE, "Suppressing node job log file\n" );
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		args.AppendArg( "log=" );
	}

		// If this is a hold job, add the attribute to submit on hold
	if ( node->GetHold() ) {
		debug_printf( DEBUG_VERBOSE, "Submitting node job on hold\n" );
		args.AppendArg( "hold=true" );
	}

		//****Always append this variable to avoid job submit failure:
	ArgList parentNameArgs;
	std::string parentNodeNames = std::string("My.DAGParentNodeNames=") + "\"";
	if (DAGParentNodeNames) parentNodeNames += DAGParentNodeNames;
	parentNodeNames += "\"";
	parentNameArgs.AppendArg( parentNodeNames.c_str() );

	// allow for $(JOB) expansions in the vars - and also in the submit file.
	std::string jobarg("JOB="); jobarg += DAGNodeName;
	args.AppendArg(jobarg.c_str());

	if (dm.jobInsertRetry && retry > 0) {
		std::string adRetryStr = "MY.DAGManNodeRetry=";
		adRetryStr += std::to_string(retry);
		args.AppendArg(adRetryStr.c_str());
	}

	for (auto & nodeVar : node->varsFromDag) {
		
			// Substitute the node retry count if necessary.  Note that
			// we can't do this in Job::ResolveVarsInterpolations()
			// because that's only called at parse time.
		std::string value = nodeVar._value;
		std::string retryStr = std::to_string( retry );
		replace_str( value, "$(RETRY)", retryStr.c_str() );
		std::string varStr(nodeVar._name);
		varStr += "=";
		varStr += value;

		if ( !nodeVar._prepend ) { args.AppendArg( "-a" ); }// Append var if prepend is false; -a == -append; using -a to save chars
		args.AppendArg( varStr.c_str() );
	}

		// Set the special DAG_STATUS variable (mainly for use by
		// "final" nodes).
	std::string var = "DAG_STATUS=";
	var += std::to_string( (int)dm.dag->_dagStatus );
	args.AppendArg( var.c_str() );

		// Set the special FAILED_COUNT variable (mainly for use by
		// "final" nodes).
	var = "FAILED_COUNT=";
	var += std::to_string( dm.dag->NumNodesFailed() );
	args.AppendArg( var.c_str() );

	if( hold_claim ){
		std::string holdit = std::string("My.") + ATTR_JOB_KEEP_CLAIM_IDLE + "="
			+ std::to_string( dm._claim_hold_time );
		args.AppendArg( holdit.c_str() );
	}
	
		//****Always append this variable to avoid overwriting
	if (dm.options[deep::b::SuppressNotification]) {
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		std::string notify = std::string("notification=never");
		args.AppendArg( notify.c_str() );
	}

		//
		// Add accounting group and user if we have them.
		//
	if ( ! dm.options[deep::str::AcctGroup].empty()) {
		std::string arg = "accounting_group=";
		arg += dm.options[deep::str::AcctGroup];
		args.AppendArg( arg );
	}

	if ( ! dm.options[deep::str::AcctGroupUser].empty()) {
		std::string arg = "accounting_group_user=";
		arg += dm.options[deep::str::AcctGroupUser];
		args.AppendArg( arg );
	}

	// Machine attrs to record in userlog and job ad
	if (!dm._requestedMachineAttrs.empty()) {
		std::string setLog = "job_ad_information_attrs=" + dm._ulogMachineAttrs;
		args.AppendArg(setLog);
		std::string setJobAd = "job_machine_attrs=" + dm._requestedMachineAttrs;
		args.AppendArg(setJobAd);
	}

		//
		// Add parents of this node to arguments, if we have room.
		//
		// This should be the last thing in the arguments, except
		// for the submit file name!!!
		//

		// how big is the command line so far?
	std::string display;
	args.GetArgsStringForDisplay( display );
	int cmdLineSize = display.length();

	parentNameArgs.GetArgsStringForDisplay( display );
	int DAGParentNodeNamesLen = display.length();
		// how many additional chars must we still add to command line
	        // NOTE: according to the POSIX spec, the args +
   	        // environ given to exec() cannot exceed
   	        // _POSIX_ARG_MAX, so we also need to calculate & add
   	        // the size of environ** to reserveNeeded
	int reserveNeeded = (int)strlen( cmdFile );
	int maxCmdLine = _POSIX_ARG_MAX;

		// if we don't have room for DAGParentNodeNames, leave it unset
	if( cmdLineSize + reserveNeeded + DAGParentNodeNamesLen > maxCmdLine ) {
		debug_printf( DEBUG_NORMAL, "Warning: node %s has too many parents "
					  "to list in its classad; leaving its DAGParentNodeNames "
					  "attribute undefined\n", DAGNodeName );
		check_warning_strictness( DAG_STRICT_3 );
	} else {
		args.AppendArgsFromArgList( parentNameArgs );
	}

	args.AppendArg( cmdFile );

	success = do_submit( args, condorID, dm.prohibitMultiJobs );

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.c_str() );
		success = false;
	}

	return success;
}

//////////////////////////////////////////////////////////////////////////////////
// direct to schedd condor submit
//////////////////////////////////////////////////////////////////////////////////

static void init_dag_vars(SubmitHash * submitHash,
	const Dagman &dm, Job* node,
	const char *workflowLogFile,
	const std::string & parents,
	const char *batchName,
	const char *batchId,
	bool before_subFile)
{
	const char* DAGNodeName = node->GetJobName();
	int priority = node->_effectivePriority;
	int retry = node->GetRetries();
	bool hold_claim = ( ! node->NoChildren()) && dm._claim_hold_time > 0;

	//These are added variables that should always be added after
	//digestion of the job submit file. init_dag_vars gets called
	//twice (before and after said digestion)
	if ( !before_subFile ) {
		// NOTE: we specify the job ID of DAGMan using only its cluster ID
		// so that it may be referenced by jobs in their priority
		// attribute (which needs an int, not a string).  Doing so allows
		// users to effectively "batch" jobs by DAG so that when they
		// submit many DAGs to the same schedd, all the ready jobs from
		// one DAG complete before any jobs from another begin.
		submitHash->set_arg_variable(ATTR_DAG_NODE_NAME_ALT, DAGNodeName);
		if (dm.DAGManJobId._cluster > 0) {
			submitHash->set_arg_variable(ATTR_DAGMAN_JOB_ID, std::to_string(dm.DAGManJobId._cluster).c_str());
		}
		
		// We need to append the DAGman default log file to the log file list
		submitHash->set_arg_variable(SUBMIT_KEY_DagmanLogFile, workflowLogFile);
		
		// Now add the event mask
		std::string workflowMask = "\"" + std::string(getEventMask()) + "\"";
		submitHash->set_arg_variable("MY." ATTR_DAGMAN_WORKFLOW_MASK, workflowMask.c_str());
		
		// Suppress the job's log file if that option is enabled.
		if (dm._suppressJobLogs) {
			debug_printf(DEBUG_VERBOSE, "Suppressing node job log file\n");
			submitHash->set_arg_variable(SUBMIT_KEY_UserLogFile, "");
		}
		
		if (dm.options[deep::b::SuppressNotification]) {
			submitHash->set_arg_variable(SUBMIT_KEY_Notification, "NEVER");
		}
		
		std::string submitEventNotes = std::string("DAG Node: ") + std::string(DAGNodeName);
		submitHash->set_arg_variable(SUBMIT_KEY_LogNotesCommand, submitEventNotes.c_str());
		
	} else { //All other variables always added to job that don't need to be appended
	
		if (batchName && batchName[0]) {
			submitHash->set_arg_variable(SUBMIT_KEY_BatchName, batchName);
		}
		if (batchId && batchId[0]) {
			submitHash->set_arg_variable(SUBMIT_KEY_BatchId, batchId);
		}

		// Append the priority, if we have one.
		if (priority != 0) {
			std::string prio = std::to_string(priority);
			submitHash->set_arg_variable(SUBMIT_KEY_Priority, prio.c_str());
		}

		// If this is a hold job, add the attribute to submit on hold
		if ( node->GetHold() ) {
			debug_printf( DEBUG_VERBOSE, "Submitting node job on hold\n" );
			submitHash->set_arg_variable(SUBMIT_KEY_Hold, "true");
		}

		// this allows for $(JOB) expansions in the vars (and in the submit file)
		submitHash->set_arg_variable("JOB", node->GetJobName());

		// set RETRY for $(RETRY) substitution
		submitHash->set_arg_variable("RETRY", std::to_string(retry).c_str());

		if (dm.jobInsertRetry && retry > 0) {
			submitHash->set_arg_variable("MY.DAGManNodeRetry", std::to_string(retry).c_str());
		}

		// Set the special DAG_STATUS variable (mainly for use by "final" nodes).
		submitHash->set_arg_variable("DAG_STATUS", std::to_string((int)dm.dag->_dagStatus).c_str());

		// Set the special FAILED_COUNT variable (mainly for use by "final" nodes).
		submitHash->set_arg_variable("FAILED_COUNT", std::to_string(dm.dag->NumNodesFailed()).c_str());

		if (hold_claim) {
			submitHash->set_arg_variable(SUBMIT_KEY_KeepClaimIdle, std::to_string(dm._claim_hold_time).c_str());
		}

		//
		// Add accounting group and user if we have them.
		//
		if (!dm.options[deep::str::AcctGroup].empty()) {
			submitHash->set_arg_variable(SUBMIT_KEY_AcctGroup, dm.options[deep::str::AcctGroup].c_str());
		}

		if (!dm.options[deep::str::AcctGroupUser].empty()) {
			submitHash->set_arg_variable(SUBMIT_KEY_AcctGroupUser, dm.options[deep::str::AcctGroupUser].c_str());
		}

		// Machine attrs to record in userlog and job ad
		if (!dm._requestedMachineAttrs.empty()) {
			submitHash->set_arg_variable("job_ad_information_attrs", dm._ulogMachineAttrs.c_str());
			submitHash->set_arg_variable("job_machine_attrs", dm._requestedMachineAttrs.c_str());
		}

		//PRAGMA_REMIND("TODO: fix the tests to use $(DAG_PARENT_NAMES), and then remove custom job attribute")
		submitHash->set_arg_variable("DAG_PARENT_NAMES", parents.c_str());
		// TODO: remove this when the tests no longer need it.
		submitHash->set_arg_variable("MY.DAGParentNodeNames", "\"$(DAG_PARENT_NAMES)\"");
	}

	//Init user defined VARS
	for (auto & it : node->varsFromDag) {
		if ( it._prepend == before_subFile ) { submitHash->set_arg_variable(it._name, it._value); }
	}
	
}

static bool send_jobset_if_allowed(SubmitHash& submitHash, int cluster)
{
	if ( ! param_boolean("USE_JOBSETS", false))
		return false;

	const ClassAd * jobsetAd = submitHash.getJOBSET();
	if (jobsetAd) {
		if (SendJobsetAd(cluster, *jobsetAd, 0) >= 0) {
			return true;
		}
	}
	return false;
}

//-------------------------------------------------------------------------
bool
direct_condor_submit(const Dagman &dm, Job* node,
	const char *workflowLogFile,
	const std::string & parents,
	const char *batchName,
	const char *batchId,
	CondorID& condorID)
{
	const char* cmdFile = node->GetCmdFile();

	// TODO: Have inline submits get digested here to allow for prepending of variables
	// Setup a SubmitHash object
	// If this was defined inline in the dag file, it's already been parsed, set the pointer
	// Otherwise we'll initialize and parse it in from the submit file later
	SubmitHash* submitHash = node->GetSubmitDesc();

	TmpDir		tmpDir;
	std::string	errMsg;
	const char* directory = node->GetDirectory();
	if (!tmpDir.Cd2TmpDir(directory, errMsg)) {
		debug_printf(DEBUG_QUIET,
			"Could not change to node directory %s: %s\n",
			directory, errMsg.c_str());
		return false;
	}
	int rval = 0;
	bool is_factory = param_boolean("SUBMIT_FACTORY_JOBS_BY_DEFAULT", false);
	bool success = false;
	std::string errmsg;
	Qmgr_connection * qmgr = NULL;
	auto_free_ptr owner(my_username());
	char * qline = NULL;
	const char * queue_args = NULL;
	MacroStreamFile ms;
	DCSchedd schedd;

	// If the submitDesc hash is not set, we need to parse it from the file
	if (!node->GetSubmitDesc()) {
		debug_printf(DEBUG_NORMAL, "Submitting node %s from file %s using direct job submission\n", node->GetJobName(), node->GetCmdFile());
		submitHash = new SubmitHash();
		// Start by populating the hash with some parameters
		submitHash->init(JSM_DAGMAN);
		submitHash->setDisableFileChecks(true);
		submitHash->setScheddVersion(CondorVersion());
		init_dag_vars(submitHash, dm, node, workflowLogFile, parents, batchName, batchId, true);

		// open the submit file
		if (! ms.open(cmdFile, false, submitHash->macros(), errmsg)) {
			debug_printf(DEBUG_QUIET, "ERROR: submit attempt failed, errno=%d %s\n", errno, strerror(errno));
			debug_printf(DEBUG_QUIET, "could not open submit file : %s - %s\n", cmdFile, errmsg.c_str());
			goto finis;
		}

		// set submit filename into the submit hash so that $(SUBMIT_FILE) works
		submitHash->insert_submit_filename(cmdFile, ms.source());

		// read the submit file until we get to the queue statement or end of file
		rval = submitHash->parse_up_to_q_line(ms, errmsg, &qline);
		if (rval) {
			goto finis;
		}

		if (qline) {
			queue_args = submitHash->is_queue_statement(qline);
		}
		if ( ! queue_args) {
			// submit file had no queue statement
			errmsg = "no QUEUE statement";
			rval = -1;
			goto finis;
		}
		// Check for invalid queue statements
		SubmitForeachArgs fea;
		if (submitHash->parse_q_args(queue_args, fea, errmsg) != 0) {
			errmsg = "Invalid queue statement (" + std::string(queue_args) + ")";
			rval = -1;
			goto finis;
		}
	}
	else {
		debug_printf(DEBUG_NORMAL, "Submitting node %s from inline description using direct job submission\n", node->GetJobName());
		submitHash = node->GetSubmitDesc();
		/*	If here then it is inline submission and a submit hash has been created already.
		*	Due to this, we need to call init_dag_vars with before_subfile = true to append
		*	all the normally prepended vars so they exist in the job ad. -Cole Bollig 2022-09-06
		*/
		init_dag_vars(submitHash, dm, node, workflowLogFile, parents, batchName, batchId, true);
	}

	// set submit keywords defined by dagman and VARS
	init_dag_vars(submitHash, dm, node, workflowLogFile, parents, batchName, batchId, false);

	submitHash->attachTransferMap(dm._protectedUrlMap);
	submitHash->init_base_ad(time(NULL), owner);

	qmgr = ConnectQ(schedd);
	if (qmgr) {
		int cluster_id = NewCluster();
		if (cluster_id <= 0) {
			errmsg = "failed to get a ClusterId";
			rval = cluster_id;
			goto finis;
		}

		int proc_id = 0, item_index = 0, step = 0;

		SubmitStepFromQArgs ssi(*submitHash);
		JOB_ID_KEY jid(cluster_id, proc_id);
		rval = ssi.begin(jid, queue_args);
		if (rval < 0) {
			goto finis;
		}

		rval = ssi.load_items(ms, false, errmsg);
		if (rval < 0) {
			goto finis;
		}

		while ((rval = ssi.next(jid, item_index, step)) > 0) {
			proc_id = NewProc(cluster_id);
			if (proc_id != jid.proc) {
				formatstr(errmsg, "expected next ProcId to be %d, but Schedd says %d", jid.proc, proc_id);
				rval = -1;
				goto finis;
			}
			// If this job has >1 procs, check if multi-proc jobs are prohibited
			if (proc_id >= 1 && dm.prohibitMultiJobs) {
				errmsg = "Submit generated multiple job procs; disallowed by DAGMAN_PROHIBIT_MULTI_JOBS setting";
				rval = -1;
				goto finis;
			}
			// DAGMan does not support multi-proc factory jobs when using direct submit
			if (proc_id >= 1 && is_factory) {
				errmsg = "Submit generated multiple job procs; disallowed when using factory jobs and DAGMan direct submission.";
				rval = -1;
				goto finis;
			}

			ClassAd *proc_ad = submitHash->make_job_ad(jid, item_index, step, false, false, NULL, NULL);
			if ( ! proc_ad) {
				errmsg = "failed to create job classad";
				rval = -1;
				goto finis;
			}

			if (rval == 2) { // we need to send the cluster ad
				classad::ClassAd * clusterad = proc_ad->GetChainedParentAd();
				if (clusterad) {
					send_jobset_if_allowed(*submitHash, cluster_id);
					rval = SendJobAttributes(JOB_ID_KEY(cluster_id, -1), *clusterad, SetAttribute_NoAck, submitHash->error_stack(), "Submit");
					if (rval < 0) {
						errmsg = "failed to send cluster classad";
						goto finis;
					}
				}
				condorID._cluster = jid.cluster;
				condorID._proc = jid.proc;
				condorID._subproc = 0;
			}

			rval = SendJobAttributes(jid, *proc_ad, SetAttribute_NoAck, submitHash->error_stack(), "Submit");
			if (rval < 0) {
				errmsg = "failed to send proc ad";
				goto finis;
			}
		}
		// commit transaction and disconnect queue
		CondorError errstack;
		success = DisconnectQ(qmgr, true, &errstack); qmgr = NULL;
		if (!success) {
			debug_printf(DEBUG_NORMAL, "Failed to submit job %s: %s\n", node->GetJobName(), errstack.getFullText().c_str());
		}
	}

finis:
	submitHash->detachTransferMap();
	if (qmgr) {
		// if qmanager object is still open, cancel any pending transaction and disconnnect it.
		DisconnectQ(qmgr, false); qmgr = NULL;
	}
	// report errors from submit
	//
	if (rval < 0) {
		debug_printf(DEBUG_QUIET, "ERROR: on Line %d of submit file: %s\n", ms.source().line, errmsg.c_str());
		if (submitHash->error_stack()) {
			std::string errstk(submitHash->error_stack()->getFullText());
			if (! errstk.empty()) {
				debug_printf(DEBUG_QUIET, "submit error: %s", errstk.c_str());
			}
			submitHash->error_stack()->clear();
		}
	}
	else {
		// If submit succeeded, we still need to log any warning messages
		if (submitHash->error_stack()) {
			submitHash->warn_unused(stderr, "DAGMAN");
			std::string errstk(submitHash->error_stack()->getFullText());
			if (!errstk.empty()) {
				debug_printf(DEBUG_QUIET, "Submit warning: %s", errstk.c_str());
			}
			submitHash->error_stack()->clear();
		}
	}

	if (!tmpDir.Cd2MainDir(errMsg)) {
		debug_printf(DEBUG_QUIET,
			"Could not change to original directory: %s\n",
			errMsg.c_str());
		success = false;
	}

	// If the submitHash was only allocated in this function (and not linked to the node) delete it now
	if (!node->GetSubmitDesc()) {
		delete submitHash;
	}

	return success;
}

bool send_reschedule(const Dagman & dm)
{
	if (!dm.useDirectSubmit)
		return true; // submit already did it

	DCSchedd schedd;
	Stream::stream_type st = schedd.hasUDPCommandPort() ? Stream::safe_sock : Stream::reli_sock;
	return schedd.sendCommand(RESCHEDULE, st, 0);
}


// Subproc ID for "fake" events (for NOOP jobs).
static int _subprocID = 0;

//-------------------------------------------------------------------------
void
set_fake_condorID( int subprocID )
{
	_subprocID = subprocID;
}

int
get_fake_condorID()
{
	return _subprocID;
}
//-------------------------------------------------------------------------
bool
fake_condor_submit( CondorID& condorID, Job* job, const char* DAGNodeName, 
			   const char* directory, const char *logFile )
{
	TmpDir		tmpDir;
	std::string	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.c_str() );
		return false;
	}

	_subprocID++;
		// Special HTCondorID for NOOP jobs -- actually indexed by
		// otherwise-unused subprocID.
	condorID._cluster = 0;
	condorID._proc = Job::NOOP_NODE_PROCID;
	condorID._subproc = _subprocID;

		// Make sure that this job gets marked as a NOOP 
	if( job ) {
		job->SetCondorID( condorID );
	}

	WriteUserLog ulog;
	ulog.setUseCLASSAD( 0 );
	ulog.initialize( logFile, condorID._cluster,
		condorID._proc, condorID._subproc );

	SubmitEvent subEvent;
	subEvent.cluster = condorID._cluster;
	subEvent.proc = condorID._proc;
	subEvent.subproc = condorID._subproc;

		// We need some value for submitHost for the event to be read
		// correctly.
	subEvent.setSubmitHost( "<dummy-submit-for-noop-job>" );

	subEvent.submitEventLogNotes = "DAG Node: ";
	subEvent.submitEventLogNotes += DAGNodeName;

	if ( !ulog.writeEvent( &subEvent ) ) {
		EXCEPT( "Error: writing dummy submit event for NOOP node failed!" );
		return false;
	}


	JobTerminatedEvent termEvent;
	termEvent.cluster = condorID._cluster;
	termEvent.proc = condorID._proc;
	termEvent.subproc = condorID._subproc;
	termEvent.normal = true;
	termEvent.returnValue = 0;
	termEvent.signalNumber = 0;

	if ( !ulog.writeEvent( &termEvent ) ) {
		EXCEPT( "Error: writing dummy terminated event for NOOP node failed!" );
		return false;
	}

	return true;
}

bool writePreSkipEvent( CondorID& condorID, Job* job, const char* DAGNodeName, 
			   const char* directory, const char *logFile )
{
	TmpDir tmpDir;
	std::string	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.c_str() );
		return false;
	}

		// Special HTCondorID for NOOP jobs -- actually indexed by
		// otherwise-unused subprocID.
	condorID._cluster = 0;
	condorID._proc = Job::NOOP_NODE_PROCID;

	condorID._subproc = 1+get_fake_condorID();
		// Increment this value
	set_fake_condorID(condorID._subproc);

	if( job ) {
		job->SetCondorID( condorID );
	}

	WriteUserLog ulog;
	ulog.setUseCLASSAD( 0 );
	ulog.initialize( logFile, condorID._cluster,
		condorID._proc, condorID._subproc );

	PreSkipEvent pEvent;
	pEvent.cluster = condorID._cluster;
	pEvent.proc = condorID._proc;
	pEvent.subproc = condorID._subproc;

	pEvent.skipEventLogNotes = "DAG Node: ";
	pEvent.skipEventLogNotes += DAGNodeName;

	if ( !ulog.writeEvent( &pEvent ) ) {
		EXCEPT( "Error: writing PRESKIP event failed!" );
		return false;
	}
	return true;
}

const char *
getEventMask()
{
	static std::string result("");
	static std::stringstream dmaskstrm("");

	if ( result == "" ) {
		//
		// IMPORTANT NOTE:  see all events that we deal with in
		// Dag::ProcessOneEvent() -- all of those need to be in the
		// event mask!! (wenger 2012-11-16)
		//
		int mask[] = {
			ULOG_SUBMIT,
			ULOG_EXECUTE,
			ULOG_EXECUTABLE_ERROR,
			ULOG_JOB_EVICTED,
			ULOG_JOB_TERMINATED,
			ULOG_SHADOW_EXCEPTION,
			ULOG_GENERIC,
			ULOG_JOB_ABORTED,
			ULOG_JOB_SUSPENDED,
			ULOG_JOB_UNSUSPENDED,
			ULOG_JOB_HELD,
			ULOG_JOB_RELEASED,
			ULOG_POST_SCRIPT_TERMINATED,
			ULOG_GLOBUS_SUBMIT,			// For Pegasus
			ULOG_JOB_RECONNECT_FAILED,
			ULOG_GRID_SUBMIT,			// For Pegasus
			ULOG_CLUSTER_SUBMIT,
			ULOG_CLUSTER_REMOVE,
			-1
		};

		for ( const int *p = &mask[0]; *p != -1; ++p ) {
			if ( p != &mask[0] ) {
				dmaskstrm << ",";
			}
			dmaskstrm << *p;
		}

		result = dmaskstrm.str();
	}

	return result.c_str();
}
