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
    _waitingQueue = new Queue<Script*>();

    if( _scriptPidTable == NULL || _waitingQueue == NULL ) {
        EXCEPT( "ERROR: out of memory!\n");
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

// run script if possible, otherwise insert it into the waiting queue
int
ScriptQ::Run( Script *script )
{
	char prefix[8];
	strcpy( prefix, script->_post ? "POST" : "PRE" );
	int maxScripts =
		script->_post ? _dag->_maxPostScripts : _dag->_maxPreScripts;
	// if we have no script limit, or we're under the limit, run now
	if( maxScripts == 0 || _numScriptsRunning < maxScripts ) {
		debug_printf( DEBUG_NORMAL, "Running %s script of Node %s...\n",
					  prefix, script->GetNodeName() );
		if( int pid = script->BackgroundRun( _scriptReaperId ) ) {
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
			_dag->PreScriptReaper( script->GetNodeName(), returnVal );
		} else {
			_dag->PostScriptReaper( script->GetNodeName(), returnVal );
		}

		return 0;
	}
	else {
			// max scripts already running
		debug_printf( DEBUG_DEBUG_1, "Max %s scripts (%d) already running; "
					  "deferring %s script of Job %s\n", prefix, maxScripts,
					  prefix, script->GetNodeName() );
		_scriptDeferredCount++;
	}
	_waitingQueue->enqueue( script );
	return 0;
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

	// get the Job* that corresponds to this pid
	_scriptPidTable->lookup( pid, script );
	ASSERT( script != NULL );

	_scriptPidTable->remove( pid );
	_numScriptsRunning--;

	if ( pid != script->_pid ) {
		EXCEPT( "Reaper pid (%d) does not match expected script pid (%d)!",
					pid, script->_pid );
	}
	script->_done = TRUE;

	// call appropriate DAG reaper
	if( ! script->_post ) {
		_dag->PreScriptReaper( script->GetNodeName(), status );
	} else {
		_dag->PostScriptReaper( script->GetNodeName(), status );
	}

	// if there's another script waiting to run, run it now
	script = NULL;
	if( !_waitingQueue->IsEmpty() ) {
		_waitingQueue->dequeue( script );
		ASSERT( script != NULL );
		Run( script );
	}

	return 1;
}
