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

#ifndef __FORKWORK_H__
#define __FORKWORK_H__

#include "condor_daemon_core.h"

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
	pid_t getPid( void ) const { return pid; };
	pid_t getParent( void ) const { return parent; };

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
	int getMaxWorkers( void ) const { return maxWorkers; };
	int getPeakWorkers( void ) const { return peakWorkers; };

	int DeleteAll( void );
	int KillAll( bool force );

  private:
	virtual int Reaper( int exitPid, int exitStatus );

  private:
	SimpleList<ForkWorker *>	workerList;
	int			maxWorkers;		// Max # of children allowed
	int			peakWorkers;	// Most # of children alive at once
	int			reaperId;		// ID Of the child reaper
	bool		childExit;		// Am I an exiting child (don't kill things!!)
};

#endif // __FORKWORK_H__

