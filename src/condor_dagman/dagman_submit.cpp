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
#include "util_lib_proto.h"
#include "my_popen.h"

#include <sstream>

//
// Local DAGMan includes
//
#include "dagman_main.h"
#include "dag.h"
#include "submit.h"
#include "util.h"
#include "debug.h"
#include "tmp_dir.h"
#include "write_user_log.h"

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
  MyString cmd; // for debug output
  args.GetArgsStringForDisplay( &cmd );

  FILE * fp = my_popen( args, "r", MY_POPEN_OPT_WANT_STDERR );
  if (fp == NULL) {
    debug_printf( DEBUG_NORMAL, 
		  "ERROR: my_popen(%s) in submit_try() failed!\n",
		  cmd.Value() );
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

  MyString  command_output("");
  MyString keyLine("");
  while (fgets(buffer, UTIL_MAX_LINE_LENGTH, fp)) {
    MyString buf_line = buffer;
	buf_line.chomp();
	debug_printf(DEBUG_VERBOSE, "From submit: %s\n", buf_line.Value());
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
      debug_printf(DEBUG_NORMAL, "Read so far: %s\n", command_output.Value());
      return false;
    }

    if (status != 0) {
		debug_printf(DEBUG_NORMAL, "Read from pipe: %s\n", 
					 command_output.Value());
		debug_printf( DEBUG_QUIET, "ERROR while running \"%s\": "
					  "my_pclose() failed with status %d (errno %d, %s)!\n",
					  cmd.Value(), status, errno, strerror( errno ) );
		return false;
    }
  }

  int	jobProcCount;
  if ( !parseFnc( keyLine.Value(), jobProcCount, condorID._cluster) ) {
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
	main_shutdown_rescue( EXIT_ERROR, Dag::DAG_STATUS_ERROR );
  }
  
  return true;
}

//-------------------------------------------------------------------------
// NOTE: this and submit_try should probably be merged into a single
// method -- submit_batch_job or something like that.  wenger/pfc 2006-04-05.
static bool
do_submit( ArgList &args, CondorID &condorID, bool prohibitMultiJobs )
{
	MyString cmd; // for debug output
	args.GetArgsStringForDisplay( &cmd );
	debug_printf( DEBUG_VERBOSE, "submitting: %s\n", cmd.Value() );
  
	bool success = false;

	success = submit_try( args, condorID, prohibitMultiJobs );

	if( !success ) {
	    debug_printf( DEBUG_QUIET, "ERROR: submit attempt failed\n" );
		debug_printf( DEBUG_QUIET, "submit command was: %s\n", cmd.Value() );
	}

	return success;
}

