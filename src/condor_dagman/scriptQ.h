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
