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
#include "ShadowHookMgr.h"
#include "store_cred.h"
#include "condor_holdcodes.h"

#include <algorithm>
#include <filesystem>
#include "my_popen.h"
#include "condor_url.h"

UniShadow::UniShadow() : delayedExitReason( -1 ) {
		// pass RemoteResource ourself, so it knows where to go if
		// it has to call something like shutDown().
	remRes = new RemoteResource( this );
	remRes->setWaitOnKillFailure(true);
}

UniShadow::~UniShadow() {
	if ( remRes ) delete remRes;
	daemonCore->Cancel_Command( CREDD_GET_CRED );
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

	// Before we even try to claim, or activate the claim, check to see if
	// it's even possible for file transfer to succeed.
	bool shouldCheckInputFileTransfer = param_boolean( "CHECK_INPUT_FILE_TRANSFER", false );
	if( shouldCheckInputFileTransfer ) {
		checkInputFileTransfer();
	}

		// Register command which the starter uses to fetch a user's Kerberose/Afs auth credential
	daemonCore->
		Register_Command( CREDD_GET_CRED, "CREDD_GET_CRED",
						  &cred_get_cred_handler,
						  "cred_get_cred_handler", DAEMON,
						  true /*force authentication*/ );

		// Register our job hooks
	m_hook_mgr = std::unique_ptr<ShadowHookMgr>(new ShadowHookMgr());
	if (!m_hook_mgr->initialize(job_ad)) {
		m_hook_mgr.reset();
	}
}


void
UniShadow::spawnFinish()
{
	hookTimerCancel();
	if( ! remRes->activateClaim() ) {
			// we're screwed, give up:
		shutDown(JOB_NOT_STARTED, "Failed to activate claim", CONDOR_HOLD_CODE::FailedToActivateClaim);
	}
	// Start the timer for the periodic user job policy
	shadow_user_policy.startTimer();

}


void
UniShadow::spawn()
{
	if (!m_hook_mgr) {
		dprintf(D_ALWAYS, "No hook manager available; will activate claim immediately.\n");
		spawnFinish();
	} else {
		auto rval = m_hook_mgr->tryHookPrepareJob();
		if (rval == -1) {
			dprintf(D_ALWAYS, "Prepare job hook has failed.  Will shutdown job.\n");
			BaseShadow::log_except("Submit-side job hook execution failed");
			shutDown(JOB_NOT_STARTED, "Shadow prepare hook failed");
		} else if (rval == 0) {
			dprintf(D_FULLDEBUG, "No prepare job hook to run - activating job immediately.\n");
			spawnFinish();
		} else if (rval == 1) {
			dprintf(D_FULLDEBUG, "Hook successfully spawned.\n");
			m_exit_hook_timer_tid = daemonCore->Register_Timer(m_hook_mgr->getHookTimeout(HOOK_SHADOW_PREPARE_JOB, 120),
				(TimerHandlercpp)&UniShadow::hookTimeout,
				"hookTimeout",
				this);
		} else {
			EXCEPT("Hook manager returned an invalid code");
		}
	}
}


void
UniShadow::hookTimeout( int /* timerID */ )
{
	dprintf(D_ERROR, "Timed out waiting for a hook to exit\n");
	BaseShadow::log_except("Submit-side job hook execution timed out");
	shutDown(JOB_NOT_STARTED, "Shadow prepare hook timed out");
}


void
UniShadow::hookTimerCancel()
{
	if (m_exit_hook_timer_tid != -1) {
		daemonCore->Cancel_Timer(m_exit_hook_timer_tid);
		m_exit_hook_timer_tid = -1;
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
	free( sinful );

	remRes->populateExecuteEvent(event.slotName, event.setProp());

	if( !uLog.writeEvent(&event, getJobAd()) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_EXECUTE event: "
				 "can't write to UserLog!\n" );
	}
}


void
UniShadow::cleanUp( bool graceful )
{
		// Deactivate (fast) the claim
	if ( remRes ) {
		remRes->killStarter(graceful);
	}
}

void
UniShadow::gracefulShutDown( void )
{
	remRes->setExitReason(JOB_SHOULD_REQUEUE);
	remRes->killStarter(true);
}


