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
#include "startd_cronmgr.h"
#include "startd_cronjob.h"
#include "condor_config.h"
#include "startd.h"

// Basic constructor
StartdCronMgr::StartdCronMgr( void ) :
		CondorCronMgr( "startd" )
{
	char *NewName = param( "STARTD_CRON_NAME" );
	if ( NULL != NewName ) {
		SetName( NewName, NewName );
		free( NewName );
	}

	// Set my initial "shutdown" state
	ShuttingDown = false;
}

// Basic destructor
StartdCronMgr::~StartdCronMgr( )
{
	dprintf( D_FULLDEBUG, "StartdCronMgr: Bye\n" );
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
CondorCronJob *
StartdCronMgr::NewJob( const char *jobname )
{
	dprintf( D_FULLDEBUG, "*** Creating a Startd job '%s'***\n", jobname );
	StartdCronJob *job = new StartdCronJob( GetName(), jobname );

	// Register our death handler...
	CronEventHandler e;
	e = (CronEventHandler) &StartdCronMgr::JobEvent;
	job->SetEventHandler( e, this );

	return (CondorCronJob *) job;
}

// Notified when a job dies
void
StartdCronMgr::JobEvent( CondorCronJob *Job, CondorCronEvent Event )
{
	(void) Job;

	if ( CONDOR_CRON_JOB_DIED == Event ) {
		if ( ShuttingDown ) {
			if ( IsAllIdle( ) ) {
				startd_check_free( );
			}
		}
	}
}
