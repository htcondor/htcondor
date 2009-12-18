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

//
// Local DAGMan includes
//
#include "dagman_main.h"
#include "dag.h"
#include "submit.h"
#include "util.h"
#include "debug.h"
#include "tmp_dir.h"

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
  if ( 2 != sscanf( buffer, "%d job(s) submitted to cluster %d",
  			&jobProcCount, &cluster) ) {
	debug_printf( DEBUG_QUIET, "ERROR: parse_condor_submit failed:\n\t%s\n",
				buffer );
    return false;
  }
  
  return true;
}

//-------------------------------------------------------------------------
/** Parse output from stork_submit, determine the number of job procs
    and the cluster.
	@param buffer containing the line to be parsed
	@param integer to be filled in with the number of job procs generated
	       by the stork_submit
	@param integer to be filled in with the cluster ID
	@return true iff the line was correctly parsed
*/
static bool
parse_stork_submit( const char *buffer, int &jobProcCount, int &cluster )
{
  if ( 1 != sscanf( buffer, "Request assigned id: %d", &cluster) ) {
	debug_printf( DEBUG_QUIET, "ERROR: parse_stork_submit failed:\n\t%s\n",
				buffer );
    return false;
  }

  jobProcCount = 1;
  return true;
}

//-------------------------------------------------------------------------
static bool
submit_try( ArgList &args, CondorID &condorID, Job::job_type_t type,
			bool prohibitMultiJobs )
{
  MyString cmd; // for debug output
  args.GetArgsStringForDisplay( &cmd );

  FILE * fp = my_popen( args, "r", TRUE );
  if (fp == NULL) {
    debug_printf( DEBUG_NORMAL, 
		  "ERROR: my_popen(%s) in submit_try() failed!\n",
		  cmd.Value() );
    return false;
  }
  
  //----------------------------------------------------------------------
  // Parse submit command output for a Condor/Stork job ID.  This
  // desperately needs to be replaced by Condor/Stork submit APIs.
  //
  // Typical condor_submit output for Condor v6 looks like:
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

  if ( type == Job::TYPE_CONDOR ) {
    marker = "cluster";

	  // Note: we *could* check how many jobs got submitted here, and
	  // correlate that with how many submit events we see later on.
	  // I'm not worrying about that for now...  wenger 2006-02-07.
	  // We also have to check the number of jobs to get an accurate
	  // count of submitted jobs to report in the dagman.out file.

	  // We should also check whether we got more than one cluster, and
	  // either deal with it correctly or generate an error message.
	parseFnc = parse_condor_submit;
  } else if ( type == Job::TYPE_STORK ) {
    marker = "assigned id";
	parseFnc = parse_stork_submit;
  } else {
	debug_printf( DEBUG_QUIET, "Illegal job type: %d\n", type );
	ASSERT(false);
  }
  
  // Take all of the output (both stdout and stderr) from condor_submit
  // or stork_submit, and echo it to the dagman.out file.  Look for
  // the line (if any) containing the word "cluster" (Condor) or
  // "assigned id" (Stork).  If we don't find such a line, something
  // went wrong with the submit, so we return false.  The caller of this
  // function can retry the submit by repeatedly calling this function.

  MyString  command_output("");
  MyString keyLine("");
  while (util_getline(fp, buffer, UTIL_MAX_LINE_LENGTH) != EOF) {
	debug_printf(DEBUG_VERBOSE, "From submit: %s\n", buffer);
	command_output += buffer;
    if (strstr(buffer, marker) != NULL) {
	  keyLine = buffer;
	}
  }

  {
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
	  return false;
  }

  // Stork job specs have only 1 dimension.  The Stork user log forces the proc
  // and sub-proc ids to "-1", so do the same here for the returned submit id.
  if ( type == Job::TYPE_STORK ) {
	  condorID._proc = -1;
	  condorID._subproc = -1;
  }

  	// Check for multiple job procs if configured to disallow that.
  if ( prohibitMultiJobs && (jobProcCount > 1) ) {
	debug_printf( DEBUG_NORMAL, "Submit generated %d job procs; "
				"disallowed by DAGMAN_PROHIBIT_MULTI_JOBS setting\n",
				jobProcCount );
	main_shutdown_rescue( EXIT_ERROR );
  }
  
  return true;
}

