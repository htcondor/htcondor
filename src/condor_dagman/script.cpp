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
#include "node.h"
#include "dag.h"
#include "tmp_dir.h"

#include "condor_daemon_core.h"

extern DLL_IMPORT_MAGIC char **environ;
constexpr int STDOUT = 1;
constexpr int STDERR = 2;

const char* Script::GetNodeName() { return _node->GetJobName(); }

void Script::WriteDebug(int status) {
	if (_output != DagScriptOutput::NONE && ! _debugFile.empty()) {
		TmpDir tmpDir;
		std::string errMsg;
		if ( ! tmpDir.Cd2TmpDir(_node->GetDirectory(), errMsg)) {
			debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n",
			             _node->GetDirectory(), errMsg.c_str());
		}

		std::string output;
		std::string *buffer;

		time_t now = time(nullptr);
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

// Helper funtion to check if the script is type PRE to prevent expanding of certain macros
static bool checkIsPre(const ScriptType& type, const std::string& macro) {
	if (type == ScriptType::PRE) {
		debug_printf(DEBUG_QUIET, "Warning: %s macro should not be used as a PRE script argument!\n",
		             macro.c_str());
		check_warning_strictness(DAG_STRICT_1);
		return true;
	}
	return false;
}

int Script::BackgroundRun(const Dag& dag, int reaperId) {
	TmpDir tmpDir;
	std::string errMsg;
	if ( ! tmpDir.Cd2TmpDir(_node->GetDirectory(), errMsg)) {
		debug_printf(DEBUG_QUIET, "Could not change to node directory %s: %s\n",
		             _node->GetDirectory(), errMsg.c_str());
		return 0;
	}

	_executedCMD.clear(); // Clear previous recorded executed command

	std::string exitCodes, exitFreq;
	for (const auto& [code, count] : _node->JobExitCodes()) {
		if ( ! exitCodes.empty()) { exitCodes += ","; }
		exitCodes += std::to_string(code);

		if ( ! exitFreq.empty()) { exitFreq += ","; }
		exitFreq += std::to_string(code) + ":" + std::to_string(count);
	}

	// Construct the command line, replacing some tokens with
	// information about the job.  All of these values would probably
	// be better inserted into the environment, rather than passed on
	// the command-line... some should be in the job's env as well...
	ArgList args;
	std::string executable;
	StringTokenIterator cmd(_cmd, " \t");
	for (const auto &token : cmd) {
		std::string arg;
		istring_view cmp_arg(token.c_str());

		if (cmp_arg == "$NODE" || cmp_arg == "$JOB") {
			arg = _node->GetJobName();

		} else if (cmp_arg == "$RETRY") {
			arg = std::to_string(_node->GetRetries());

		} else if (cmp_arg == "$MAX_RETRIES") {
			arg = std::to_string(_node->GetRetryMax());

		} else if (cmp_arg == "$DAG_STATUS") {
			arg = std::to_string(dag._dagStatus);

		} else if (cmp_arg == "$FAILED_COUNT") {
			arg = std::to_string(dag.NumNodesFailed());

		} else if (cmp_arg == "$FUTILE_COUNT") {
			arg = std::to_string(dag.NumNodesFutile());

		} else if (cmp_arg == "$DONE_COUNT") {
			arg = std::to_string(dag.NumNodesDone(true));

		} else if (cmp_arg == "$QUEUED_COUNT") {
			arg = std::to_string(dag.NumNodesSubmitted());

		} else if (cmp_arg == "$NODE_COUNT") {
			arg = std::to_string(dag.NumNodes(true));

		} else if (cmp_arg == "$DAGID") {
			arg = std::to_string(dag.DAGManJobId()->_cluster);

		// Macros available for POST & HOLD scripts
		} else if (cmp_arg == "$JOBID") {
			std::string id = std::to_string(_node->GetCluster()) + "." + std::to_string(_node->GetProc());
			arg = checkIsPre(_type, "$JOBID") ? token : id;

		} else if (cmp_arg == "$CLUSTERID") {
			arg = checkIsPre(_type, "$CLUSTERID") ? token : std::to_string(_node->GetCluster());

		} else if (cmp_arg == "$JOB_COUNT") {
			arg = checkIsPre(_type, "$JOB_COUNT") ? token : std::to_string(_node->NumSubmitted());

		} else if (cmp_arg == "$SUCCESS") {
			arg = checkIsPre(_type, "$SUCCESS") ? token : (_node->IsSuccessful() ? "True" : "False");

		} else if (cmp_arg == "$RETURN") {
			arg = checkIsPre(_type, "$RETURN") ? token : std::to_string(_retValJob);

		} else if (cmp_arg == "$EXIT_CODES") {
			arg = checkIsPre(_type, "$EXIT_CODES") ? token : exitCodes;

		} else if (cmp_arg == "$EXIT_CODE_COUNTS") {
			arg = checkIsPre(_type, "$EXIT_CODE_COUNTS") ? token : exitFreq;

		} else if (cmp_arg == "$PRE_SCRIPT_RETURN") {
			arg = checkIsPre(_type, "$PRE_SCRIPT_RETURN") ? token : std::to_string(_retValScript);

		} else if (cmp_arg == "$JOB_ABORT_COUNT") {
			arg = checkIsPre(_type, "$JOB_ABORT_COUNT") ? token : std::to_string(_node->JobsAborted());

		// Non DAGMan sanctioned script macros
		} else if (token[0] == '$') {
			debug_printf(DEBUG_QUIET, "Warning: unrecognized macro %s in node %s %s script arguments\n",
			             token.c_str(), _node->GetJobName(), GetScriptName());
			check_warning_strictness(DAG_STRICT_1);
			arg = token;
		} else {
			arg = token;
		}

		args.AppendArg(arg);

		if (executable.empty()) { executable = arg; }
	}

	args.GetArgsStringForDisplay(_executedCMD);

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
