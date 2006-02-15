/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_attributes.h"

//
// Local DAGMan includes
//
#include "dagman_main.h"
#include "dag.h"
#include "submit.h"
#include "util.h"
#include "debug.h"
#include "tmp_dir.h"


		// argQuote: the characters we should use to quote
		// command-line arguments when specifying shell commands

		// innerQuote: the characters we should use to represent a
		// double-quote character *within* a quoted command-line
		// argument

#ifdef WIN32
	static const char* argQuote = "\"";
	static const char* innerQuote = "\\\"";
#else
	static const char* argQuote = "\'";
	static const char* innerQuote = "\"";
#endif

static bool
submit_try( const char *command, CondorID &condorID, Job::job_type_t type )
{
  MyString  command_output("");

  // this is dangerous ... we need to be much more careful about
  // auditing what we're about to run via popen(), rather than
  // accepting an arbitary command string...
  FILE * fp = popen(command, "r");
  if (fp == NULL) {
    debug_printf( DEBUG_NORMAL, "%s: popen() in submit_try failed!\n", command);
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
  const char *scanfStr = NULL;

  if ( type == Job::TYPE_CONDOR ) {
    marker = "cluster";

	  // Note: we *could* check how many jobs got submitted here, and
	  // correlate that with how many submit events we see later on.
	  // I'm not worrying about that for now...  wenger 2006-02-07.
	  // We also have to check the number of jobs to get an accurate
	  // count of submitted jobs to report in the dagman.out file.

	  // We should also check whether we got more than one cluster, and
	  // either deal with it correctly or generate an error message.
    scanfStr = "%*d job(s) submitted to cluster %d";
  } else if ( type == Job::TYPE_STORK ) {
    marker = "assigned id";
    scanfStr = "Request assigned id: %d";
  } else {
	debug_printf( DEBUG_QUIET, "Illegal job type: %d\n", type );
	ASSERT(false);
  }
  
  // Look for the line containing the word "cluster" (Condor) or
  // "assigned id" (Stork).  If we get an EOF, then something went
  // wrong with the submit, so we return false.  The caller of this
  // function can retry the submit by repeatedly calling this function.

  do {
    if (util_getline(fp, buffer, UTIL_MAX_LINE_LENGTH) == EOF) {
      pclose(fp);
	  debug_printf(DEBUG_NORMAL, "failed while reading from pipe.\n");
	  debug_printf(DEBUG_NORMAL, "Read so far: %s\n", command_output.Value());
      return false;
    }
	command_output += buffer;
  } while (strstr(buffer, marker) == NULL);
  
  {
    int status = pclose(fp);
    if (status == -1) {
		debug_printf(DEBUG_NORMAL, "Read from pipe: %s\n", 
					 command_output.Value());
		debug_printf( DEBUG_QUIET, "ERROR while running \"%s\": "
					  "pclose() failed!\n", command );
		return false;
    }
  }

  if ( 1 != sscanf( buffer, scanfStr, &condorID._cluster) ) {
	  debug_printf( DEBUG_QUIET, "ERROR: submit failed:\n\t%s\n", buffer );
	  return false;
  }

  // Stork job specs have only 1 dimension.  The Stork user log forces the proc
  // and sub-proc ids to "-1", so do the same here for the returned submit id.
  if ( type == Job::TYPE_STORK ) {
	  condorID._proc = -1;
	  condorID._subproc = -1;
  }
  
  return true;
}

//-------------------------------------------------------------------------
static bool
do_submit( const char *command, CondorID &condorID, Job::job_type_t jobType )
{
	debug_printf(DEBUG_VERBOSE, "submitting: %s\n", command);
  
	bool success = false;

	success = submit_try( command, condorID, jobType );

	if( !success ) {
	    debug_printf( DEBUG_QUIET, "ERROR: submit attempt failed\n" );
		debug_printf( DEBUG_QUIET, "submit command was: %s\n", command );
	}

	return success;
}

//-------------------------------------------------------------------------
bool
condor_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName, MyString DAGParentNodeNames,
			   List<MyString>* names, List<MyString>* vals,
			   const char* directory )
{
	TmpDir		tmpDir;
	MyString	errMsg;
	if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				directory, errMsg.Value() );
		return false;
	}

	MyString prependLines;
	MyString command;

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

	prependLines = prependLines +
		" -a " + argQuote + ATTR_DAG_NODE_NAME_ALT + " = " + DAGNodeName + argQuote +
		" -a " + argQuote + "+" + ATTR_DAGMAN_JOB_ID + " = " + dm.DAGManJobId._cluster + argQuote +
		" -a " + argQuote + "submit_event_notes = DAG Node: " + DAGNodeName + argQuote;

	MyString DAGParentNodeNamesStr;
	DAGParentNodeNamesStr = DAGParentNodeNamesStr + " -a " + argQuote + "+DAGParentNodeNames = " + innerQuote + DAGParentNodeNames + innerQuote + argQuote;

		// set any VARS specified in the DAG file
	MyString anotherLine;
	ListIterator<MyString> nameIter(*names);
	ListIterator<MyString> valIter(*vals);
	MyString name, val;
	while(nameIter.Next(name) && valIter.Next(val)) {
		anotherLine = MyString(" -a ") + argQuote +
			name + " = " + val + argQuote;
		prependLines += anotherLine;
	}

		// how big is the command line so far
	int cmdLineSize = prependLines.Length();
		// how big is the DAGParentNodeNames string
	int DAGParentNodeNamesLen = DAGParentNodeNamesStr.Length();
		// how many additional chars must we still add to command line
	int reserveNeeded = strlen( dm.condorSubmitExe ) + strlen( cmdFile ) + 10;
	int maxCmdLine = _POSIX_ARG_MAX;

		// if we don't have room for DAGParentNodeNames, leave it unset
	if( cmdLineSize + reserveNeeded + DAGParentNodeNamesLen > maxCmdLine ) {
		debug_printf( DEBUG_NORMAL, "WARNING: node %s has too many parents "
					  "to list in its classad; leaving its DAGParentNodeNames "
					  "attribute undefined\n", DAGNodeName );
	} else {
		prependLines = prependLines + DAGParentNodeNamesStr;
	}

#ifdef WIN32
	command = MyString( dm.condorSubmitExe ) + " " + prependLines + " " +
		cmdFile;
#else
	// we use 2>&1 to make sure we get both stdout and stderr from command
	command = MyString( dm.condorSubmitExe ) + " " + prependLines + " " +
		cmdFile + " 2>&1";
#endif
	
	bool success = do_submit( command.Value(), condorID, Job::TYPE_CONDOR );

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

  MyString command;

  // we use 2>&1 to make sure we get both stdout and stderr from command
  command.sprintf("%s -lognotes %sDAG Node: %s%s %s 2>&1", 
				  dm.storkSubmitExe, argQuote, DAGNodeName, argQuote,
				  cmdFile );

  bool success = do_submit( command.Value(), condorID, Job::TYPE_STORK );

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.Value() );
		success = false;
	}

  return success;
}
