/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
 ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"

//
// Local DAGMan includes
//
#include "dag.h"
#include "submit.h"
#include "util.h"
#include "debug.h"

extern char * DAP_SERVER;

static bool
submit_try( const char *exe, const char *command, CondorID &condorID )
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
  // Parse Condor's return message for Condor ID.  This desperately
  // needs to be replaced by a Condor Submit API.
  //
  // Typical condor_submit output for Condor v6 looks like:
  //
  //   Submitting job(s).
  //   Logging submit event(s).
  //   1 job(s) submitted to cluster 2267.
  //----------------------------------------------------------------------

  char buffer[UTIL_MAX_LINE_LENGTH];
  buffer[0] = '\0';
  
  // Look for the line containing the word "cluster".
  // If we get an EOF, then something went wrong with condor_submit, so
  // we return false.  The caller of this function can retry the submit
  // by repeatedly calling this function.

  do {
    if (util_getline(fp, buffer, UTIL_MAX_LINE_LENGTH) == EOF) {
      pclose(fp);
	  debug_printf(DEBUG_NORMAL, "failed while reading from pipe.\n");
	  debug_printf(DEBUG_NORMAL, "Read so far: %s\n", command_output.Value());
      return false;
    }
	command_output += buffer;
  } while (strstr(buffer, "cluster") == NULL);
  
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

  if (1 == sscanf(buffer, "1 job(s) submitted to cluster %d",
                  & condorID._cluster)) {
  }
  
  return true;
}


//-------------------------------------------------------------------------
bool
submit_submit( const char* cmdFile, CondorID& condorID,
	const char* DAGNodeName, List<MyString>* names, List<MyString>* vals )
{
	const char * exe = "condor_submit";
	MyString prependLines;
	MyString command;
	char quote[2] = {commandLineQuoteChar, '\0'};

	// add arguments to condor_submit to identify the job's name in the
	// DAG and the DAGMan's job id, and to print the former in the
	// submit log

	prependLines = MyString(" -a ") + quote +
		"dag_node_name = " + DAGNodeName + quote +
		" -a " + quote +
		"dagman_job_id = " + DAGManJobId + quote +
		" -a " + quote +
		"submit_event_notes = DAG Node: $(dag_node_name)" +
		quote;

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
	
	debug_printf(DEBUG_VERBOSE, "submitting: %s\n", command.Value());
  
	bool success = false;
	const int tries = 6;
	int wait = 1;

	success = submit_try( exe, command.Value(), condorID );
	for (int i = 1 ; i < tries && !success ; i++) {
		debug_printf( DEBUG_NORMAL, "condor_submit try %d/%d failed, "
			"will try again in %d second%s\n", i, tries, wait,
			wait == 1 ? "" : "s" );
		sleep( wait );
		success = submit_try( exe, command.Value(), condorID );
		wait = wait * 2;
	}
	if (!success && DEBUG_LEVEL(DEBUG_QUIET)) {
		dprintf( D_ALWAYS, "condor_submit failed after %d tr%s.\n", tries,
			tries == 1 ? "y" : "ies" );
		dprintf( D_ALWAYS, "submit command was: %s\n", command.Value() );
		return false;
	}
	return success;
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

//--> DAP
static bool
dap_try( const char *exe, const char *command, CondorID &condorID )
{
  MyString  command_output("");

  FILE * fp = popen(command, "r");
  if (fp == NULL) {
    debug_printf( DEBUG_NORMAL, "%s: popen() in submit_try failed!\n", command);
    return false;
  }
  
  //----------------------------------------------------------------------
  // Parse DAP server's return message for dap_id.  This desperately
  // needs to be replaced by a DAP Submit API.
  //
  // Typical stork_submit output looks like:

  //skywalker(6)% stork_submit skywalker 1.dap
  //connected to skywalker..
  //sending request:
  //     [
  //        dap_type = "transfer";
  //        src_url = "nest://db16.cs.wisc.edu/test4.txt"; 
  //        dest_url = "nest://db18.cs.wisc.edu/test8.txt"; 
  //    ]
  //request accepted by the server and assigned a dap_id: 1
  //----------------------------------------------------------------------

  char buffer[UTIL_MAX_LINE_LENGTH];
  buffer[0] = '\0';
  
  // Look for the line containing the word "dap_id".
  // If we get an EOF, then something went wrong with _submit, so
  // we return false.  The caller of this function can retry the submit
  // by repeatedly calling this function.

  do {
    if (util_getline(fp, buffer, UTIL_MAX_LINE_LENGTH) == EOF) {
      pclose(fp);
	  debug_printf(DEBUG_NORMAL, "failed while reading from pipe.\n");
	  debug_printf(DEBUG_NORMAL, "Read so far: %s\n", command_output.Value());
      return false;
    }
	command_output += buffer;

  } while (strstr(buffer, "dap_id") == NULL);

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


  if (1 == sscanf(buffer, "Request accepted by the server and assigned a dap_id: %d",
                  & condorID._cluster)) {
  }
  
  return true;
}

//-------------------------------------------------------------------------
bool
dap_submit( const char* cmdFile, CondorID& condorID,
			   const char* DAGNodeName )
{
  char* command;
  int cmdLen;
  const char * exe = "stork_submit";

  cmdLen = strlen( exe ) + strlen( cmdFile ) + 512;
  command = new char[cmdLen];
  if (command == NULL) {
	  debug_printf( DEBUG_SILENT, "\nERROR: out of memory (%s:%d)!\n",
				   __FILE__, __LINE__ );
	  return false;
  }

  // we use 2>&1 to make sure we get both stdout and stderr from command
  sprintf( command, "%s %s %s -lognotes \"DAG Node: %s\" 2>&1", 
  	   exe, DAP_SERVER, cmdFile, DAGNodeName );


  //  dprintf( D_ALWAYS, "submit command is: %s\n", command );

  bool success = false;
  const int tries = 6;
  int wait = 1;
  
  success = dap_try( exe, command, condorID );
  for (int i = 1 ; i < tries && !success ; i++) {
      debug_printf( DEBUG_NORMAL, "stork_submit try %d/%d failed, "
                     "will try again in %d second%s\n", i, tries, wait,
					 wait == 1 ? "" : "s" );
      sleep( wait );
	  success = dap_try( exe, command, condorID );
	  wait = wait * 2;
  }
  if (!success && DEBUG_LEVEL(DEBUG_QUIET)) {
    dprintf( D_ALWAYS, "stork_submit failed after %d tr%s.\n", tries,
			tries == 1 ? "y" : "ies" );
    dprintf( D_ALWAYS, "submit command was: %s\n", command );
	delete[] command;
	return false;
  }
  delete [] command;
  return success;
}
//<-- DAP





