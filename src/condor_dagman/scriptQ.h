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


#ifndef _SCRIPTQ_H
#define _SCRIPTQ_H

#include <utility>
#include <queue>

#include "condor_daemon_core.h"

class Dag;
class Script;

// external reaper function type
typedef int (*extReaperFunc_t)(Node* node, int status);

// NOTE: the ScriptQ class must be derived from Service so we can
// register ScriptReaper() as a reaper function with DaemonCore

class ScriptQ : public Service {
public:

	ScriptQ(Dag* dag) {
		_dag = dag;
		// register daemonCore reaper for PRE/POST/HOLD script completion
		_scriptReaperId = daemonCore->Register_Reaper("PRE/POST/HOLD Script Reaper",
		                                              (ReaperHandlercpp)&ScriptQ::ScriptReaper,
		                                              "ScriptQ::ScriptReaper", this);
	}

	// Being executing a script
	bool Run(Script *script);

	// Run waiting/deferred scripts. Return number of started scripts
	int RunWaitingScripts(bool justOne = false);

	// Catch all script reaper function
	int ScriptReaper(int pid, int status);

	// Number of running scripts
	int NumScriptsRunning() const { return _numScriptsRunning; };

	// Number of deferred scripts
	int GetScriptDeferredCount() const { return _scriptDeferredCount; };

private:
	// Call DAG level script reaper function
	void ReapScript(Script& script, int status);

	Dag* _dag{nullptr};

	std::map<int, Script*> _scriptPidTable{}; // map script pids to Script* objects
	std::queue<Script*> _waitingQueue{}; // queue of scripts waiting to be run

	int _scriptReaperId{-1};
	int _numScriptsRunning{0};
	int _scriptDeferredCount{0}; // Number of scripts deferred due to throttling limits
};

#endif	// _SCRIPTQ_H
