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

    _scriptPidTable = new HashTable<int,Script*>( 499, &hashFuncInt );
    _waitingQueue = new Queue<Script*>();

    if( _scriptPidTable == NULL || _waitingQueue == NULL ) {
        EXCEPT( "ERROR: out of memory (%s() in %s:%d)!\n",
                __FUNCTION__, __FILE__, __LINE__ );
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
	// if we have no script limit, or we're under the limit, run now
	if( _dag->_maxScriptsRunning == 0 ||
		_numScriptsRunning <= _dag->_maxScriptsRunning ) {
		if( int pid = script->BackgroundRun( _scriptReaperId ) ) {
			_numScriptsRunning++;
			_scriptPidTable->insert( pid, script );
			if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) ) {
				printf( "  spawned pid %d: %s\n", pid, script->_cmd );
			}
			return 1;
		}
		// BackgroundRun() returned pid 0
		else if( DEBUG_LEVEL( DEBUG_QUIET ) ) {
			printf( "  error: daemonCore->Create_Process() failed; deferring "
					"%s script of Job ", script->_post ? "POST" : "PRE" );
			script->_job->Print();
			printf( "\n" );
		}
	}
	// max scripts already running
	else if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) ) {
		printf( "  max. scripts (%d) already running; deferring %s script of "
				"Job ", _dag->_maxScriptsRunning,
				script->_post ? "POST" : "PRE" );
		script->_job->Print();
		printf( "\n" );
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
	assert( script != NULL );

	_scriptPidTable->remove( pid );
	_numScriptsRunning--;

	assert( pid == script->_pid );
	script->_pid = 0;

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
		assert( script != NULL );
		Run( script );
	}

	return 1;
}
