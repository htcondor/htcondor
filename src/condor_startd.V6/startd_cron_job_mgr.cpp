/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "startd_cron_job_mgr.h"
#include "startd_cron_job.h"
#include "condor_config.h"
#include "startd.h"

// Basic constructor
StartdCronJobMgr::StartdCronJobMgr( void )
		: CronJobMgr( ),
		  m_shutting_down( false ),
		  m_auto_publish( CAP_NEVER )
{
}

// Basic destructor
StartdCronJobMgr::~StartdCronJobMgr( void )
{
	dprintf( D_FULLDEBUG, "StartdCronJobMgr: Bye\n" );
}

int
StartdCronJobMgr::Initialize( const char *name )
{
	int status;

	SetName( name, name, "_cron" );
	
	char *cron_name = param( "STARTD_CRON_NAME" );
	if ( NULL != cron_name ) {
		dprintf( D_ALWAYS,
				 "WARNING: The use of STARTD_CRON_NAME to 'name' your "
				 "Startd's Daemon ClassAd Hook Manager is not longer supported "
				 "and will be removed in a future release of Condor." );
		name = cron_name;
		SetName( name, name );
	}

	status = CronJobMgr::Initialize( name );
	if ( NULL != cron_name ) {
		free( cron_name );
	}

	ParamAutoPublish( );
	return status;
}

int
StartdCronJobMgr::Reconfig( void )
{
	ParamAutoPublish();
	return CronJobMgr::HandleReconfig();
}

void
StartdCronJobMgr::ParamAutoPublish( void )
{
	m_auto_publish = CAP_NEVER;  // always default to never if not set
	char* tmp = param("STARTD_CRON_AUTOPUBLISH");
	if( tmp ) {
		m_auto_publish = getCronAutoPublishNum(tmp);
		if( m_auto_publish == CAP_ERROR ) {
			dprintf( D_ALWAYS, "StartdCronJobMgr::Reconfig(): "
					 "invalid value for STARTD_CRON_AUTOPUBLISH: \"%s\" "
					 "using default value of NEVER\n", tmp );
			m_auto_publish = CAP_NEVER;
		} else {
			dprintf( D_FULLDEBUG, "StartdCronJobMgr::Reconfig(): "
					 "STARTD_CRON_AUTOPUBLISH set to \"%s\"\n", tmp );
		}
		free( tmp );
		tmp = NULL;
	}
}

// Perform shutdown
int
StartdCronJobMgr::Shutdown( bool force )
{
	dprintf( D_FULLDEBUG, "StartdCronJobMgr: Shutting down\n" );
	m_shutting_down = true;
	return KillAll( force );
}

// Check shutdown
bool
StartdCronJobMgr::ShutdownOk( void )
{
	bool	idle = IsAllIdle( );

	// dprintf( D_ALWAYS, "ShutdownOk: %s\n", idle ? "Idle" : "Busy" );
	return idle;
}

StartdCronJobParams *
StartdCronJobMgr::CreateJobParams( const char *job_name )
{
	return new StartdCronJobParams( job_name, *this );
}

StartdCronJob *
StartdCronJobMgr::CreateJob( CronJobParams *job_params )
{
	const char * jobName = job_params->GetName();

	dprintf( D_FULLDEBUG,
			 "*** Creating Startd Cron job '%s'***\n",
			 jobName );
	StartdCronJobParams *params =
		dynamic_cast<StartdCronJobParams *>( job_params );
	ASSERT( params );

	char * metricString = params->Lookup( "METRICS" );
	if( metricString != NULL && metricString[0] != '\0' ) {
		StringList pairs( metricString );
		for( char * pair = pairs.first(); pair != NULL; pair = pairs.next() ) {
			StringList tn( pair, ":" );
			char * metricType = tn.first();
			if (!metricType) continue;

			char * attributeName = tn.next();
			if(! params->addMetric( metricType, attributeName )) {
				dprintf( 	D_ALWAYS, "Unknown metric type '%s' for attribute "
							"'%s' in monitor '%s', ignoring.\n", metricType,
							attributeName, jobName );
			} else {
				dprintf(	D_FULLDEBUG, "Added %s as %s metric for %s job\n",
							attributeName, metricType, jobName );
			}
		}
	}
	if (metricString) free( metricString );

	return new StartdCronJob( params, *this );
}

// Should we start a job?
bool
StartdCronJobMgr::ShouldStartJob( const CronJob &job ) const
{
	if ( m_shutting_down ) {
		return false;
	}

	// Don't start *any* new jobs during benchmarks
	if ( bench_job_mgr && bench_job_mgr->NumActiveBenchmarks() ) {
		return false;
	}

	return CronJobMgr::ShouldStartJob( job );
}

// Should we allow a benchmark to run?
bool
StartdCronJobMgr::ShouldStartBenchmarks( void ) const
{
	dprintf( D_FULLDEBUG,
			 "ShouldStartBenchmarks: load=%.2f max=%.2f\n",
			 GetCurJobLoad(), GetMaxJobLoad() );
	return ( GetCurJobLoad() < GetMaxJobLoad() );
}

// Job is started
bool
StartdCronJobMgr::JobStarted( const CronJob &job )
{
	return CronJobMgr::JobStarted( job );
}

// Job exitted
bool
StartdCronJobMgr::JobExited( const CronJob &job )
{
	bool status = CronJobMgr::JobExited( job );
	if ( m_shutting_down &&  IsAllIdle() ) {
		startd_check_free();
	}
	if ( bench_job_mgr ) {
		bench_job_mgr->ScheduleAllJobs();
	}
	return status;
}
