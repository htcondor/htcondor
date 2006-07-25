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
#include "condor_string.h"

#include "script.h"
#include "util.h"
#include "job.h"
#include "condor_id.h"
#include "tmp_dir.h"

#include "HashTable.h"
#include "env.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

extern DLL_IMPORT_MAGIC char **environ;

//-----------------------------------------------------------------------------
Script::Script( bool post, const char* cmd, const Job* node ) :
    _post         (post),
    _retValScript (-1),
    _retValJob    (-1),
	_pid		  (0),
	_done         (FALSE),
	_node         (node)
{
	ASSERT( cmd != NULL );
    _cmd = strnewp (cmd);
    return;
}

//-----------------------------------------------------------------------------
Script::~Script () {
    delete [] _cmd;
    return;
}

const char *Script::GetNodeName()
{
    return _node->GetJobName();
}

//-----------------------------------------------------------------------------
int
Script::BackgroundRun( int reaperId )
{
	TmpDir		tmpDir;
	MyString	errMsg;
	if ( !tmpDir.Cd2TmpDir( _node->GetDirectory(), errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				_node->GetDirectory(), errMsg.Value() );

		return 0;
	}

	// Construct the command line, replacing some tokens with
    // information about the job.  All of these values would probably
    // be better inserted into the environment, rather than passed on
    // the command-line... some should be in the job's env as well...

    const char *delimiters = " \t";
    char * token;
	ArgList args;
    char * cmd = strnewp(_cmd);
    for (token = strtok (cmd,  delimiters) ; token != NULL ;
         token = strtok (NULL, delimiters)) {
		MyString arg;
		if( !strcasecmp( token, "$JOB" ) ) {
			arg += _node->GetJobName();
		} 
        else if ( !strcasecmp( token, "$RETRY" ) ) {
            arg += _node->retries;
        }
        else if ( _post && !strcasecmp( token, "$JOBID" ) ) {
            arg += _node->_CondorID._cluster;
            arg += '.';
            arg += _node->_CondorID._proc;
        }
        else if (!strcasecmp(token, "$RETURN")) arg += _retValJob;
        else                                    arg += token;

		args.AppendArg(arg.Value());
    }

	_pid = daemonCore->Create_Process( cmd, args,
									   PRIV_UNKNOWN, reaperId, FALSE,
									   NULL, NULL, FALSE, NULL, NULL, 0 );
    delete [] cmd;

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.Value() );
		return 0;
	}

	return _pid;
}
