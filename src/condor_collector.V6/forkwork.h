/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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
#ifndef __FORKWORK_H__
#define __FORKWORD_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Return values of ForkerWorker::Fork()
enum ForkStatus { FORK_FAILED = -1, FORK_PARENT = 0, FORK_BUSY = 1, FORK_CHILD = 2 };

// A fork worker process
class ForkWorker
{
  public:
	ForkWorker( void );
	virtual ~ForkWorker( void );

	ForkStatus Fork( void );
	pid_t getPid( void ) { return pid; };
	pid_t getParent( void ) { return parent; };

  private:
	pid_t		pid;
	pid_t		parent;
	int			valid;
};

class ForkWork : public Service
{
  public:
	ForkWork( int max_workers = 0 );
	virtual ~ForkWork( );
	int Initialize( void );

	// Create / finish work
	ForkStatus NewJob( void );
	void WorkerDone( int exit_status = 0 );

	// # of worker stats
	void setMaxWorkers( int max_workers );
	int getNumWorkers( void ) { return workerList.Number(); };
	int getMaxwOrkers( void ) { return maxWorkers; };

	int DeleteAll( void );
	int KillAll( bool force );

  private:
	virtual int Reaper( int exitPid, int exitStatus );

  private:
	SimpleList<ForkWorker *>	workerList;
	int			maxWorkers;		// Max # of children
	int			reaperId;		// ID Of the child reaper
	bool			childExit;		// Am I an exiting child (don't kill things!!)
};

#endif // __FORKWORK_H__