int
UniShadow::getExitReason( void )
{
	if ( isDataflowJob ) {
		return JOB_EXITED;
	}
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
	double recvd_bytes = 0, sent_bytes = 0;

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

uint64_t
UniShadow::bytesSent()
{
	return remRes->bytesSent();
}


uint64_t
UniShadow::bytesReceived()
{
	return remRes->bytesReceived();
}

void
UniShadow::getFileTransferStats(ClassAd &upload_stats, ClassAd &download_stats)
{
	upload_stats = remRes->m_upload_file_stats;
	download_stats = remRes->m_download_file_stats;
}

void
UniShadow::getFileTransferStatus(FileTransferStatus &upload_status,FileTransferStatus &download_status)
{
	remRes->getFileTransferStatus(upload_status,download_status);
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


int64_t
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
	if (isDataflowJob) {
		return 0;
	}
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
UniShadow::resourceDisconnected( RemoteResource* rr )
{
	ASSERT( rr == remRes );


	// All of our children should be gone already, but let's be sure.
	daemonCore->kill_immediate_children();


	const char* txt = "Socket between submit and execute hosts "
		"closed unexpectedly";
	logDisconnectedEvent( txt );

	time_t now = time(NULL);
	jobAd->Assign(ATTR_JOB_DISCONNECTED_DATE, now);

	if (m_lazy_queue_update) {
			// For lazy update, we just want to make sure the
			// job_updater object knows about this attribute (which we
			// already updated our copy of).
		job_updater->watchAttribute(ATTR_JOB_DISCONNECTED_DATE);
	}
	else {
			// They want it now, so do the qmgmt operation directly.
		updateJobAttr(ATTR_JOB_DISCONNECTED_DATE, now);
	}
}

void
UniShadow::resourceReconnected( RemoteResource* rr )
{
	ASSERT( rr == remRes );

		// We've only got one remote resource, so if it successfully
		// reconnected, we can safely log our reconnect event
	logReconnectedEvent();

	// If the shadow started in reconnect mode, check the job ad to see
	// if we previously heard about the job starting execution, and set
	// up our state accordingly.
	if ( attemptingReconnectAtStartup ) {
		time_t job_execute_date = 0;
		time_t claim_start_date = 0;
		jobAd->LookupInteger(ATTR_JOB_CURRENT_START_EXECUTING_DATE, job_execute_date);
		jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, claim_start_date);
		if ( job_execute_date >= claim_start_date ) {
			began_execution = true;
		}
		// Start the timer for the periodic user job policy
		shadow_user_policy.startTimer();
	}

		// Since our reconnect worked, clear attemptingReconnectAtStartup
		// flag so if we disconnect again and fail, we will exit
		// with JOB_SHOULD_REQUEUE instead of JOB_RECONNECT_FAILED.
		// See gt #4783.
	attemptingReconnectAtStartup = false;

		// Update NumJobReconnects in the schedd
		// TODO Should we do the update through the job_updater?
	int job_reconnect_cnt = 0;
	jobAd->LookupInteger(ATTR_NUM_JOB_RECONNECTS, job_reconnect_cnt);
	job_reconnect_cnt++;
	jobAd->Assign(ATTR_NUM_JOB_RECONNECTS, job_reconnect_cnt);
	jobAd->AssignExpr(ATTR_JOB_DISCONNECTED_DATE, "Undefined");

	if (m_lazy_queue_update) {
			// For lazy update, we just want to make sure the
			// job_updater object knows about this attribute (which we
			// already updated our copy of).
		job_updater->watchAttribute(ATTR_NUM_JOB_RECONNECTS);
		job_updater->watchAttribute(ATTR_JOB_DISCONNECTED_DATE);
	}
	else {
			// They want it now, so do the qmgmt operation directly.
		updateJobAttr(ATTR_NUM_JOB_RECONNECTS, job_reconnect_cnt);
		updateJobAttr(ATTR_JOB_DISCONNECTED_DATE, "Undefined");
	}

		// if we're trying to remove this job, now that connection is
		// reestablished, we can kill the starter, evict the job, and
		// handle any output/update messages from the starter.
	if( remove_requested ) {
		requestJobRemoval();
	}

	if (began_execution) {
			// Start the timer for updating the job queue for this job
		startQueueUpdateTimer();
	}
}


void
UniShadow::logDisconnectedEvent( const char* reason )
{
	JobDisconnectedEvent event;
	if (reason) { event.setDisconnectReason(reason); }

	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.startd_addr = dc_startd->addr();
	event.startd_name = dc_startd->name();

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
	event.startd_addr = dc_startd->addr();
	event.startd_name = dc_startd->name();

	char* starter = NULL;
	remRes->getStarterAddress( starter );
	event.starter_addr = starter;
	free( starter );
	starter = NULL;

	if( !uLog.writeEventNoFsync(&event,getJobAd()) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECTED event\n" );
	}
}


void
UniShadow::logReconnectFailedEvent( const char* reason )
{
	JobReconnectFailedEvent event;

	if (reason) { event.setReason(reason); }

	DCStartd* dc_startd = remRes->getDCStartd();
	if( ! dc_startd ) {
		EXCEPT( "impossible: remRes::getDCStartd() returned NULL" );
	}
	event.startd_name = dc_startd->name();

	if( !uLog.writeEventNoFsync(&event,getJobAd()) ) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_RECONNECT_FAILED event\n" );
	}
}

