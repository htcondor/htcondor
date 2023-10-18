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
#include "baseshadow.h"
#include "condor_classad.h"      // for ClassAds.
#include "condor_qmgr.h"         // have to talk to schedd's Q manager
#include "condor_attributes.h"   // for ATTR_ ClassAd stuff
#include "condor_config.h"       // for param()
#include "condor_email.h"        // for (you guessed it) email stuff
#include "condor_version.h"
#include "condor_ver_info.h"
#include "enum_utils.h"
#include "condor_holdcodes.h"
#include "classad_helpers.h"
#include "classad_merge.h"
#include "dc_startd.h"
#include "limit_directory_access.h"
#include "spooled_job_files.h"
#include "condor_url.h"
#include "ipv6_hostname.h"
#include <math.h>
#include "job_ad_instance_recording.h"

// these are declared static in baseshadow.h; allocate space here
BaseShadow* BaseShadow::myshadow_ptr = NULL;


// this appears at the bottom of this file:
int display_dprintf_header(char **buf,int *bufpos,int *buflen);
extern bool sendUpdatesToSchedd;

// some helper functions
int getJobAdExitCode(ClassAd *jad, int &exit_code);
int getJobAdExitedBySignal(ClassAd *jad, int &exited_by_signal);
int getJobAdExitSignal(ClassAd *jad, int &exit_signal);

BaseShadow::BaseShadow() {
	jobAd = NULL;
	remove_requested = false;
	cluster = proc = -1;
	gjid = NULL;
	core_file_name = NULL;
	scheddAddr = NULL;
	job_updater = NULL;
	ASSERT( !myshadow_ptr );	// make cetain we're only instantiated once
	myshadow_ptr = this;
	exception_already_logged = false;
	began_execution = FALSE;
	reconnect_e_factor = 0.0;
	reconnect_ceiling = 300;
	prev_run_bytes_sent = 0.0;
	prev_run_bytes_recvd = 0.0;
	m_num_cleanup_retries = 0;
	m_max_cleanup_retries = 5;
	m_lazy_queue_update = true;
	m_cleanup_retry_tid = -1;
	m_cleanup_retry_delay = 30;
	m_RunAsNobody = false;
	attemptingReconnectAtStartup = false;
	m_force_fast_starter_shutdown = false;
	m_committed_time_finalized = false;
}

BaseShadow::~BaseShadow() {
	myshadow_ptr = NULL;
	if (jobAd) FreeJobAd(jobAd);
	if (gjid) free(gjid); 
	if (scheddAddr) free(scheddAddr);
	if( job_updater ) delete job_updater;
	if (m_cleanup_retry_tid != -1) daemonCore->Cancel_Timer(m_cleanup_retry_tid);
	free( core_file_name );
}

void
BaseShadow::baseInit( ClassAd *job_ad, const char* schedd_addr, const char *xfer_queue_contact_info )
{
	int pending = FALSE;

	if( ! job_ad ) {
		EXCEPT("baseInit() called with NULL job_ad!");
	}
	jobAd = job_ad;

	if (sendUpdatesToSchedd && ! is_valid_sinful(schedd_addr)) {
		EXCEPT("schedd_addr not specified with valid address");
	}
	scheddAddr = sendUpdatesToSchedd ? strdup( schedd_addr ) : strdup("noschedd");

	m_xfer_queue_contact_info = xfer_queue_contact_info ? xfer_queue_contact_info : "";

	if ( !jobAd->LookupString(ATTR_OWNER, owner)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_OWNER);
	}

	if( !jobAd->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		EXCEPT("Job ad doesn't contain a %s attribute.", ATTR_CLUSTER_ID);
	}

	if( !jobAd->LookupInteger(ATTR_PROC_ID, proc)) {
		EXCEPT("Job ad doesn't contain a %s attribute.", ATTR_PROC_ID);
	}


		// Grab the GlobalJobId if we've got it.
	if( ! jobAd->LookupString(ATTR_GLOBAL_JOB_ID, &gjid) ) {
		gjid = NULL;
	}

	// grab the NT domain if we've got it
	jobAd->LookupString(ATTR_NT_DOMAIN, domain);
	if ( !jobAd->LookupString(ATTR_JOB_IWD, iwd)) {
		EXCEPT("Job ad doesn't contain an %s attribute.", ATTR_JOB_IWD);
	}

	if( !jobAd->LookupFloat(ATTR_BYTES_SENT, prev_run_bytes_sent) ) {
		prev_run_bytes_sent = 0;
	}
	if( !jobAd->LookupFloat(ATTR_BYTES_RECVD, prev_run_bytes_recvd) ) {
		prev_run_bytes_recvd = 0;
	}

	ClassAd* prev_upload_stats = dynamic_cast<ClassAd*>(jobAd->Lookup(ATTR_TRANSFER_INPUT_STATS));
	if (prev_upload_stats) {
		m_prev_run_upload_file_stats.Update(*prev_upload_stats);
	}
	ClassAd* prev_download_stats = dynamic_cast<ClassAd*>(jobAd->Lookup(ATTR_TRANSFER_OUTPUT_STATS));
	if (prev_download_stats) {
		m_prev_run_download_file_stats.Update(*prev_download_stats);
	}

		// construct the core file name we'd get if we had one.
	std::string tmp_name = iwd;
	formatstr_cat( tmp_name, "%ccore.%d.%d", DIR_DELIM_CHAR, cluster, proc );
	core_file_name = strdup( tmp_name.c_str() );

        // put the shadow's sinful string into the jobAd.  Helpful for
        // the mpi shadow, at least...and a good idea in general.
    if ( !jobAd->Assign( ATTR_MY_ADDRESS, daemonCore->InfoCommandSinfulString() )) {
        EXCEPT( "Failed to insert %s!", ATTR_MY_ADDRESS );
    }

	DebugId = display_dprintf_header;
	
	config();

		// Make sure we've got enough swap space to run
	checkSwap();

	// handle system calls with Owner's privilege
