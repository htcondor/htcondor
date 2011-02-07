/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include "condor_cron_job_mgr.h"
#include "condor_cron_job_list.h"

// Instantiate the list of Cron Jobs

// Basic constructor
CondorCronJobList::CondorCronJobList( CronJobMgr &mgr )
		: m_mgr( mgr )
{
}

// Basic destructor
CondorCronJobList::~CondorCronJobList( void )
{
	// Walk through the list
	DeleteAll( );
}

// Kill & delete all running jobs
int
CondorCronJobList::DeleteAll( void )
{
	CronJob	*cur_job;

	// Kill 'em all
	KillAll( true );

	// Walk through the list
	dprintf( D_ALWAYS, "CronJobList: Deleting all jobs\n" );
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		dprintf( D_ALWAYS,
				 "CronJobList: Deleting job '%s'\n",
				 cur_job->GetName() );
		m_job_list.DeleteCurrent( );
		delete cur_job;
	}
	return 0;
}

// Kill all running jobs
int
CondorCronJobList::KillAll( bool force )
{
	CronJob	*cur_job;

	// Walk through the list
	dprintf( D_ALWAYS, "Cron: Killing all jobs\n" );
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		cur_job->KillJob( force );
	}
	return 0;
}

// Get the number of jobs not ready to shutdown
int
CondorCronJobList::NumAliveJobs( void ) const
{
	int			 num_alive = 0;
	CronJob		*cur_job;

	// Walk through the list
	SimpleList<CronJob *>	*jl = 
		const_cast<SimpleList<CronJob*> *>(&m_job_list);
	jl->Rewind( );
	while ( jl->Next( cur_job ) ) {
		if ( cur_job->IsAlive( ) ) {
			num_alive++;
		}
	}
	return num_alive;
}

// Get the "load" of all currently running jobs
double
CondorCronJobList::RunningJobLoad( void ) const
{
	double		 load = 0.0;
	CronJob		*cur_job;

	// Walk through the list
	SimpleList<CronJob *>	*jl = 
		const_cast<SimpleList<CronJob*> *>(&m_job_list);
	jl->Rewind( );
	while ( jl->Next( cur_job ) ) {
		load += cur_job->GetRunLoad();
	}
	return load;
}

// Get the number of jobs that are active
int
CondorCronJobList::NumActiveJobs( void ) const
{
	int			 num_active = 0;
	CronJob		*cur_job;

	// Walk through the list
	SimpleList<CronJob *>	*jl = 
		const_cast<SimpleList<CronJob *> *>(&m_job_list);
	jl->Rewind( );
	while ( jl->Next( cur_job ) ) {
		if ( cur_job->IsActive( ) ) {
			num_active++;
		}
	}
	return num_active;
}

// Reconfigure all jobs
int
CondorCronJobList::HandleReconfig( void )
{
	CronJob		*cur_job;

	// Walk through the list
	m_job_list.Rewind( );
	while ( m_job_list.Next(cur_job) ) {
		cur_job->HandleReconfig( );
	}
	return 0;
}

// Initialize all jobs
int
CondorCronJobList::InitializeAll( void )
{
	CronJob		*cur_job;

	// Walk through the list
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		cur_job->Initialize( );
	}
	return 0;
}

// Schedule all jobs
int
CondorCronJobList::ScheduleAll( void )
{
	CronJob		*cur_job;

	// Walk through the list
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		cur_job->Schedule( );
	}
	return 0;
}

// Schedule all jobs
int
CondorCronJobList::StartOnDemandJobs( void )
{
	CronJob		*cur_job;
	int			 num = 0;

	// Walk through the list
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		if ( cur_job->Params().IsOnDemand() ) {
			cur_job->StartOnDemand( );
			num++;
		}
	}
	return num;
}

// Clear all job marks
void
CondorCronJobList::ClearAllMarks( void )
{
	CronJob		*cur_job;

	// Walk through the list
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		cur_job->ClearMark( );
	}
}

// Delete all unmarked jobs
void
CondorCronJobList::DeleteUnmarked( void )
{
	CronJob		*cur_job;

	// Walk through the list, delete all that didn't get marked
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		if ( ! cur_job->IsMarked( ) ) {

			// Delete it from the list
			m_job_list.DeleteCurrent( );

			// Finally, delete the job itself
			delete cur_job;
		}
	}
}

bool
CondorCronJobList::AddJob( const char *name, CronJob *job )
{
	// Do we already have a job by this name?
	if ( NULL != FindJob( name ) ) {
		dprintf( D_ALWAYS,
				 "CronJobList: Not creating duplicate job '%s'\n", name );
		return false;
	}

	// It doesn't exit; put it on the list
	dprintf( D_ALWAYS, "CronJobList: Adding job '%s'\n", name );
	m_job_list.Append( job );

	// Done
	return true;
}

// Delete a job
int
CondorCronJobList::DeleteJob( const char *job_name )
{
	// Does the job exist?
	const CronJob *job = FindJob( job_name );
	if ( NULL == job ) {
		dprintf( D_ALWAYS,
				 "CronJobList: Not deleting non-existent job '%s'\n",
				 job_name );
		return 1;
	}

	// Remove it from the list
	dprintf( D_ALWAYS, "CronJobList: Deleting job '%s'\n", job_name );
	m_job_list.DeleteCurrent( );

	// After that, free up it's resources..
	delete( job );

	// Done
	return 0;
}

// Find a job from the list
CronJob *
CondorCronJobList::FindJob( const char *job_name )
{
	CronJob		*cur_job;

	// Walk through the list
	m_job_list.Rewind( );
	while ( m_job_list.Next( cur_job ) ) {
		if ( ! strcmp( job_name, cur_job->GetName( ) ) ) {
			return cur_job;
		}
	}

	// No matches found
	return NULL;
}
