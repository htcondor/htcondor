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

#include <limits.h>
#include <string.h>
#include "condor_common.h"
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
		dprintf( D_JOB, "ForkWork %d: Killed %d jobs\n", mypid, workerList.Number() );
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