// XXX this belong here?  We'll see...
	// Calling init_user_ids() while in user priv causes badness.
	// Make sure we're in another priv state.
	set_condor_priv();
	if ( !init_user_ids(owner.c_str(), domain.c_str())) {
		dprintf(D_ALWAYS, "init_user_ids() failed as user %s\n",owner.c_str() );
		// uids.C will EXCEPT when we set_user_priv() now
		// so there's not much we can do at this point
		
#if ! defined(WIN32)
		if ( param_boolean( "SHADOW_RUN_UNKNOWN_USER_JOBS", false ) )
		{
			dprintf(D_ALWAYS, "trying init_user_ids() as user nobody\n" );
			
			owner="nobody";
			domain="";
			if (!init_user_ids(owner.c_str(), domain.c_str()))
			{
				dprintf(D_ALWAYS, "init_user_ids() failed!\n");
			}
			else
			{
				jobAd->Assign( ATTR_JOB_RUNAS_OWNER, false );
				m_RunAsNobody=true;
				dprintf(D_ALWAYS, "init_user_ids() now running as user nobody\n");
			}
		}
#endif

	}
	set_user_priv();
	daemonCore->Register_Priv_State( PRIV_USER );

	dumpClassad( "BaseShadow::baseInit()", this->jobAd, D_JOB );

		// initialize for LIMIT_DIRECTORY_ACCESS
	{
		std::string job_ad_whitelist, spoolDir;
		jobAd->EvaluateAttrString(ATTR_JOB_LIMIT_DIRECTORY_ACCESS,job_ad_whitelist);
		SpooledJobFiles::getJobSpoolPath(jobAd, spoolDir);
		allow_shadow_access(NULL, true, job_ad_whitelist.c_str(),spoolDir.c_str());
	}

		// initialize the UserPolicy object
	shadow_user_policy.init( jobAd, this );

		// setup an object to keep our job ad updated to the schedd's
		// permanent job queue.  this clears all the dirty bits on our
		// copy of the classad, so anything we touch after this will
		// be updated to the schedd when appropriate.

		// Unless we got a command line arg asking us not to
	if (sendUpdatesToSchedd) {
		// the usual case
		job_updater = new QmgrJobUpdater( jobAd, scheddAddr );
	} else {
		job_updater = new NullQmgrJobUpdater( jobAd, scheddAddr );
	}

		// init user log; hold on failure
		// NOTE: job_updater must be initialized _before_ initUserLog(),
		// in order to handle the case of the job going on hold as a
		// result of failure in initUserLog().
	initUserLog();

		// change directory; hold on failure
	if ( cdToIwd() == -1 ) {
		EXCEPT("Could not cd to initial working directory");
	}

		// check to see if this invocation of the shadow is just to write
		// a terminate event and exit since this job had been recorded as
		// pending termination, but somehow the job didn't leave the queue
		// and the schedd is trying to restart it again..
	if( jobAd->LookupInteger(ATTR_TERMINATION_PENDING, pending)) {
		if (pending == TRUE) {
			// If the classad of this job "thinks" that this job should be
			// finished already, let's enact that belief.
			// This function does not return.
			this->terminateJob(US_TERMINATE_PENDING);
		}
	}

		// If we need to claim the startd before activating the claim
	bool wantClaiming = false;
	jobAd->LookupBool(ATTR_CLAIM_STARTD, wantClaiming);
	if (wantClaiming) {
		std::string startdSinful;
		std::string claimid;

			// Pull startd addr and claimid out of the jobad
		jobAd->LookupString(ATTR_STARTD_IP_ADDR, startdSinful);
		jobAd->LookupString(ATTR_CLAIM_ID, claimid);

		dprintf(D_ALWAYS, "%s is true, trying to claim startd %s\n", ATTR_CLAIM_STARTD, startdSinful.c_str());

		classy_counted_ptr<DCStartd> startd = new DCStartd("description", NULL, startdSinful.c_str(), claimid.c_str());
	
		classy_counted_ptr<DCMsgCallback> cb = 
			new DCMsgCallback((DCMsgCallback::CppFunction)&BaseShadow::startdClaimedCB,
			this, jobAd);
																 
			// this can't fail, will always call the callback
		startd->asyncRequestOpportunisticClaim(jobAd, 
											   "description", 
											   daemonCore->InfoCommandSinfulString(), 
											   1200 /*alive interval*/,
											   false, /* don't claim pslot */
											   20 /* net timeout*/, 
											   100 /*total timeout*/, 
											   cb);
	}
}

	// We land in this callback when we need to claim the startd
	// when we get here, the claiming is finished, successful
	// or not
void BaseShadow::startdClaimedCB(DCMsgCallback *) {

	// We've claimed the startd, the following kicks off the
	// activation of the claim, and runs the job
	this->spawn();
}

void BaseShadow::config()
{
	reconnect_ceiling = param_integer( "RECONNECT_BACKOFF_CEILING", 300 );

	reconnect_e_factor = 0.0;
	reconnect_e_factor = param_double( "RECONNECT_BACKOFF_FACTOR", 2.0, 0.0 );
	if( reconnect_e_factor < -1e-4 || reconnect_e_factor > 1e-4) {
    	reconnect_e_factor = 2.0;
    }

	m_cleanup_retry_tid = -1;
	m_num_cleanup_retries = 0;
		// NOTE: these config knobs are very similar to
		// LOCAL_UNIVERSE_MAX_JOB_CLEANUP_RETRIES and
		// LOCAL_UNIVERSE_JOB_CLEANUP_RETRY_DELAY in the local starter.
	m_max_cleanup_retries = param_integer("SHADOW_MAX_JOB_CLEANUP_RETRIES", 5);
	m_cleanup_retry_delay = param_integer("SHADOW_JOB_CLEANUP_RETRY_DELAY", 30);

	m_lazy_queue_update = param_boolean("SHADOW_LAZY_QUEUE_UPDATE", true);
}


int BaseShadow::cdToIwd() {
	int iRet =0;
	
#if ! defined(WIN32)
	priv_state p = PRIV_UNKNOWN;
	
	if (m_RunAsNobody)
		p = set_root_priv();
#endif
	
	if (chdir(iwd.c_str()) < 0) {
		int chdir_errno = errno;
		dprintf(D_ALWAYS, "\n\nPath does not exist.\n"
				"He who travels without bounds\n"
				"Can't locate data.\n\n" );
		std::string hold_reason;
		formatstr(hold_reason, "Cannot access initial working directory %s: %s",
		                    iwd.c_str(), strerror(chdir_errno));
		dprintf( D_ALWAYS, "%s\n",hold_reason.c_str());
		holdJobAndExit(hold_reason.c_str(),CONDOR_HOLD_CODE::IwdError,chdir_errno);
		iRet = -1;
	}
	
#if ! defined(WIN32)
	if ( m_RunAsNobody )
		set_priv(p);
#endif
	
	return iRet;
}


void
BaseShadow::shutDownFast( int reason ) {
	m_force_fast_starter_shutdown = true;
	shutDown( reason );
}

void
BaseShadow::shutDown( int reason ) 
{
		// exit now if there is no job ad
	if ( !getJobAd() ) {
		DC_Exit( reason );
	}
		//Attempt to write Job ad to epoch file
		//If knob isn't set or there is no job ad the function will just log and return
	writeJobEpochFile(getJobAd());
		// if we are being called from the exception handler, return
		// now to prevent infinite loop in case we call EXCEPT below.
	if ( reason == JOB_EXCEPTION ) {
		return;
	}

		// Only if the job is trying to leave the queue should we
		// evaluate the user job policy...
	if( reason == JOB_EXITED || reason == JOB_COREDUMPED ) {
		if( !waitingToUpdateSchedd() ) {
			shadow_user_policy.checkAtExit();
				// WARNING: 'this' may have been deleted by the time we get here!!!
		}
	}
	else {
		// if we aren't trying to evaluate the user's policy, we just
		// want to evict this job.
		evictJob( reason );
	}
}


int
BaseShadow::nextReconnectDelay( int attempts ) const
{
	if( ! attempts ) {
			// first time, do it right away
		return 0;
	}
	int n = (int)ceil(::pow(reconnect_e_factor, (attempts+2)));
	if( n > reconnect_ceiling || n < 0 ) {
		n = reconnect_ceiling;
	}
	return n;
}


void
BaseShadow::reconnectFailed( const char* reason )
{
		// try one last time to release the claim, write a UserLog event
		// about it, and exit with a special status. 
	dprintf( D_ALWAYS, "Reconnect FAILED: %s\n", reason );
	
	logReconnectFailedEvent( reason );

		// if the shadow was born disconnected, exit with 
		// JOB_RECONNECT_FAILED so the schedd can make 
		// an accurate restart report.  otherwise just
		// exist with JOB_SHOULD_REQUEUE.
	if ( attemptingReconnectAtStartup ) {
		dprintf(D_ALWAYS,"Exiting with JOB_RECONNECT_FAILED\n");
		// does not return
		DC_Exit( JOB_RECONNECT_FAILED );
	} else {
		int exit_reason = getExitReason();
		if (exit_reason == -1) {
			exit_reason = JOB_SHOULD_REQUEUE;
		}
		dprintf(D_ALWAYS,"Exiting with %d\n", exit_reason);
		// does not return
		DC_Exit( exit_reason );
	}

	// Should never get here....
	ASSERT(true);
}

