/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef _SCRIPTQ_H
#define _SCRIPTQ_H

#include "HashTable.h"
#include "Queue.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

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
	int Run( Script *script );

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
