/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "script.h"
#include "util.h"
#include "job.h"
#include "types.h"

#include "dag.h"

ScriptQ::ScriptQ( Dag* dag )
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
		debug_printf( DEBUG_NORMAL, "Running %s script of Job %s...\n",
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
					  "failed; deferring %s script of Job %s\n", prefix,
					  script->GetNodeName() );
	}
	else {
			// max scripts already running
		debug_printf( DEBUG_VERBOSE, "Max %s scripts (%d) already running; "
					  "deferring %s script of Job %s\n", prefix, maxScripts,
					  prefix, script->GetNodeName() );
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

	ASSERT( pid == script->_pid );
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