std::string
BaseShadow::improveHoldAttributes(const char* const orig_hold_reason, int & hold_reason_code, int & /*hold_reason_subcode*/ )
{
	/* This method is invokved by the shadow when we are putting a job on hold, and allow us to
	   change/edit the HoldReason attribute and HoldReason codes before the shadow sends the info
	   to the schedd.
	   To change HoldReason string:  just set variable new_hold_reason - if it remains the empty string,
	       then the orig_hold_reason will be used unmodified.
	   To change HoldReasonCode or HoldReasonSubCode: just change the value of hold_reason_code and
	       hold_reason_subcode respectively (not they are passed in by reference).
	*/
	std::string new_hold_reason;

	/*
	Improve hold reason/codes related to file transfer.  Here is a table of what
	gets set by the file transfer object (from the starter), which is pretty
	much useless to anyone who is not an expert:

		1. HoldReasonCode = 13 (UploadFileError) if transfer of input files fail w/ error at AP
		   Sanple HoldReason = "Error from slot1@TODDS480S: SHADOW at 127.0.0.1 failed to send file(s) to <127.0.0.1:50240>: error reading from C:\condor\test\trans1.bat trans2.bat: (errno 2) No such file or directory; STARTER failed to receive file(s) from <127.0.0.1:50211>"

		2. HoldReasonCode = 13 (UploadFileError) if transfer of output files fail w/ error at EP
		   Sample HoldReason = "Error from slot2@TODDS480S: STARTER at 127.0.0.1 failed to send file(s) to <127.0.0.1:50212>: error reading from C:\condor\execute\dir_13724\trans2.bat: (errno 2) No such file or directory; SHADOW failed to receive file(s) from <127.0.0.1:50258>"

		3. HoldReasonCode = 12 (DownloadFileError) if transfer of input files fail w/ error at EP
		   Sample HoldReason = "Error from slot3@TODDS480S: FILETRANSFER:1:non-zero exit (1) from C:\condor\bin\curl_plugin.exe. Error: Could not resolve host: neversslxxx.com (http://neversslxxx.com/index.html)|FILETRANSFER:1:Failed to execute C:\condor\bin/onedrive_plugin.py, ignoring|FILETRANSFER:1:Failed to execute C:\condor\bin/gdrive_plugin.py, ignoring|FILETRANSFER:1:Failed to execute C:\condor\bin\box_plugin.py, ignoring"

		4. HoldReasonCode = 12 (DownloadFileError) if transfer of output files fail w/ error at AP
		   Sample HoldReason = "Error from slot1@TODDS480S: STARTER at 127.0.0.1 failed to send file(s) to <127.0.0.1:50288>; SHADOW at 127.0.0.1 failed to write to file C:\condor\test\not_there\blah: (errno 2) No such file or directory"

	Here we will rewrite the hold reason code to be TransferInputError or TransferOutputError, instead of
	the alsmost meaningless UploadFileError and DownloadFileError we get from the File Transfer object.
	And try to improve the hold reason string as well, so people who shower have a chance understanding it.
	*/
	if (hold_reason_code == FILETRANSFER_HOLD_CODE::DownloadFileError ||
		hold_reason_code == FILETRANSFER_HOLD_CODE::UploadFileError)
	{
		bool transfer_input_error = !began_execution;
		bool transfer_output_error = began_execution;
		bool err_occurred_at_EP =
			(transfer_input_error && hold_reason_code == FILETRANSFER_HOLD_CODE::DownloadFileError) ||
			(transfer_output_error && hold_reason_code == FILETRANSFER_HOLD_CODE::UploadFileError);

		std::string old_reason = orig_hold_reason;
		size_t pos = std::string::npos;

		// Try to parse out the name of the slot / EP involved
		std::string slot_name;
		if (old_reason.find("Error from ") == 0) {
			size_t len = strlen("Error from ");
			pos = old_reason.find(':',len);
			if (pos > len && pos < len + 50) {   // sanity check: slot name less than 50 chars
				slot_name = old_reason.substr(len, pos-len);
			}
		}

		// If transfer failure involved an URL (via transfer plugin), try to parse out the URL
		std::string url_file;
		std::string url_file_type;
		pos = old_reason.find("URL file = ");
		if (pos != std::string::npos) {
			pos += strlen("URL file = ");
			size_t end = old_reason.find_first_of(" |;", pos);
			if (end == std::string::npos) {
				end = old_reason.length() - 1;
			}
			if (end > pos && end < pos + 150) { // sanity check
				url_file = old_reason.substr(pos, end);
				url_file_type = getURLType(url_file.c_str(), true);
			}
		}


		// Try to parse out the actual error encountered from the old HoldReason string.
		std::string actual_error;
		pos = old_reason.find("|Error: ");
		if (pos != std::string::npos) {
			pos += strlen("|Error: ");
			size_t end = old_reason.find_first_of("|;", pos);
			if (end == std::string::npos) {
				end = old_reason.length() - 1;
			}
			if (end > pos && end < pos + 150) { // sanity check
				actual_error = old_reason.substr(pos, end);
				if ((pos = actual_error.find("; SHADOW")) == std::string::npos) {
					if ((pos = actual_error.find("; STARTER")) == std::string::npos) {
						pos = actual_error.find("||");
					}
				}
				if (pos != std::string::npos) {
					actual_error = actual_error.substr(0, pos);
				}
			}
		}

		// Currently, the hold_reason_code is set to either FILETRANSFER_HOLD_CODE::DownloadFileError
		// or FILETRANSFER_HOLD_CODE::UploadFileError.  These are not very useful to users.
		// So reset the hold_reason_code to something based on if the failure happened
		// during transfer of input files or during transfer of output/checkpoint files.
		if (transfer_input_error) {
			hold_reason_code = CONDOR_HOLD_CODE::TransferInputError;
		}
		else {
			hold_reason_code = CONDOR_HOLD_CODE::TransferOutputError;
		}

		// Now create a more human-readable HoldReason string.
		std::string EP_name = slot_name.empty() ? "the execution point" : ("execution point " + slot_name);
		formatstr(new_hold_reason, "Transfer %s files failure at ",
			transfer_input_error ? "input" : "output");
		if (err_occurred_at_EP) {
			// Error happened at the EP
			new_hold_reason += EP_name;
			if (url_file_type.empty()) {
				formatstr_cat(new_hold_reason, " while %s files %s access point %s",
					transfer_input_error ? "receiving" : "sending",
					transfer_input_error ? "from" : "to",
					get_local_hostname().c_str());
			}
		}
		else {
			// Error happened at the AP
			formatstr_cat(new_hold_reason, "access point %s while %s files %s %s",
				get_local_hostname().c_str(),
				transfer_output_error ? "receiving" : "sending",
				transfer_output_error ? "from" : "to",
				EP_name.c_str());
		}
		if (!url_file_type.empty()) {
			formatstr_cat(new_hold_reason, " using protocol %s", url_file_type.c_str());
		}
		formatstr_cat(new_hold_reason, ". Details: %s",
			actual_error.empty() ? orig_hold_reason : actual_error.c_str());
	}

	return new_hold_reason;
}

void
BaseShadow::holdJob( const char* reason, int hold_reason_code, int hold_reason_subcode )
{


	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In HoldJob() for job %d.%d w/ NULL JobAd!\n", getCluster(), getProc() );
		DC_Exit( JOB_SHOULD_HOLD );
	}

	// Note: improveHoldAttributescan change the hold_reason_code and subcode,
	// and will pass back a potentially improved hold reason string.
	std::string improved_hold_reason =
		improveHoldAttributes(reason, hold_reason_code, hold_reason_subcode);
	// If we have an improved hold reason string, use it instead.
	if (!improved_hold_reason.empty()) {
		reason = improved_hold_reason.c_str();
	}

	dprintf(D_ALWAYS, "Job %d.%d going into Hold state (code %d,%d): %s\n",
		getCluster(), getProc(), hold_reason_code, hold_reason_subcode, reason);

		// cleanup this shadow (kill starters, etc)
	cleanUp( jobWantsGracefulRemoval() );

		// Put the reason in our job ad.
	jobAd->Assign( ATTR_HOLD_REASON, reason );
	jobAd->Assign( ATTR_HOLD_REASON_CODE, hold_reason_code );
	jobAd->Assign( ATTR_HOLD_REASON_SUBCODE, hold_reason_subcode );

		// try to send email (if the user wants it)
	emailHoldEvent( reason );

		// update the job queue for the attributes we care about
	if( !updateJobInQueue(U_HOLD) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

}

