/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include "startd_cronmgr.h"
#include "startd_cronjob.h"
#include "condor_config.h"
#include "startd.h"

// Basic constructor
StartdCronMgr::StartdCronMgr( void ) :
		CronMgrBase( "startd" )
{
	char *NewName = param( "STARTD_CRON_NAME" );
	if ( NULL != NewName ) {
		SetName( NewName, NewName );
		free( NewName );
	}

	// Set my initial "shutdown" state
	ShuttingDown = false;
	
	auto_publish = CAP_NEVER;
}

// Basic destructor
StartdCronMgr::~StartdCronMgr( )
{
	dprintf( D_FULLDEBUG, "StartdCronMgr: Bye\n" );
}


int
StartdCronMgr::Initialize()
{
	ParamAutoPublish();
	return CronMgrBase::Initialize();
}


int
StartdCronMgr::Reconfig()
{
	ParamAutoPublish();
	return CronMgrBase::Reconfig();
}


void
StartdCronMgr::ParamAutoPublish()
{
	auto_publish = CAP_NEVER;  // always default to never if not set
	char* tmp = param("STARTD_CRON_AUTOPUBLISH");
	if( tmp ) {
		auto_publish = getCronAutoPublishNum(tmp);
		if( auto_publish == CAP_ERROR ) {
			dprintf( D_ALWAYS, "StartdCronMgr::Reconfig(): "
					 "invalid value for STARTD_CRON_AUTOPUBLISH: \"%s\" "
					 "using default value of NEVER\n", tmp );
			auto_publish = CAP_NEVER;
		} else {
			dprintf( D_FULLDEBUG, "StartdCronMgr::Reconfig(): "
					 "STARTD_CRON_AUTOPUBLISH set to \"%s\"\n", tmp );
		}
		free( tmp );
		tmp = NULL;
	}
}


// Perform shutdown
int
StartdCronMgr::Shutdown( bool force )
{
	dprintf( D_FULLDEBUG, "StartdCronMgr: Shutting down\n" );
	ShuttingDown = false;
	return KillAll( force );
}

// Check shutdown
bool
StartdCronMgr::ShutdownOk( void )
{
	bool	idle = IsAllIdle( );

	// dprintf( D_ALWAYS, "ShutdownOk: %s\n", idle ? "Idle" : "Busy" );
	return idle;
}


// Create a new job
CronJobBase *
StartdCronMgr::NewJob( const char *jobname )
{
	dprintf( D_FULLDEBUG, "*** Creating Startd Cron job '%s'***\n", jobname );
	StartdCronJob *job = new StartdCronJob( GetName(), jobname );

	// Register our death handler...
	CronEventHandler e;
	e = (CronEventHandler) &StartdCronMgr::JobEvent;
	job->SetEventHandler( e, this );

	return (CronJobBase *) job;
}


// Notified when a job dies
void
StartdCronMgr::JobEvent( CronJobBase *Job, CondorCronEvent Event )
{
	(void) Job;

	if ( CONDOR_CRON_JOB_DIED == Event ) {
		if ( ShuttingDown ) {
			if ( IsAllIdle( ) ) {
				startd_check_free();
			}
		}
	}
}
