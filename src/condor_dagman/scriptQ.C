/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
					  prefix, script->_job->GetJobName() );
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
					  script->_job->GetJobName() );
	}
	// max scripts already running
	debug_printf( DEBUG_VERBOSE, "Max %s scripts (%d) already running; "
				  "deferring %s script of Job %s\n", prefix, maxScripts,
				  prefix, script->_job->GetJobName() );

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
		_dag->PreScriptReaper( script->_job, status );
	} else {
		_dag->PostScriptReaper( script->_job, status );
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
