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
#include "job.h"
#include "tmp_dir.h"

#include "condor_daemon_core.h"

extern DLL_IMPORT_MAGIC char **environ;
constexpr int STDOUT = 1;
constexpr int STDERR = 2;

//-----------------------------------------------------------------------------
Script::Script(ScriptType type, const char* cmd, int deferStatus, time_t deferTime) :
	_type         (type),
	_output       (DagScriptOutput::NONE),
	_retValScript (-1),
	_retValJob    (-1),
	_pid          (0),
	_done         (FALSE),
	_deferStatus  (deferStatus),
	_deferTime    (deferTime),
	_nextRunTime  (0),
	_debugFile    (""),
	_node         (nullptr)
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
void
Script::WriteDebug(int status) {
	if (_output != DagScriptOutput::NONE && ! _debugFile.empty()) {
		TmpDir tmpDir;
		std::string errMsg;
		if ( ! tmpDir.Cd2TmpDir(_node->GetDirectory(), errMsg)) {
			debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n",
			             _node->GetDirectory(), errMsg.c_str());
		}

		std::string output;
		std::string *buffer;

		time_t now = time(NULL);
		formatstr(output, "*** Node=%s Type=%s Status=%d Completion=%lld Cmd='%s'\n",
		          _node->GetJobName(), GetScriptName(), status, (long long)now,
		          _executedCMD.c_str());

		buffer = daemonCore->Read_Std_Pipe(_pid, STDOUT);
		if (buffer) { output += *buffer; }
		buffer = daemonCore->Read_Std_Pipe(_pid, STDERR);
		if (buffer) { output += *buffer; }

		FILE* debug_fp = safe_fopen_wrapper(_debugFile.c_str(), "a");
		if ( ! debug_fp) {
			// don't return here in case we need to cd back to main working dir
			debug_printf(DEBUG_NORMAL, "ERROR: Failed to open %s to write %s script output for %s\n",
			             _debugFile.c_str(), GetScriptName(), _node->GetJobName());
		} else {
			int debug_fd = fileno(debug_fp);
			if (write(debug_fd, output.c_str(), output.length()) == -1) {
				debug_printf(DEBUG_NORMAL, "ERROR (%d): Failed to write %s %s Script output to %s | %s\n",
				             errno, _node->GetJobName(), GetScriptName(),
				             _debugFile.c_str(), strerror(errno));
			}
			fclose(debug_fp);
		}

		if ( ! tmpDir.Cd2MainDir(errMsg)) {
			debug_printf(DEBUG_QUIET, "Could not change to original directory: %s\n",
			             errMsg.c_str());
		}
	}
};

//-----------------------------------------------------------------------------
int
Script::BackgroundRun( int reaperId, int dagStatus, int failedCount )
{
	TmpDir tmpDir;
	std::string	errMsg;
	if ( ! tmpDir.Cd2TmpDir(_node->GetDirectory(), errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n",
		             _node->GetDirectory(), errMsg.c_str());
		return 0;
	}

	// Construct the command line, replacing some tokens with
	// information about the job.  All of these values would probably
	// be better inserted into the environment, rather than passed on
	// the command-line... some should be in the job's env as well...
	ArgList args;
	std::string executable;
	StringTokenIterator cmd(_cmd, " \t");
	for (auto &token : cmd) {
		std::string arg;

		if (strcasecmp(token.c_str(), "$JOB") == MATCH) {
			arg += _node->GetJobName();

		} else if (strcasecmp(token.c_str(), "$RETRY") == MATCH) {
			arg += std::to_string(_node->GetRetries());

		} else if (strcasecmp(token.c_str(), "$MAX_RETRIES") == MATCH) {
			arg += std::to_string( _node->GetRetryMax() );

		} else if (strcasecmp(token.c_str(), "$JOBID") == MATCH) {
			if (_type == ScriptType::PRE) {
				debug_printf(DEBUG_QUIET,
				             "Warning: $JOBID macro should not be used as a PRE script argument!\n");
				check_warning_strictness(DAG_STRICT_1);
				arg += token;
			} else {
				arg += std::to_string( _node->GetCluster() );
				arg += '.';
				arg += std::to_string( _node->GetProc() );
			}

		} else if (strcasecmp(token.c_str(), "$RETURN") == MATCH) {
			if (_type == ScriptType::PRE) {
				debug_printf(DEBUG_QUIET,
				             "Warning: $RETURN macro should not be used as a PRE script argument!\n");
				check_warning_strictness(DAG_STRICT_1);
			}
			arg += std::to_string( _retValJob );

		} else if (strcasecmp(token.c_str(), "$PRE_SCRIPT_RETURN") == MATCH) {
			if (_type == ScriptType::PRE) {
				debug_printf(DEBUG_QUIET,
				             "Warning: $PRE_SCRIPT_RETURN macro should not be used as a PRE script argument!\n");
				check_warning_strictness(DAG_STRICT_1);
			}
			arg += std::to_string( _retValScript );

		} else if (strcasecmp(token.c_str(), "$DAG_STATUS") == MATCH) {
			arg += std::to_string( dagStatus );

		} else if (strcasecmp(token.c_str(), "$FAILED_COUNT") == MATCH) {
			arg += std::to_string( failedCount );

		} else if (token[0] == '$') {
			// This should probably be a fatal error when -strict is
			// implemented.
			debug_printf(DEBUG_QUIET, "Warning: unrecognized macro %s in node %s %s script arguments\n",
			             token.c_str(), _node->GetJobName(), GetScriptName());
			check_warning_strictness(DAG_STRICT_1);
			arg += token;
		} else {
			arg += token;
		}

		args.AppendArg(arg);

		if ( ! _executedCMD.empty()) { _executedCMD += " "; }
		_executedCMD += arg;

		if (executable.empty()) { executable = arg; }
	}

	OptionalCreateProcessArgs cpArgs;
	cpArgs.reaperID(reaperId).wantCommandPort(FALSE).wantUDPCommandPort(FALSE)
	      .fdInheritList(0);
	int std_fds[3] = {-1, DC_STD_FD_PIPE, DC_STD_FD_PIPE};
	if (_output != DagScriptOutput::NONE) {
		bool unknown = FALSE;
		switch (_output) {
			case DagScriptOutput::STDOUT: // Want stdout
				std_fds[STDERR] = -1;
				break;
			case DagScriptOutput::STDERR: // Want stderr
				std_fds[STDOUT] = -1;
				break;
			case DagScriptOutput::ALL: // Want both stdout & stderr
				break;
			default: // Unknown
				unknown = TRUE;
				debug_printf(DEBUG_NORMAL, "ERROR: Unknown Script output stream type: %d\n",
				             (int)_output);
				break;
		}
		if ( ! unknown) { cpArgs.std(std_fds); }
	}
	_pid = daemonCore->CreateProcessNew(executable, args, cpArgs);

	if ( ! tmpDir.Cd2MainDir(errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to original directory: %s\n",
		             errMsg.c_str());
		return 0;
	}

	return _pid;
}
