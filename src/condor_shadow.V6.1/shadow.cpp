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
#include "shadow.h"
#include "condor_daemon_core.h"
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
	daemonCore->Cancel_Command( SHADOW_UPDATEINFO );
}


int
UniShadow::updateFromStarter(int /* command */, Stream *s)
{
	ClassAd update_ad;

	// get info from the starter encapsulated in a ClassAd
	s->decode();
	if( ! update_ad.initFromStream(*s) ) {
		dprintf( D_ALWAYS, "ERROR in UniShadow::updateFromStarter:"
				 "Can't read ClassAd, aborting.\n" );
		return FALSE;
	}
	s->end_of_message();

	return updateFromStarterClassAd(&update_ad);
}


int
UniShadow::updateFromStarterClassAd(ClassAd* update_ad) {
	ClassAd *job_ad = getJobAd();
	if (!job_ad) {
		// should never really happen...
		return FALSE;
	}

		// Save the current image size, so that if it changes as a
		// result of this update, we can log an event to the userlog.
	int64_t prev_usage = 0, prev_rss = 0, prev_pss = -1;
	int64_t prev_image = getImageSize(prev_usage, prev_rss, prev_pss);

		// Stick everything we care about into our RemoteResource.
		// For the UniShadow (with only 1 RemoteResource) it has a
		// pointer to the identical copy of the job classad, so when
		// it updates its copy, it's really updating ours, too.
	remRes->updateFromStarter(update_ad);

	int64_t cur_usage = 0, cur_rss = 0, cur_pss = -1;
	int64_t cur_image = getImageSize(cur_usage, cur_rss, cur_pss);
	if (cur_image != prev_image || 
		cur_usage != prev_usage ||
		cur_rss != prev_rss ||
		cur_pss != prev_pss) {
		JobImageSizeEvent event;
		event.image_size_kb = cur_image;
		event.memory_usage_mb = cur_usage;
		event.resident_set_size_kb = cur_rss;
		event.proportional_set_size_kb = cur_pss;

			// for performance, do not bother fsyncing this event
		if (!uLog.writeEventNoFsync(&event, job_ad)) {
			dprintf(D_ALWAYS, "Unable to log ULOG_IMAGE_SIZE event\n");
		}
	}

	return TRUE;
}


void
UniShadow::init( ClassAd* job_ad, const char* schedd_addr, const char *xfer_queue_contact_info )
{
	if ( !job_ad ) {
		EXCEPT("No job_ad defined!");
	}

		// base init takes care of lots of stuff:
	baseInit( job_ad, schedd_addr, xfer_queue_contact_info );

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
}

