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

#include "condor_common.h"
#include <limits.h>
#include <string.h>
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_cronmgr.h"
#include "condor_cron.h"

// Instantiate the list of Cron Jobs
template class SimpleList<CronJobBase*>;

// Basic constructor
CondorCron::CondorCron( )
{
}

// Basic destructor
CondorCron::~CondorCron( )
{
	// Walk through the list
	DeleteAll( );
}

// Kill & delete all running jobs
int
CondorCron::DeleteAll( )
{
	CronJobBase	*curJob;

	// Kill 'em all
	KillAll( true );

	// Walk through the list
	dprintf( D_JOB, "Cron: Deleting all jobs\n" );
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		dprintf( D_JOB, "Cron: Deleting job '%s'\n", curJob->GetName() );
		JobList.DeleteCurrent( );
		delete curJob;
	}
	return 0;
}

// Kill all running jobs
int
CondorCron::KillAll( bool force )
{
	CronJobBase	*curJob;

	// Walk through the list
	dprintf( D_JOB, "Cron: Killing all jobs\n" );
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		curJob->KillJob( force );
	}
	return 0;
}

// Get the number of jobs not ready to shutdown
int
CondorCron::NumAliveJobs( void )
{
	int				NumAlive = 0;
	CronJobBase	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		if ( curJob->IsAlive( ) ) {
			NumAlive++;
		}
	}
	return NumAlive;
	
}

// Reconfigure all jobs
int
CondorCron::Reconfig( )
{
	CronJobBase	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		curJob->Reconfig( );
	}
	return 0;
}

// Initialize all jobs
int
CondorCron::InitializeAll( )
{
	CronJobBase	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		curJob->Initialize( );
	}
	return 0;
}

// Clear all job marks
void
CondorCron::ClearAllMarks( )
{
	CronJobBase	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		curJob->ClearMark( );
	}
}

// Delete all unmarked jobs
void
CondorCron::DeleteUnmarked( )
{
	CronJobBase	*curJob;

	// Walk through the list, delete all that didn't get marked
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		if ( ! curJob->IsMarked( ) ) {

			// Delete it from the list
			JobList.DeleteCurrent( );

			// Finally, delete the job itself
			delete curJob;
		}
	}
}

int
CondorCron::AddJob( const char *name, CronJobBase *newJob )
{
	// Do we already have a job by this name?
	if ( NULL != FindJob( name ) ) {
		dprintf( D_ALWAYS, "Cron: Not creating duplicate job '%s'\n", name );
		return 1;
	}

	// It doesn't exit; put it on the list
	dprintf( D_JOB, "Cron: Adding job '%s'\n", name );
	JobList.Append( newJob );

	// Done
	return 0;
}

// Delete a job
int
CondorCron::DeleteJob( const char *jobName )
{
	// Does the job exist?
	const CronJobBase *Job = FindJob( jobName );
	if ( NULL == Job ) {
		dprintf( D_ALWAYS, "Cron: Not deleting non-existent job '%s'\n",
				 jobName );
		return 1;
	}

	// Remove it from the list
	dprintf( D_JOB, "Cron: Deleting job '%s'\n", jobName );
	JobList.DeleteCurrent( );

	// After that, free up it's resources..
	delete( Job );

	// Done
	return 0;
}

// Find a job from the list
CronJobBase *
CondorCron::FindJob( const char *jobName )
{
	CronJobBase	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		if ( ! strcmp( jobName, curJob->GetName( ) ) ) {
			return curJob;
		}
	}

	// No matches found
	return NULL;
}
