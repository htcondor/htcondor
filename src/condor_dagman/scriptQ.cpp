/***************************************************************
 *
 * Copyright (C) 1990-2026, Condor Team, Computer Sciences Department,
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
#include "scriptQ.h"

namespace shallow = DagmanShallowOptions;

// run script if possible, otherwise insert it into the waiting queue
ScriptExecResult ScriptQ::Run(Script *script, ScriptDeferAction act) {
	const char *prefix = script->GetScriptName();

	bool deferScript = false;

	// Defer PRE scripts if the DAG is halted (we need to go ahead
	// and run POST scripts so we don't "waste" the fact that the
	// job(s) completed).  (Allow PRE script for final node, though.)
	if (_dag->IsHalted() && script->_type == ScriptType::PRE && script->GetNode()->GetType() != NodeType::FINAL) {
		debug_printf(DEBUG_DEBUG_1, "Deferring %s script of node %s because DAG is halted\n",
		             prefix, script->GetNodeName());
		deferScript = true;
	}

	// Defer the script if we've hit the max PRE/POST scripts
	// running limit.
	int maxScripts = 20;
	switch(script->_type) {
		case ScriptType::PRE:
			maxScripts = _dag->dagOpts[shallow::i::MaxPre];
			break;
		case ScriptType::POST:
			maxScripts = _dag->dagOpts[shallow::i::MaxPost];
			break;
		case ScriptType::HOLD:
			maxScripts = _dag->dagOpts[shallow::i::MaxHold];
			break;
	}

	if (maxScripts != 0 && _numScriptsRunning >= maxScripts) {
		// max scripts already running
		debug_printf(DEBUG_DEBUG_1, "Max %s scripts (%d) already running; deferring %s script of node %s\n",
		             prefix, maxScripts, prefix, script->GetNodeName());
		deferScript = true;
	}

	if (deferScript) {
		_scriptDeferredCount++;
		if (act == ScriptDeferAction::PUSH_QUEUE) { _waitingQueue.push_back(script); }
		return ScriptExecResult::DEFERRED;
	}

	debug_printf(DEBUG_NORMAL, "Running %s script of Node %s...\n", prefix, script->GetNodeName());
	_dag->GetJobstateLog().WriteScriptStarted(script->GetNode(), script->_type);
	if (int pid = script->BackgroundRun(*_dag, _scriptReaperId)) {
		_numScriptsRunning++;
		auto insertResult = _scriptPidTable.insert(std::make_pair(pid, script));
		ASSERT(insertResult.second == true);
		debug_printf(DEBUG_DEBUG_1, "\tspawned pid %d: %s\n", pid, script->GetExecuted());
		return ScriptExecResult::SUCCESS;
	}

	// BackgroundRun() returned pid 0
	debug_printf(DEBUG_NORMAL, "Error: Failed to execute %s for Node %s\n",
	             prefix, script->GetNodeName());

	// Putting this code here fixes PR 906 (Missing PRE or POST script
	// causes DAGMan to hang); also, without this code a node for which
	// the script spawning fails will permanently add to the running
	// script count, which will throw off the maxpre/maxpost throttles.
	// wenger 2007-11-08
	const int returnVal = 1<<8;
	ReapScript(*script, returnVal);

	return ScriptExecResult::ERROR;
}

int ScriptQ::RunWaitingScripts(bool justOne) {
	// Just return if the queue is empty
	if (_waitingQueue.empty()) { return 0; }

	time_t now = time(nullptr);

	// Use std::ranges::stable_partition to separate ready scripts from deferred ones.
	// This is more efficient than the previous pop/push loop, especially when
	// many scripts are deferred. Complexity: O(n) vs O(n * deferred_count)
	auto defer_partition = std::ranges::stable_partition(_waitingQueue,
		[now](Script* script) {
			// Partition: ready scripts (true) come before deferred scripts (false)
			return script->_nextRunTime == 0 || script->_nextRunTime <= now;
		});

	// If we are to only start one script then manually try non-deferred script execution
	// until the first success
	if (justOne) {
		auto it = _waitingQueue.begin();
		auto end = defer_partition.begin();
		while (it != end) {
			Script* script = *it;
			ASSERT(script != nullptr);
			ScriptExecResult res = Run(script, ScriptDeferAction::DO_NOTHING);
			if (res != ScriptExecResult::DEFERRED) {
				_waitingQueue.erase(it);
				if (res == ScriptExecResult::SUCCESS) {
					return 1;
				}
			}
			it++;
		}

		// If here, then we didn't execute anything successfully
		return 0;
	}

	// If here: We want to attempt running all non-deferred scripts
	int scriptsRun = 0;

	// Use std::stable_partition as a driver to Run Scripts that are not deferred
	// Executed scripts will be removed from the waiting queue in a following erase
	// call. NOTE: This will iterate over all non-deferred scripts unfortunately (even if justOne is true)
	auto executed_end = std::stable_partition(_waitingQueue.begin(), defer_partition.begin(),
		[this, &scriptsRun](Script* script){
			// Partition: executed (true) before skipped/deferred (false)
			ASSERT(script != nullptr);
			bool remove_from_q = false;

			switch (Run(script, ScriptDeferAction::DO_NOTHING)) {
				case ScriptExecResult::SUCCESS:
					scriptsRun++;
					[[fallthrough]];
				// NOTE: Script execution error means DAGMan has faux reaped the script as a failure (thus consider it as executed)
				case ScriptExecResult::ERROR:
					remove_from_q = true;
					break;
				default:
					break;
			}

			return remove_from_q;
		});

	// Erase scripts that executed from the waiting queue
	// Waiting Queue Partitioning: [ Executed | Waiting (skipped) | Deferred ]
	_waitingQueue.erase(_waitingQueue.begin(), executed_end);

	debug_printf(DEBUG_DEBUG_1, "Started %d deferred scripts\n", scriptsRun);
	return scriptsRun;
}

int ScriptQ::ScriptReaper(int pid, int status) {
	// get the Script* that corresponds to this pid
	auto findResult = _scriptPidTable.find(pid);
	ASSERT(findResult != _scriptPidTable.end());
	Script* script = (*findResult).second;

	_scriptPidTable.erase(pid);
	_numScriptsRunning--;

	// Ensure reaper pid is the same as the script pid
	ASSERT(pid == script->_pid);

	script->WriteDebug(status);

	// Check to see if we should re-run this later.
	if (script->_deferStatus != SCRIPT_DEFER_STATUS_NONE && WEXITSTATUS(status) == script->_deferStatus) {
		++_scriptDeferredCount;
		script->_nextRunTime = time(nullptr) + script->_deferTime;
		_waitingQueue.push_back(script);
		debug_printf(DEBUG_NORMAL, "Deferring %s script of node %s for %ld seconds (exit status was %d)...\n",
		             script->GetScriptName(), script->GetNodeName(), script->_deferTime, script->_deferStatus);
	} else {
		script->_done = TRUE;
		ReapScript(*script, status);
	}

	// if there's another script waiting to run, run it now
	RunWaitingScripts(true);

	return 1;
}

void ScriptQ::ReapScript(Script& script, int status) {
	switch(script._type) {
		case ScriptType::PRE:
			_dag->PreScriptReaper(script.GetNode(), status);
			break;
		case ScriptType::POST:
			_dag->PostScriptReaper(script.GetNode(), status);
			break;
		case ScriptType::HOLD:
			_dag->HoldScriptReaper(script.GetNode());
			break;
	}
}
