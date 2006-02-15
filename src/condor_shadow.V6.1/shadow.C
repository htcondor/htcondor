/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "shadow.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_qmgr.h"         // need to talk to schedd's qmgr
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_email.h"        // for email.
#include "metric_units.h"

extern "C" char* d_format_time(double);

UniShadow::UniShadow() {
		// pass RemoteResource ourself, so it knows where to go if
		// it has to call something like shutDown().
	remRes = new RemoteResource( this );
	
}

UniShadow::~UniShadow() {
	if ( remRes ) delete remRes;
}


int
UniShadow::updateFromStarter(int command, Stream *s)
{
	ClassAd updateAd;
	ClassAd *jobad = getJobAd();
	char buf[300];
	int int_val;
	struct rusage rusage_val;
	
	// get info from the starter encapsulated in a ClassAd
	s->decode();
	if( ! updateAd.initFromStream(*s) ) {
		dprintf( D_ALWAYS, "ERROR in UniShadow::updateFromStarter:"
				 "Can't read ClassAd, aborting.\n" );
		return FALSE;
	}
	s->end_of_message();

	if ( !jobad ) {
		// should never really happen...
		return FALSE;
	}

	int prev_image = remRes->getImageSize();
	int prev_disk = remRes->getDiskUsage();
	struct rusage prev_rusage = remRes->getRUsage();

		// Stick everything we care about in our RemoteResource. 
	remRes->updateFromStarter( &updateAd );

		// Now, update our local copy of the job classad for
		// anything that's changed. 
	int_val = remRes->getImageSize();
	if( int_val > prev_image ) {
		sprintf( buf, "%s=%d", ATTR_IMAGE_SIZE, int_val );
		jobad->Insert( buf );

			// also update the User Log with an image size event
		JobImageSizeEvent event;
		event.size = int_val;
		if (!uLog.writeEvent (&event)) {
			dprintf( D_ALWAYS, "Unable to log ULOG_IMAGE_SIZE event\n" );
		}
	}

	int_val = remRes->getDiskUsage();
	if( int_val > prev_disk ) {
		sprintf( buf, "%s=%d", ATTR_DISK_USAGE, int_val );
		jobad->Insert( buf );
	}

	rusage_val = remRes->getRUsage();
	if( rusage_val.ru_stime.tv_sec > prev_rusage.ru_stime.tv_sec ) {
		sprintf( buf, "%s=%f", ATTR_JOB_REMOTE_SYS_CPU,
				 (float)rusage_val.ru_stime.tv_sec );
		jobad->Insert( buf );
	}
	if( rusage_val.ru_utime.tv_sec > prev_rusage.ru_utime.tv_sec ) {
		sprintf( buf, "%s=%f", ATTR_JOB_REMOTE_USER_CPU,
				 (float)rusage_val.ru_utime.tv_sec );
		jobad->Insert( buf );
	}

	return TRUE;
}



void
UniShadow::init( ClassAd* job_ad, const char* schedd_addr )
{
	if ( !job_ad ) {
		EXCEPT("No job_ad defined!");
	}

		// base init takes care of lots of stuff:
	baseInit( job_ad, schedd_addr );

		// we're only dealing with one host, so the rest is pretty
		// trivial.  we can just lookup everything we need in the job
		// ad, since it'll have the ClaimId, address (in the ClaimId)
		// startd's name (RemoteHost) and pool (RemotePool).
	remRes->setStartdInfo( jobAd );
	
		// In this case we just pass the pointer along...
	remRes->setJobAd( jobAd );
	
		// Register command which gets updates from the starter
		// on the job's image size, cpu usage, etc.  Each kind of
		// shadow implements it's own version of this to deal w/ it
		// properly depending on parallel vs. serial jobs, etc. 
	daemonCore->
		Register_Command( SHADOW_UPDATEINFO, "SHADOW_UPDATEINFO",
						  (CommandHandlercpp)&UniShadow::updateFromStarter, 
						  "UniShadow::updateFromStarter", this, DAEMON );

    	//
    	// If the jobAd will ask to be deferred for execution on
    	// the Starter side, the Shadow will first check to determine
    	// what the time difference is between the Shadow & Starter
    	// This offset will be stuffed into teh jobAd when it is shipped
    	// over and the Starter will add the offset to the deferral time
    	// 
//    int deferralTime;
//    if ( this->jobAd->LookupInteger( "DEFERRAL_TIME", deferralTime ) ) {
//    	//
//    	// Ask the Starter what the time difference is
//    	//
//		DCStartd* dc_startd = this->remRes->getDCStartd();
//		if( ! dc_startd ) {
//			dprintf( D_FULLDEBUG, "UniShadow::init() Failed to get dc_startd\n");
//		} else {
//				//
//				// I am allocating this in since most of the time
//				// it will never be needed
//				//
//			char buf[300];
//			long offset;
//			
//			offset = dc_startd->getTimeOffset( );			
//			sprintf( buf, "%s=%d", ATTR_DEFERRAL_OFFSET, offset );
//		    if ( !this->jobAd->Insert( buf )) {
//        		EXCEPT( "Failed to insert %s!", "DEFFERAL_OFFSET" );
//    		} else {
//    			dprintf( D_FULLDEBUG, "UniShadow::init(): Stored deferral time "
//   									  "offset for starter as %d\n", offset );
//		    }
//		}
//	} // DEFERRAL OFFSET
}


