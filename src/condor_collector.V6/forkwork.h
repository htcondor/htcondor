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
#ifndef __FORKWORK_H__
#define __FORKWORD_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Return values of ForkerWorker::Fork()
enum ForkStatus {
	FORK_FAILED = -1,
	FORK_PARENT = 0,
	FORK_BUSY = 1,
	FORK_CHILD = 2
};

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

