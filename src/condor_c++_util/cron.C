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
#include "condor_cronmgr.h"
#include "condor_cron.h"

// Instantiate the list of Cron Jobs
template class SimpleList<CondorCronJob*>;

// Basic constructor
CondorCron::CondorCron( )
{
}

// Basic destructor
CondorCron::~CondorCron( )
{
	// Walk through the list
	KillAll( );
}

// Kill all running jobs
int
CondorCron::KillAll( )
{
	CondorCronJob	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		JobList.DeleteCurrent( );
		delete curJob;
	}
	return 0;
}

// Reconfigure all jobs
int
CondorCron::Reconfig( )
{
	CondorCronJob	*curJob;

	// Walk through the list
	JobList.Rewind( );
	while ( JobList.Next( curJob ) ) {
		curJob->Reconfig( );
	}
	return 0;
}

// Clear all job marks
void
CondorCron::ClearAllMarks( )
{
	CondorCronJob	*curJob;

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
	CondorCronJob	*curJob;

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
CondorCron::AddJob( const char *name, CondorCronJob *newJob )
{
	// Do we already have a job by this name?
	if ( NULL != FindJob( name ) ) {
		dprintf( D_ALWAYS, "Cron: Not creating duplicate job '%s'\n", name );
		return 1;
	}

	// It doesn't exit; put it on the list
	JobList.Append( newJob );

	// Done
	return 0;
}

// Delete a job
int
CondorCron::DeleteJob( const char *jobName )
{
	// Does the job exist?
	const CondorCronJob *Job = FindJob( jobName );
	if ( NULL == Job ) {
		dprintf( D_ALWAYS, "Cron: Not deleting non-existent job '%s'\n",
				 jobName );
		return 1;
	}

	// Remove it from the list
	JobList.DeleteCurrent( );

	// After that, free up it's resources..
	delete( Job );

	// Done
	return 0;
}

// Find a job from the list
CondorCronJob *
CondorCron::FindJob( const char *jobName )
{
	CondorCronJob	*curJob;

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
