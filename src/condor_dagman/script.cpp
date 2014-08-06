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
#include "condor_string.h"

#include "script.h"
#include "util.h"
#include "job.h"
#include "tmp_dir.h"
#include "dagman_main.h"

#include "env.h"
#include "condor_daemon_core.h"

extern DLL_IMPORT_MAGIC char **environ;

//-----------------------------------------------------------------------------
Script::Script( bool post, const char* cmd, Job* node ) :
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
Script::BackgroundRun( int reaperId, int dagStatus, int failedCount )
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

		if ( !strcasecmp( token, "$JOB" ) ) {
			arg += _node->GetJobName();

		} else if ( !strcasecmp( token, "$RETRY" ) ) {
            arg += _node->GetRetries();

		} else if ( !strcasecmp( token, "$MAX_RETRIES" ) ) {
            arg += _node->GetRetryMax();

        } else if ( !strcasecmp( token, "$JOBID" ) ) {
			if ( !_post ) {
				debug_printf( DEBUG_QUIET, "Warning: $JOBID macro should "
							"not be used as a PRE script argument!\n" );
				check_warning_strictness( DAG_STRICT_1 );
				arg += token;
			} else {
            	arg += _node->GetCluster();
            	arg += '.';
            	arg += _node->GetProc();
			}

        } else if (!strcasecmp(token, "$RETURN")) {
			if ( !_post ) {
				debug_printf( DEBUG_QUIET, "Warning: $RETURN macro should "
							"not be used as a PRE script argument!\n" );
				check_warning_strictness( DAG_STRICT_1 );
			}
			arg += _retValJob;

		} else if (!strcasecmp( token, "$PRE_SCRIPT_RETURN" ) ) {
			if ( !_post ) {
				debug_printf( DEBUG_QUIET, "Warning: $PRE_SCRIPT_RETURN macro should "
						"not be used as a PRE script argument!\n" );
				check_warning_strictness( DAG_STRICT_1 );
			}
			arg += _retValScript;

		} else if (!strcasecmp(token, "$DAG_STATUS")) {
			arg += dagStatus;

		} else if (!strcasecmp(token, "$FAILED_COUNT")) {
			arg += failedCount;

		} else if (token[0] == '$') {
			// This should probably be a fatal error when -strict is
			// implemented.
			debug_printf( DEBUG_QUIET, "Warning: unrecognized macro %s "
						"in node %s %s script arguments\n", token,
						_node->GetJobName(), _post ? "POST" : "PRE" );
			check_warning_strictness( DAG_STRICT_1 );
			arg += token;
        } else {
			arg += token;
		}

		args.AppendArg(arg.Value());
    }

	_pid = daemonCore->Create_Process( cmd, args,
									   PRIV_UNKNOWN, reaperId, FALSE, FALSE,
									   NULL, NULL, NULL, NULL, NULL, 0 );
    delete [] cmd;

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.Value() );
		return 0;
	}

	return _pid;
}
