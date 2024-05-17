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

// Get the event mask for the workflow/default log file.
const char *getEventMask();

struct NodeVar {
	NodeVar(std::string k, std::string v, bool a) : key(k), value(v), append(a) {};
	std::string key{};
	std::string value{};
	bool append{false};
};

// Check if node var key is in specified list of items to defer (True add to other structure) return True for deferred
static bool check_defer_var(std::vector<NodeVar>& deferred, const NodeVar& var, const std::set<std::string>& key_filter) {
	if (key_filter.contains(var.key)) {
		deferred.push_back(std::move(var));
		return true;
	}
	return false;
}

//Create a vector of variable keys and values to be added to the node job(s).
static std::vector<NodeVar> init_vars(const Dagman& dm, const Job& node) {
	std::vector<NodeVar> vars;

	const char* nodeName = node.GetJobName();
	int retry = node.GetRetries();
	std::string parents, batchName, batchId;

	if ( ! node.NoParents()) {
		parents.reserve(2048);
		node.PrintParents(parents, 2000, dm.dag, ",");
	}

	if (!node.GetDagFile() && dm.options[deep::str::BatchName] == " ") {
		batchName = "";
	} else {
		batchName = dm.options[deep::str::BatchName];
	}
	if (!node.GetDagFile() && dm.options[deep::str::BatchId] == " ") {
		batchId = "";
	} else {
		batchId = dm.options[deep::str::BatchId];
	}

	vars.push_back(NodeVar("JOB", nodeName, false));
	vars.push_back(NodeVar("RETRY", std::to_string(retry), false));

	vars.push_back(NodeVar("DAG_STATUS", std::to_string((int)dm.dag->_dagStatus), false));
	vars.push_back(NodeVar("FAILED_COUNT", std::to_string(dm.dag->NumNodesFailed()), false));

	vars.push_back(NodeVar("DAG_PARENT_NAMES", parents, false));
	vars.push_back(NodeVar("MY.DAGParentNodeNames", "\"$(DAG_PARENT_NAMES)\"", false));

	if ( ! batchName.empty()) {
		vars.push_back(NodeVar(SUBMIT_KEY_BatchName, batchName, false));
	}
	if ( ! batchId.empty()) {
		vars.push_back(NodeVar(SUBMIT_KEY_BatchId, batchId, false));
	}

	if (dm.jobInsertRetry && retry > 0) {
		vars.push_back(NodeVar("MY.DAGManNodeRetry", std::to_string(retry), false));
	}

	if (node._effectivePriority != 0) {
		vars.push_back(NodeVar(SUBMIT_KEY_Priority, std::to_string(node._effectivePriority), false));
	}

	if (node.GetHold()) {
		debug_printf(DEBUG_VERBOSE, "Submitting node %s job on hold\n", nodeName);
		vars.push_back(NodeVar(SUBMIT_KEY_Hold, "true", false));
	}

	if (!node.NoChildren() && dm._claim_hold_time > 0) {
		vars.push_back(NodeVar(SUBMIT_KEY_KeepClaimIdle, std::to_string(dm._claim_hold_time), false));
	}

	if (!dm.options[deep::str::AcctGroup].empty()) {
		vars.push_back(NodeVar(SUBMIT_KEY_AcctGroup, dm.options[deep::str::AcctGroup], false));
	}

	if (!dm.options[deep::str::AcctGroupUser].empty()) {
		vars.push_back(NodeVar(SUBMIT_KEY_AcctGroupUser, dm.options[deep::str::AcctGroupUser], false));
	}

	if (!dm._requestedMachineAttrs.empty()) {
		vars.push_back(NodeVar(SUBMIT_KEY_JobAdInformationAttrs, dm._ulogMachineAttrs, false));
		vars.push_back(NodeVar(SUBMIT_KEY_JobMachineAttrs, dm._requestedMachineAttrs, false));
	}

	vars.push_back(NodeVar(ATTR_DAG_NODE_NAME_ALT, nodeName, true));
	vars.push_back(NodeVar(SUBMIT_KEY_LogNotesCommand, std::string("DAG Node: ") + nodeName, true));
	vars.push_back(NodeVar(SUBMIT_KEY_DagmanLogFile, dm._defaultNodeLog, true));
	vars.push_back(NodeVar("My." ATTR_DAGMAN_WORKFLOW_MASK, std::string("\"") + getEventMask() + "\"", true));

	if (dm.DAGManJobId._cluster > 0) {
		vars.push_back(NodeVar("My." ATTR_DAGMAN_JOB_ID, std::to_string(dm.DAGManJobId._cluster), true));
		vars.push_back(NodeVar(ATTR_DAGMAN_JOB_ID, std::to_string(dm.DAGManJobId._cluster), true));
	}

	if (dm._suppressJobLogs) {
		vars.push_back(NodeVar(SUBMIT_KEY_UserLogFile, "", true));
	}

	if (dm.options[deep::b::SuppressNotification]) {
		vars.push_back(NodeVar(SUBMIT_KEY_Notification, "NEVER", true));
	}

	for (auto &dagVar : node.varsFromDag) {
		vars.push_back(NodeVar(dagVar._name, dagVar._value, !dagVar._prepend));
	}

	return vars;
}

