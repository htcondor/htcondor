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
};

#endif	// _SCRIPTQ_H