void
BaseShadow::holdJobAndExit( const char* reason, int hold_reason_code, int hold_reason_subcode )
{
	m_force_fast_starter_shutdown = true;
	holdJob(reason,hold_reason_code,hold_reason_subcode);
	writeJobEpochFile(getJobAd());

	// Doing this neither prevents scary network-level error messages in
	// the starter log, nor actually works: if the shadow doesn't exit
	// here it exits later with a different error code that causes the job
	// to be rescheduled.
	// exitAfterEvictingJob( JOB_SHOULD_HOLD );
	DC_Exit( JOB_SHOULD_HOLD );
}

void
BaseShadow::mockTerminateJob( std::string exit_reason,
		bool exited_by_signal, int exit_code, int exit_signal, 
		bool core_dumped )
{
	if (exit_reason == "") {
		exit_reason = "Exited normally";
	}
	
	dprintf( D_ALWAYS, "Mock terminating job %d.%d: "
			"exited_by_signal=%s, exit_code=%d OR exit_signal=%d, "
			"core_dumped=%s, exit_reason=\"%s\"\n", 
			 getCluster(),
			 getProc(), 
			 exited_by_signal ? "TRUE" : "FALSE",
			 exit_code,
			 exit_signal,
			 core_dumped ? "TRUE" : "FALSE",
			 exit_reason.c_str());

	if( ! jobAd ) {
		dprintf(D_ALWAYS, "BaseShadow::mockTerminateJob(): NULL JobAd! "
			"Holding Job!");
		DC_Exit( JOB_SHOULD_HOLD );
	}

	// Insert the various exit attributes into our job ad.
	jobAd->Assign( ATTR_JOB_CORE_DUMPED, core_dumped );
	jobAd->Assign( ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal );

	if (exited_by_signal) {
		jobAd->Assign( ATTR_ON_EXIT_SIGNAL, exit_signal );
	} else {
		jobAd->Assign( ATTR_ON_EXIT_CODE, exit_code );
	}

	jobAd->Assign( ATTR_EXIT_REASON, exit_reason );

		// update the job queue for the attributes we care about
	if( !updateJobInQueue(U_TERMINATE) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}
}


void BaseShadow::removeJobPre( const char* reason )
{
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In removeJob() w/ NULL JobAd!\n" );
	}
	dprintf( D_ALWAYS, "Job %d.%d is being removed: %s\n", 
			 getCluster(), getProc(), reason );

	// cleanup this shadow (kill starters, etc)
	cleanUp( jobWantsGracefulRemoval() );

	// Put the reason in our job ad.
	jobAd->Assign( ATTR_REMOVE_REASON, reason );

	emailRemoveEvent( reason );

	// update the job ad in the queue with some important final
	// attributes so we know what happened to the job when using
	// condor_history...
	if( !updateJobInQueue(U_REMOVE) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}
}

void BaseShadow::removeJob( const char* reason )
{
	this->removeJobPre(reason);

	exitAfterEvictingJob( JOB_SHOULD_REMOVE );
}

void
BaseShadow::retryJobCleanup()
{
	m_num_cleanup_retries++;
	if (m_num_cleanup_retries > m_max_cleanup_retries) {
		dprintf(D_ALWAYS,
		        "Maximum number of job cleanup retry attempts "
		        "(SHADOW_MAX_JOB_CLEANUP_RETRIES=%d) reached"
		        "; Forcing job requeue!\n",
		        m_max_cleanup_retries);
		DC_Exit(JOB_SHOULD_REQUEUE);
	}
	ASSERT(m_cleanup_retry_tid == -1);
	m_cleanup_retry_tid = daemonCore->Register_Timer(m_cleanup_retry_delay, 0,
					(TimerHandlercpp)&BaseShadow::retryJobCleanupHandler,
					"retry job cleanup", this);
	dprintf(D_FULLDEBUG, "Will retry job cleanup in "
	        "SHADOW_JOB_CLEANUP_RETRY_DELAY=%d seconds\n",
	        m_cleanup_retry_delay);
}


void
BaseShadow::retryJobCleanupHandler( int /* timerID */ )
{
	m_cleanup_retry_tid = -1;
	dprintf(D_ALWAYS, "Retrying job cleanup, calling terminateJob()\n");
	terminateJob();
}

void
BaseShadow::terminateJob( update_style_t kind ) // has a default argument of US_NORMAL
{
	int reason;
	bool signaled;

	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In terminateJob() w/ NULL JobAd!\n" );
	}

	/* The first thing we do is record that we are in a termination pending
		state. */
	if (kind == US_NORMAL) {
		jobAd->Assign( ATTR_TERMINATION_PENDING, true );
	}

	if (kind == US_TERMINATE_PENDING) {
		// In this case, the job had already completed once and the
		// status had been saved to the job queue, however, for
		// some reason, the shadow didn't exit with a good value and
		// the job had been requeued. When this style of update
		// is used, it is a shortcut from the very birth of the shadow
		// to here, and so there will not be a remote resource or
		// anything like that set up. In this situation, we just
		// want to write the log event and mail the user and exit
		// with a good exit code so the schedd removes the job from
		// the queue. If for some reason the logging fails once again,
		// the process continues to repeat. 
		// This means at least once semantics for the termination event
		// and user email, but at no time should the job actually execute
		// again.

		int exited_by_signal = FALSE;
		int exit_signal = 0;
		int exit_code = 0;

		getJobAdExitedBySignal(jobAd, exited_by_signal);
		if (exited_by_signal == TRUE) {
			getJobAdExitSignal(jobAd, exit_signal);
		} else {
			getJobAdExitCode(jobAd, exit_code);
		}

		if (exited_by_signal == TRUE) {
			reason = JOB_COREDUMPED;
			jobAd->Assign( ATTR_JOB_CORE_FILENAME, core_file_name );
		} else {
			reason = JOB_EXITED;
		}

		dprintf( D_ALWAYS, "Job %d.%d terminated: %s %d\n",
	 		getCluster(), getProc(), 
	 		exited_by_signal? "killed by signal" : "exited with status",
	 		exited_by_signal ? exit_signal : exit_code );
		
			// write stuff to user log, but get values from jobad
		logTerminateEvent( reason, kind );

			// email the user, but get values from jobad
		emailTerminateEvent( reason, kind );

		DC_Exit( reason );
	}

	// the default path when kind == US_NORMAL

	// cleanup this shadow (kill starters, etc)
	cleanUp();

	reason = getExitReason();
	signaled = exitedBySignal();

	/* also store the corefilename into the jobad so we can recover this 
		during a termination pending scenario. */
	if( reason == JOB_COREDUMPED ) {
		jobAd->Assign( ATTR_JOB_CORE_FILENAME, getCoreName() );
	}

    // Update final Job committed time
    time_t last_ckpt_time = 0;
    if(! jobAd->LookupInteger(ATTR_LAST_CKPT_TIME, last_ckpt_time)) {
        jobAd->LookupInteger(ATTR_JOB_LAST_CHECKPOINT_TIME, last_ckpt_time);
    }

    time_t current_start_time = 0;
    jobAd->LookupInteger(ATTR_JOB_CURRENT_START_DATE, current_start_time);
    time_t int_value = (last_ckpt_time > current_start_time) ?
                        last_ckpt_time : current_start_time;

    if( int_value > 0 && !m_committed_time_finalized ) {
        int job_committed_time = 0;
        jobAd->LookupInteger(ATTR_JOB_COMMITTED_TIME, job_committed_time);
		int delta = (int)(time(nullptr) - int_value);
        job_committed_time += delta;
        jobAd->Assign(ATTR_JOB_COMMITTED_TIME, job_committed_time);

		double slot_weight = 1;
		jobAd->LookupFloat(ATTR_JOB_MACHINE_ATTR_SLOT_WEIGHT0, slot_weight);
		double slot_time = 0;
		jobAd->LookupFloat(ATTR_COMMITTED_SLOT_TIME, slot_time);
		slot_time += slot_weight * delta;
		jobAd->Assign(ATTR_COMMITTED_SLOT_TIME, slot_time);

		m_committed_time_finalized = true;
    }

	CommitSuspensionTime(jobAd);

	// update the job ad in the queue with some important final
	// attributes so we know what happened to the job when using
	// condor_history...
    if (m_num_cleanup_retries < 1 &&
        param_boolean("SHADOW_TEST_JOB_CLEANUP_RETRY", false)) {
		dprintf( D_ALWAYS,
				 "Testing Failure to perform final update to job queue!\n");
		retryJobCleanup();
		return;
    }
	if( !updateJobInQueue(U_TERMINATE) ) {
		dprintf( D_ALWAYS, 
				 "Failed to perform final update to job queue!\n");
		retryJobCleanup();
		return;
	}

	// Let's maximize the effectiveness of that queue synchronization and
	// only record the job as done if the update to the queue was successful.
	// If any of these next operations fail and the shadow exits with an
	// exit code which causes the job to get requeued, it will be in the
	// "terminate pending" state marked by the ATTR_TERMINATION_PENDING
	// attribute.

	dprintf( D_ALWAYS, "Job %d.%d terminated: %s %d\n",
	 	getCluster(), getProc(), 
	 	signaled ? "killed by signal" : "exited with status",
	 	signaled ? exitSignal() : exitCode() );

	// write stuff to user log:
	logTerminateEvent( reason );

	// email the user
	emailTerminateEvent( reason );

	if( reason == JOB_EXITED && claimIsClosing() ) {
			// Startd not accepting any more jobs on this claim.
			// We do this here to avoid having to treat this case
			// identically to JOB_EXITED in the code leading up to
			// this point.
		dprintf(D_FULLDEBUG,"Startd is closing claim, so no more jobs can be run on it.\n");
		reason = JOB_EXITED_AND_CLAIM_CLOSING;
	}

	// try to get a new job for this shadow
	if( recycleShadow(reason) ) {
		// recycleShadow delete's this, so we must return immediately
		return;
	}

	// does not return.
	DC_Exit( reason );
}


