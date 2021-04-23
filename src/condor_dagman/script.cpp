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

#include "script.h"
#include "util.h"
#include "job.h"
#include "tmp_dir.h"
#include "dagman_main.h"

#include "env.h"
#include "condor_daemon_core.h"

extern DLL_IMPORT_MAGIC char **environ;

//-----------------------------------------------------------------------------
Script::Script( ScriptType type, const char* cmd, int deferStatus, time_t deferTime,
			Job* node ) :
    _type         (type),
    _retValScript (-1),
    _retValJob    (-1),
	_pid		  (0),
	_done         (FALSE),
	_deferStatus  (deferStatus),
	_deferTime    (deferTime),
	_nextRunTime  (0),
	_node         (node)
{
	ASSERT( cmd != NULL );
    _cmd = strdup (cmd);
    return;
}

//-----------------------------------------------------------------------------
Script::~Script () {
    free(_cmd);
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
	std::string	errMsg;
	if ( !tmpDir.Cd2TmpDir( _node->GetDirectory(), errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to node directory %s: %s\n",
				_node->GetDirectory(), errMsg.c_str() );

		return 0;
	}

	// Construct the command line, replacing some tokens with
    // information about the job.  All of these values would probably
    // be better inserted into the environment, rather than passed on
    // the command-line... some should be in the job's env as well...

    const char *delimiters = " \t";
    char * token;
	ArgList args;
    char * cmd = strdup(_cmd);
    for (token = strtok (cmd,  delimiters) ; token != NULL ;
         token = strtok (NULL, delimiters)) {

		std::string arg;

		if ( !strcasecmp( token, "$JOB" ) ) {
			arg += _node->GetJobName();

		} else if ( !strcasecmp( token, "$RETRY" ) ) {
            arg += std::to_string( _node->GetRetries() );

		} else if ( !strcasecmp( token, "$MAX_RETRIES" ) ) {
            arg += std::to_string( _node->GetRetryMax() );

        } else if ( !strcasecmp( token, "$JOBID" ) ) {
			if ( _type == ScriptType::PRE ) {
				debug_printf( DEBUG_QUIET, "Warning: $JOBID macro should "
							"not be used as a PRE script argument!\n" );
				check_warning_strictness( DAG_STRICT_1 );
				arg += token;
			} else {
				arg += std::to_string( _node->GetCluster() );
            	arg += '.';
				arg += std::to_string( _node->GetProc() );
			}

        } else if (!strcasecmp(token, "$RETURN")) {
			if ( _type == ScriptType::PRE ) {
				debug_printf( DEBUG_QUIET, "Warning: $RETURN macro should "
							"not be used as a PRE script argument!\n" );
				check_warning_strictness( DAG_STRICT_1 );
			}
			arg += std::to_string( _retValJob );

		} else if (!strcasecmp( token, "$PRE_SCRIPT_RETURN" ) ) {
			if ( _type == ScriptType::PRE ) {
				debug_printf( DEBUG_QUIET, "Warning: $PRE_SCRIPT_RETURN macro should "
						"not be used as a PRE script argument!\n" );
				check_warning_strictness( DAG_STRICT_1 );
			}
			arg += std::to_string( _retValScript );

		} else if (!strcasecmp(token, "$DAG_STATUS")) {
			arg += std::to_string( dagStatus );

		} else if (!strcasecmp(token, "$FAILED_COUNT")) {
			arg += std::to_string( failedCount );

		} else if (token[0] == '$') {
			// This should probably be a fatal error when -strict is
			// implemented.
			debug_printf( DEBUG_QUIET, "Warning: unrecognized macro %s "
						"in node %s %s script arguments\n", token,
						_node->GetJobName(), GetScriptName() );
			check_warning_strictness( DAG_STRICT_1 );
			arg += token;
        } else {
			arg += token;
		}

		args.AppendArg(arg.c_str());
    }

	_pid = daemonCore->Create_Process( cmd, args,
									   PRIV_UNKNOWN, reaperId, FALSE, FALSE,
									   NULL, NULL, NULL, NULL, NULL, 0 );
    free( cmd );

	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		debug_printf( DEBUG_QUIET,
				"Could not change to original directory: %s\n",
				errMsg.c_str() );
		return 0;
	}

	return _pid;
}
