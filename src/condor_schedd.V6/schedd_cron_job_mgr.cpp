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
#include "schedd_cron_job_mgr.h"
#include "schedd_cron_job.h"
#include "condor_config.h"

// Basic constructor
ScheddCronJobMgr::ScheddCronJobMgr( ) :
		CronJobMgr( ),
		m_shutting_down( false )
{
}

// Basic destructor
ScheddCronJobMgr::~ScheddCronJobMgr( )
{
	dprintf( D_FULLDEBUG, "ScheddCronJobMgr: Bye\n" );
}

int
ScheddCronJobMgr::Initialize( const char *name )
{
	int status = 0;

	SetName( name, name, "_cron" );
	
	char *cron_name = param( "SCHEDD_CRON_NAME" );
	if ( nullptr != cron_name ) {
		dprintf( D_ALWAYS,
				 "WARNING: The use of SCHEDD_CRON_NAME to 'name' your "
				 "Schedd's Daemon ClassAd Hook Manager is not longer supported "
				 "and will be removed in a future release of Condor." );
		name = cron_name;
		SetName( name, name );
	}

	status = CronJobMgr::Initialize( name );
	if ( nullptr != cron_name ) {
		free( cron_name );
	}
	return status;
}

// Perform shutdown
int
ScheddCronJobMgr::Shutdown( bool force )
{
	dprintf( D_FULLDEBUG, "ScheddCronJobMgr: Shutting down\n" );
	m_shutting_down = false;
	return DeleteAll(force);
}

// Check shutdown
bool
ScheddCronJobMgr::ShutdownOk( )
{
	bool	idle = IsAllIdle( );

	// dprintf( D_ALWAYS, "ShutdownOk: %s\n", idle ? "Idle" : "Busy" );
	return idle;
}

CronJobParams *
ScheddCronJobMgr::CreateJobParams( const char *job_name )
{
	return new ClassAdCronJobParams( job_name, *this );
}

// Create a new job
CronJob *
ScheddCronJobMgr::CreateJob( CronJobParams *job_params )
{
	dprintf( D_FULLDEBUG,
			 "*** Creating Schedd Cron job '%s'***\n",
			 job_params->GetName() );
	auto *params =
		dynamic_cast<ClassAdCronJobParams *>(job_params);
	ASSERT( params );
	return new ScheddCronJob( params, *this );
}
