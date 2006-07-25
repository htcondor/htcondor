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

#include "condor_common.h"
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "forkwork.h"

// Instantiate the list of Cron Jobs
template class SimpleList<ForkWorker *>;

// Fork worker class constructor
ForkWorker::ForkWorker( void )
{
	valid = 0x5a5a;
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
		daemonCore->Forked_Child_Wants_Exit_By_Exec( true );
		parent = getppid( );
		pid = -1;
		return FORK_CHILD;
	} else {
		// Parent
		parent = getpid( );
		dprintf( D_JOB, "ForkWorker::Fork: New child of %d = %d\n",
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
}

// Finish initialization
int
ForkWork::Initialize( void )
{
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
	if ( workerList.Number() > maxWorkers ) {
		dprintf( D_JOB, "Warning: # forked workers exceeds max\n" );
	}
}

// Kill & delete all running workers
int
ForkWork::DeleteAll( void )
{
	ForkWorker	*worker;

	// Kill 'em all
	KillAll( true );

	// Walk through the list
	workerList.Rewind( );
	while ( workerList.Next( worker ) ) {
		workerList.DeleteCurrent( );
		delete worker;
	}
	return 0;
}

// Kill all running jobs
int
ForkWork::KillAll( bool force )
{
	ForkWorker	*worker;
	int		mypid = getpid();
	int		num_killed = 0;

	// Walk through the list
	workerList.Rewind( );
	while ( workerList.Next( worker ) ) {
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
		dprintf( D_JOB, "ForkWork %d: Killed %d jobs\n",
				 mypid, workerList.Number() );
	}
	return 0;
}

// Try to fork off another worker
ForkStatus
ForkWork::NewJob( void )
{
	// Any open slots?
	if ( workerList.Number() >= maxWorkers ) {
		if ( maxWorkers ) {
			dprintf( D_JOB, "ForkWork: busy\n" );
		}
		return FORK_BUSY;
	}

	// Fork off the child
	ForkWorker	*worker = new ForkWorker( );
	ForkStatus status = worker->Fork( );

	// Ok, let's see what happenned..
	if ( FORK_PARENT == status ) {
		workerList.Append( worker );
		return FORK_PARENT;
	} else if ( FORK_FAILED == status ) {
		delete worker;
		return FORK_FAILED;
	} else {
		delete worker;
		return FORK_CHILD;
	}
}

// Child worker is done
void
ForkWork::WorkerDone( int exit_status )
{
	dprintf( D_JOB,
			 "ForkWork: Child %d done, status %d\n",
			 getpid(), exit_status );
	exit( exit_status );
}

// Child reaper
int
ForkWork::Reaper( int exitPid, int exitStatus )
{
	ForkWorker	*worker;

	// Let's find out if it's one of our children...
	workerList.Rewind( );
	while ( workerList.Next( worker ) ) {
		if ( worker->getPid() == exitPid ) {
			workerList.DeleteCurrent( );
			delete worker;	
		return 0;
		}
	}
	return 0;
}
