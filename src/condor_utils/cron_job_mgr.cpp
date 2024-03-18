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
#include "condor_config.h"
#include "condor_cron_job_list.h"
#include "condor_cron_job_mgr.h"
#include "condor_cron_job_params.h"

// Basic constructor
CronJobMgr::CronJobMgr( void ) 
		: m_job_list(),
		  m_name( NULL ),
		  m_param_base( NULL ),
		  m_params( NULL ),
		  m_config_val_prog( NULL ),
		  m_max_job_load( 0.2 ),
		  m_cur_job_load( 0.0 ),
		  m_schedule_timer(-1)
{
}

// Basic destructor
CronJobMgr::~CronJobMgr( void )
{
	// Kill all running jobs
	m_job_list.DeleteAll( m_name );

	// Free up name, etc. buffers
	if ( NULL != m_name ) {
		free( const_cast<char *>(m_name) );
	}
	if ( NULL != m_param_base ) {
		free( const_cast<char *>(m_param_base) );
	}
	if( m_config_val_prog ) {
		free( const_cast<char *>(m_config_val_prog) );
	}
	if ( m_params ) {
		delete m_params;
	}

	// Log our death
	dprintf( D_CRON | D_VERBOSE, "CronJobMgr: bye\n" );
}

// Handle initialization
int
CronJobMgr::Initialize( const char *name )
{
	dprintf( D_CRON | D_VERBOSE, "CronJobMgr: Initializing '%s'\n", name );
	if ( DoConfig( true ) ) {
		return false;
	}
	return ScheduleAllJobs( ) ? 0 : -1;
}

// Set new name..
int
CronJobMgr::SetName( const char *name, 
					 const char *param_base,
					 const char *param_ext )
{
	int		retval = 0;

	// Debug...
	dprintf( D_CRON | D_VERBOSE, "CronJobMgr: Setting name to '%s'\n", name );
	if ( NULL != m_name ) {
		free( const_cast<char *>(m_name) );
	}

	// Copy it out..
	m_name = strdup( name );
	if ( NULL == m_name ) {
		retval = -1;
	}

	// Set the parameter base name
	if ( NULL != param_base ) {
		retval = SetParamBase( param_base, param_ext );
	}

	// Done
	return retval;
}

// Set new name..
int CronJobMgr::SetParamBase( const char *param_base,
							  const char *param_ext )
{
	// Free the old one..
	if ( NULL != m_param_base ) {
		free( const_cast<char *>(m_param_base) );
		m_param_base = NULL;
	}
	if ( NULL != m_params ) {
		delete m_params;
		m_params = NULL;
	}

	// Default?
	if ( NULL == param_base ) {
		param_base = "CRON";
	}
	if ( NULL == param_ext ) {
		param_ext = "";
	}

	// Calc length & allocate
	size_t len = strlen( param_base ) + strlen( param_ext ) + 1;
	char *tmp = (char * ) malloc( len );
	if ( NULL == tmp ) {
		return -1;
	}

	// Copy it out..
	strcpy( tmp, param_base );
	strcat( tmp, param_ext );
	m_param_base = tmp;

	dprintf( D_CRON | D_VERBOSE,
			 "CronJobMgr: Setting parameter base to '%s'\n", m_param_base );
	m_params = CreateMgrParams( *m_param_base );

	return 0;
}

CronJobMgrParams *
CronJobMgr::CreateMgrParams( const char &base )
{
	return new CronJobMgrParams( base );
}

CronJobParams *
CronJobMgr::CreateJobParams( const char *job_name )
{
	return new CronJobParams( job_name, *this );
}

CronJob *
CronJobMgr::CreateJob( CronJobParams *job_params )
{
	return new CronJob( job_params, *this );
}

// Kill all running jobs
int
CronJobMgr::KillAll( bool force)
{
	// Log our death
	dprintf( D_CRON | D_VERBOSE, "CronJobMgr: %sKilling all jobs\n", force ? "force " : "" );

	// Kill all running jobs
	return m_job_list.KillAll( force , m_name);
}

// Check: Are we ready to shutdown?
bool
CronJobMgr::IsAllIdle(std::string * names /*= nullptr*/)
{
	int		AliveJobs = m_job_list.NumAliveJobs(names);

	dprintf( D_CRON | D_VERBOSE, "CronJobMgr: %d jobs alive [%s]\n", AliveJobs, names ? names->c_str() : "" );
	return AliveJobs ? false : true;
}

// Get the number of active jobs
int
CronJobMgr::GetNumActiveJobs( void ) const
{
	return m_job_list.NumActiveJobs( );
}

// Should we start a job?
bool
CronJobMgr::ShouldStartJob( const CronJob &job ) const
{
	dprintf( D_CRON | D_VERBOSE,
			 "ShouldStartJob: job=%.2f cur=%.2f max=%.2f\n",
			 job.GetJobLoad( ), m_cur_job_load, m_max_job_load );
	return ( (job.GetJobLoad( ) + m_cur_job_load) <= GetMaxJobLoad() );
}

// Job is started
bool
CronJobMgr::JobStarted( const CronJob & /*job*/ )
{
	m_cur_job_load = m_job_list.RunningJobLoad();
	return true;
}