bool
UniShadow::getMachineName( std::string &machineName )
{
	if( remRes ) {
		char *name = NULL;
		remRes->getMachineName(name);
		if( name ) {
			machineName = name;
			free(name);
			return true;
		}
	}
	return false;
}

void
UniShadow::exitAfterEvictingJob( int reason ) {
	// Don't exit unless the starter has signalled it's done, or until
	// twenty seconds after we'd otherwise have exited.  This should
	// reduce the number of scary-looking error messages from the network
	// layer in both the shadow and the starter logs.
	//
	// This function should be called in BaseShadow functions which call
	// cleanUp() and then DC_Exit() before returning to the event loop.  It's
	// not called from UniShadow::cleanUp() because a bunch of those functions
	// do important-looking things between calling cleanUp() and calling
	// DC_Exit().
	if( remRes->gotJobDone() || remRes->getClaimSock() == NULL ) {
		DC_Exit( reason );
	} else {
		this->delayedExitReason = reason;
		remRes->setExitReason( reason );
		daemonCore->Register_Timer( 20, 0,
				(TimerHandlercpp)&UniShadow::exitLeaseHandler,
				"exit lease handler", this );
	}
}

bool
UniShadow::exitDelayed( int &reason ) {
	if ( delayedExitReason != -1 ) {
		reason = delayedExitReason;
		return true;
	}
	return false;
}

void
UniShadow::exitLeaseHandler( int /* timerID */ ) const {
	DC_Exit( delayedExitReason );
}

void
UniShadow::recordFileTransferStateChanges( ClassAd * jobAd, ClassAd * ftAd ) {
	bool tq = false; bool tqSet = ftAd->LookupBool( ATTR_TRANSFER_QUEUED, tq );
	bool ti = false; bool tiSet = ftAd->LookupBool( ATTR_TRANSFERRING_INPUT, ti );
	bool to = false; bool toSet = ftAd->LookupBool( ATTR_TRANSFERRING_OUTPUT, to );

	// If either ATTR_TRANSFER_QUEUED or ATTR_TRANSFERRING_INPUT hasn't
	// been set yet, file transfer hasn't done anything yet and there's
	// event to record.
	if( (!tqSet) || (!tiSet) ) {
		return;
	}

	// If all six of the booleans in the ftAd are the same as the booleans
	// in the job ad, then we've already written out the event.
	bool jtq = false; bool jtqSet = jobAd->LookupBool( ATTR_TRANSFER_QUEUED, jtq );
	bool jti = false; bool jtiSet = jobAd->LookupBool( ATTR_TRANSFERRING_INPUT, jti );
	bool jto = false; bool jtoSet = jobAd->LookupBool( ATTR_TRANSFERRING_OUTPUT, jto );
	if(  jtq == tq && jtqSet == tqSet
	  && jti == ti && jtiSet == tiSet
	  && jto == to && jtoSet == toSet ) {
		return;
	}

	//
	// I suppose for maximum geek cred I should shift tq, ti, toSet, and to
	// into a bitfield and switch on that, instead.
	//

	FileTransferEvent te;
	if( tq && ti && (!toSet) ) {
		te.setType( FileTransferEvent::IN_QUEUED );

		// There really ought to be a remote resource if we're doing
		// file transfer...
		if( remRes ) {
			char * starterAddr = NULL;
			remRes->getStarterAddress( starterAddr );
			if( starterAddr ) {
				te.setHost( starterAddr );
				free( starterAddr );
			}
		}

		jobAd->Assign( "TransferInQueued", time(nullptr) );
	} else if( (!tq) && ti && (!toSet) ) {
		te.setType( FileTransferEvent::IN_STARTED );

		time_t now = time(nullptr);
		jobAd->Assign( "TransferInStarted", now );

		time_t then;
		if( jobAd->LookupInteger( "TransferInQueued", then ) ) {
			te.setQueueingDelay( now - then );
		} else {
			if( remRes ) { // this should always be true...
				char * starterAddr = NULL;
				remRes->getStarterAddress( starterAddr );
				if( starterAddr ) {
					te.setHost( starterAddr );
					free( starterAddr );
				}
			}
		}
	} else if( (!tq) && (!ti) && (!toSet) ) {
		te.setType( FileTransferEvent::IN_FINISHED );
		// te.setSuccess( ... );

		jobAd->Assign( "TransferInFinished", time(nullptr) );
	} else if( tq && (!ti) && (toSet && to) ) {
		te.setType( FileTransferEvent::OUT_QUEUED );

		jobAd->Assign( "TransferOutQueued", time(nullptr) );
	} else if( (!tq) && (!ti) && (toSet && to) ) {
		te.setType( FileTransferEvent::OUT_STARTED );

		time_t now = time(nullptr);
		jobAd->Assign( "TransferOutStarted", now );

		time_t then;
		if( jobAd->LookupInteger( "TransferInQueued", then ) ) {
			te.setQueueingDelay( now - then );
		}
	} else if( (!tq) && (!ti) && (toSet && (!to)) ) {
		te.setType( FileTransferEvent::OUT_FINISHED );
		// te.setSuccess( ... );

		jobAd->Assign( "TransferOutFinished", time(nullptr) );
	}

	if(! uLog.writeEvent( &te, jobAd )) {
		dprintf( D_ALWAYS, "Unable to log file transfer event.\n" );
	}
}


