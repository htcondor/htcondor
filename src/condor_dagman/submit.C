/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
  // Parse Condor's return message for Condor ID or the DAP server's
  // return message for dap_id.  This desperately needs to be replaced by
  // a Condor Submit API and a DAP submit API.
  //
  // Typical condor_submit output for Condor v6 looks like:
  //
  //   Submitting job(s).
  //   Logging submit event(s).
  //   1 job(s) submitted to cluster 2267.
  //
  // Typical stork_submit output looks like:
  //
  // skywalker(6)% stork_submit skywalker 1.dap
  // connected to skywalker..
  // sending request:
  //     [
  //         dap_type = "transfer";
  //         src_url = "nest://db16.cs.wisc.edu/test4.txt";
  //         dest_url = "nest://db18.cs.wisc.edu/test8.txt";
  //     ]
  // request accepted by the server and assigned a dap_id: 1
  //----------------------------------------------------------------------

  char buffer[UTIL_MAX_LINE_LENGTH];
  buffer[0] = '\0';

  	// Configure what we look for in the command output according to
	// which type of job we have.
  const char *marker = NULL;
  const char *scanfStr = NULL;
  const char *submitExec = NULL;

  if ( type == Job::TYPE_CONDOR ) {
    marker = "cluster";
    scanfStr = "1 job(s) submitted to cluster %d";
	submitExec = "condor_submit";

  } else if ( type == Job::TYPE_STORK ) {
    marker = "assigned id";
    //scanfStr = "Requests accepted by the server and assigned a dap_id: %d";
    scanfStr = "Request assigned id: %d";
	submitExec = "stork_submit";

  } else {
	debug_printf( DEBUG_QUIET, "Illegal job type: %d\n", type );
	ASSERT(false);
  }
  
  // Look for the line containing the word "cluster" (Condor) or "dap_id"
  // (DAP).  If we get an EOF, then something went wrong with condor_submit
  // or stork_submit, so  we return false.  The caller of this function can
  // retry the submit by repeatedly calling this function.

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
	  debug_printf( DEBUG_QUIET, "ERROR: %s failed:\n\t%s\n",
					submitExec, buffer );
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
do_submit( const Dagman &dm, const char *command, CondorID &condorID,
		Job::job_type_t jobType, const char *exe )
{
	debug_printf(DEBUG_VERBOSE, "submitting: %s\n", command);
  
	bool success = false;
	const int tries = dm.max_submit_attempts;

	success = submit_try( command, condorID, jobType );

	if( !success ) {
	    debug_printf( DEBUG_QUIET, "%s try failed\n", exe );
		debug_printf( DEBUG_QUIET, "submit command was: %s\n",
			      command );
	}

	return success;
}

//-------------------------------------------------------------------------
bool
condor_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName, MyString DAGParentNodeNames,
			   List<MyString>* names, List<MyString>* vals )
{
	const char * exe = "condor_submit";
	MyString prependLines;
	MyString command;
	char quote[2] = {commandLineQuoteChar, '\0'};

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
		" -a " + quote + "dag_node_name = " + DAGNodeName + quote +
		" -a " + quote + "+DAGParentNodeNames = \"" + DAGParentNodeNames + "\"" + quote +
		" -a " + quote + "+" + DAGManJobIdAttrName + " = " + dm.DAGManJobId._cluster + quote +
		" -a " + quote + "submit_event_notes = DAG Node: " + DAGNodeName + quote;

	MyString anotherLine;
	ListIterator<MyString> nameIter(*names);
	ListIterator<MyString> valIter(*vals);
	MyString name, val;
	while(nameIter.Next(name) && valIter.Next(val)) {
		anotherLine = MyString(" -a ") + quote +
			name + " = " + val + quote;
		prependLines += anotherLine;
	}

#ifdef WIN32
	command = MyString(exe) + " " + prependLines + " " + cmdFile;
#else
	// we use 2>&1 to make sure we get both stdout and stderr from command
	command = MyString(exe) + " " + prependLines + " " + cmdFile + " 2>&1";;
#endif
	
	bool success = do_submit( dm, command.Value(), condorID,
			Job::TYPE_CONDOR, exe );

	return success;
}

//-------------------------------------------------------------------------
bool
dap_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName )
{
  MyString command;
  const char * exe = "stork_submit";

  // we use 2>&1 to make sure we get both stdout and stderr from command
  command.sprintf("%s -lognotes \"DAG Node: %s\" %s 2>&1", 
  	   exe, DAGNodeName, cmdFile );

  bool success = do_submit( dm, command.Value(), condorID, Job::TYPE_STORK,
  	  exe );

  return success;
}