// data {key,value,append}
// static vector<data> init_node_vars
// shell_submit
// direct_submit
// dag_node_submit

typedef bool (* parse_submit_fnc)( const char *buffer, int &jobProcCount,
			int &cluster );

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
condor_submit(const Dagman &dm, Job* node, CondorID& condorID)
{
	const char* directory = node->GetDirectory();
	const char* cmdFile = node->GetCmdFile();
	TmpDir		tmpDir;
	std::string	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.c_str() );
		return false;
	}
	bool success = false;

	auto vars = init_vars(dm, *node);

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

	std::set<std::string> defer_list = {"DAG_PARENT_NAMES", "MY.DAGParentNodeNames"};
	std::vector<NodeVar> deferred;
	for (const auto& var : vars) {
		if (check_defer_var(deferred, var, defer_list)) { continue; }
		if (var.append) { args.AppendArg("-a"); }
		std::string cmd = var.key + "=" + var.value;
		args.AppendArg(cmd);
	}

	ArgList extraArgs;
	for (const auto& var : deferred) {
		if (var.append) { extraArgs.AppendArg("-a"); }
		std::string cmd = var.key + "=" + var.value;
		args.AppendArg(cmd);
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

	extraArgs.GetArgsStringForDisplay( display );
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
					  "attribute undefined\n", node->GetJobName());
		check_warning_strictness( DAG_STRICT_3 );
	} else {
		args.AppendArgsFromArgList( extraArgs );
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
direct_condor_submit(const Dagman &dm, Job* node, CondorID& condorID)
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

	auto vars = init_vars(dm, *node);

	auto sort_prepend = [](NodeVar lhs, NodeVar rhs) -> bool {
		if (lhs.append == rhs.append) { return false; }
		return ! lhs.append;
	};
	std::stable_sort(vars.begin(), vars.end(), sort_prepend);

	// If the submitDesc hash is not set, we need to parse it from the file
	if (!node->GetSubmitDesc()) {
		debug_printf(DEBUG_NORMAL, "Submitting node %s from file %s using direct job submission\n", node->GetJobName(), node->GetCmdFile());
		submitHash = new SubmitHash();
		// Start by populating the hash with some parameters
		submitHash->init(JSM_DAGMAN);
		submitHash->setDisableFileChecks(true);
		submitHash->setScheddVersion(CondorVersion());

		struct AddVar {
			AddVar(SubmitHash* h) : hash(h) {};
			void operator()(NodeVar v) {
				hash->set_arg_variable(v.key.c_str(), v.value.c_str());
			}
			SubmitHash* hash; /* DONT delete pointer!!! */
		};

		AddVar setVar(submitHash);
		auto append_point = std::find_if(vars.begin(), vars.end(), [](NodeVar v) -> bool { return v.append; });
		std::for_each(vars.begin(), append_point, setVar); // Add node vars (prepend)

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

		std::for_each(append_point, vars.end(), setVar); // Add node vars (append)
	}
	else {
		debug_printf(DEBUG_NORMAL, "Submitting node %s from inline description using direct job submission\n", node->GetJobName());
		submitHash = node->GetSubmitDesc();
		/* If here then it is inline submission and a submit hash has been created already.
		*  Due to this, we can only append all node vars - Cole Bollig 2024-05-17 */
		for (const auto& var : vars) {
			submitHash->set_arg_variable(var.key.c_str(), var.value.c_str());
		}
	}

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

		while ((rval = ssi.next(jid, item_index, step, true)) > 0) {
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
	if (dm.options[deep::i::SubmitMethod] == (int)DagSubmitMethod::CONDOR_SUBMIT)
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
	static std::string dmaskstr("");

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
				dmaskstr += ',';
			}
			dmaskstr += std::to_string(*p);
		}

		result = dmaskstr;
	}

	return result.c_str(); // somewhat safe because result is static.
}
