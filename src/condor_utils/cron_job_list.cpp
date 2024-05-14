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
#include <list>
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_cron_job_mgr.h"
#include "condor_cron_job_list.h"

// Instantiate the list of Cron Jobs

// Basic constructor
CondorCronJobList::CondorCronJobList() {}

// Basic destructor
CondorCronJobList::~CondorCronJobList( void )
{
	// Walk through the list
	DeleteAll( "~" );
}

// Kill & delete all running jobs
int
CondorCronJobList::DeleteAll(const char * label)
{
	if (m_job_list.empty()) return 0;

	if ( ! label) label="";
	// Kill 'em all
	KillAll(true, label);

	dprintf( D_CRON, "%sCron: Deleting all (%d) jobs\n", label, (int)m_job_list.size() );
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		dprintf( D_CRON, "%sCron: Deleting job '%s'\n", label, job->GetName() );
		delete job;
	}
	m_job_list.clear();

	return 0;
}

// Kill all running jobs
int
CondorCronJobList::KillAll(bool force, const char * label)
{
	if (m_job_list.empty()) return 0;
	int alive = NumAliveJobs(); // alive is running or unreaped jobs.
	if ( ! alive) return 0;

	if ( ! label) label="";
	// Walk through the list
	dprintf( D_CRON, "%sCron: %sKilling all (%d) jobs\n", label, force?"force ":"", alive );
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		dprintf( D_CRON, "%sCron: Checking/Killing job %s\n", label, job->GetName() );
		job->KillJob( force );
	}
	return 0;
}

// Get the number of jobs not ready to shutdown, running or waiting to be reaped
int
CondorCronJobList::NumAliveJobs(std::string * names) const
{
	int			 num_alive = 0;

	// Walk through the list
	std::list<CronJob *>::const_iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		const CronJob	*job = *iter;
		if ( job->IsAlive( ) ) {
			if (names) {
				if ( ! names->empty()) names->append(",");
				names->append(job->GetName());
			}
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

	// Walk through the list
	std::list<CronJob *>::const_iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		const CronJob	*job = *iter;
		load += job->GetRunLoad();
	}
	return load;
}

// Get the number of jobs that are active (i.e. running or ready to run)
int
CondorCronJobList::NumActiveJobs() const
{
	int			 num_active = 0;

	// Walk through the list
	std::list<CronJob *>::const_iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		const CronJob	*job = *iter;
		if ( job->IsActive( ) ) {
			num_active++;
		}
	}
	return num_active;
}

// Get a string list representation of the current job list
bool
CondorCronJobList::GetStringList( std::vector<std::string> &sl ) const
{
	sl.clear();

	// Walk through the list
	std::list<CronJob *>::const_iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		const CronJob	*job = *iter;
		sl.emplace_back(job->GetName());
	}
	return true;
}

// Reconfigure all jobs
int
CondorCronJobList::HandleReconfig( void )
{
	// Walk through the list
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		job->HandleReconfig( );
	}
	return 0;
}

// Initialize all jobs
int
CondorCronJobList::InitializeAll( void )
{
	// Walk through the list
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		job->Initialize( );
	}
	return 0;
}

// Schedule all jobs
int
CondorCronJobList::ScheduleAll( void )
{
	// Walk through the list
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		job->Schedule( );
	}
	return 0;
}

// Schedule all jobs
int
CondorCronJobList::StartOnDemandJobs( void )
{
	int			 num = 0;

	// Walk through the list
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		if ( job->Params().IsOnDemand() ) {
			job->StartOnDemand( );
			num++;
		}
	}
	return num;
}

// Clear all job marks
void
CondorCronJobList::ClearAllMarks( void )
{
	// Walk through the list
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		job->ClearMark( );
	}
}

// Delete all unmarked jobs
void
CondorCronJobList::DeleteUnmarked( void )
{
	std::list<CronJob *>		 kill_list;

	// Walk through the list, delete all that didn't get marked
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		if ( ! job->IsMarked( ) ) {
			kill_list.push_back( job );
		}
	}

	for( iter = kill_list.begin(); iter != kill_list.end(); iter++ ) {
		CronJob	*job = *iter;
		dprintf( D_CRON, "Killing job %p '%s'\n", job, job->GetName() );
		job->KillJob( true );
		//dprintf( D_CRON | D_VERBOSE, "Erasing iterator\n" );
		m_job_list.remove( job );
		//dprintf( D_CRON | D_VERBOSE, "Deleting job %p\n", job );
		delete job;
	}
}

bool
CondorCronJobList::AddJob( const char *name, CronJob *job )
{
	// Do we already have a job by this name?
	if ( NULL != FindJob( name ) ) {
		dprintf( D_CRON,
				 "CronJobList: Not creating duplicate job '%s'\n", name );
		return false;
	}

	// It doesn't exit; put it on the list
	dprintf( D_CRON, "CronJobList: Adding job '%s'\n", name );
	m_job_list.push_back( job );

	// Done
	return true;
}

// Delete a job
int
CondorCronJobList::DeleteJob( const char *job_name )
{
	// Walk through the list, delete all that didn't get marked
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		if ( ! strcmp( job_name, job->GetName( ) ) ) {
			m_job_list.erase( iter );
			delete job;
			return 0;
		}
	}

	dprintf( D_CRON,
			 "CronJobList: Attempt to delete non-existent job '%s'\n",
			 job_name );
	return 1;
}

// Find a job from the list
CronJob *
CondorCronJobList::FindJob( const char *job_name )
{
	// Walk through the list
	std::list<CronJob *>::iterator iter;
	for( iter = m_job_list.begin(); iter != m_job_list.end(); iter++ ) {
		CronJob	*job = *iter;
		if ( ! strcmp( job_name, job->GetName( ) ) ) {
			return job;
		}
	}

	// No matches found
	return NULL;
}