void
UniShadow::spawn( void )
{
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
	char* sinful = NULL;
	remRes->getStartdAddress( sinful );
	event.setExecuteHost( sinful );
	delete[] sinful;
	char* remote_name = NULL;
	remRes->getStartdName(remote_name);
	event.setRemoteName(remote_name);
	delete[] remote_name;
	if( !uLog.writeEvent(&event, getJobAd()) ) {
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
	if (remRes) {
		int remain = remRes->remainingLeaseDuration();
		if (!remain) {
			// Only attempt to deactivate (gracefully) the claim if
			// there's no lease or it has already expired.
			remRes->killStarter(true);
		} else {
			DC_Exit( JOB_SHOULD_REQUEUE );
		}
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

bool
UniShadow::claimIsClosing( void )
{
	if( remRes ) {
		return remRes->claimIsClosing();
	}
	return false;
}


void
UniShadow::emailTerminateEvent( int exitReason, update_style_t kind )
{
	Email mailer;
	float recvd_bytes, sent_bytes;

	if (kind == US_TERMINATE_PENDING) {
		/* I don't have a remote resource, so get the values directly from
			the jobad, and I know they should be present at this stage. */
		jobAd->LookupFloat(ATTR_BYTES_SENT, sent_bytes);
		jobAd->LookupFloat(ATTR_BYTES_RECVD, recvd_bytes);

		// note, we want to reverse the order of the send/recv in the
		// call, since we want the email from the job's perspective,
		// not the shadow's.. 

		mailer.sendExitWithBytes( jobAd, exitReason, 0, 0, 
						  sent_bytes, recvd_bytes);
		// all done.
		return;
	}

	// the default case of kind == US_NORMAL
	
		// note, we want to reverse the order of the send/recv in the
		// call, since we want the email from the job's perspective,
		// not the shadow's.. 
	mailer.sendExitWithBytes( jobAd, exitReason, 
							  bytesReceived(), bytesSent(), 
							  prev_run_bytes_sent + bytesReceived(), 
							  prev_run_bytes_recvd + bytesSent() );
}

void UniShadow::holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode )
{
	/*int iPrevExitReason=*/ remRes->getExitReason();
	
	remRes->setExitReason( JOB_SHOULD_HOLD );
	BaseShadow::holdJob(reason, hold_reason_code, hold_reason_subcode);
}

void UniShadow::removeJob( const char* reason )
{
	int iPrevExitReason=remRes->getExitReason();
	
	remRes->setExitReason( JOB_SHOULD_REMOVE );
	this->removeJobPre(reason);
	
	// exit immediately if the remote side is failing out or has already exited.
	if ( iPrevExitReason != JOB_SHOULD_REMOVE && iPrevExitReason != -1)
	{
		// don't wait for final update b/c there isn't one.
		DC_Exit( JOB_SHOULD_REMOVE );
	}
}

void
UniShadow::requestJobRemoval() {
	remRes->setExitReason( JOB_KILLED );
	bool job_wants_graceful_removal = jobWantsGracefulRemoval();
	dprintf(D_ALWAYS,"Requesting %s removal of job.\n",
			job_wants_graceful_removal ? "graceful" : "fast");
	remRes->killStarter( job_wants_graceful_removal );
}

int UniShadow::handleJobRemoval(int sig) {
    dprintf ( D_FULLDEBUG, "In handleJobRemoval(), sig %d\n", sig );
	remove_requested = true;
		// if we're not in the middle of trying to reconnect, we
		// should immediately kill the starter.  if we're
		// reconnecting, we'll do the right thing once a connection is
		// established now that the remove_requested flag is set... 
	if( remRes->getResourceState() != RR_RECONNECT ) {
		requestJobRemoval();
	}
		// more?
	return 0;
}


int UniShadow::JobSuspend( int sig )
{
	int iRet=1;
	
	if ( remRes->suspend() )
		iRet = 0;
	else
		dprintf ( D_ALWAYS, "JobSuspend() sig %d FAILED\n", sig );
	
	return iRet;
}

int UniShadow::JobResume( int sig )
{
	int iRet =1;
	
	if ( remRes->resume() )
		iRet =0;
	else
		dprintf ( D_ALWAYS, "JobResume() sig %d FAILED\n", sig );
	
	return iRet;
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


int64_t
UniShadow::getImageSize( int64_t & mem_usage, int64_t & rss, int64_t & pss )
{
	return remRes->getImageSize(mem_usage, rss, pss);
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

		// Invoke the base copy of this function to handle shared code.
	BaseShadow::resourceBeganExecution(rr);
}


void
UniShadow::resourceReconnected( RemoteResource* rr )
{
	ASSERT( rr == remRes );

		// We've only got one remote resource, so if it successfully
		// reconnected, we can safely log our reconnect event
	logReconnectedEvent();

		// Update NumJobReconnects in the schedd
		// TODO Should we do the update through the job_updater?
	int job_reconnect_cnt = 0;
	jobAd->LookupInteger(ATTR_NUM_JOB_RECONNECTS, job_reconnect_cnt);
	job_reconnect_cnt++;
	jobAd->Assign(ATTR_NUM_JOB_RECONNECTS, job_reconnect_cnt);

	if (m_lazy_queue_update) {
			// For lazy update, we just want to make sure the
			// job_updater object knows about this attribute (which we
			// already updated our copy of).
		job_updater->watchAttribute(ATTR_NUM_JOB_RECONNECTS);
	}
	else {
			// They want it now, so do the qmgmt operation directly.
		updateJobAttr(ATTR_NUM_JOB_RECONNECTS, job_reconnect_cnt);
	}

		// if we're trying to remove this job, now that connection is
		// reestablished, we can kill the starter, evict the job, and
		// handle any output/update messages from the starter.
	if( remove_requested ) {
		requestJobRemoval();
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

	if( !uLog.writeEventNoFsync(&event,getJobAd()) ) {
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

	if( !uLog.writeEventNoFsync(&event,getJobAd()) ) {
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

	if( !uLog.writeEventNoFsync(&event,getJobAd()) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECT_FAILED event\n" );
	}
}

bool
UniShadow::getMachineName( MyString &machineName )
{
	if( remRes ) {
		char *name = NULL;
		remRes->getMachineName(name);
		if( name ) {
			machineName = name;
			delete [] name;
			return true;
		}
	}
	return false;
}