// Job exitted
bool
CronJobMgr::JobExited( const CronJob & /*job*/ )
{
	m_cur_job_load = m_job_list.RunningJobLoad();
	if (  (m_cur_job_load < GetMaxJobLoad()) && (m_schedule_timer < 0)  ) {
		m_schedule_timer = daemonCore->Register_Timer(
			0,
			(TimerHandlercpp)& CronJobMgr::ScheduleJobsFromTimer,
			"ScheduleJobs",
			this );
		if ( m_schedule_timer < 0 ) {
			dprintf( D_ERROR, "CronJobMgr: Failed to register job scheduler timer\n" );
			return false;
		}
	}
	return true;
}

// Schedule all jobs
void
CronJobMgr::ScheduleJobsFromTimer( int /* timerID */ )
{
	m_schedule_timer = -1;		// I've fired; reset for next time I'm needed
	ScheduleAllJobs();
}

// Schedule all jobs
bool
CronJobMgr::ScheduleAllJobs( void )
{
	if ( m_job_list.ScheduleAll( ) < 0 ) {
		return false;
	}
	return true;
}

// Start "on demand" jobs
bool
CronJobMgr::StartOnDemandJobs( void )
{
	if ( m_job_list.StartOnDemandJobs( ) < 0 ) {
		return false;
	}
	return ScheduleAllJobs( );
}

// Handle Reconfig
int
CronJobMgr::HandleReconfig( void )
{
	DoConfig( false );

	// Done
	return ScheduleAllJobs( ) ? 0 : -1;
}

// Handle configuration
int
CronJobMgr::DoConfig( bool initial )
{
	const char *param_buf;

	// Is the config val program specified?
	if( m_config_val_prog ) {
		free( const_cast<char *>(m_config_val_prog) );
	}
	m_config_val_prog = m_params->Lookup( "CONFIG_VAL" );

	m_params->Lookup( "MAX_JOB_LOAD", m_max_job_load, 0.1, 0.01, 1000.0 );

	// Clear all marks
	m_job_list.ClearAllMarks( );

	// Look for _JOBLIST
	param_buf = m_params->Lookup( "JOBLIST" );
	if ( param_buf != NULL ) {
		ParseJobList( param_buf );
		free( const_cast<char *>(param_buf) );
	}

	// Delete all jobs that didn't get marked
	m_job_list.DeleteUnmarked( );

	// And, initialize all jobs (they ignore it if already initialized)
	m_job_list.InitializeAll( );

	// Find our environment variable, if it exits..
	dprintf( D_FULLDEBUG,
			 "CronJobMgr: Doing config (%s)\n",
			 initial ? "initial" : "reconfig" );

	// Reconfigure all running jobs
	m_job_list.HandleReconfig( );

	return 0;
}

// Parse the "Job List"
int
CronJobMgr::ParseJobList( const char *job_list_string )
{
	// Debug
	dprintf( D_FULLDEBUG,
			 "CronJobMgr: Job list string is '%s'\n",
			 job_list_string );

	// Parse out the job names
	for (auto& job_name: StringTokenIterator(job_list_string)) {
		dprintf( D_CRON, "CronJobMgr: Job name is '%s'\n", job_name.c_str() );

		CronJobParams	*job_params = CreateJobParams( job_name.c_str() );
		if ( !job_params->Initialize() ) {
			dprintf( D_ERROR,
					 "Failed to initialize job '%s'; skipping\n",
					 job_name.c_str() );
			delete job_params;
			continue;
		}

		// Create the job & add it to the list (if it's new)
		CronJob *job = m_job_list.FindJob( job_name.c_str() );

		// If job mode changed, delete it & start over
		if ( job   &&  ( ! job_params->Compatible(job->Params()) )  ) {
			dprintf( D_STATUS,
					 "CronJob: Mode of job '%s' changed from "
					 "'%s' to '%s' -- creating new job object\n",
					 job_name.c_str(),
					 job->Params().GetModeString(),
					 job_params->GetModeString() );
			m_job_list.DeleteJob( job_name.c_str() );
			job = NULL;			// New mode?  Build a new job object
		}

		if ( job ) {
			// Set new job parameters
			job->SetParams( job_params );
			job->Mark( );
			dprintf( D_CRON | D_VERBOSE,
					 "CronJobMgr: Done processing job '%s'\n", job_name.c_str() );
			continue;
		}

		// No job?  Create a new one
		job = CreateJob( job_params );
		if ( NULL == job ) {
			dprintf( D_ERROR,
					 "Cron: Failed to create job object for '%s'\n",
					 job_name.c_str() );
			delete job_params;
			continue;
		}
		if ( !m_job_list.AddJob( job_name.c_str(), job ) ) {
			// this only fails if the job is a duplicate, which AddJob already logs
			delete job;
			delete job_params;
			continue;
		}

		// Debug info
		job->Mark( );
		dprintf( D_CRON | D_VERBOSE,
				 "CronJobMgr: Done creating job '%s'\n", job_name.c_str() );
	}

	// All ok
	return 0;
}