void
BaseShadow::evictJob( int reason )
{
	std::string from_where;
	std::string machine;

	// If we previously delayed exiting to let the starter wrap up, then
	// immediately try exiting now. None of the cleanup below here is
	// appropriate in that case.
	if ( exitDelayed( reason ) ) {
		exitAfterEvictingJob( reason );
		return;
	}

	if( getMachineName(machine) ) {
		formatstr(from_where, " from %s" ,machine.c_str());
	}
	dprintf( D_ALWAYS, "Job %d.%d is being evicted%s\n",
			 getCluster(), getProc(), from_where.c_str() );

	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In evictJob() w/ NULL JobAd!\n" );
		DC_Exit( reason );
	}

		// cleanup this shadow (kill starters, etc)
	cleanUp( jobWantsGracefulRemoval() );

		// write stuff to user log:
	logEvictEvent( reason );

		// record the time we were vacated into the job ad 
	jobAd->Assign( ATTR_LAST_VACATE_TIME, time(nullptr) );

		// update the job ad in the queue with some important final
		// attributes so we know what happened to the job when using
		// condor_history...
	if( !updateJobInQueue(U_EVICT) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

	exitAfterEvictingJob( reason );
}


void
BaseShadow::requeueJob( const char* reason )
{
	if( ! jobAd ) {
		dprintf( D_ALWAYS, "In requeueJob() w/ NULL JobAd!\n" );
	}
	dprintf( D_ALWAYS, 
			 "Job %d.%d is being put back in the job queue: %s\n", 
			 getCluster(), getProc(), reason );

		// cleanup this shadow (kill starters, etc)
	cleanUp();

		// Put the reason in our job ad.
	jobAd->Assign( ATTR_REQUEUE_REASON, reason );

		// write stuff to user log:
	logRequeueEvent( reason );

		// update the job ad in the queue with some important final
		// attributes so we know what happened to the job when using
		// condor_history...
	if( !updateJobInQueue(U_REQUEUE) ) {
			// trouble!  TODO: should we do anything else?
		dprintf( D_ALWAYS, "Failed to update job queue!\n" );
	}

	exitAfterEvictingJob( JOB_SHOULD_REQUEUE );
}


void
BaseShadow::emailHoldEvent( const char* reason ) 
{
	Email mailer;
	mailer.sendHold( jobAd, reason );
}


void
BaseShadow::emailRemoveEvent( const char* reason ) 
{
	Email mailer;
	mailer.sendRemove( jobAd, reason );
}


void BaseShadow::initUserLog()
{
		// we expect job_updater to already be initialized, in case we
		// need to put the job on hold as a result of failure to open
		// the log
	ASSERT( job_updater );

	if( !uLog.initialize(*jobAd) ) {
		// TODO Should we keep inclusion of filename in hold message?
		std::string hold_reason;
		formatstr(hold_reason,"Failed to initialize user log");
		dprintf( D_ALWAYS, "%s\n",hold_reason.c_str());
		holdJobAndExit(hold_reason.c_str(),
				CONDOR_HOLD_CODE::UnableToInitUserLog,0);
			// holdJobAndExit() should not return, but just in case it does
			// EXCEPT
		EXCEPT("Failed to initialize user log: %s",hold_reason.c_str());
	}
}


// returns TRUE if attribute found.
int getJobAdExitCode(ClassAd *jad, int &exit_code)
{
	if( ! jad->LookupInteger(ATTR_ON_EXIT_CODE, exit_code) ) {
		return FALSE;
	}

	return TRUE;
}

// returns TRUE if attribute found.
int getJobAdExitedBySignal(ClassAd *jad, int &exited_by_signal)
{
	if( ! jad->LookupInteger(ATTR_ON_EXIT_BY_SIGNAL, exited_by_signal) ) {
		return FALSE;
	}

	return TRUE;
}

// returns TRUE if attribute found.
int getJobAdExitSignal(ClassAd *jad, int &exit_signal)
{
	if( ! jad->LookupInteger(ATTR_ON_EXIT_SIGNAL, exit_signal) ) {
		return FALSE;
	}

	return TRUE;
}