void UniShadow::checkInputFileTransfer() {
	dprintf( D_FULLDEBUG, "checkInputFileTransfer(): entry.\n" );

	//
	// This is the only real way to get the actual list of transfers, sadly.
	//
	remRes->initFileTransfer();
	std::vector<std::string> entries = remRes->filetrans.getAllInputEntries();
	FileTransfer::AddFilesFromSpoolTo(& remRes->filetrans);

	std::vector<std::string> URLs;
	std::vector<std::filesystem::path> paths;
	for( auto & entry : entries ) {
		if( IsUrl(entry.c_str()) ) {
			URLs.push_back(entry);
			continue;
		}
		std::filesystem::path path( entry );
		if(! path.is_absolute()) {
			path = std::filesystem::path(getIwd()) / path;
		}
		paths.push_back( path );
	}


	// ENTRY EXISTS SIZE_IN_BYTES SIZE_IS_KNOWN
	//
	// This presumes that EXISTS is "CONFIRMED" and "UNKNOWN", because
	// there's no "COULDN'T TELL" enumeration.
	std::vector<std::tuple< std::string, bool, size_t, bool >> results;

	for( const auto & path : paths ) {
		std::error_code errorCode;

		bool got_size = false;
		size_t size = (size_t)-1;
		bool exists = std::filesystem::exists(path, errorCode);
		if( exists ) {
			// Don't hoist this into the conditional, because it will
			// suppress error-reporting for all other reasons there.
			if( std::filesystem::is_directory(path, errorCode) ) { continue; }

			size = std::filesystem::file_size(path, errorCode);
			if( errorCode.value() != 0 ) {
				dprintf( D_FULLDEBUG, "checkInputFileTransfer(): failed to obtain size of '%s', error code %d (%s)\n",
					path.string().c_str(), errorCode.value(), errorCode.message().c_str()
				);
			} else {
			    got_size = true;
			}
		}
		results.emplace_back( path.string(), exists, size, got_size );
	}


	dprintf( D_FULLDEBUG, "checkInputFileTransfer(): checking URLs.\n" );
	for( const auto & URL : URLs ) {
		std::string scheme = getURLType(URL.c_str(), true);
		if( scheme == "http" || scheme == "https" ) {
			// curl --disable --silent --location --head
			// [--oauth2-bearer?] [--header "Authorization: Bearer ..."]
			//
			// An HTTP HEAD command may _optionally_ include a(n optionally
			// capitalized) 'Content-Length' header.  However, if it says
			// `HTTP/\d+[\.\d+]? 200`, the file exists and is fetchable.
			//
			// It seems highly prudent to execute this check in another
			// process and use one of the my_popen() variants with a
			// built-in time-out.


			ArgList args;
			std::string libexec;
			param( libexec, "LIBEXEC" );
			std::filesystem::path check_url =
				std::filesystem::path(libexec) / "check-url";
			args.AppendArg( check_url.string() );
			args.AppendArg( URL );

			int timeout = 5;
			int options = 0;
			int exit_status = 0xFFFF;
			char * buffer = run_command( timeout, args, options, NULL, & exit_status );

			if( exit_status == 0 ) {
				// Oddly, we don't have any blank line -preserving utitities.
				unsigned int lineNo = 0;
				std::string b( buffer );
				std::array<std::string, 3> lines;
				auto i = b.begin();
				auto j = b.begin();
				while( lineNo < 3 ) {
					while( *j != '\n' ) { ++j; }
					lines[lineNo].insert( lines[lineNo].begin(),
						i, j
					);
					++j;
					i = j;
					++lineNo;
				}
				auto [CURL_EXIT_CODE, HTTP_CODE, SIZE_IN_BYTES] = lines;

				bool exists = false;
				size_t size = (size_t)-1;
				bool got_size = false;

				if(! CURL_EXIT_CODE.empty()) {
					char * endptr = nullptr;
					int code = strtol( CURL_EXIT_CODE.c_str(), & endptr, 10 );
					if( * endptr == '\0' ) {
						if( code == 0 && ! HTTP_CODE.empty() ) {
							code = strtol( HTTP_CODE.c_str(), & endptr, 10 );
							if( * endptr == '\0' ) {
								if( code == 200 ) {
									exists = true;

									if(! SIZE_IN_BYTES.empty()) {
										size = strtol( SIZE_IN_BYTES.c_str(), & endptr, 10 );
										if( * endptr == '\0' ) {
											got_size = true;
										}
									}
								}
							}
						}
					}
				}

				results.emplace_back( URL, exists, size, got_size );
			} else {
				results.emplace_back( URL, false, (size_t)-1, false );
			}
		} else if( scheme == "osdf" || scheme == "pelican" ) {
			ArgList args;
			args.AppendArg( "/usr/bin/pelican" );
			args.AppendArg( "object" );
			args.AppendArg( "stat" );
			args.AppendArg( "--json" );
			args.AppendArg( URL );

			int timeout = 5;
			int options = 0;
			int exit_status = 0xFFFF;
			char * buffer = run_command( timeout, args, options, NULL, & exit_status );

			if( exit_status == 0 ) {
				classad::ClassAd stat;
				classad::ClassAdJsonParser cajp;
				if(! cajp.ParseClassAd( buffer, stat, true )) {
					results.emplace_back( URL, false, (size_t)-1, false );
				} else {
					bool got_size = false;
					long long int size = -1;
					got_size = stat.LookupInteger("Size", size);
					results.emplace_back( URL, true, size, got_size );
				}
			} else {
				results.emplace_back( URL, false, (size_t)-1, false );
			}
		} else {
			dprintf( D_ALWAYS, "Skipping URL '%s': don't know how to check it.\n", URL.c_str() );
			continue;
		}
	}


	for( const auto & result : results ) {
		dprintf( D_TEST, "checkInputFileTransfer():\t%s\t%s\t%zu\t%s\n",
			std::get<0>(result).c_str(),
			std::get<1>(result) ? "true" : "false",
			std::get<2>(result),
			std::get<3>(result) ? "true" : "false"
		);

		std::string readable = "checkInputFileTransfer(): ";
		formatstr_cat( readable, "%s %s",
			std::get<0>(result).c_str(),
			std::get<1>(result) ? "exists" : "may not exist"
		);
		if( std::get<3>(result) ) {
			formatstr_cat( readable, " and has size %zu", std::get<2>(result) );
		}
		dprintf( D_FULLDEBUG, "checkInputFileTransfer(): %s.\n",
			readable.c_str()
		);

		if(! std::get<1>(result)) {
			// If we wanted to put the job on hold right now, we'd better
			// be sure that the entry didn't exist, and not just that we
			// failed to confirm its existence.  We presently do the
			// latter, for simplicity.
		}
	}


	dprintf( D_FULLDEBUG, "checkInputFileTransfer(): exit.\n" );
}
