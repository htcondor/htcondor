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

#include "dag.h"

ScriptQ::ScriptQ( Dag* dag ) : 
	_scriptDeferredCount	(0)
{
	_dag = dag;
	_numScriptsRunning = 0;

    _scriptPidTable = new HashTable<int,Script*>( 127, &hashFuncInt );
    _waitingQueue = new Queue<std::pair<Script*, int> *>();

    if( _scriptPidTable == NULL || _waitingQueue == NULL ) {
        EXCEPT( "ERROR: out of memory!");
    }

 	// register daemonCore reaper for PRE/POST script completion
    _scriptReaperId =
		daemonCore->Register_Reaper( "PRE/POST Script Reaper",
									 (ReaperHandlercpp)&ScriptQ::ScriptReaper,
									 "ScriptQ::ScriptReaper", this );
}

ScriptQ::~ScriptQ()
{
	// should we un-register the daemonCore reaper here?
	delete _scriptPidTable;
	delete _waitingQueue;
};

//TEMPTEMP -- merge this into RunAllWaitingScripts/RunWaitingScript
int
ScriptQ::CheckDeferredScripts()
{
	std::pair<Script *, int> *scriptInfo = NULL;
	std::pair<Script *, int> *firstScriptInfo = NULL;
	int now = time(NULL);
	unsigned startedThisRound = 0;
	while (!_waitingQueue->IsEmpty()) {
		_waitingQueue->dequeue( scriptInfo );
		ASSERT( (scriptInfo != NULL) && (scriptInfo->first != NULL) );//TEMPTEMP -- valgrind says invalid read here! (reading freed memory)
		if ( !firstScriptInfo ) {
			firstScriptInfo = scriptInfo;
		//TEMPTEMP -- is this a dummy node or something?
		} else if ( firstScriptInfo == scriptInfo) {
			_waitingQueue->enqueue( scriptInfo );
			break;
		}
		if (scriptInfo->second <= now) {//TEMPTEMP -- valgrind says invalid read here! (reading freed memory)
			Script *script = scriptInfo->first;
			int maxScripts = script->_post ? _dag->_maxPostScripts : _dag->_maxPreScripts;
			if ( maxScripts != 0 && (_numScriptsRunning >= maxScripts) ) {
				_waitingQueue->enqueue( scriptInfo );
			}
			delete scriptInfo; //TEMPTEMP -- valgrind says invalid free here (already freed)
			startedThisRound += Run(script);
			firstScriptInfo = NULL;
		} else {
			_waitingQueue->enqueue( scriptInfo );
		}
	}
	debug_printf( DEBUG_NORMAL, "Started %d deferred scripts\n", startedThisRound );
	return startedThisRound;
}