static void set_usageAd (ClassAd* jobAd, ClassAd ** ppusageAd) 
{
	std::string resslist;
	if ( ! jobAd->LookupString("ProvisionedResources", resslist))
		resslist = "Cpus, Disk, Memory";

	StringList reslist(resslist.c_str());
	if (reslist.number() > 0) {
		ClassAd * puAd = new ClassAd();

		reslist.rewind();
		while (const char * resname = reslist.next()) {
			std::string attr;
			std::string res = resname;
			title_case(res); // capitalize it to make it print pretty.
			const int copy_ok = classad::Value::ERROR_VALUE | classad::Value::BOOLEAN_VALUE | classad::Value::INTEGER_VALUE | classad::Value::REAL_VALUE;
			classad::Value value;
			attr = res + "Provisioned";	 // provisioned value
			if (jobAd->EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(resname, plit); // usage ad has attribs like they appear in Machine ad
				}
			}
			// /*for debugging*/ else { puAd->Assign(resname, 42); }
			attr = "Request"; attr += res;   	// requested value
			if (jobAd->EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}
			// /*for debugging*/ else { puAd->Assign(attr, 99); }

			attr = res + "Usage"; // (implicitly) peak usage value
			if (jobAd->EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "AverageUsage"; // average usage
			if (jobAd->EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "MemoryUsage"; // special case for GPUs.
			if (jobAd->EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = res + "MemoryAverageUsage"; // just in case.
			if (jobAd->EvaluateAttr(attr, value, classad::Value::SCALAR_EX_VALUES) && (value.GetType() & copy_ok) != 0) {
				classad::ExprTree * plit = classad::Literal::MakeLiteral(value);
				if (plit) {
					puAd->Insert(attr, plit);
				}
			}

			attr = "Assigned"; attr += res;
			CopyAttribute( attr, *puAd, *jobAd );
		}

		// Hard code a couple of useful time-based attributes that are not "Requested" yet
		// and shorten their names to display more reasonably
		int jaed = 0;
		if (jobAd->LookupInteger(ATTR_JOB_ACTIVATION_EXECUTION_DURATION, jaed)) {
			puAd->Assign("TimeExecuteUsage", jaed);
		}
		int jad = 0;
		if (jobAd->LookupInteger(ATTR_JOB_ACTIVATION_DURATION, jad)) {
			puAd->Assign("TimeSlotBusyUsage", jad);
		}
		*ppusageAd = puAd;
	}
}

// kind defaults to US_NORMAL.
void
BaseShadow::logTerminateEvent( int exitReason, update_style_t kind )
{
	struct rusage run_remote_rusage;
	struct rusage total_remote_rusage;
	JobTerminatedEvent event;
	std::string corefile;

	memset( &run_remote_rusage, 0, sizeof(struct rusage) );
	memset( &total_remote_rusage, 0, sizeof(struct rusage) );

	switch( exitReason ) {
	case JOB_EXITED:
	case JOB_COREDUMPED:
		break;
	default:
		dprintf( D_ALWAYS, 
				 "UserLog logTerminateEvent with unknown reason (%d), aborting\n",
				 exitReason ); 
		return;
	}

	if (kind == US_TERMINATE_PENDING) {

		int exited_by_signal = FALSE;
		int exit_signal = 0;
		int exit_code = 0;

		getJobAdExitedBySignal(jobAd, exited_by_signal);
		if (exited_by_signal == TRUE) {
			getJobAdExitSignal(jobAd, exit_signal);
			event.normal = false;
			event.signalNumber = exit_signal;
		} else {
			getJobAdExitCode(jobAd, exit_code);
			event.normal = true;
			event.returnValue = exit_code;
		}

		/* grab usage information out of job ad */
		double real_value;
		if( jobAd->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, real_value) ) {
			run_remote_rusage.ru_stime.tv_sec = (time_t) real_value;
		}

		if( jobAd->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, real_value) ) {
			run_remote_rusage.ru_utime.tv_sec = (time_t) real_value;
		}
		event.run_remote_rusage = run_remote_rusage;

		if( jobAd->LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU, real_value) ) {
			total_remote_rusage.ru_stime.tv_sec = (time_t) real_value;
		}

		if( jobAd->LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU, real_value) ) {
			total_remote_rusage.ru_utime.tv_sec = (time_t) real_value;
		}
		event.total_remote_rusage = total_remote_rusage;
		/*
		  Both the job ad and the terminated event record bytes
		  transferred from the perspective of the job, not the shadow.
		*/
		jobAd->LookupFloat(ATTR_BYTES_RECVD, event.recvd_bytes);
		jobAd->LookupFloat(ATTR_BYTES_SENT, event.sent_bytes);

		event.total_recvd_bytes = event.recvd_bytes;
		event.total_sent_bytes = event.sent_bytes;

		if( exited_by_signal == TRUE ) {
			jobAd->LookupString(ATTR_JOB_CORE_FILENAME, corefile);
			event.core_file = corefile;
		}

		classad::ClassAd * toeTag = dynamic_cast<classad::ClassAd *>(jobAd->Lookup(ATTR_JOB_TOE));
		event.setToeTag( toeTag );
		if (!uLog.writeEvent (&event,jobAd)) {
			dprintf (D_ALWAYS,"Unable to log "
				 	"ULOG_JOB_TERMINATED event\n");
			EXCEPT("UserLog Unable to log ULOG_JOB_TERMINATED event");
		}

		return;
	}

	// the default kind == US_NORMAL path

	run_remote_rusage = getRUsage();
		/* grab usage information out of job ad */
		double real_value;

		if( jobAd->LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_SYS_CPU, real_value) ) {
			total_remote_rusage.ru_stime.tv_sec = (time_t) real_value;
		}

		if( jobAd->LookupFloat(ATTR_JOB_CUMULATIVE_REMOTE_USER_CPU, real_value) ) {
			total_remote_rusage.ru_utime.tv_sec = (time_t) real_value;
		}


	if( exitedBySignal() ) {
		event.normal = false;
		event.signalNumber = exitSignal();
	} else {
		event.normal = true;
		event.returnValue = exitCode();
	}

		// TODO: fill in local/total rusage
		// event.run_local_rusage = r;
	event.run_remote_rusage = run_remote_rusage;
		// event.total_local_rusage = r;
	event.total_remote_rusage = total_remote_rusage;

		/*
		  we want to log the events from the perspective of the user
		  job, so if the shadow *sent* the bytes, then that means the
		  user job *received* the bytes
		*/
	event.recvd_bytes = bytesSent();
	event.sent_bytes = bytesReceived();

	event.total_recvd_bytes = prev_run_bytes_recvd + bytesSent();
	event.total_sent_bytes = prev_run_bytes_sent + bytesReceived();

	if( exitReason == JOB_COREDUMPED && core_file_name ) {
		event.core_file = core_file_name;
	}

#if 1
	set_usageAd(jobAd, &event.pusageAd);
#else
	std::string resslist;
	if ( ! jobAd->LookupString("PartitionableResources", resslist))
		resslist = "Cpus, Disk, Memory";

	StringList reslist(resslist.c_str());
	if (reslist.number() > 0) {
		int64_t int64_value = 0;
		ClassAd * puAd = new ClassAd();

		reslist.rewind();
		char * resname = NULL;
		while ((resname = reslist.next()) != NULL) {
			std::string attr;
			int64_value = -1;
			attr = resname; // provisioned value
			if (jobAd->LookupInteger(attr, int64_value)) {
				puAd->Assign(attr, int64_value);
			} 
			// /*for debugging*/ else { puAd->Assign(attr, 42); }
			int64_value = -2;
			attr = "Request";
			attr += resname; // requested value
			if (jobAd->LookupInteger(attr, int64_value)) {
				puAd->Assign(attr, int64_value);
			}
			// /*for debugging*/ else { puAd->Assign(attr, 99); }
			int64_value = -3;
			attr = resname;
			attr += "Usage"; // usage value
			if (jobAd->LookupInteger(attr, int64_value)) {
				puAd->Assign(attr, int64_value);
			}
		}
		event.pusageAd = puAd;
	}
#endif

	classad::ClassAd * toeTag = dynamic_cast<classad::ClassAd *>(jobAd->Lookup(ATTR_JOB_TOE));
	event.setToeTag( toeTag );
	if (!uLog.writeEvent (&event,jobAd)) {
		dprintf (D_ALWAYS,"Unable to log "
				 "ULOG_JOB_TERMINATED event\n");
		EXCEPT("UserLog Unable to log ULOG_JOB_TERMINATED event");
	}
}


void
BaseShadow::logEvictEvent( int exitReason )
{
	struct rusage run_remote_rusage;
	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	run_remote_rusage = getRUsage();

	switch( exitReason ) {
	case JOB_CKPTED:
	case JOB_SHOULD_REQUEUE:
	case JOB_KILLED:
		break;
	default:
		dprintf( D_ALWAYS, 
				 "logEvictEvent with unknown reason (%d), not logging.\n",
				 exitReason ); 
		return;
	}

	JobEvictedEvent event;
	event.checkpointed = (exitReason == JOB_CKPTED);
	
		// TODO: fill in local rusage
		// event.run_local_rusage = ???
			
		// remote rusage
	event.run_remote_rusage = run_remote_rusage;
	
	set_usageAd(jobAd, &event.pusageAd);

		/*
		  we want to log the events from the perspective of the user
		  job, so if the shadow *sent* the bytes, then that means the
		  user job *received* the bytes
		*/
	event.recvd_bytes = bytesSent();
	event.sent_bytes = bytesReceived();
	
	if (!uLog.writeEvent (&event,jobAd)) {
		dprintf (D_ALWAYS, "Unable to log ULOG_JOB_EVICTED event\n");
	}
}


