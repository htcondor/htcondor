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
ScheddCronMgr::JobEvent( CronJobBase *Job, CondorCronEvent Event )
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