// run script if possible, otherwise insert it into the waiting queue
int
ScriptQ::Run( Script *script )
{
	//TEMP -- should ScriptQ object know whether it's pre or post?
	const char *prefix = script->_post ? "POST" : "PRE";

	bool deferScript = false;

		// Defer PRE scripts if the DAG is halted (we need to go ahead
		// and run POST scripts so we don't "waste" the fact that the
		// job completed).  (Allow PRE script for final node, though.)
	if ( _dag->IsHalted() && !script->_post &&
				!script->GetNode()->GetFinal() ) {
		debug_printf( DEBUG_DEBUG_1,
					"Deferring %s script of node %s because DAG is halted\n",
					prefix, script->GetNodeName() );
		deferScript = true;
	}

		// Defer the script if we've hit the max PRE/POST scripts
		// running limit.
		//TEMP -- the scriptQ object should really know the max scripts
		// limit, instead of getting it from the Dag object.  wenger 2015-03-18
	int maxScripts =
		script->_post ? _dag->_maxPostScripts : _dag->_maxPreScripts;
	if ( maxScripts != 0 && _numScriptsRunning >= maxScripts ) {
			// max scripts already running
		debug_printf( DEBUG_DEBUG_1, "Max %s scripts (%d) already running; "
					  "deferring %s script of Job %s\n", prefix, maxScripts,
					  prefix, script->GetNodeName() );
		deferScript = true;
	}

	if ( deferScript ) {
		_scriptDeferredCount++;
		_waitingQueue->enqueue( new std::pair<Script*, int>(script, 0) );
		return 0;
	}

	debug_printf( DEBUG_NORMAL, "Running %s script of Node %s...\n",
				  prefix, script->GetNodeName() );
	_dag->GetJobstateLog().WriteScriptStarted( script->GetNode(),
				script->_post );
	if( int pid = script->BackgroundRun( _scriptReaperId,
				_dag->_dagStatus, _dag->NumNodesFailed() ) ) {
		_numScriptsRunning++;
		_scriptPidTable->insert( pid, script );
		debug_printf( DEBUG_DEBUG_1, "\tspawned pid %d: %s\n", pid,
					  script->_cmd );
		return 1;
	}
	// BackgroundRun() returned pid 0
	debug_printf( DEBUG_NORMAL, "  error: daemonCore->Create_Process() "
				  "failed; %s script of Job %s failed\n", prefix,
				  script->GetNodeName() );

	// Putting this code here fixes PR 906 (Missing PRE or POST script
	// causes DAGMan to hang); also, without this code a node for which
	// the script spawning fails will permanently add to the running
	// script count, which will throw off the maxpre/maxpost throttles.
	// wenger 2007-11-08
	const int returnVal = 1<<8;
	if( ! script->_post ) {
		_dag->PreScriptReaper( script->GetNode(), returnVal );
	} else {
		_dag->PostScriptReaper( script->GetNode(), returnVal );
	}

	return 0;
}

int
ScriptQ::RunWaitingScript()
{
	std::pair<Script *, int> *firstScriptInfo = NULL;
	std::pair<Script *, int> *scriptInfo = NULL;

	time_t now = time(NULL);
	while( !_waitingQueue->IsEmpty() ) {
		_waitingQueue->dequeue( scriptInfo );
		if ( !firstScriptInfo ) {
			firstScriptInfo = scriptInfo;
		} else if ( scriptInfo == firstScriptInfo ) {
			_waitingQueue->enqueue( scriptInfo );
			break;
		}
		ASSERT( (scriptInfo != NULL) && (scriptInfo->first != NULL) );
		if ( now >= scriptInfo->second ) {
			Script *script = scriptInfo->first;
			delete scriptInfo;
			return Run( script );
		} else {
			_waitingQueue->enqueue( scriptInfo );
		}
	}

	return 0;
}

int
ScriptQ::RunAllWaitingScripts()
{
	int scriptsRun = 0;

	while ( RunWaitingScript() > 0 ) {
		scriptsRun++;
	}

	debug_printf( DEBUG_DEBUG_1, "Ran %d scripts\n", scriptsRun );
	return scriptsRun;
}

int
ScriptQ::NumScriptsRunning()
{
	return _numScriptsRunning;
}

int
ScriptQ::ScriptReaper( int pid, int status )
{
	Script* script = NULL;

	// get the Script* that corresponds to this pid
	_scriptPidTable->lookup( pid, script );
	ASSERT( script != NULL );

	_scriptPidTable->remove( pid );
	_numScriptsRunning--;

	if ( pid != script->_pid ) {
		EXCEPT( "Reaper pid (%d) does not match expected script pid (%d)!",
					pid, script->_pid );
	}

	// Check to see if we should re-run this later.
	if ( status == script->_defer_status ) {
		std::pair<Script *, int> *scriptInfo = new std::pair<Script *, int>(script, time(NULL)+script->_defer_time);
		_waitingQueue->enqueue( scriptInfo );
		const char *prefix = script->_post ? "POST" : "PRE";
		debug_printf( DEBUG_NORMAL, "Deferring %s script of Node %s for %ld seconds (exit status was %d)...\n",
			prefix, script->GetNodeName(), script->_defer_time, script->_defer_status );
	}
	else
	{
		script->_done = TRUE;

		// call appropriate DAG reaper
		if( ! script->_post ) {
			_dag->PreScriptReaper( script->GetNode(), status );
		} else {
			_dag->PostScriptReaper( script->GetNode(), status );
		}
	}

	// if there's another script waiting to run, run it now
	RunWaitingScript();

	return 1;
}