//-------------------------------------------------------------------------
// NOTE: this and submit_try should probably be merged into a single
// method -- submit_batch_job or something like that.  wenger/pfc 2006-04-05.
static bool
do_submit( ArgList &args, CondorID &condorID, Job::job_type_t jobType,
			bool prohibitMultiJobs )
{
	MyString cmd; // for debug output
	args.GetArgsStringForDisplay( &cmd );
	debug_printf( DEBUG_VERBOSE, "submitting: %s\n", cmd.Value() );
  
	bool success = false;

	success = submit_try( args, condorID, jobType, prohibitMultiJobs );

	if( !success ) {
	    debug_printf( DEBUG_QUIET, "ERROR: submit attempt failed\n" );
		debug_printf( DEBUG_QUIET, "submit command was: %s\n", cmd.Value() );
	}

	return success;
}

//-------------------------------------------------------------------------
bool
condor_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName, MyString DAGParentNodeNames,
			   List<MyString>* names, List<MyString>* vals,
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

	args.AppendArg( "-a" );
	MyString nodeName = MyString(ATTR_DAG_NODE_NAME_ALT) + " = " + DAGNodeName;
	args.AppendArg( nodeName.Value() );

		// append a line adding the parent DAGMan's cluster ID to the job ad
	args.AppendArg( "-a" );
	MyString dagJobId = MyString( "+" ) + ATTR_DAGMAN_JOB_ID + " = " +
				dm.DAGManJobId._cluster;
	args.AppendArg( dagJobId.Value() );

		// now we append a line setting the same thing as a submit-file macro
		// (this is necessary so the user can reference it in the priority)
	args.AppendArg( "-a" );
	MyString dagJobIdMacro = MyString( "" ) + ATTR_DAGMAN_JOB_ID + " = " +
				dm.DAGManJobId._cluster;
	args.AppendArg( dagJobIdMacro.Value() );

	args.AppendArg( "-a" );
	MyString submitEventNotes = MyString(
				"submit_event_notes = DAG Node: " ) + DAGNodeName;
	args.AppendArg( submitEventNotes.Value() );

	if ( logFile ) {
		args.AppendArg( "-a" );
		MyString logFileArg = MyString(
					"log = " ) + logFile;
		args.AppendArg( logFileArg.Value() );
	}

	ArgList parentNameArgs;
	parentNameArgs.AppendArg( "-a" );
	MyString parentNodeNames = MyString( "+DAGParentNodeNames = " ) +
	                        "\"" + DAGParentNodeNames + "\"";
	parentNameArgs.AppendArg( parentNodeNames.Value() );

		// set any VARS specified in the DAG file
	MyString anotherLine;
	ListIterator<MyString> nameIter(*names);
	ListIterator<MyString> valIter(*vals);
	MyString name, val;
	while(nameIter.Next(name) && valIter.Next(val)) {
		args.AppendArg( "-a" );
		MyString var = name + " = " + val;
		args.AppendArg( var.Value() );
	}

		// how big is the command line so far
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
		debug_printf( DEBUG_NORMAL, "WARNING: node %s has too many parents "
					  "to list in its classad; leaving its DAGParentNodeNames "
					  "attribute undefined\n", DAGNodeName );
	} else {
		args.AppendArgsFromArgList( parentNameArgs );
	}

	args.AppendArg( cmdFile );

	bool success = do_submit( args, condorID, Job::TYPE_CONDOR,
				dm.prohibitMultiJobs );

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.Value() );
		success = false;
	}

	return success;
}

//-------------------------------------------------------------------------
bool
stork_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName, const char* directory )
{
	TmpDir		tmpDir;
	MyString	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to DAG directory %s: %s\n",
				directory, errMsg.Value() );
		return false;
	}

  ArgList args;

  args.AppendArg( dm.storkSubmitExe );
  args.AppendArg( "-lognotes" );

  MyString logNotes = MyString( "DAG Node: " ) + DAGNodeName;
  args.AppendArg( logNotes.Value() );

  args.AppendArg( cmdFile );

  bool success = do_submit( args, condorID, Job::TYPE_STORK,
  			dm.prohibitMultiJobs );

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.Value() );
		success = false;
	}

  return success;
}