void
UniShadow::spawn( void )
{
	/*
		DCStartd* dc_startd = remRes->getDCStartd();
		if( ! dc_startd ) {
			dprintf( D_ALWAYS, "PAVLO: Failed to get the dc_startd!\n");
		} else {
			dprintf( D_ALWAYS, "PAVLO: Got the dc_startd, let's see if we "
							   "can get the offset!\n" );
			long offset = dc_startd->getTimeOffset( );
		}
	*/
	
	if( ! remRes->activateClaim() ) {
			// we're screwed, give up:
		shutDown( JOB_NOT_STARTED );
	}
}


void
UniShadow::reconnect( void )
{
	remRes->reconnect();
}


bool 
UniShadow::supportsReconnect( void )
{
		// For the UniShadow, the answer to this depends on our remote
		// starter.  If that supports it, so do we.  If not, we don't. 
	return remRes->supportsReconnect();
}


void
UniShadow::logExecuteEvent( void )
{
	ExecuteEvent event;
	char* sinful = event.executeHost;
	remRes->getStartdAddress( sinful );
	if( !uLog.writeEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event: "
				 "can't write to UserLog!\n" );
	}
}


void
UniShadow::cleanUp( void )
{
		// Deactivate (fast) the claim
	if ( remRes ) {
		remRes->killStarter();
	}
}


void
UniShadow::gracefulShutDown( void )
{
		// Deactivate (gracefully) the claim
	if ( remRes ) {
		remRes->killStarter( true );
	}
}


int
UniShadow::getExitReason( void )
{
	if( remRes ) {
		return remRes->getExitReason();
	}
	return -1;
}


void
UniShadow::emailTerminateEvent( int exitReason )
{
	Email mailer;
		// note, we want to reverse the order of the send/recv in the
		// call, since we want the email from the job's perspective,
		// not the shadow's.. 
	mailer.sendExitWithBytes( jobAd, exitReason, 
							  bytesReceived(), bytesSent(), 
							  prev_run_bytes_sent + bytesReceived(), 
							  prev_run_bytes_recvd + bytesSent() );
}


int UniShadow::handleJobRemoval(int sig) {
    dprintf ( D_FULLDEBUG, "In handleJobRemoval(), sig %d\n", sig );
	remove_requested = true;
		// if we're not in the middle of trying to reconnect, we
		// should immediately kill the starter.  if we're
		// reconnecting, we'll do the right thing once a connection is
		// established now that the remove_requested flag is set... 
	if( remRes->getResourceState() != RR_RECONNECT ) {
		remRes->setExitReason( JOB_KILLED );
		remRes->killStarter();
	}
		// more?
	return 0;
}


float
UniShadow::bytesSent()
{
	return remRes->bytesSent();
}


float
UniShadow::bytesReceived()
{
	return remRes->bytesReceived();
}


struct rusage
UniShadow::getRUsage( void ) 
{
	return remRes->getRUsage();
}


int
UniShadow::getImageSize( void )
{
	return remRes->getImageSize();
}


int
UniShadow::getDiskUsage( void )
{
	return remRes->getDiskUsage();
}


bool
UniShadow::exitedBySignal( void )
{
	return remRes->exitedBySignal();
}


int
UniShadow::exitSignal( void )
{
	return remRes->exitSignal();
}


int
UniShadow::exitCode( void )
{
	return remRes->exitCode();
}


void
UniShadow::resourceBeganExecution( RemoteResource* rr )
{
	ASSERT( rr == remRes );

		// We've only got one remote resource, so if it's started
		// executing, we can safely log our execute event
	logExecuteEvent();

		// Start the timer for the periodic user job policy  
	shadow_user_policy.startTimer();

		// Start the timer for updating the job queue for this job 
	startQueueUpdateTimer();
}


void
UniShadow::resourceReconnected( RemoteResource* rr )
{
	ASSERT( rr == remRes );

		// We've only got one remote resource, so if it successfully
		// reconnected, we can safely log our reconnect event
	logReconnectedEvent();

		// if we're trying to remove this job, now that connection is
		// reestablished, we can kill the starter, evict the job, and
		// handle any output/update messages from the starter.
	if( remove_requested ) {
		remRes->setExitReason( JOB_KILLED );
		remRes->killStarter();
	}

		// Start the timer for the periodic user job policy  
	shadow_user_policy.startTimer();

		// Start the timer for updating the job queue for this job 
	startQueueUpdateTimer();
}


void
UniShadow::logDisconnectedEvent( const char* reason )
{
	JobDisconnectedEvent event;
	event.setDisconnectReason( reason );

	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.setStartdAddr( dc_startd->addr() );
	event.setStartdName( dc_startd->name() );

	if( !uLog.writeEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_DISCONNECTED event\n" );
	}
}


void
UniShadow::logReconnectedEvent( void )
{
	JobReconnectedEvent event;

	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.setStartdAddr( dc_startd->addr() );
	event.setStartdName( dc_startd->name() );

	char* starter = NULL;
	remRes->getStarterAddress( starter );
	event.setStarterAddr( starter );
	delete [] starter;
	starter = NULL;

	if( !uLog.writeEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECTED event\n" );
	}
}


void
UniShadow::logReconnectFailedEvent( const char* reason )
{
	JobReconnectFailedEvent event;

	event.setReason( reason );

	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.setStartdName( dc_startd->name() );

	if( !uLog.writeEvent(&event) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECT_FAILED event\n" );
	}
}
