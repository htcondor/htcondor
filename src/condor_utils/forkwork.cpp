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


#include "condor_common.h"
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "forkwork.h"
#include <algorithm>

// Instantiate the list of Cron Jobs

// Fork worker class constructor
ForkWorker::ForkWorker( void )
{
	valid = 0x5a5a;
	pid = -1;
	parent = -1;
}

// Fork worker class destructor
ForkWorker::~ForkWorker( void )
{
	if ( valid != 0x5a5a ) {
		dprintf( D_ALWAYS, "ForkWorker: delete invalid!!\n" );
	}
	valid = 0;
}

// Fork off a real worker job
ForkStatus
ForkWorker::Fork( void )
{
# ifndef WIN32
	// Fork
	pid = fork( );

	if ( pid < 0 ) {
		dprintf( D_ALWAYS, "ForkWorker::Fork: Fork failed\n" );
		return FORK_FAILED;
	} else if ( 0 == pid ) {
			// We should really be using DC::Create_Thread() for this.
			// However, since we're not, we need to call this method
			// to tell DC we're a forked child and that we want to
			// exit via exec(), not using exit(), so that destructors
			// don't get called...
		daemonCore->Forked_Child_Wants_Fast_Exit( true );
			// Release the debug lock if we have it, and don't fight
			// with our parent to rotate the debug log file.
		dprintf_init_fork_child();
		parent = getppid( );
		pid = -1;
		return FORK_CHILD;
	} else {
		// Parent
		parent = getpid( );
		dprintf( D_FULLDEBUG, "ForkWorker::Fork: New child of %d = %d\n",
				 parent,  pid );
		return FORK_PARENT;
	}
# else
	return FORK_FAILED;
# endif
}

// Fork work constructor
ForkWork::ForkWork( int max_workers )
{
# ifdef WIN32
	max_workers = 0;
# endif
	maxWorkers = max_workers;
	peakWorkers = 0;
	reaperId = -1;
	childExit = false;
}

// Finish initialization
int
ForkWork::Initialize( void )
{
	if( reaperId != -1 ) {
			// already initialized
		return 0;
	}

	// Register my reaper
	reaperId = daemonCore->Register_Reaper( 
		"ForkWork_Reaper",
		(ReaperHandlercpp) &ForkWork::Reaper,
		"ForkWork Reaper",
		this );
    daemonCore->Set_Default_Reaper( reaperId );
	return 0;
}

// Fork work denstructor
ForkWork::~ForkWork( void )
{
	// Kill 'em all
	DeleteAll( );
}

// Set max # of children
void
ForkWork::setMaxWorkers( int max_workers )
{
# ifdef WIN32
	max_workers = 0;
# endif
	maxWorkers = max_workers;
	if ( (int)workerList.size() > maxWorkers ) {
		dprintf( D_FULLDEBUG, "Warning: # forked workers (%zu) exceeds new max (%d)\n", workerList.size(), maxWorkers );
	}
}

// Kill & delete all running workers
int
ForkWork::DeleteAll( void )
{
	// Kill 'em all
	KillAll( true );

	// Walk through the list
	for (ForkWorker *worker: workerList) {
		delete worker;
	}
	workerList.clear();
	return 0;
}

// Kill all running jobs
int
ForkWork::KillAll( bool force )
{
	int		mypid = getpid();
	int		num_killed = 0;

	// Walk through the list
	for (ForkWorker *worker: workerList) {
		// If I'm the parent, kill it
		if ( mypid == worker->getParent() ) {
			num_killed++;
			if ( force ) {
				daemonCore->Send_Signal( worker->getPid(), SIGKILL );
			} else {
				daemonCore->Send_Signal( worker->getPid(), SIGTERM );
			}
		}
	}

	// If we killed some, log it...
	if ( num_killed ) {
		dprintf( D_ALWAYS, "ForkWork %d: Killed %zu jobs\n",
				 mypid, workerList.size() );
	}
	return 0;
}

// Try to fork off another worker
ForkStatus
ForkWork::NewJob( void )
{
	ForkStatus status = FORK_BUSY;
	
	// Any open slots?
	if ( (int) workerList.size() >= maxWorkers ) {
		if ( maxWorkers ) {
			dprintf( D_ALWAYS, "ForkWork: not forking because reached max workers %d\n", maxWorkers );
		}
	}
	else
	{
	  // Fork off the child
	  ForkWorker	*worker = new ForkWorker( );
	  status = worker->Fork( );

	  // Ok, let's see what happenned..
	  if ( FORK_PARENT == status ) {
		  dprintf( D_ALWAYS, "Number of Active Workers %zu\n", workerList.size());
		  workerList.push_back( worker );
		  peakWorkers = std::max( peakWorkers, (int)workerList.size() );
	  } else if ( FORK_FAILED == status ) {
		  delete worker;
	  } else {
		  delete worker;
		  status = FORK_CHILD;
	  }
	}
	
	
	return status;
}

// Child worker is done
void
ForkWork::WorkerDone( int exit_status )
{
	dprintf( D_FULLDEBUG,
			 "ForkWork: Child %d done, status %d\n",
			 getpid(), exit_status );
	exit( exit_status );
}

// Child reaper
int
ForkWork::Reaper( int exitPid, int /*exitStatus*/ )
{
	// Let's find out if it's one of our children...
	auto findChild = [exitPid](const ForkWorker *worker) {
		if (worker->getPid() == exitPid) {
			delete worker;
			return true;
		}
		return false;
	};

	auto it = std::remove_if(workerList.begin(), workerList.end(),
			findChild);
	workerList.erase(it, workerList.end());
	return 0;
}