void
BaseShadow::logRequeueEvent( const char* reason )
{
	struct rusage run_remote_rusage;
	memset( &run_remote_rusage, 0, sizeof(struct rusage) );

	run_remote_rusage = getRUsage();

	int exit_reason = getExitReason();

	JobEvictedEvent event;

	event.terminate_and_requeued = true;

	if( exitedBySignal() ) {
		event.normal = false;
		event.signal_number = exitSignal();
	} else {
		event.normal = true;
		event.return_value = exitCode();
	}
			
	if( exit_reason == JOB_COREDUMPED && core_file_name ) {
		event.core_file = core_file_name;
	}

	if( reason ) {
		event.reason = reason;
	}

		// TODO: fill in local rusage
		// event.run_local_rusage = r;
	event.run_remote_rusage = run_remote_rusage;

		/* we want to log the events from the perspective 
		   of the user job, so if the shadow *sent* the 
		   bytes, then that means the user job *received* 
		   the bytes */
	event.recvd_bytes = bytesSent();
	event.sent_bytes = bytesReceived();
	
	if (!uLog.writeEvent (&event,jobAd)) {
		dprintf( D_ALWAYS, "Unable to log ULOG_JOB_EVICTED "
				 "(and requeued) event\n" );
	}
}

void
BaseShadow::logDataflowJobSkippedEvent()
{
	DataflowJobSkippedEvent event;
	if (!uLog.writeEvent (&event, jobAd)) {
		dprintf( D_ALWAYS, "Unable to log ULOG_DATAFLOW_JOB_SKIPPED "
				 "(and requeued) event\n" );
	}
}


void
BaseShadow::checkSwap( void )
{
	int	reserved_swap, free_swap;
		// Reserved swap is specified in megabytes
	reserved_swap = param_integer( "RESERVED_SWAP", 0 );
	reserved_swap *= 1024;

	if( reserved_swap == 0 ) {
			// We're not supposed to care about swap space at all, so
			// none of the rest of the checks matter at all.
		return;
	}

	free_swap = sysapi_swap_space();

	dprintf( D_FULLDEBUG, "*** Reserved Swap = %d\n", reserved_swap );
	dprintf( D_FULLDEBUG, "*** Free Swap = %d\n", free_swap );

	if( free_swap < reserved_swap ) {
		dprintf( D_ALWAYS, "Not enough reserved swap space\n" );
		DC_Exit( JOB_NO_MEM );
	}
}	


// Note: log_except is static
void
BaseShadow::log_except(const char *msg)
{
	if(!msg) msg = "";

	if ( BaseShadow::myshadow_ptr == NULL ) {
		::dprintf (D_ALWAYS, "Unable to log ULOG_SHADOW_EXCEPTION event (no Shadow object): %s\n", msg);
		return;
	}

	// log shadow exception event
	ShadowExceptionEvent event;
	bool exception_already_logged = false;

	snprintf(event.message, sizeof(event.message), "%s", msg);
	event.message[sizeof(event.message)-1] = '\0';

	BaseShadow *shadow = BaseShadow::myshadow_ptr;

	// we want to log the events from the perspective of the
	// user job, so if the shadow *sent* the bytes, then that
	// means the user job *received* the bytes
	event.recvd_bytes = shadow->bytesSent();
	event.sent_bytes = shadow->bytesReceived();
	exception_already_logged = shadow->exception_already_logged;

	if (shadow->began_execution) {
		event.began_execution = TRUE;
	}

	if (!exception_already_logged && !shadow->uLog.writeEventNoFsync (&event,shadow->jobAd))
	{
		::dprintf (D_ALWAYS, "Failed to log ULOG_SHADOW_EXCEPTION event: %s\n", msg);
	}
}


bool
BaseShadow::updateJobAttr( const char *name, const char *expr, bool log )
{
	return job_updater->updateAttr( name, expr, false, log );
}


bool
BaseShadow::updateJobAttr( const char *name, int value, bool log )
{
	return job_updater->updateAttr( name, value, false, log );
}


void
BaseShadow::watchJobAttr( const std::string & name )
{
	job_updater->watchAttribute( name.c_str() );
}


bool
BaseShadow::updateJobInQueue( update_t type )
{
		// insert the bytes sent/recv'ed by this job into our job ad.
		// we want this from the perspective of the job, so it's
		// backwards from the perspective of the shadow.  if this
		// value hasn't changed, it won't show up as dirty and we
		// won't actually connect to the job queue for it.  we do this
		// here since we want it for all kinds of updates...
	ClassAd ftAd;
	ftAd.Assign(ATTR_BYTES_SENT, (prev_run_bytes_sent + bytesReceived()) );

	ftAd.Assign(ATTR_BYTES_RECVD, (prev_run_bytes_recvd + bytesSent()) );

	ClassAd upload_file_stats;
	ClassAd download_file_stats;
	getFileTransferStats(upload_file_stats, download_file_stats);
	ClassAd* updated_upload_stats = updateFileTransferStats(m_prev_run_upload_file_stats, upload_file_stats);
	ClassAd* updated_download_stats = updateFileTransferStats(m_prev_run_download_file_stats, download_file_stats);
	ftAd.Insert(ATTR_TRANSFER_INPUT_STATS, updated_upload_stats);
	ftAd.Insert(ATTR_TRANSFER_OUTPUT_STATS, updated_download_stats);

	FileTransferStatus upload_status = XFER_STATUS_UNKNOWN;
	FileTransferStatus download_status = XFER_STATUS_UNKNOWN;
	getFileTransferStatus(upload_status,download_status);
	switch(upload_status) {
	case XFER_STATUS_UNKNOWN:
		break;
	case XFER_STATUS_QUEUED:
		ftAd.Assign(ATTR_TRANSFER_QUEUED,true);
		ftAd.Assign(ATTR_TRANSFERRING_INPUT,true);
		break;
	case XFER_STATUS_ACTIVE:
		ftAd.Assign(ATTR_TRANSFER_QUEUED,false);
		ftAd.Assign(ATTR_TRANSFERRING_INPUT,true);
		break;
	case XFER_STATUS_DONE:
		ftAd.Assign(ATTR_TRANSFER_QUEUED,false);
		ftAd.Assign(ATTR_TRANSFERRING_INPUT,false);
		break;
	}
	switch(download_status) {
	case XFER_STATUS_UNKNOWN:
		break;
	case XFER_STATUS_QUEUED:
		ftAd.Assign(ATTR_TRANSFER_QUEUED,true);
		ftAd.Assign(ATTR_TRANSFERRING_OUTPUT,true);
		break;
	case XFER_STATUS_ACTIVE:
		ftAd.Assign(ATTR_TRANSFER_QUEUED,false);
		ftAd.Assign(ATTR_TRANSFERRING_OUTPUT,true);
		break;
	case XFER_STATUS_DONE:
		ftAd.Assign(ATTR_TRANSFER_QUEUED,false);
		ftAd.Assign(ATTR_TRANSFERRING_OUTPUT,false);
		break;
	}

	recordFileTransferStateChanges( jobAd, & ftAd );
	MergeClassAdsCleanly(jobAd,&ftAd);

	ASSERT( job_updater );

	if( type == U_PERIODIC ) {
			// Make an update happen soon.
		job_updater->resetUpdateTimer();
		return true;
	}

		// Now that the ad is current, just let our QmgrJobUpdater
		// object take care of the rest...
		//
		// Note that we force a non-durable update for X509 updates; if the
		// schedd crashes, we don't really care when the proxy was updated
		// on the worker node.
	return job_updater->updateJob( type, 0 );
}


void
BaseShadow::evalPeriodicUserPolicy( void )
{
	// We may be in the middle of an RPC from the starter.
	// If a policy expression fires, we may do some blocking network
	// operations and/or close the syscall socket.
	// So do the policy evaluation in a new DaemonCore callout.
	shadow_user_policy.checkPeriodicSoon();
}


