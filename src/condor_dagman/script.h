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


#ifndef _SCRIPT_H_
#define _SCRIPT_H_

#include <string>
#include "condor_debug.h" // For ASSERT

static const int SCRIPT_DEFER_STATUS_NONE = -1;

enum ScriptType {
	PRE,
	POST,
	HOLD
};

enum class DagScriptOutput {
	NONE = 0,
	STDOUT,
	STDERR,
	ALL,
};

class Job;
class Dag;

class Script {
public:
	Script() = delete;
	Script(ScriptType type, const char* cmd, int deferStatus, time_t deferTime) {
		ASSERT(cmd != nullptr);
		_type = type;
		_cmd = cmd;
		_deferStatus = deferStatus;
		_deferTime = deferTime;
	}

	int BackgroundRun(const Dag& dag, int reaperId);

	void WriteDebug(int status);
	inline void SetDebug(std::string& file, DagScriptOutput type) { _debugFile = file; _output = type; }

	const char* GetNodeName();
	inline const char* GetCmd() const { return _cmd.c_str(); }

	Job* GetNode() { return _node; }
	inline void SetNode(Job* node) { _node = node; }

	inline ScriptType GetType() const { return _type; }
	const char* GetScriptName() {
		switch(_type) {
			case ScriptType::PRE: return "PRE";
			case ScriptType::POST: return "POST";
			case ScriptType::HOLD: return "HOLD";
		}
		return "";
	}

	time_t _deferTime{0}; // How long to sleep when deferred.
	time_t _nextRunTime{0}; // The next time at which we're eligible to run (0 means any time).

	int _retValScript{-1}; // PRE Script exit code (Only valid for POST Script)
	int _retValJob{-1}; // First failed job exit code or 0 for success (Only valid for POST script)
	int _pid{0}; // Running script pid: 0 is not currently running
	int _deferStatus{0}; // Return value that prompts deferred execution

	ScriptType _type{}; // Script Type (PRE, POST, HOLD)

	bool _done{false}; // Script has been run?

private:
	Job *_node{nullptr};

	std::string _cmd{}; // cmd from DAG description file
	std::string _debugFile{}; // Path/Name of script debug file relative to node dir
	std::string _executedCMD{}; // Exact command ran w/ replaced macro info

	DagScriptOutput _output{DagScriptOutput::NONE}; // STD output stream to catch: None, stdout, stderr, all
};

#endif
