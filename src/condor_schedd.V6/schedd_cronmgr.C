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
#include "schedd_cronmgr.h"
#include "schedd_cronjob.h"
#include "condor_config.h"

// Basic constructor
ScheddCronMgr::ScheddCronMgr( void ) :
		CronMgrBase( "schedd" )
{
	char *NewName = param( "SCHEDD_CRON_NAME" );
	if ( NULL != NewName ) {
		SetName( NewName, NewName );
		free( NewName );
	}

	// Set my initial "shutdown" state
	ShuttingDown = false;
}

// Basic destructor
ScheddCronMgr::~ScheddCronMgr( )
{
	dprintf( D_FULLDEBUG, "ScheddCronMgr: Bye\n" );
}

// Perform shutdown
int
ScheddCronMgr::Shutdown( bool force )
{
	dprintf( D_FULLDEBUG, "ScheddCronMgr: Shutting down\n" );
	ShuttingDown = false;
	return KillAll( force );
}

// Check shutdown
bool
ScheddCronMgr::ShutdownOk( void )
{
	bool	idle = IsAllIdle( );

	// dprintf( D_ALWAYS, "ShutdownOk: %s\n", idle ? "Idle" : "Busy" );
	return idle;
}

// Create a new job
CronJobBase *
ScheddCronMgr::NewJob( const char *jobname )
{
	dprintf( D_FULLDEBUG, "*** Creating Schedd Cron job '%s'***\n", jobname );
	ScheddCronJob *job = new ScheddCronJob( GetName(), jobname );

	// Register our death handler...
	CronEventHandler e;
	e = (CronEventHandler) &ScheddCronMgr::JobEvent;
	job->SetEventHandler( e, this );

	return (CronJobBase *) job;
}

// Notified when a job dies
void
ScheddCronMgr::JobEvent( CronJobBase *Job, CondorCronEvent  /* Event */ )
{
	(void) Job;

# if 0
	if ( CONDOR_CRON_JOB_DIED == Event ) {
		if ( ShuttingDown ) {
			if ( IsAllIdle( ) ) {
				schedd_check_free( );
			}
		}
	}
# endif
}
