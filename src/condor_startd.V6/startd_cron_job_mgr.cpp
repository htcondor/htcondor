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
	dprintf( D_CRON | D_VERBOSE, "StartdCronJobMgr: Bye\n" );
}

int
StartdCronJobMgr::Initialize( const char *name )
{
	int status;

	SetName( name, name, "_cron" );
	
#if 0
	auto_free_ptr cron_name(param( "STARTD_CRON_NAME" ));
	if (cron_name) {
		dprintf( D_ALWAYS,
				 "WARNING: The use of STARTD_CRON_NAME to 'name' your "
				 "Startd's Daemon ClassAd Hook Manager is not longer supported "
				 "and will be removed in a future release of Condor." );
		name = cron_name;
		SetName( name, name );
	}
#endif

	status = CronJobMgr::Initialize( name );

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
			dprintf( D_ERROR, "StartdCronJobMgr::Reconfig(): "
					 "invalid value for STARTD_CRON_AUTOPUBLISH: \"%s\" "
					 "using default value of NEVER\n", tmp );
			m_auto_publish = CAP_NEVER;
		} else {
			dprintf( D_CRON | D_VERBOSE, "StartdCronJobMgr::Reconfig(): "
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
	std::string nonidle;
	bool	idle = IsAllIdle(&nonidle);
	if (idle) {
		dprintf( D_FULLDEBUG, "StartdCronJobMgr::ShutdownOk: All Idles\n");
	} else {
		dprintf( D_STATUS, "StartdCronJobMgr::ShutdownOk: No. Busy=[%s]\n", nonidle.c_str());
	}
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

	dprintf( D_CRON,
			 "*** Creating Startd Cron job '%s'***\n",
			 jobName );
	StartdCronJobParams *params =
		dynamic_cast<StartdCronJobParams *>( job_params );
	ASSERT( params );

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

	bool shouldStartJob = CronJobMgr::ShouldStartJob( job );
	if(! shouldStartJob) { return false; }

	// Don't run a job if it has a condition and that condition does not
	// evaluate to TRUE in the context of the machine ad.
	const ConstraintHolder & condition = job.Params().GetCondition();
	if(condition.Expr() != NULL) {

		ClassAd context;
		resmgr->publish_daemon_ad(context);

		classad::Value v;
		bool rv = false;
		if( context.EvaluateExpr( condition.Expr(), v ) && v.IsBooleanValueEquiv(rv)) {
			if (rv) { return true; }
			dprintf(D_CRON, "CronJob: job '%s' CONDITION evaluated to false\n", job.GetName());
		} else {
			dprintf(D_ERROR, "CronJob: job '%s' CONDITION is not boolean, treating as false\n", job.GetName());
			if (IsDebugCategory(D_FULLDEBUG)) {
				std::string buf;
				dprintf( D_MACHINE, "StartdCronJobMgr::ShouldStartJob(%s): evaluated in context:\n%s\n",
					job.GetName(), formatAd(buf,context,"\t") );
				buf.clear(); ClassAdValueToString(v, buf);
				dprintf( D_FULLDEBUG, "StartdCronJobMgr::ShouldStartJob(%s) Condition[%s] = %s\n", job.GetName(), condition.c_str(), buf.c_str());
			}
		}
		return false;
	}

	return true;
}

// Should we allow a benchmark to run?
bool
StartdCronJobMgr::ShouldStartBenchmarks( void ) const
{
	dprintf( D_CRON | D_VERBOSE,
			 "ShouldStartBenchmarks: load=%.2f max=%.2f\n",
			 GetCurJobLoad(), GetMaxJobLoad() );
	return ( GetCurJobLoad() < GetMaxJobLoad() );
}

// Job is started
bool
StartdCronJobMgr::JobStarted( const CronJob &job )
{
	dprintf(D_STATUS, "STARTD_CRON job %s started. pid=%d\n", job.GetName(), job.GetPid());
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