//-------------------------------------------------------------------------
bool
condor_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName, MyString &DAGParentNodeNames,
			   List<Job::NodeVar> *vars, int priority, int retry,
			   const char* directory, const char *workflowLogFile,
			   bool hold_claim, const MyString &batchName )
{
	TmpDir		tmpDir;
	MyString	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.Value() );
		return false;
	}

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

	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	MyString nodeName = MyString(ATTR_DAG_NODE_NAME_ALT) + " = " + DAGNodeName;
	args.AppendArg( nodeName.Value() );

		// append a line adding the parent DAGMan's cluster ID to the job ad
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	MyString dagJobId = MyString( "+" ) + ATTR_DAGMAN_JOB_ID + " = " +
				IntToStr( dm.DAGManJobId._cluster );
	args.AppendArg( dagJobId.Value() );

		// now we append a line setting the same thing as a submit-file macro
		// (this is necessary so the user can reference it in the priority)
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	MyString dagJobIdMacro = MyString( "" ) + ATTR_DAGMAN_JOB_ID + " = " +
				IntToStr( dm.DAGManJobId._cluster );
	args.AppendArg( dagJobIdMacro.Value() );

		// Pass the batch name to lower levels.
	if ( batchName != "" ) {
		args.AppendArg( "-batch-name" );
		args.AppendArg( batchName.Value() );
	}

	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	MyString submitEventNotes = MyString(
				"submit_event_notes = DAG Node: " ) + DAGNodeName;
	args.AppendArg( submitEventNotes.Value() );

	ASSERT( workflowLogFile );

		// We need to append the DAGman default log file to
		// the log file list
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	std::string dlog( "dagman_log = " );
	dlog += workflowLogFile;
	args.AppendArg( dlog.c_str() );
	debug_printf( DEBUG_VERBOSE, "Adding a DAGMan workflow log %s\n",
				workflowLogFile );

		// Now append the mask
	debug_printf( DEBUG_VERBOSE, "Masking the events recorded in the DAGMAN workflow log\n" );
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	std::string dmask("+");
	dmask += ATTR_DAGMAN_WORKFLOW_MASK;
	dmask += " = \"";
	const char *eventMask = getEventMask();
	debug_printf( DEBUG_VERBOSE, "Mask for workflow log is %s\n",
				eventMask );
	dmask += eventMask;
	dmask += "\"";
	args.AppendArg( dmask.c_str() );

		// Append the priority, if we have one.
	if ( priority != 0 ) {
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		MyString prioStr = "priority=";
		prioStr += IntToStr( priority );
		args.AppendArg( prioStr.Value() );
	}


		// Suppress the job's log file if that option is enabled.
	if ( dm._suppressJobLogs ) {
		debug_printf( DEBUG_VERBOSE, "Suppressing node job log file\n" );
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		args.AppendArg( "log=" );
	}

	ArgList parentNameArgs;
	parentNameArgs.AppendArg( "-a" ); // -a == -append; using -a to save chars
	MyString parentNodeNames = MyString( "+DAGParentNodeNames = " ) +
	                        "\"" + DAGParentNodeNames + "\"";
	parentNameArgs.AppendArg( parentNodeNames.Value() );

		// set any VARS specified in the DAG file
	MyString anotherLine;
	ListIterator<Job::NodeVar> varsIter(*vars);
	Job::NodeVar nodeVar;
	while ( varsIter.Next(nodeVar) ) {

			// Substitute the node retry count if necessary.  Note that
			// we can't do this in Job::ResolveVarsInterpolations()
			// because that's only called at parse time.
		MyString value = nodeVar._value;
		MyString retryStr = IntToStr( retry );
		value.replaceString( "$(RETRY)", retryStr.Value() );
		MyString varStr = nodeVar._name + " = " + value;

		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		args.AppendArg( varStr.Value() );
	}

		// Set the special DAG_STATUS variable (mainly for use by
		// "final" nodes).
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	MyString var = "DAG_STATUS = ";
	var += IntToStr( (int)dm.dag->_dagStatus );
	args.AppendArg( var.Value() );

		// Set the special FAILED_COUNT variable (mainly for use by
		// "final" nodes).
	args.AppendArg( "-a" ); // -a == -append; using -a to save chars
	var = "FAILED_COUNT = ";
	var += IntToStr( dm.dag->NumNodesFailed() );
	args.AppendArg( var.Value() );

	if( hold_claim ){
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		MyString holdit = MyString("+") + MyString(ATTR_JOB_KEEP_CLAIM_IDLE) + " = "
			+ IntToStr( dm._claim_hold_time );
		args.AppendArg( holdit.Value() );	
	}
	
	if (dm._submitDagDeepOpts.suppress_notification) {
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		MyString notify = MyString("notification = never");
		args.AppendArg( notify.Value() );
	}

		//
		// Add accounting group and user if we have them.
		//
	if ( dm._submitDagDeepOpts.acctGroup != "" ) {
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		MyString arg = "accounting_group=";
		arg += dm._submitDagDeepOpts.acctGroup;
		args.AppendArg( arg );
	}

	if ( dm._submitDagDeepOpts.acctGroupUser != "" ) {
		args.AppendArg( "-a" ); // -a == -append; using -a to save chars
		MyString arg = "accounting_group_user=";
		arg += dm._submitDagDeepOpts.acctGroupUser;
		args.AppendArg( arg );
	}

		//
		// Add parents of this node to arguments, if we have room.
		//
		// This should be the last thing in the arguments, except
		// for the submit file name!!!
		//

		// how big is the command line so far?
	MyString display;
	args.GetArgsStringForDisplay( &display );
	int cmdLineSize = display.Length();

	parentNameArgs.GetArgsStringForDisplay( &display );
	int DAGParentNodeNamesLen = display.Length();
		// how many additional chars must we still add to command line
	        // NOTE: according to the POSIX spec, the args +
   	        // environ given to exec() cannot exceed
   	        // _POSIX_ARG_MAX, so we also need to calculate & add
   	        // the size of environ** to reserveNeeded
	int reserveNeeded = strlen( cmdFile );
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

	bool success = do_submit( args, condorID, dm.prohibitMultiJobs );

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.Value() );
		success = false;
	}

	return success;
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
	MyString	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.Value() );
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
	ulog.setEnableGlobalLog( false );
	ulog.setUseXML( false );
	ulog.initialize( logFile, condorID._cluster,
		condorID._proc, condorID._subproc );

	SubmitEvent subEvent;
	subEvent.cluster = condorID._cluster;
	subEvent.proc = condorID._proc;
	subEvent.subproc = condorID._subproc;

		// We need some value for submitHost for the event to be read
		// correctly.
	subEvent.setSubmitHost( "<dummy-submit-for-noop-job>" );

	MyString subEventNotes("DAG Node: " );
	subEventNotes += DAGNodeName;
		// submitEventLogNotes get deleted in SubmitEvent destructor.
	subEvent.submitEventLogNotes = strnewp( subEventNotes.Value() );

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
	MyString	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.Value() );
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
	ulog.setEnableGlobalLog( false );
	ulog.setUseXML( false );
	ulog.initialize( std::vector<const char*>(1,logFile), condorID._cluster,
		condorID._proc, condorID._subproc );

	PreSkipEvent pEvent;
	pEvent.cluster = condorID._cluster;
	pEvent.proc = condorID._proc;
	pEvent.subproc = condorID._subproc;

	MyString pEventNotes("DAG Node: " );
	pEventNotes += DAGNodeName;
		// skipEventLogNotes gets deleted in PreSkipEvent destructor.
	pEvent.skipEventLogNotes = strnewp( pEventNotes.Value() );

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
