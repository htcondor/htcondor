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

#include "HashTable.h"
#include "Queue.h"

#include "condor_daemon_core.h"

class Dag;
class Script;

// external reaper function type
typedef int (*extReaperFunc_t)( Job* job, int status );

// NOTE: the ScriptQ class must be derived from Service so we can
// register ScriptReaper() as a reaper function with DaemonCore

class ScriptQ : public Service {
 public:

	ScriptQ( Dag* dag );
	~ScriptQ();

	// runs script if possible, otherwise inserts into a wait queue
	// return: 1 means script was spawned, 0 means it was not (error
	// or deferred).
	int Run( Script *script );

	/** Run one waiting script, if possible.
		@return 1 if a script was spawned, 0 if not (error or deferred).
	*/
	int RunWaitingScript();

	/** Run all waiting scripts, if possible (keep trying until a script
		is not run).
		@return the number of scripts spawned.
	*/
	int RunAllWaitingScripts();

	/** Return the number of scripts actually running (does not include
	    scripts that are queued to run but have been deferred).
	*/
    int NumScriptsRunning();

    // reaper function for PRE & POST script completion
    int ScriptReaper( int pid, int status );

	// the number of script deferrals in this queue
	int GetScriptDeferredCount() const { return _scriptDeferredCount; };

 protected:

	Dag* _dag;

    // number of PRE/POST scripts currently running
	int _numScriptsRunning;

	// hash table to map PRE/POST script pids to Script* objects
	HashTable<int, Script*> *_scriptPidTable;

	// queue of scripts waiting to be run
	Queue<Script*> *_waitingQueue;

	// daemonCore reaper id for PRE/POST script reaper function
	int _scriptReaperId;

	// Total count of scripts deferred because of MaxPre or MaxPost limit
	// (note that a single script getting deferred multiple times is counted
	// multiple times).
	int _scriptDeferredCount;
};

#endif	// _SCRIPTQ_H