/**
 * Shared code that should be run once the shadow is sure that
 * everything it's watching over has actually started running.  In the
 * uni-shadow case (vanilla, java, etc) this is called immediately
 * as soon as the starter sends the CONDOR_begin_execution RSC.  For
 * multi-shadow (parallel, MPI), this is invoked once all of the
 * starters have reported in.
 */
void
BaseShadow::resourceBeganExecution( RemoteResource* /* rr */ )
{
		// Set our flag to remember we've really started.
	began_execution = true;
		// Start the timer for the periodic user job policy evaluation.
	shadow_user_policy.startTimer();

	int allowed_job_duration;
	if( jobAd->LookupInteger( ATTR_JOB_ALLOWED_JOB_DURATION, allowed_job_duration ) ) {
		int tid = daemonCore->Register_Timer( allowed_job_duration + 1, 0,
			(TimerHandlercpp)&BaseUserPolicy::checkPeriodic,
			"check_for_allowed_job_duration",
			& shadow_user_policy );
		if( tid < 0 ) {
			dprintf( D_ALWAYS, "Failed to register timer to check for allowed job duration, jobs may run a little long.\n" );
		}
	}

		// Start the timer for updating the job queue for this job.
	startQueueUpdateTimer();

		// Update our copy of NumJobStarts, so that the periodic user
		// policy expressions, etc, all have the correct value.
	int job_start_cnt = 0;
	jobAd->LookupInteger(ATTR_NUM_JOB_STARTS, job_start_cnt);
	job_start_cnt++;
	jobAd->Assign(ATTR_NUM_JOB_STARTS, job_start_cnt);
	dprintf(D_FULLDEBUG, "Set %s to %d\n", ATTR_NUM_JOB_STARTS, job_start_cnt);

		// Update NumJobStarts in the schedd.  We honor a config knob
		// for this since there's a big trade-off: if we update
		// agressively and fsync() it to the job queue as soon as we
		// hear from the starter, the semantics are about as solid as
		// we can hope for, but it's a schedd scalability problem.  If
		// we do a lazy update, there's no additional cost to the
		// schedd, but it means that condor_q won't see the
		// change for N minutes, and if we happen to crash during that
		// time, the attribute is never incremented.  However, the
		// semantics aren't 100% solid, even if we don't update lazy,
		// and since the copy in RAM is already updated, all the
		// periodic user policy expressions will work right, so the
		// default is to do it lazy.
		// We want other attributes updated at the same time
		// (e.g. JobCurrentStartExecutingDate), so do a full update to
		// the schedd if we're not being lazy.
	if (!m_lazy_queue_update) {
			// They want it now, so do an update right here.
		updateJobInQueue(U_STATUS);
	}
}


const char*
BaseShadow::getCoreName( void )
{
	if( core_file_name ) {
		return core_file_name;
	} 
	return "unknown";
}


void
BaseShadow::startQueueUpdateTimer( void )
{
	job_updater->startUpdateTimer();
}


void
BaseShadow::publishShadowAttrs( ClassAd* ad )
{
	ad->Assign( ATTR_SHADOW_IP_ADDR, daemonCore->InfoCommandSinfulString() );

	ad->Assign( ATTR_SHADOW_VERSION, CondorVersion() );

	char* my_uid_domain = param( "UID_DOMAIN" );
	if( my_uid_domain ) {
		ad->Assign( ATTR_UID_DOMAIN, my_uid_domain );
		free( my_uid_domain );
	}
}


// This is declared in main.C, and is a pointer to one of the 
// various flavors of derived classes of BaseShadow.  
// It is only needed for this last function.
extern BaseShadow *Shadow;

// This function is called by dprintf - always display our job, proc,
// and pid in our log entries. 
int
display_dprintf_header(char **buf,int *bufpos,int *buflen)
{
	static pid_t mypid = 0;
	int mycluster = -1;
	int myproc = -1;

	if (!mypid) {
		mypid = daemonCore->getpid();
	}

	if (Shadow) {
		mycluster = Shadow->getCluster();
		myproc = Shadow->getProc();
	}

	if ( mycluster != -1 ) {
		return sprintf_realloc( buf, bufpos, buflen, "(%d.%d) (%ld): ", mycluster, myproc, (long)mypid );
	} else {
		return sprintf_realloc( buf, bufpos, buflen, "(?.?) (%ld): ", (long)mypid );
	}

	return 0;
}

bool
BaseShadow::getMachineName( std::string & /*machineName*/ )
{
	return false;
}

void
BaseShadow::CommitSuspensionTime(ClassAd *jobAd)
{
	int uncommitted_suspension_time = 0;
	jobAd->LookupInteger(ATTR_UNCOMMITTED_SUSPENSION_TIME,uncommitted_suspension_time);
	if( uncommitted_suspension_time > 0 ) {
		int committed_suspension_time = 0;
		jobAd->LookupInteger( ATTR_COMMITTED_SUSPENSION_TIME,
							  committed_suspension_time );
		committed_suspension_time += uncommitted_suspension_time;
		jobAd->Assign( ATTR_COMMITTED_SUSPENSION_TIME, committed_suspension_time );
		jobAd->Assign( ATTR_UNCOMMITTED_SUSPENSION_TIME, 0 );
	}
}

int
BaseShadow::handleUpdateJobAd( int sig )
{
	dprintf ( D_FULLDEBUG, "In handleUpdateJobAd, sig %d\n", sig );
	if (!job_updater->retrieveJobUpdates()) {
		dprintf(D_ALWAYS, "Error: Failed to update JobAd\n");
		return -1;
	}

	// Attributes might have changed that would cause the job policy
	// to evaluate differently, so evaluate now.
	shadow_user_policy.checkPeriodic();
	return 0;
}

bool
BaseShadow::jobWantsGracefulRemoval()
{
	if ( m_force_fast_starter_shutdown ) {
		return false;
	}
	bool job_wants_graceful_removal = param_boolean("GRACEFULLY_REMOVE_JOBS", true);
	bool job_request;
	ClassAd *thejobAd = getJobAd();
	if( thejobAd ) {
		if( thejobAd->LookupBool( ATTR_WANT_GRACEFUL_REMOVAL, job_request ) ) {
			job_wants_graceful_removal = job_request;
		}
	}
	return job_wants_graceful_removal;
}

/**
 * updateFileTransferStats takes two input parameters:
 * old_stats: A classad of old (existing) file transfer statistics
 * new_stats: A class of new incoming file transfer statistics
 * It then cumulates the attribute values of these two ads and returns a new
 * classad with the updated values. In the parallel universe (where a single
 * job contains many parralel instances) it adds the values from all the
 * different instances into the updated ad.
 **/
ClassAd*
BaseShadow::updateFileTransferStats(const ClassAd& old_stats, const ClassAd &new_stats)
{
	ClassAd* updated_stats = new ClassAd();

	// If new_stats is empty, just return a copy of the old stats
	if (new_stats.size() == 0) {
		updated_stats->CopyFrom(old_stats);
		return updated_stats;
	}

	// Iterate over the list of new stats
	for (auto it = new_stats.begin(); it != new_stats.end(); it++) {
		const std::string& attr = it->first;
		std::string attr_lastrun = attr + "LastRun";
		std::string attr_total = attr + "Total";

		// Lookup the value of this attribute. We only count integer values.
		classad::Value attr_val;
		long long value;
		it->second->Evaluate(attr_val);
		if (!attr_val.IsIntegerValue(value)) {
			continue;
		}
		updated_stats->InsertAttr(attr_lastrun, value);

		// If this attribute has a previous Total value, add that to the new total
		long long old_total = 0;
		if (old_stats.LookupInteger(attr_total, old_total)) {
			value += old_total;
		}
		updated_stats->InsertAttr(attr_total, value);
	}

	// Return the pointer to our newly-created classad
	// This will be memory-managed later by the ClassAd::Insert() function
	return updated_stats;
}
