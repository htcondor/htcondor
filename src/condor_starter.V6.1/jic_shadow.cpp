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
#include "condor_debug.h"
#include "condor_version.h"

#include "starter.h"
#include "jic_shadow.h"

#include "NTsenders.h"
#include "ipv6_hostname.h"
#include "internet.h"
#include "basename.h"
#include "condor_config.h"
#include "cred_dir.h"
#include "util_lib_proto.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "command_strings.h"
#include "classad_command_util.h"
#include "directory.h"
#include "nullfile.h"
#include "stream_handler.h"
#include "condor_vm_universe_types.h"
#include "authentication.h"
#include "condor_mkstemp.h"
#include "globus_utils.h"
#include "store_cred.h"
#include "secure_file.h"
#include "credmon_interface.h"
#include "condor_base64.h"
#include "zkm_base64.h"
#include <filesystem>
#include "manifest.h"
#include "checksum.h"
#include "tmp_dir.h"

#include <fstream>
#include <algorithm>

#include "filter.h"

extern class Starter *starter;
ReliSock *syscall_sock = NULL;
time_t syscall_last_rpc_time = 0;
JICShadow* syscall_jic_shadow = nullptr;
extern const char* JOB_AD_FILENAME;
extern const char* JOB_EXECUTION_OVERLAY_AD_FILENAME;
extern const char* MACHINE_AD_FILENAME;
const char* CHIRP_CONFIG_FILENAME = ".chirp.config";

// Filenames are case insensitive on Win32, but case sensitive on Unix
#ifdef WIN32
#	define file_contains contains_anycase
#	define file_remove remove_anycase
#else
#	define file_contains contains
#	define file_remove remove
#endif

// At some point, we'll install modern-enough compilers
// that we can leave the template specifier off and not
// have to count this list every time we modify it.
//
// Maybe if we're really clever we'll manage to make it constexpr, too.
std::array<std::string, 8> ALWAYS_EXCLUDED_FILES {{
    JOB_AD_FILENAME,
    JOB_EXECUTION_OVERLAY_AD_FILENAME,
    MACHINE_AD_FILENAME,
    ".docker_sock",
    ".docker_stdout",
    ".docker_stderr",
    ".update.ad",
    ".update.ad.tmp"
}};

namespace {

class ShadowCredDirCreator final : public htcondor::CredDirCreator {
public:
	ShadowCredDirCreator(const classad::ClassAd &ad, const std::string &cred_dir)
	: CredDirCreator(ad, cred_dir, "starter")
	{m_creddir_user_priv = true;}

	/**
	 * Fetch the credentials from the remote shadow
	 * - err: On error, this will be popuated with an error message
	 * - returns: True on success
	 */
	bool GetCreds(CondorError &err);

private:
	// Kerberos credential management logic is quite embedded in the JIC shadow; for now,
	// don't extract it out into the shared logic
	virtual bool GetKerberosCredential(const std::string & /*user*/, const std::string & /*domain*/,
		htcondor::CredData &/*cred*/, CondorError & /*err*/) override {return true;}

	virtual bool GetOAuth2Credential(const std::string &service_name, const std::string &user, htcondor::CredData &cred,
		CondorError &err) override;

	std::unordered_map<std::string, std::unique_ptr<htcondor::CredData>> m_creds;
};


bool
ShadowCredDirCreator::GetCreds(CondorError &err)
{
	dprintf(D_FULLDEBUG, "Starter is retrieving credentials from the shadow.\n");
	if (REMOTE_CONDOR_getcreds(CredDir().c_str(), m_creds) <= 0) {
		err.push("GetCreds", 1, "Failed to receive user credentials");
		dprintf(D_ERROR, "%s\n", err.message());
		return false;
	}
	return true;
}


bool
ShadowCredDirCreator::GetOAuth2Credential(const std::string &service_name, const std::string &/*user*/,
	htcondor::CredData &cred, CondorError &err)
{
	auto iter = m_creds.find(service_name);
	if (iter == m_creds.end()) {
		err.pushf("GetOAuth2Credential", 1, "Shadow failed to provide credential for service %s",
			service_name.c_str());
		dprintf(D_ERROR, "%s\n", err.message());
		return false;
	}
	cred = *iter->second;
	return true;
}

}

JICShadow::JICShadow( const char* shadow_name ) : JobInfoCommunicator(),
	m_wrote_chirp_config(false), m_job_update_attrs_set(false)
{
	if( ! shadow_name ) {
		EXCEPT( "Trying to instantiate JICShadow with no shadow name!" );
	}
	m_shadow_name = strdup( shadow_name );

	shadow = NULL;
	shadow_version = NULL;
	filetrans = NULL;
	m_did_transfer = false;
	m_filetrans_sec_session = NULL;
	m_reconnect_sec_session = NULL;
	
		// just in case transferOutputMopUp gets called before transferOutput
	m_ft_rval = true;
	trust_uid_domain = false;
	trust_local_uid_domain = true;
	uid_domain = NULL;
	fs_domain = NULL;

	transfer_at_vacate = false;
	m_job_setup_done = false;
	wants_file_transfer = false;
	wants_x509_proxy = false;
	job_cleanup_disconnected = false;
	syscall_sock_registered  = false;
	syscall_sock_lost_tid = -1;
	syscall_sock_lost_time = 0;

		// now we need to try to inherit the syscall sock from the startd
	Stream **socks = daemonCore->GetInheritedSocks();
	if (socks[0] == NULL ||
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit job ClassAd startd update socket.\n");
		starter->StarterExit( STARTER_EXIT_GENERAL_FAILURE );
	}
	m_job_startd_update_sock = socks[0];
	socks++;

	if (socks[0] == NULL || 
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit remote system call socket.\n");
		starter->StarterExit( STARTER_EXIT_GENERAL_FAILURE );
	}
	syscall_sock = (ReliSock *)socks[0];
	syscall_last_rpc_time = time(nullptr);
	syscall_jic_shadow = this;
	socks++;

	m_proxy_expiration_tid = -1;
	m_refresh_sandbox_creds_tid = -1;
	memset(&m_sandbox_creds_last_update, 0, sizeof(m_sandbox_creds_last_update));

		/* Set a timeout on remote system calls.  This is needed in
		   case the user job exits in the middle of a remote system
		   call, leaving the shadow blocked.  -Jim B. */
	syscall_sock->timeout(param_integer( "STARTER_UPLOAD_TIMEOUT", 300));

	ASSERT( socks[0] == NULL );
}


JICShadow::~JICShadow()
{
	// On exit, the global daemonCore object may have been
	// destructed before us
	if (daemonCore) {
		if( m_refresh_sandbox_creds_tid != -1 ){
			daemonCore->Cancel_Timer(m_refresh_sandbox_creds_tid);
		}
		if( m_proxy_expiration_tid != -1 ){
			daemonCore->Cancel_Timer(m_proxy_expiration_tid);
		}
	}
	if( shadow ) {
		delete shadow;
	}
	if( shadow_version ) {
		delete shadow_version;
	}
	if( filetrans ) {
		delete filetrans;
	}
	if( m_shadow_name ) {
		free( m_shadow_name );
	}
	if( uid_domain ) {
		free( uid_domain );
	}
	if( fs_domain ) {
		free( fs_domain );
	}
	free(m_reconnect_sec_session);
	free(m_filetrans_sec_session);

	delete m_job_startd_update_sock;
	m_job_startd_update_sock = nullptr;

}


bool
JICShadow::init( void ) 
{ 
		// First, get a copy of the job classad by doing an RSC to the
		// shadow.  This is totally independent of the shadow version,
		// etc, and is the first step to everything else. 
	if( ! getJobAdFromShadow() ) {
		dprintf( D_ERROR,
				 "Failed to get job ad from shadow!\n" );
		return false;
	}

	if ( m_job_startd_update_sock )
	{
		receiveMachineAd(m_job_startd_update_sock);
		receiveExecutionOverlayAd(m_job_startd_update_sock);
	}

		// stash a copy of the unmodified job ad in case we decide
		// below that we want to write out an execution visa
	ClassAd orig_ad = *job_ad;	

	if (job_execution_overlay_ad) {
		// For now, we apply the execution overlay as soon as we have copied the original job classad
		// TODO: do this update later (or never)? but we would have to fix a bunch of call sites first.
		job_ad->Update(*job_execution_overlay_ad);
	}

		// now that we have the job ad, see if we should go into an
		// infinite loop, waiting for someone to attach w/ the
		// debugger.
	checkForStarterDebugging();

		// Next, instantiate our DCShadow object.  We want to do this
		// right away, since the rest of this stuff might depend on
		// what shadow version we're talking to...
	if( shadow ) {
		delete shadow;
	}
	shadow = new DCShadow( m_shadow_name );
	ASSERT( shadow );

		// Now, initalize our version information about the shadow
		// with whatever attributes we can find in the job ad.
	initShadowInfo( job_ad );

	dprintf( D_ALWAYS, "Submitting machine is \"%s\"\n", shadow->name());

		// Now that we know what version of the shadow we're talking
		// to, we can register information about ourselves with the
		// shadow in a method that it understands.
	registerStarterInfo();

		// If we are supposed to specially create a security session
		// for file transfer and reconnect, do that now.
	initMatchSecuritySession();

		// Grab all the interesting stuff out of the ClassAd we need
		// to know about the job itself, like are we doing file
		// transfer, what should the std files be called, etc.
	if( ! initJobInfo() ) { 
		dprintf( D_ERROR,
				 "Failed to initialize job info from ClassAd!\n" );
		return false;
	}
	
		// Now that we have the job ad, figure out what the owner
		// should be and initialize our priv_state code:
	if( ! initUserPriv() ) {
		dprintf( D_ALWAYS, "ERROR: Failed to determine what user "
				 "to run this job as, aborting\n" );
		return false;
	}

		// Now that we have the user_priv, we can make the temp
		// execute dir
	if( ! starter->createTempExecuteDir() ) { 
		return false;
	}

		// If the user wants it, initialize our io proxy
		// Must have user priv to drop the config info	
		// into the execute dir.
	priv_state priv = set_user_priv();
	initIOProxy();
	priv = set_priv(priv);

		// Now that the user priv is setup and the temp execute dir
		// exists, we can initialize the LocalUserLog.  if the job
		// defines StarterUserLog, we'll write the events.  if not,
		// all attemps to log events will just be no-ops.
	if( ! u_log->initFromJobAd(job_ad) ) {
		return false;
	}

		// Drop a job ad "visa" into the sandbox now if the job
		// requested it
	writeExecutionVisa(orig_ad);

		// If the job requests a user credential (SendCredential is
		// true) then grab it from the shadow and get everything
		// prepared.
	if (! initUserCredentials() ) {
		return false;
	}

	return true;
}


void
JICShadow::config( void ) 
{ 
	if( uid_domain ) {
		free( uid_domain );
	}
	uid_domain = param( "UID_DOMAIN" );  

	if( fs_domain ) {
		free( fs_domain );
	}
	fs_domain = param( "FILESYSTEM_DOMAIN" );  

	trust_uid_domain = param_boolean_crufty("TRUST_UID_DOMAIN", false);
	trust_local_uid_domain = param_boolean("TRUST_LOCAL_UID_DOMAIN", true);
}


void
JICShadow::setupJobEnvironment(void)
{
#if HAVE_JOB_HOOKS
	if (m_hook_mgr) {
		int rval = m_hook_mgr->tryHookPrepareJobPreTransfer();
		switch (rval) {
		case -1:   // Error
			starter->RemoteShutdownFast(0);
			return;
			break;

		case 0:    // Hook not configured
				// Nothing to do, break out and finish.
			break;

		case 1:    // Spawned the hook.
				// We need to bail now, and let the handler call
				// setupJobEnvironment_part2() when the hook returns.
			return;
			break;
		}
	}
#endif /* HAVE_JOB_HOOKS */

	setupJobEnvironment_part2();
}

void
JICShadow::setupJobEnvironment_part2(void)
{
		// call our helper method to see if we want to do a file
		// transfer at all, and if so, initiate it.
	if( beginFileTransfer() ) {
			// We started a transfer, so we just want to return to
			// DaemonCore asap and wait for the file transfer callback
			// that was registered
		return;
	} 

		// Otherwise, there were no files to transfer, so we can
		// pretend the transfer just finished and try to spawn the job
		// now.
	transferInputStatus(nullptr);
}

bool
JICShadow::streamInput()
{
	bool result=false;
	job_ad->LookupBool(ATTR_STREAM_INPUT,result);
	return result;
}

bool
JICShadow::streamOutput()
{
	bool result=false;
	job_ad->LookupBool(ATTR_STREAM_OUTPUT,result);
	return result;
}

bool
JICShadow::streamError()
{
	bool result=false;
	job_ad->LookupBool(ATTR_STREAM_ERROR,result);
	return result;
}

float
JICShadow::bytesSent( void )
{
	if( filetrans ) {
		return filetrans->TotalBytesSent();
	} 
	return 0.0;
}


float
JICShadow::bytesReceived( void )
{
	if( filetrans ) {
		return filetrans->TotalBytesReceived();
	}
	return 0.0;
}


void
JICShadow::Suspend( void )
{

		// If we have a file transfer object, we want to tell it to
		// suspend any work it might be doing...
	if( filetrans ) {
		filetrans->Suspend();
	}
	
		// Next, we need the update ad for our job.  We'll use this
		// for the LocalUserLog (if it's doing anything), updating the
		// shadow, etc, etc.
	ClassAd update_ad;
	publishUpdateAd( &update_ad );
	
		// See if the LocalUserLog wants it
	u_log->logSuspend( &update_ad );

		// Now that everything is suspended, we want to send another
		// update to the shadow to let it know the job state.  We want
		// to confirm the update gets there on this important state
		// change, to pass in "true" to updateShadow() for that.
	updateShadow( &update_ad );

}


void
JICShadow::Continue( void )
{
		// If we have a file transfer object, we want to tell it to
		// resume any work it might want to be doing...
	if( filetrans ) {
		filetrans->Continue();
	}

		// Next, we need the update ad for our job.  We'll use this
		// for the LocalUserLog (if it's doing anything), updating the
		// shadow, etc, etc.
	ClassAd update_ad;
	publishUpdateAd( &update_ad );

		// See if the LocalUserLog wants it
	u_log->logContinue( &update_ad );

		// Now that everything is running again, we want to send
		// another update to the shadow to let it know the job state.
		// We want to confirm the update gets there on this important
		// state change, to pass in "true" to updateShadow() for that.
	updateShadow( &update_ad );
}


bool JICShadow::allJobsDone( void )
{
	ClassAd update_ad;

	bool r1 = JobInfoCommunicator::allJobsDone();

	// Tell shadow job is done, and moving to job state transfer output
	if (!m_did_transfer) {
		publishJobExitAd( &update_ad );
		// Note if updateShadow() fails, it will dprintf into the log.
		updateShadow( &update_ad );
	}

	// only report success if our parent class is also done
	return r1;
}


bool
JICShadow::transferOutput( bool &transient_failure )
{
	dprintf(D_FULLDEBUG, "Inside JICShadow::transferOutput(void)\n");

	transient_failure = false;

	if (m_did_transfer) {
		return true;
	}

	// if we are writing a sandbox starter log, flush and temporarily close it before we make the manifest
	if (job_ad->Lookup(ATTR_JOB_STARTER_DEBUG)) {
		dprintf_close_logs_in_directory(starter->GetWorkingDir(false), false);
	}

	std::string dummy;
	bool want_manifest = false;
	if( job_ad->LookupString( ATTR_JOB_MANIFEST_DIR, dummy ) ||
		(job_ad->LookupBool( ATTR_JOB_MANIFEST_DESIRED, want_manifest ) && want_manifest) ) {
		recordSandboxContents( "out" );
	}

	bool spool_on_evict = true, tmp_value;
	if (job_ad->EvaluateAttrBool("SpoolOnEvict", tmp_value))
	{
		spool_on_evict = tmp_value;
	}

	dprintf(D_FULLDEBUG, "JICShadow::transferOutput(void): Transferring...\n");

		// transfer output files back if requested job really
		// finished.  may as well do this in the foreground,
		// since we do not want to be interrupted by anything
		// short of a hardkill. 
		// Don't transfer if we haven't started the job.
	if( filetrans && m_job_setup_done && ((requested_exit == false) || transfer_at_vacate) ) {

		if ( shadowDisconnected() ) {
				// trigger retransfer on reconnect
			job_cleanup_disconnected = true;

				// inform our caller that transfer will be retried
				// when the shadow reconnects
			transient_failure = true;

			return false;
		}

			// add any dynamically-added output files to the FT
			// object's list
		for (const auto& filename: m_added_output_files) {
			// Removed wins over added
			if (!file_contains(m_removed_output_files, filename)) {
				filetrans->addOutputFile(filename.c_str());
			}
		}

		_remove_files_from_output();

		// If the has asked for it, include the starter log.  It is
		// otherwise already excluded by _remove_files_from_output().
		if (job_ad->Lookup(ATTR_JOB_STARTER_DEBUG)) {
			if ( job_ad->Lookup(ATTR_JOB_STARTER_LOG)) {
				filetrans->addOutputFile(SANDBOX_STARTER_LOG_FILENAME);
				filetrans->addFailureFile(SANDBOX_STARTER_LOG_FILENAME);
			}
		}

			// true if job exited on its own or if we are set to not spool
			// on eviction.
		bool final_transfer = !spool_on_evict || (requested_exit == false);

			// For the final transfer, we obey the output file remaps.
		if (final_transfer) {
			if ( !filetrans->InitDownloadFilenameRemaps(job_ad) ) {
				dprintf( D_ALWAYS, "Failed to setup output file remaps.\n" );
				m_did_transfer = false;
				return false;
			}
		}


			// The shadow may block on disk I/O for long periods of
			// time, so set a big timeout on the starter's side of the
			// file transfer socket.

		int timeout = param_integer( "STARTER_UPLOAD_TIMEOUT", 200 );
		filetrans->setClientSocketTimeout(timeout);

			// The user job may have created files only readable
			// by the user, so set_user_priv here.
		priv_state saved_priv = set_user_priv();

		dprintf( D_FULLDEBUG, "Begin transfer of sandbox to shadow.\n");
			// this will block
		if( job_failed && final_transfer ) {
			// Only attempt failure file upload if we have failure files
			if (filetrans->hasFailureFiles()) {
				sleep(1); // Delay to give time for shadow side to reap previous upload
				m_ft_rval = filetrans->UploadFailureFiles( true );
				// We would otherwise not send any UnreadyReasons we
				// may have queued, as they could be skipped in favor of
				// putting the job on hold for failing to transfer ouput.
				transferredFailureFiles = true;
			}
		} else {
			m_ft_rval = filetrans->UploadFiles( true, final_transfer );
		}
		m_ft_info = filetrans->GetInfo();
		dprintf( D_FULLDEBUG, "End transfer of sandbox to shadow.\n");


		updateShadowWithPluginResults("Output");


		const char *stats = m_ft_info.tcp_stats.c_str();
		if (strlen(stats) != 0) {
			std::string full_stats = "(peer stats from starter): ";
			full_stats += stats;

			if (shadow_version && shadow_version->built_since_version(8, 5, 8))
			{
				REMOTE_CONDOR_dprintf_stats(full_stats.c_str());
			}
		}
		set_priv(saved_priv);

		if( m_ft_rval ) {
			job_ad->Assign(ATTR_SPOOLED_OUTPUT_FILES,
							m_ft_info.spooled_files.c_str());
		} else {
			dprintf( D_FULLDEBUG, "Sandbox transfer failed.\n");
			// Failed to transfer.
			// JICShadow::transferOutputMopUp() will figure out what to do
			// when you call it after JICShadow::transferOutput() returns.

			if( !m_ft_info.success && m_ft_info.try_again ) {
				// Some kind of transient error, such as a timeout or
				// disconnect.  Would like to know for sure whether
				// this really means we are disconnected from the
				// shadow, but for now, force the socket to disconnect
				// by closing it.

				// Please forgive us these hacks
				// as we forgive those who hack against us

				static int timesCalled = 0;
				timesCalled++;
				if (timesCalled < 5) {
					dprintf(D_ALWAYS,"File transfer failed, forcing disconnect.\n");

						// force disconnect, start a timer to exit after lease gone
					syscall_sock_disconnect();

						// trigger retransfer on reconnect
					job_cleanup_disconnected = true;

						// inform our caller that transfer will be retried
						// when the shadow reconnects
					transient_failure = true;
				}
			}

			m_did_transfer = false;
			return false;
		}
	}
		// Either the job doesn't need transfer, or we just succeeded.
		// In both cases, we should record that we were successful so
		// that if we ever come through here again to retry the whole
		// job cleanup process we don't attempt to transfer again.
	m_did_transfer = true;
	return true;
}

bool
JICShadow::transferOutputMopUp(void)
{

	dprintf(D_FULLDEBUG, "Inside JICShadow::transferOutputMopUp(void)\n");

	if (m_did_transfer) {
		/* nothing was marked wrong if we were able to do the transfer */
		return true;
	}

	// We saved the return value of the last filetransfer attempt...
	// We also saved the ft_info structure when we did the file transfer.
	if( (! m_ft_rval) && (! transferredFailureFiles) ) {
		dprintf(D_FULLDEBUG, "JICShadow::transferOutputMopUp(void): "
			"Mopping up failed transfer...\n");

		// Failed to transfer.  See if there is a reason to put
		// the job on hold.
		if(!m_ft_info.success && !m_ft_info.try_again) {
			ASSERT(m_ft_info.hold_code != 0);
			// The shadow will immediately cut the connection to the
			// starter when this is called. 
			notifyStarterError(m_ft_info.error_desc.c_str(), true,
			                   m_ft_info.hold_code,m_ft_info.hold_subcode);
			return false;
		}

		// We hit some "transient" error, but we've retried too many times,
		// so tell the shadow we are giving up.
		notifyStarterError("Repeated attempts to transfer output failed for unknown reasons", true,0,0);
		return false;
	} else if( ! m_ft_rval ) {
	    //
	    // We failed to transfer failure files; ignore this error in
	    // favor of reporting whatever caused the failure.
	    //
	    // If the failure was caused by the job terminating in the wrong
	    // way, this will be very confusing.  [FIXME]
	    //
	}

	return true;
}


void
JICShadow::allJobsGone( void )
{
	if ( shadow_version && shadow_version->built_since_version(8,7,8) ) {
		dprintf( D_ALWAYS, "All jobs have exited... starter exiting\n" );
		starter->StarterExit( starter->GetShutdownExitCode() );
	}
}

void
JICShadow::gotShutdownFast( void )
{
		// Despite what the user wants, do not transfer back any files 
		// on a ShutdownFast.
	transfer_at_vacate = false;   

		// let our parent class do the right thing, too.
	JobInfoCommunicator::gotShutdownFast();
}

void
JICShadow::disconnect()
{
	syscall_sock_disconnect();
}

int
JICShadow::reconnect( ReliSock* s, ClassAd* ad )
{
		// first, make sure the entity requesting this is authorized.
		/*
		  TODO!!  UGH, since the results of getFullyQualifiedUser()
		  are not serialized properly when we inherit the relisock, we
		  have no way to figure out the original entity that
		  authenticated to the startd to spawn this job.  :( We really
		  should call getFullyQualifiedUser() on the socket we
		  inherit, stash that in a variable, and compare that against
		  the result on this socket right here.

		  But, we can't do that. :( So, instead, we call getOwner()
		  and compare that to whatever ATTR_OWNER is in our job ad
		  (the original ad, not whatever they're requesting now).

		  WORSE YET, even if it did work, there's no user credential
		  management when you use strong authentication, so this
		  socket would be authenticated with the shadow's system
		  credentials, not the user.  So, *NONE* of this will work,
		  anyway.  

		  We could try to introduce some kind of real shared
		  secret/capability to make this more secure, but that's going
		  to have to wait for time to make some bigger changes.
		*/
#if 0
	const char* new_owner = s->getOwner();
	char* my_owner = NULL; 
	if( ! job_ad->LookupString(ATTR_OWNER, &my_owner) ) {
		EXCEPT( "impossible: ATTR_OWNER must be in job ad by now" );
	}

	if( strcmp(new_owner, my_owner) ) {
		std::string err_msg = "User '";
		err_msg += new_owner;
		err_msg += "' does not match the owner of this job";
		sendErrorReply( s, getCommandString(CA_RECONNECT_JOB), 
						CA_NOT_AUTHORIZED, err_msg.c_str() ); 
		dprintf( D_COMMAND, "Denied request for %s by invalid user '%s'\n", 
				 getCommandString(CA_RECONNECT_JOB), new_owner );
		return FALSE;
	}
#endif

	ClassAd reply;
	publishStarterInfo( &reply );

	reply.Assign( ATTR_RESULT, getCAResultString( CA_SUCCESS ) );

	if( ! sendCAReply(s, getCommandString(CA_RECONNECT_JOB), &reply) ) {
		dprintf( D_ALWAYS, "Failed to reply to request\n" );
		return FALSE;
	}

		// If we managed to send the reply, finally commit to the
		// switch.  Destroy all the info we're storing about the
		// previous shadow and switch over to the new info.

		// Destroy our old DCShadow object and make a new one with the
		// current info.

	dprintf( D_ALWAYS, "Accepted request to reconnect from %s\n",
			generate_sinful(syscall_sock->peer_ip_str(),
						syscall_sock->peer_port()).c_str());
	dprintf( D_ALWAYS, "Ignoring old shadow %s\n", shadow->addr() );
	delete shadow;
	shadow = new DCShadow;
	initShadowInfo( ad );	// this dprintf's D_ALWAYS for us
	free( m_shadow_name );
		/*
		  normally, it'd be nice to have a hostname here for
		  dprintf(), etc.  unfortunately, because of how the
		  information flows, we don't know the real hostname at this
		  point.  the reconnect command classad only contains the
		  ip/port (sinful string), not the shadow's hostname.  so, if
		  we want a hostname here, we'd have to do a reverse DNS
		  lookup.  someday, we might do that, but admins have
		  (rightfully) complained about the load Condor puts on DNS
		  servers, and since we don't *really* need this to be a
		  hostname anymore (we've already decided what UID/FS domain
		  to spawn the starter as, so we don't need it for that),
		  we'll just use the sinful string as the shadow's name...
		*/
	m_shadow_name = strdup( shadow->addr() );

		// switch over to the new syscall_sock
	dprintf( D_FULLDEBUG, "Closing old syscall sock %s\n",
			generate_sinful(syscall_sock->peer_ip_str(),
					syscall_sock->peer_port()).c_str());
		// make sure old syscall_sock is no longer registered before we blow it away
	if (syscall_sock_registered) {
		daemonCore->Cancel_Socket(syscall_sock);
		syscall_sock_registered = false;
	}
	delete syscall_sock;
	syscall_sock = s;
	syscall_sock->timeout(param_integer( "STARTER_UPLOAD_TIMEOUT", 300));
	syscall_last_rpc_time = time(nullptr);
	dprintf( D_FULLDEBUG, "Using new syscall sock %s\n",
			generate_sinful(syscall_sock->peer_ip_str(),
					syscall_sock->peer_port()).c_str());

	syscall_sock_reconnect();  // cancels any disconnected timers etc

	initMatchSecuritySession();

		// tell our FileTransfer object to point to the new 
		// shadow using the transsock and key in the ad from the
		// reconnect request
	if ( filetrans ) {
		char *value1 = NULL;
		char *value2 = NULL;
		ad->LookupString(ATTR_TRANSFER_KEY,&value1);
		ad->LookupString(ATTR_TRANSFER_SOCKET,&value2);
		bool result = filetrans->changeServer(value1,value2);
		if ( value1 && value2 && result ) {
			dprintf(D_FULLDEBUG,
				"Switching to new filetrans server, "
				"sock=%s key=%s\n",value2,value1);
		} else {
			dprintf(D_ALWAYS,
				"ERROR Failed to switch to new filetrans server, "
				"sock=%s key=%s\n", value2 ? value2 : "(null)",
				value1 ? value1 : "(null)");
		}
		if ( value1 ) free(value1);
		if ( value2 ) free(value2);

		if ( shadow_version == NULL ) {
			dprintf( D_ALWAYS, "Can't determine shadow version for FileTransfer!\n" );
		} else {
			filetrans->setPeerVersion( *shadow_version );
		}
		filetrans->setSecuritySession(m_filetrans_sec_session);
	}

	StreamHandler::ReconnectAll();

	if( job_cleanup_disconnected ) {
			/*
			  if we were trying to cleanup our job and we noticed we
			  were disconnected from the shadow, this flag will be
			  set.  in this case, want to call out to the Starter
			  object to tell it to try to clean up the job again.
			*/
		if( starter->allJobsDone() ) {
			dprintf(D_ALWAYS,"Job cleanup finished, now Starter is exiting\n");
			starter->StarterExit(STARTER_EXIT_NORMAL);
		}
	}

		// Now that we're holding onto the ReliSock, we can't let
		// DaemonCore close it on us!
	return KEEP_STREAM;
}


void
JICShadow::notifyJobPreSpawn( void )
{
	m_job_setup_done = true;

			// Notify the shadow we're about to exec.
	REMOTE_CONDOR_begin_execution();

		// let the LocalUserLog know so it can log if necessary.  it
		// doesn't use the ClassAd for this event at all, so it's not
		// worth the trouble of creating one and publishing anything
		// into it.
	u_log->logExecute( NULL );
}

void
JICShadow::notifyExecutionExit( void ) {
	// We don't send the time because all of the other Activation* times
	// are shadow-side times.  It's not hard to get UTC, but it's easier
	// to ignore clock synchronization entirely.
	if( shadow_version && shadow_version->built_since_version(9, 4, 1) ) {
		ClassAd ad;
		ad.InsertAttr( "EventType", "ActivationExecutionExit" );
		REMOTE_CONDOR_event_notification(ad);
	}
}

void
JICShadow::notifyGenericEvent( const ClassAd & event ) {
	if( shadow_version && shadow_version->built_since_version(9, 4, 1) ) {
		REMOTE_CONDOR_event_notification(event);
	}
}

bool
JICShadow::notifyJobExit( int exit_status, int reason, UserProc*
						  /* user_proc */ )
{
	static bool wrote_local_log_event = false;

	ClassAd ad;

		// We want the update ad anyway, in case we want it for the
		// LocalUserLog
	publishUpdateAd( &ad );

		// depending on the exit reason, we want a different event. 
		// however, don't write multiple events if we've already been
		// here, which might happen if we were disconnected when we
		// first tried and we're trying again...
	if( ! wrote_local_log_event ) {
		if( u_log->logJobExit(&ad, reason) ) {
			wrote_local_log_event = true;
		}
	}

	// If shadow exits before the startd kills us, don't worry about
	// getting notified that the syscall socket has closed.

	if (syscall_sock) {
		if (syscall_sock_registered) {
			daemonCore->Cancel_Socket(syscall_sock);
			syscall_sock_registered = false;
		}
	}

	dprintf( D_FULLDEBUG, "Notifying exit status=%d reason=%d\n",exit_status,reason );
	updateStartd(&ad, true);

	if( !had_hold ) {
		if( REMOTE_CONDOR_job_exit(exit_status, reason, &ad) < 0) {
			dprintf( D_ALWAYS, "Failed to send job exit status to shadow\n" );
			if (job_universe != CONDOR_UNIVERSE_PARALLEL)
			{
				job_cleanup_disconnected = true;
					// If we're doing a fast shutdown, ignore any failures
					// in talking to the shadow.
				if ( !fast_exit ) {
					// force disconnect, start a timer to exit after lease gone
					// We just unregistered the syscall socket from DaemonCore,
					// so it won't call us back later about the bad socket
					syscall_sock_disconnect();
					return false;
				}
			}
		}
	}

	return true;
}

int
JICShadow::notifyJobTermination( UserProc *user_proc )
{
	int rval = 0;
	ClassAd ad;

	dprintf(D_FULLDEBUG, "Inside JICShadow::notifyJobTermination()\n");

	if (shadow_version && shadow_version->built_since_version(7,4,4)) {
		dprintf(D_ALWAYS, "JICShadow::notifyJobTermination(): "
			"Sending mock terminate event.\n");

		user_proc->PublishUpdateAd( &ad );

		rval = REMOTE_CONDOR_job_termination(ad);
	}

	return rval;
}

void
JICShadow::updateStartd( ClassAd *ad, bool final_update )
{
	ASSERT( ad );

	// update the startd's copy of the job ClassAd
	if( !m_job_startd_update_sock ) {
		return;
	}

	m_job_startd_update_sock->encode();
	if( !m_job_startd_update_sock->put((int)final_update) ||
		!putClassAd(m_job_startd_update_sock, *ad) ||
		!m_job_startd_update_sock->end_of_message() )
	{
		dprintf(D_FULLDEBUG,"Failed to send job ClassAd update to startd.\n");
	}
	else {
		dprintf(D_FULLDEBUG,"Sent job ClassAd update to startd.\n");
	}
	if( IsDebugVerbose(D_JOB) ) {
		dPrintAd(D_JOB, *ad);
	}

	if( final_update ) {
		delete m_job_startd_update_sock;
		m_job_startd_update_sock = NULL;
	}
}

bool
JICShadow::notifyStarterError( const char* err_msg, bool critical, int hold_reason_code, int hold_reason_subcode )
{
	u_log->logStarterError( err_msg, critical );

	// If the scratch working directory has disappeared when it should exist,
	// that is likely connected to the error. Don't tell the shadow to put
	// the job on hold, as it's likely not the fault of the job.
	// Make an exception for local universe jobs
	// (SCHEDD_USES_STARTD_FOR_LOCAL_UNIVERSE=True), as they have nowhere
	// else to go if this is a recurring problem.
	if( starter->WorkingDirExists() && job_universe != CONDOR_UNIVERSE_LOCAL ) {
		StatInfo si(starter->GetWorkingDir(false));
		if( si.Error() == SINoFile ) {
			dprintf(D_ALWAYS, "Scratch execute directory disappeared unexpectedly, declining to put job on hold.\n");
			hold_reason_code = 0;
			hold_reason_subcode = 0;
		}
	}

	if( critical ) {
		if( REMOTE_CONDOR_ulog_error(hold_reason_code, hold_reason_subcode, err_msg) < 0 ) {
			dprintf( D_ALWAYS, 
					 "Failed to send starter error string to Shadow.\n" );
			return false;
		}
	} else {
		ClassAd * ad;
		RemoteErrorEvent event;
		event.error_str = err_msg;
		event.daemon_name = "starter";
		event.setCriticalError( false );
		event.setHoldReasonCode( hold_reason_code );
		event.setHoldReasonSubCode( hold_reason_subcode );
		ad = event.toClassAd(true);
		ASSERT( ad );
		int rv = REMOTE_CONDOR_ulog(*ad);
		delete ad;
		if( rv ) {
			dprintf( D_ALWAYS,
					 "Failed to send starter error string to Shadow.\n" );
			return false;
		}
	}

	return true;
}

bool
JICShadow::holdJob( const char* hold_reason, int hold_reason_code, int hold_reason_subcode )
{
	gotHold();
	return notifyStarterError( hold_reason, true, hold_reason_code, hold_reason_subcode );
}

bool
JICShadow::registerStarterInfo( void )
{
	int rval;

	if( ! shadow ) {
		EXCEPT( "registerStarterInfo called with NULL DCShadow object" );
	}

	ClassAd starter_info;
	publishStarterInfo( &starter_info );
	rval = REMOTE_CONDOR_register_starter_info(starter_info);

	if( rval < 0 ) {
		return false;
	}
	return true;
}


void
JICShadow::publishStarterInfo( ClassAd* ad )
{
	ad->Assign( ATTR_UID_DOMAIN, uid_domain );

	ad->Assign( ATTR_FILE_SYSTEM_DOMAIN, fs_domain );

	std::string slotName = starter->getMySlotName();
	slotName += '@';
	slotName += get_local_fqdn();
	ad->Assign( ATTR_NAME, slotName );

	ad->Assign(ATTR_STARTER_IP_ADDR, daemonCore->InfoCommandSinfulString() );

	const char * sandbox_dir = starter->GetWorkingDir(false);
	if (sandbox_dir && sandbox_dir[0]) {
		ad->Assign(ATTR_CONDOR_SCRATCH_DIR, sandbox_dir);
	}
	if (starter->jic) {
		ClassAd * machineAd = starter->jic->machClassAd();
		if( machineAd ) {
			CopyMachineResources(*ad, *machineAd, true);
			//Check for requested machine attrs to return for execution event
			if (job_ad) {
				std::string requestAttrs;
				if (job_ad->LookupString(ATTR_ULOG_EXECUTE_EVENT_ATTRS,requestAttrs)) {
					CopySelectAttrs(*ad, *machineAd, requestAttrs, false);
				}
			}
		}
	}

	ad->Assign( ATTR_HAS_RECONNECT, true );

		// Finally, publish all the DC-managed attributes.
		// this sets CondorVersion, Address and MyAddress
	daemonCore->publish(ad);
}

void
JICShadow::addToOutputFiles( const char* filename )
{
	if (!file_contains(m_removed_output_files, filename)) {
		m_added_output_files.emplace_back(filename);
	}
}

void
JICShadow::removeFromOutputFiles( const char* filename )
{
	m_removed_output_files.emplace_back(filename);
}

bool
JICShadow::uploadCheckpointFiles(int checkpointNumber)
{
	if(! filetrans) {
		return false;
	}

	_remove_files_from_output();

	// The shadow may block on disk I/O for long periods of
	// time, so set a big timeout on the starter's side of the
	// file transfer socket.

	int timeout = param_integer( "STARTER_UPLOAD_TIMEOUT", 200 );
	filetrans->setClientSocketTimeout( timeout );

	// The user job may have created files only readable
	// by the user, so set_user_priv here.
	priv_state saved_priv = set_user_priv();

	// this will block
	bool rval = filetrans->UploadCheckpointFiles( checkpointNumber, true );
	set_priv( saved_priv );

	updateShadowWithPluginResults("Checkpoint");

	if( !rval ) {
		// Failed to transfer.
		m_ft_info = filetrans->GetInfo();
		if (m_ft_info.try_again) {
			dprintf(D_ALWAYS, "JICShadow::uploadCheckpointFiles() failed: %s\n", m_ft_info.error_desc.c_str());
			dprintf(D_ALWAYS, "JICShadow::uploadCheckpointFiles() will retry checkpoint upload later.\n");
			return false;
		} else {
			dprintf(D_ALWAYS, "JICShadow::uploadCheckpointFiles() putting job on hold, checkpoint failure was: %s\n", m_ft_info.error_desc.c_str());
			holdJob("Starter failed to upload checkpoint", CONDOR_HOLD_CODE::FailedToCheckpoint, -1);
			return false;
		}

		return false;
	}
	dprintf( D_FULLDEBUG, "JICShadow::uploadCheckpointFiles() succeeded.\n" );
	return true;
}

bool
JICShadow::uploadWorkingFiles(void)
{
	if( ! filetrans ) {
		return false;
	}

	// The shadow may block on disk I/O for long periods of
	// time, so set a big timeout on the starter's side of the
	// file transfer socket.

	int timeout = param_integer( "STARTER_UPLOAD_TIMEOUT", 200 );
	filetrans->setClientSocketTimeout(timeout);

	// The user job may have created files only readable
	// by the user, so set_user_priv here.
	priv_state saved_priv = set_user_priv();

	// this will block
	bool rval = filetrans->UploadFiles( true, false );
	set_priv(saved_priv);

	if( !rval ) {
		// Failed to transfer.
		dprintf(D_ALWAYS,"JICShadow::uploadWorkingFiles() failed.\n");
		return false;
	}
	dprintf(D_FULLDEBUG,"JICShadow::uploadWorkingFiles() succeeds.\n");
	return true;
}

void
JICShadow::updateCkptInfo(void)
{
	// We need the update ad for our job. 
	ClassAd update_ad;
	publishUpdateAd( &update_ad );

	// See if the LocalUserLog wants it
	u_log->logCheckpoint( &update_ad );

	// Now we want to send another update to the shadow.
	// To confirm this update, we pass in "true" to updateShadow() for that.
	updateShadow( &update_ad );
}

bool
JICShadow::initUserPriv( void )
{

#ifdef WIN32
		// Windoze

	return initUserPrivWindows();

#else
		// Unix

		// by default, the uid we choose is assumed not to be
		// dedicated to this job
	setExecuteAccountIsDedicated( NULL );

		// Before we go through any trouble, see if we even need
		// ATTR_OWNER to initialize user_priv.  If not, go ahead and
		// initialize it as appropriate.  
	if( initUserPrivNoOwner() ) {
		return true;
	}

	bool run_as_owner = allowRunAsOwner( true, true );

	std::string owner;
	if( run_as_owner ) {
		if( job_ad->LookupString( ATTR_OWNER, owner ) != 1 ) {
			dprintf( D_ALWAYS, "ERROR: %s not found in JobAd.  Aborting.\n", 
			         ATTR_OWNER );
			return false;
		}
	}

		// First, we decide if we're in the same UID_DOMAIN as the
		// submitting machine.  If so, we'll try to initialize
		// user_priv via ATTR_OWNER.  If there's no such user in the
		// passwd file, SOFT_UID_DOMAIN is True, and we're talking to
		// at least a 6.3.3 version of the shadow, we'll do a remote 
		// system call to ask the shadow what uid and gid we should
		// use.  If SOFT_UID_DOMAIN is False and there's no such user
		// in the password file, but the UID_DOMAIN's match, it's a
		// fatal error.  If the UID_DOMAIN's just don't match, we
		// initialize as "nobody".

	if( run_as_owner && !sameUidDomain() ) {
		run_as_owner = false;
		dprintf( D_FULLDEBUG, "Submit host is in different UidDomain\n" ); 
	}

	std::string vm_univ_type;
	if( job_universe == CONDOR_UNIVERSE_VM ) {
		job_ad->LookupString(ATTR_JOB_VM_TYPE, vm_univ_type);
	}

	if( run_as_owner ) {
			// Cool, we can try to use ATTR_OWNER directly.
			// NOTE: we want to use the "quiet" version of
			// init_user_ids, since if we're dealing with a
			// "SOFT_UID_DOMAIN = True" scenario, it's entirely
			// possible this call will fail.  We don't want to fill up
			// the logs with scary and misleading error messages.
		if( init_user_ids_quiet(owner.c_str()) ) {
			dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s\"\n", 
			         owner.c_str() );
			if( checkDedicatedExecuteAccounts( owner.c_str() ) ) {
				setExecuteAccountIsDedicated( owner.c_str() );
			}
		}
		else {
				// There's a problem, maybe SOFT_UID_DOMAIN can help.
			bool try_soft_uid = param_boolean( "SOFT_UID_DOMAIN", false );

			if( !try_soft_uid ) {
					// No soft_uid_domain or it's set to False.  No
					// need to do the RSC, we can just fail.
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file and SOFT_UID_DOMAIN is False\n",
						 owner.c_str() );
				return false;
            }

				// if we're here, it means that 1) the owner we want
				// isn't in the passwd file, and 2) SOFT_UID_DOMAIN is
				// True. So, we'll do a CONDOR_REMOTE_get_user_info RSC
				// to get the uid/gid pair we need
				// and initialize user priv with that. 

			ClassAd user_info;
			if( REMOTE_CONDOR_get_user_info( &user_info ) < 0 ) {
				dprintf( D_ALWAYS, "ERROR: "
						 "REMOTE_CONDOR_get_user_info() failed\n" );
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file, SOFT_UID_DOMAIN is True, but the "
						 "condor_shadow failed to send the required Uid.\n",
						 owner.c_str() );
				return false;
			}

			int user_uid, user_gid;
			if( user_info.LookupInteger( ATTR_UID, user_uid ) != 1 ) {
				dprintf( D_ALWAYS, "user_info ClassAd does not contain %s!\n", 
						 ATTR_UID );
				return false;
			}
			if( user_info.LookupInteger( ATTR_GID, user_gid ) != 1 ) {
				dprintf( D_ALWAYS, "user_info ClassAd does not contain %s!\n", 
						 ATTR_GID );
				return false;
			}

				// now, we should have the uid and gid of the user.	
			dprintf( D_FULLDEBUG, "Got UserInfo from the Shadow: "
					 "uid: %d, gid: %d\n", user_uid, user_gid );
			if( ! set_user_ids((uid_t)user_uid, (gid_t)user_gid) ) {
					// This should never really fail, unless the
					// shadow told us it wants us to use 0 for either
					// the uid or gid...
				dprintf( D_ALWAYS, "ERROR: Could not initialize user "
						 "priv with uid %d and gid %d\n", user_uid,
						 user_gid );
				return false;
			}
		}
	} 

	if( !run_as_owner) {

       // first see if we define SLOTx_USER in the config file
        char *nobody_user = NULL;
			// 20 is the longest param: len(VM_UNIV_NOBODY_USER) + 1
        char nobody_param[20];
		std::string slotName = starter->getMySlotName();
		if (slotName.length() > 4) {
			// We have a real slot of the form slotX or slotX_Y
		} else {
			slotName = "slot1";
		}
		upper_case(slotName);

		snprintf( nobody_param, 20, "%s_USER", slotName.c_str() );
		nobody_user = param(nobody_param);
		if (! nobody_user) {
			nobody_user = param("NOBODY_SLOT_USER");
		}

        if ( nobody_user != NULL ) {
            if ( strcmp(nobody_user, "root") == MATCH ) {
                dprintf(D_ALWAYS, "WARNING: %s set to root, which is not "
                       "allowed. Ignoring.\n", nobody_param);
                free(nobody_user);
                nobody_user = strdup("nobody");
            } else {
                dprintf(D_ALWAYS, "%s set, so running job as %s\n",
                        nobody_param, nobody_user);
            }
        } else {
            nobody_user = strdup("nobody");
        }

			// passing NULL for the domain is ok here since this is
			// UNIX code
		if( ! init_user_ids(nobody_user, NULL) ) { 
			dprintf( D_ALWAYS, "ERROR: Could not initialize user_priv "
					 "as \"%s\"\n", nobody_user );
			free( nobody_user );
			return false;
		} else {
			dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s\"\n",
				  nobody_user );
			if( checkDedicatedExecuteAccounts( nobody_user ) ) {
				setExecuteAccountIsDedicated( nobody_user );
			}
			free( nobody_user );
		}			
	}

	user_priv_is_initialized = true;
	return true;
#endif
}


bool
JICShadow::initJobInfo( void ) 
{
		// Give our base class a chance.
	if (!JobInfoCommunicator::initJobInfo()) return false;

	std::string orig_job_iwd;
	std::string x509userproxy;

	if( ! job_ad ) {
		EXCEPT( "JICShadow::initJobInfo() called with NULL job ad!" );
	}

		// stash the executable name in orig_job_name
	if( ! job_ad->LookupString(ATTR_JOB_CMD, &orig_job_name) ) {
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_JOB_CMD );
		return false;
	} else {
			// put the orig job name in class ad
		dprintf(D_ALWAYS, "setting the orig job name in starter\n");
		job_ad->Assign(ATTR_ORIG_JOB_CMD,orig_job_name);
	}

		// stash the iwd name in orig_job_iwd
	if( ! job_ad->LookupString(ATTR_JOB_IWD, orig_job_iwd) ) {
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_JOB_IWD );
		return false;
	} else {
			// put the orig job iwd in class ad
		dprintf(D_ALWAYS, "setting the orig job iwd in starter\n");
		job_ad->Assign(ATTR_ORIG_JOB_IWD, orig_job_iwd);
	}

	if( ! job_ad->LookupInteger(ATTR_JOB_UNIVERSE, job_universe) ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
		job_universe = CONDOR_UNIVERSE_VANILLA;
	}

	if( ! job_ad->LookupInteger(ATTR_CLUSTER_ID, job_cluster) ) { 
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_CLUSTER_ID );
		return false;
	}

	if( job_ad->LookupString(ATTR_X509_USER_PROXY, x509userproxy) ) {
		wants_x509_proxy = true;
	}
	else {
		dprintf( D_FULLDEBUG, "Job doesn't specify x509userproxy, assuming not "
				 "needed\n" );
	}

	if( ! job_ad->LookupInteger(ATTR_PROC_ID, job_proc) ) { 
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_PROC_ID );
		return false;
	}

	job_ad->LookupString( ATTR_JOB_REMOTE_IWD, &job_remote_iwd );

		// everything else is related to if we're transfering files or
		// not. that can get a little complicated, so it all lives in
		// its own set of functions.
	return initFileTransfer();
}


bool
JICShadow::initFileTransfer( void )
{
	bool rval = false;

		// first, figure out if the job supports the new file transfer
		// attributes.  if so, we can see if it wants optional file
		// transfer, and if we need to use it or not...

	ShouldTransferFiles_t should_transfer;
	char* should_transfer_str = NULL;

	job_ad->LookupString( ATTR_SHOULD_TRANSFER_FILES, &should_transfer_str );
	if( should_transfer_str ) {
		should_transfer = getShouldTransferFilesNum( should_transfer_str );
		if( should_transfer < 0 ) {
			dprintf( D_ALWAYS, "ERROR: Invalid %s (%s) in job ad, "
					 "aborting\n", ATTR_SHOULD_TRANSFER_FILES, 
					 should_transfer_str );
			free( should_transfer_str );
			return false;
		}
	} else { 
		dprintf( D_ALWAYS, "ERROR: No file transfer attributes in job "
				 "ad, aborting\n" );
		return false;
	}

	switch( should_transfer ) {
	case STF_IF_NEEDED:
		if( sameFSDomain() ) {
			dprintf( D_FULLDEBUG, "%s is \"%s\" and job's FileSystemDomain "
					 "matches local value, NOT transfering files\n",
					 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
			rval = initNoFileTransfer();
		} else {
			dprintf( D_FULLDEBUG, "%s is \"%s\" but job's FileSystemDomain "
					 "does NOT match local value, transfering files\n",
					 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
			rval = initWithFileTransfer();
		}
		break;
	case STF_YES:
		dprintf( D_FULLDEBUG, "%s is \"%s\", transfering files\n", 
				 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
		rval = initWithFileTransfer();
		break;
	case STF_NO:
		dprintf( D_FULLDEBUG, "%s is \"%s\", NOT transfering files\n", 
				 ATTR_SHOULD_TRANSFER_FILES, should_transfer_str );
		rval = initNoFileTransfer();
		break;
	}
	free( should_transfer_str );
	return rval;
}


bool
JICShadow::initNoFileTransfer( void )
{
	wants_file_transfer = false;
	change_iwd = false;

		/* We assume that transfer_files == Never means that we want
		   to live in the submit directory, so we DON'T change the
		   ATTR_JOB_CMD or the ATTR_JOB_IWD.  This is important to
		   MPI!  -MEY 12-8-1999 */
	job_ad->LookupString( ATTR_JOB_IWD, &job_iwd );
	if( ! job_iwd ) {
		dprintf( D_ALWAYS, "Can't find job's IWD, aborting\n" );
		return false;
	}

		// now that we've got the iwd we're using and all our
		// transfer-related flags set, we can finally initialize the
		// job's standard files.  this is shared code if we're
		// transfering or not, so it's in its own function.
	return initStdFiles();
}


bool
JICShadow::initWithFileTransfer()
{
		// First, see if the new attribute to decide when to transfer
		// the output back is defined, and if so, use it. 
	char* when_str = NULL;
	FileTransferOutput_t when;
	job_ad->LookupString( ATTR_WHEN_TO_TRANSFER_OUTPUT, &when_str );
	if( when_str ) {
		when = getFileTransferOutputNum( when_str );
		if( when < 0 ) {
			dprintf( D_ALWAYS, "ERROR: Invalid %s (%s) in job ad, "
					 "aborting\n", ATTR_WHEN_TO_TRANSFER_OUTPUT, when_str );
			free( when_str );
			return false;
		}
		free( when_str );
		if( when == FTO_ON_EXIT_OR_EVICT ) {
			transfer_at_vacate = true;
		}
	} else { 
		dprintf( D_ALWAYS, "ERROR: %s attribute missing from job "
				 "ad, aborting\n", ATTR_WHEN_TO_TRANSFER_OUTPUT );
		return false;
	}

		// if we're here, it means we're transfering files, so we need
		// to reset the job's iwd to the starter directory

	// When using file transfer, always rename the stdout/err files to
	// the special names StdoutRemapName and StderrRemapName.
	// The shadow will remap these to the original names when transferring
	// the output.
	bool stream;
	std::string stdout_name;
	std::string stderr_name;
	job_ad->LookupString( ATTR_JOB_OUTPUT, stdout_name );
	job_ad->LookupString( ATTR_JOB_ERROR, stderr_name );
	if ( job_ad->LookupBool( ATTR_STREAM_OUTPUT, stream ) && !stream &&
		 !nullFile( stdout_name.c_str() ) ) {
		job_ad->Assign( ATTR_JOB_OUTPUT, StdoutRemapName );
		job_ad->Assign( ATTR_JOB_ORIGINAL_OUTPUT, stdout_name );
	}
	if ( job_ad->LookupBool( ATTR_STREAM_ERROR, stream ) && !stream &&
		 !nullFile( stderr_name.c_str() ) ) {
		if ( stdout_name == stderr_name ) {
			job_ad->Assign( ATTR_JOB_ERROR, StdoutRemapName );
		} else {
			job_ad->Assign( ATTR_JOB_ERROR, StderrRemapName );
		}
		job_ad->Assign( ATTR_JOB_ORIGINAL_ERROR, stderr_name );
	}

	wants_file_transfer = true;
	change_iwd = true;
	job_iwd = strdup( starter->GetWorkingDir(0) );
	job_ad->Assign( ATTR_JOB_IWD, job_iwd );

		// now that we've got the iwd we're using and all our
		// transfer-related flags set, we can finally initialize the
		// job's standard files.  this is shared code if we're
		// transfering or not, so it's in its own function.
	return initStdFiles();
}


bool
JICShadow::initStdFiles( void )
{
		// now that we know about file transfer and the real iwd we'll
		// be using, we can initialize the std files... 
	if( ! job_input_name ) {
		job_input_name = getJobStdFile( ATTR_JOB_INPUT );
	}

	if( ! job_output_name ) {
		job_output_name = getJobStdFile( ATTR_JOB_OUTPUT );
	}
	if( ! job_error_name ) {
		job_error_name = getJobStdFile( ATTR_JOB_ERROR );
	}

		// so long as all of the above are initialized, we were
		// successful, regardless of if any of them are NULL...
	return true;
}


char* 
JICShadow::getJobStdFile( const char* attr_name )
{
	char* tmp = NULL;
	const char* base = NULL;
	std::string filename;

	if(streamStdFile(attr_name)) {
		if(!tmp && attr_name) job_ad->LookupString(attr_name,&tmp);
		return tmp;
	}

	if( !tmp ) {
		job_ad->LookupString( attr_name, &tmp );
	}
	if( !tmp ) {
		return NULL;
	}
	if ( !nullFile(tmp) ) {
		if( wants_file_transfer ) {
			base = condor_basename( tmp );
		} else {
			base = tmp;
		}
		if( ! fullpath(base) ) {	// prepend full path
			formatstr( filename, "%s%c", job_iwd, DIR_DELIM_CHAR );
		}
		filename += base;
	}
	free( tmp );
	if (!filename.empty()) { 
		return strdup( filename.c_str() );
	}
	return NULL;
}


bool
JICShadow::sameUidDomain( void ) 
{
	char* job_uid_domain = NULL;
	bool same_domain = false;

	ASSERT( uid_domain );
	ASSERT( shadow->name() );

	if( job_ad->LookupString( ATTR_UID_DOMAIN, &job_uid_domain ) != 1 ) {
			// No UidDomain in the job ad, what should we do?
			// For now, we'll just have to assume that we're not in
			// the same UidDomain...
		dprintf( D_FULLDEBUG, "SameUidDomain(): Job ClassAd does not "
				 "contain %s, returning false\n", ATTR_UID_DOMAIN );
		return false;
	}

	dprintf( D_FULLDEBUG, "Submit UidDomain: \"%s\"\n",
			 job_uid_domain );
	dprintf( D_FULLDEBUG, " Local UidDomain: \"%s\"\n",
			 uid_domain );

	if( strcasecmp(job_uid_domain, uid_domain) == MATCH ) {
		same_domain = true;
	}

	free( job_uid_domain );

	if( ! same_domain ) {
		return false;
	}

		// finally, for "security", make sure that the submitting host
		// contains our UidDomain as a substring of its hostname.
		// this way, we know someone's not just lying to us about what
		// UidDomain they're in.
		// However, if the site defines "TRUST_UID_DOMAIN = True" in
		// their config file, don't perform this check, so sites can
		// use UID domains that aren't substrings of DNS names if they
		// have to.
	if( trust_local_uid_domain && syscall_sock && syscall_sock->peer_is_local() ) {
		dprintf( D_FULLDEBUG, "SameUidDomain(): Peer is on a local "
		         "interface and TRUST_LOCAL_UID_DOMAIN=True, "
		         "returning true\n" );
		return true;
	}

	if( trust_uid_domain ) {
		dprintf( D_FULLDEBUG, "TRUST_UID_DOMAIN is 'True' in the config "
				 "file, not comparing shadow's UidDomain (%s) against its "
				 "hostname (%s)\n", uid_domain, shadow->name() );
		return true;
	}
	if( host_in_domain(shadow->name(), uid_domain) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: the submitting host claims to be in our "
			 "UidDomain (%s), yet its hostname (%s) does not match.  "
			 "If the above hostname is actually an IP address, Condor "
			 "could not perform a reverse DNS lookup to convert the IP "
			 "back into a name.  To solve this problem, you can either "
			 "correctly configure DNS to allow the reverse lookup, or you "
			 "can enable TRUST_UID_DOMAIN in your condor configuration.\n",
			 uid_domain, shadow->name() );

		// TODO: maybe we should be more harsh in this case than just
		// running their job as nobody... perhaps we should EXCEPT()
		// in this case and force the user/admins to get it right
		// before we agree to run *any* jobs from a shadow like this? 

	return false;
}


bool
JICShadow::sameFSDomain( void ) 
{
	char* job_fs_domain = NULL;
	bool same_domain = false;

	ASSERT( fs_domain );

	if( ! job_ad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, &job_fs_domain) ) {
			// No FileSystemDoamin in the job ad, what should we do?
			// For now, we'll just have to assume that we're not in
			// the same FSDomain...
		dprintf( D_FULLDEBUG, "SameFSDomain(): Job ClassAd does not "
				 "contain %s, returning false\n", ATTR_FILE_SYSTEM_DOMAIN );
		return false;
	}

	dprintf( D_FULLDEBUG, "Submit FsDomain: \"%s\"\n",
			 job_fs_domain );
	dprintf( D_FULLDEBUG, " Local FsDomain: \"%s\"\n",
			 fs_domain );

	if( strcasecmp(job_fs_domain, fs_domain) == MATCH ) {
		same_domain = true;
	}

		// do we want to do any "security" checking on the FS domain
		// here?  i don't think so...  it should just be an arbitrary
		// string, not necessarily a domain name or substring.

	free( job_fs_domain );
	return same_domain;
}

bool
JICShadow::usingFileTransfer( void )
{
	return wants_file_transfer;
}


static void
refuse(ReliSock * s)
{
	ASSERT(s);
	s->encode();
	int i = 0; // == failure;
	if (!s->code(i)) {
		dprintf(D_ALWAYS, "Unable to refuse X509 proxy update -- has client gone away?\n");
	}
	s->end_of_message();
}

// Based on Scheduler::updateGSICred
static bool
updateX509Proxy(int cmd, ReliSock * rsock, const char * path)
{
	ASSERT(rsock);
	ASSERT(path);

	rsock->timeout(10);
	rsock->decode();

	dprintf(D_FULLDEBUG,
	        "Remote side requests to update X509 proxy at %s\n",
	        path);

	std::string tmp_path;
	tmp_path = path;
	tmp_path += ".tmp";

	priv_state old_priv = set_priv(PRIV_USER);

	int reply;
	filesize_t size = 0;
	int rc;
	if ( cmd == UPDATE_GSI_CRED ) {
		rc = rsock->get_file(&size,tmp_path.c_str());
	} else if ( cmd == DELEGATE_GSI_CRED_STARTER ) {
		rc = rsock->get_x509_delegation(tmp_path.c_str(), false, NULL);
	} else {
		dprintf( D_ALWAYS,
		         "unknown CEDAR command %d in updateX509Proxy\n",
		         cmd );
		rc = -1;
	}

	if ( rc < 0 ) {
			// transfer failed
		reply = 0; // == failure
	} else { // transfer worked, now rename the file to
		// final_proxy_path
		if ( rotate_file(tmp_path.c_str(), path) < 0 ) 
		{
			// the rename failed!!?!?!
			dprintf( D_ALWAYS,
					"updateX509Proxy failed, "
					"could not rename file\n");
			reply = 0; // == failure
		} else {
			reply = 1; // == success
		}
	}
	set_priv(old_priv);

		// Send our reply back to the client
	rsock->encode();
	if (!rsock->code(reply)) {
		dprintf(D_ALWAYS,
		        "Attempt to refresh X509 proxy failed to reply\n");
		reply = false;
	}
	rsock->end_of_message();

	if(reply) {
		dprintf(D_FULLDEBUG,
		        "Attempt to refresh X509 proxy succeeded.\n");
	} else {
		dprintf(D_ALWAYS,
		        "Attempt to refresh X509 proxy FAILED.\n");
	}

	return reply;
}

void
JICShadow::proxyExpiring()
{
	// we log the return value, but even if it failed we still try to clean up
	// because we are about to lose control of the job otherwise.
	holdJob("Proxy about to expire", CONDOR_HOLD_CODE::CorruptedCredential, 0);

	// this will actually clean up the job
	if ( starter->Hold( ) ) {
		dprintf( D_FULLDEBUG, "JICSHADOW: Hold() returns true\n" );
		this->allJobsDone();
	} else {
		dprintf( D_FULLDEBUG, "JICSHADOW: Hold() returns false\n" );
	}

	// and this causes us to exit relatively cleanly.  it tries to communicate
	// with the shadow, which fails, but i'm not sure what to do about that.
	starter->ShutdownFast();

	return;
}

bool
JICShadow::updateX509Proxy(int cmd, ReliSock * s)
{
	if( ! usingFileTransfer() ) {
		s->encode();
		int i = 2; // == success, but please don't call any more.
		if (!s->code(i)) { // == success, but please don't call any more.
			dprintf(D_ALWAYS, "Unable to update X509 proxy request -- has client gone away?\n");
		}
		s->end_of_message();
		refuse(s);
		return false;
	}
	std::string path;
	if( ! job_ad->LookupString(ATTR_X509_USER_PROXY, path) ) {
		dprintf(D_ALWAYS, "Refusing shadow's request to update proxy as this job has no proxy\n");
		return false;
	}
	const char * proxyfilename = condor_basename(path.c_str());

	bool retval = ::updateX509Proxy(cmd, s, proxyfilename);
	return retval;
}


bool
JICShadow::recordDelayedUpdate( const std::string &name, const classad::ExprTree &expr )
{
	std::string prefix;
	param(prefix, "CHIRP_DELAYED_UPDATE_PREFIX");
	if (!prefix.size())
	{
		dprintf(D_ALWAYS, "Got an invalid prefix for updates: %s\n", name.c_str());
	}
	std::vector<std::string> sl = split(prefix);
	if (contains_anycase_withwildcard(sl, name))
	{
		std::vector<std::string>::const_iterator it = std::find(m_delayed_update_attrs.begin(),
			m_delayed_update_attrs.end(), name);
		if (it == m_delayed_update_attrs.end())
		{
			unsigned int max_attrs = param_integer("CHIRP_DELAYED_UPDATE_MAX_ATTRS",100);
			if (m_delayed_update_attrs.size() >= max_attrs)
			{
				dprintf(D_ALWAYS, "Ignoring update for %s because %d attributes have already been set.\n",
					name.c_str(),max_attrs);
				return false;
			}
			m_delayed_update_attrs.push_back(name);
		}
		// Note that the ClassAd takes ownership of the copy.
		dprintf(D_FULLDEBUG, "Got a delayed update for attribute %s.\n", name.c_str());
		classad::ExprTree *expr_copy = expr.Copy();
		m_delayed_updates.Insert(name, expr_copy);
		return true;
	}
	else
	{
		dprintf(D_ALWAYS, "Got an invalid prefix for updates: %s\n", name.c_str());
		return false;
	}
}



std::unique_ptr<classad::ExprTree>
JICShadow::getDelayedUpdate( const std::string &name )
{
	std::unique_ptr<classad::ExprTree> expr;
	classad::ExprTree *borrowed_expr = NULL;
	ClassAd *ad = jobClassAd();
	dprintf(D_FULLDEBUG, "Looking up delayed attribute named %s.\n", name.c_str());
	if (!(borrowed_expr = m_delayed_updates.Lookup(name)) && (!ad || !(borrowed_expr = ad->Lookup(name))))
	{
		return expr;
	}
	expr.reset(borrowed_expr->Copy());
	return expr;
}

bool
JICShadow::publishStartdUpdates( ClassAd* ad ) {
	// Construct the list of attributes to pull from the slot's update ad.
	// Arguably, this list should be passed to the starter from the startd,
	// because you can theoretically run more than one, but we'll ignore
	// that for now (and the startd doesn't produce the list itself).
	if(! m_job_update_attrs_set) {
		m_job_update_attrs.emplace_back( ATTR_CPUS_USAGE );

		std::string scjl;
		if( param( scjl, "STARTD_CRON_JOBLIST" ) ) {
			for (const auto& cronName: StringTokenIterator(scjl)) {
				std::string metrics;
				std::string paramName;
				formatstr( paramName, "STARTD_CRON_%s_METRICS", cronName.c_str() );
				if( param( metrics, paramName.c_str() ) ) {
					for (const auto& pair: StringTokenIterator(metrics)) {
						StringTokenIterator tn( pair, ":" );
						/* const char * metricType = */ tn.first();
						const char * resourceName = tn.next();

						std::string metricName;
						formatstr( metricName, "%sUsage", resourceName );
						m_job_update_attrs.emplace_back( metricName.c_str() );

						// We could use metricType to determine if we need
						// this attribute or the preceeding one, but for now
						// don't bother.
						formatstr( metricName, "%sAverageUsage", resourceName );
						m_job_update_attrs.emplace_back( metricName.c_str() );

						formatstr( metricName, "Recent%sUsage", resourceName );
						m_job_update_attrs.emplace_back( metricName.c_str() );
					}
				}
			}
		}

		m_job_update_attrs_set = true;
	}

	// Pull the list of attributes from the slot's update ad.
	bool published = false;
	if(! m_job_update_attrs.empty()) {

		std::string updateAdPath;
		formatstr( updateAdPath, "%s/%s",
			starter->GetWorkingDir(0), ".update.ad"
		);
		FILE * updateAdFile = NULL;
		{
			TemporaryPrivSentry p( PRIV_USER );
			updateAdFile = safe_fopen_wrapper_follow( updateAdPath.c_str(), "r" );
		}
		if( updateAdFile ) {
			int isEOF, error, empty;
			ClassAd updateAd;
			InsertFromFile( updateAdFile, updateAd, "\n", isEOF, error, empty );
			fclose( updateAdFile );

			for (const auto& attrName: m_job_update_attrs) {
				// dprintf( D_ALWAYS, "Updating job ad: %s\n", attrName.c_str() );
				CopyAttribute( attrName, *ad, updateAd );
				published = true;
			}
		} else {
			dprintf( D_ALWAYS, "Failed to open '%s' to read update ad: %s (%d).\n",
				updateAdPath.c_str(), strerror(errno), errno );
		}
	}

	return published;
}

bool
JICShadow::publishUpdateAd( ClassAd* ad )
{
	// These are updates taken from Chirp
	ad->Update(m_delayed_updates);
	m_delayed_updates.Clear();

	// if there is a filetrans object, then let's send the current
	// size of the starter execute directory back to the shadow.  this
	// way the ATTR_DISK_USAGE will be updated, and we won't end
	// up on a machine without enough local disk space.
	if ( filetrans ) {
		auto [execsz, file_count] = starter->GetDiskUsage();
		ad->Assign(ATTR_DISK_USAGE, (execsz+1023) / 1024);
		ad->Assign(ATTR_SCRATCH_DIR_FILE_COUNT, file_count);
	}

	ad->Assign(ATTR_EXECUTE_DIRECTORY_ENCRYPTED, starter->hasEncryptedWorkingDir());

	std::string spooled_files;
	if( job_ad->LookupString(ATTR_SPOOLED_OUTPUT_FILES,spooled_files) )
	{
		ad->Assign(ATTR_SPOOLED_OUTPUT_FILES,spooled_files);
	}

	// Insert the starter's address into the update ad, because all
	// parties who subscribe to updates (shadow & startd) also should
	// be informed of any changes in the starter's contact info
	// (important for CCB and shadow-starter reconnect, because startd
	// needs to relay starter's full contact info to the shadow when
	// queried).  It's a bit of a hack to do it through this channel,
	// but better than nothing.
	ad->Assign( ATTR_STARTER_IP_ADDR, daemonCore->publicNetworkIpAddr() );

		// Now, get our Starter object to publish, as well.  This will
		// walk through all the UserProcs and have those publish, as
		// well.  It returns true if there was anything published,
		// false if not.
	bool retval = starter->publishUpdateAd( ad );

	// These are updates taken from Chirp
	// Note they should not go to the starter!
	ad->Update(m_delayed_updates);
	m_delayed_updates.Clear();

	if( publishStartdUpdates( ad ) ) { return true; }
	return retval;
}


bool
JICShadow::publishJobExitAd( ClassAd* ad )
{
	// if there is a filetrans object, then let's send the current
	// size of the starter execute directory back to the shadow.  this
	// way the ATTR_DISK_USAGE will be updated, and we won't end
	// up on a machine without enough local disk space.
	if ( filetrans ) {
		auto [execsz, file_count] = starter->GetDiskUsage(true);
		ad->Assign(ATTR_DISK_USAGE, (execsz+1023) / 1024);
		ad->Assign(ATTR_SCRATCH_DIR_FILE_COUNT, file_count);
	}

	ad->Assign(ATTR_EXECUTE_DIRECTORY_ENCRYPTED, starter->hasEncryptedWorkingDir());

	std::string spooled_files;
	if( job_ad->LookupString(ATTR_SPOOLED_OUTPUT_FILES,spooled_files) && spooled_files.length() > 0 )
	{
		ad->Assign(ATTR_SPOOLED_OUTPUT_FILES,spooled_files);
	}

	// Insert the starter's address into the update ad, because all
	// parties who subscribe to updates (shadow & startd) also should
	// be informed of any changes in the starter's contact info
	// (important for CCB and shadow-starter reconnect, because startd
	// needs to relay starter's full contact info to the shadow when
	// queried).  It's a bit of a hack to do it through this channel,
	// but better than nothing.
	ad->Assign( ATTR_STARTER_IP_ADDR, daemonCore->publicNetworkIpAddr() );

		// Now, get our Starter object to publish, as well.  This will
		// walk through all the UserProcs and have those publish, as
		// well.  It returns true if there was anything published,
		// false if not.
	bool retval = starter->publishJobExitAd( ad );

	if( publishStartdUpdates( ad ) ) { return true; }
	return retval;
}


bool
JICShadow::periodicJobUpdate( ClassAd* update_ad )
{
	bool r1, r2;
	// call updateShadow first, because this may have the side effect of clearing
	// the m_delayed_update_attrs and we want to make sure that the shadow gets to see them.
	r1 = updateShadow(update_ad);
	r2 = JobInfoCommunicator::periodicJobUpdate(update_ad);
	return (r1 && r2);
}


bool
JICShadow::updateShadow( ClassAd* update_ad )
{
	dprintf( D_FULLDEBUG, "Entering JICShadow::updateShadow()\n" );

	ClassAd local_ad;
	ClassAd* ad;
	if( update_ad ) {
			// we already have the update info, so don't bother trying
			// to publish another one.
		ad = update_ad;
	} else {
		ad = &local_ad;
		if( ! publishUpdateAd(ad) ) {  
			dprintf( D_FULLDEBUG, "JICShadow::updateShadow(): "
					 "Didn't find any info to update!\n" );
			return false;
		}
	}

	bool rval;

	// Add an attribute that says when the shadow can expect to receive
	// another update.  We do this because the shadow uses these updates
	// as a heartbeat; if the shadow does not receive an expected update from
	// us, it assumes the connection is dead (even if the syscall sock is still alive,
	// since that may be the doing of a misbehaving NAT box).
	ad->Assign(ATTR_JOBINFO_MAXINTERVAL, periodicJobUpdateTimerMaxInterval());

	// Invoke the remote syscall
	rval = (REMOTE_CONDOR_register_job_info(*ad) == 0);

	if (syscall_sock && !rval) {
		// Failed to send update to the shadow.  Since the shadow
		// never returns failure for this pseudo call, we assume
		// we failed to communicate with the shadow.  Close up the
		// socket and wait for a reconnect.  
		syscall_sock_disconnect();
	}

	if ( syscall_sock && syscall_sock->is_connected() ) {
		ad->Delete("STARTER_DISCONNECTED_FROM_SUBMIT_NODE");
		syscall_sock_reconnect();
	} else {
		ad->Assign("STARTER_DISCONNECTED_FROM_SUBMIT_NODE",true);
	}

	updateStartd(ad, false);

	if (rval) {
		dprintf(D_FULLDEBUG, "Leaving JICShadow::updateShadow(): success\n");
		return true;
	}
	dprintf(D_FULLDEBUG, "JICShadow::updateShadow(): failed to send update\n");
	return false;
}

void
JICShadow::syscall_sock_disconnect()
{
	/* 
	  This method is invoked whenever we failed to 
	  communicate on the syscall_sock.  Here we close up
	  the socket and start a timer.
	*/

	if ( syscall_sock_lost_time > 0 ) {
		// Already in disconnected state.  This
		// method must have already been invoked, so 
		// no work left to do.
		return;
	}

	// Record time of disconnect
	time_t now = time(NULL);
	syscall_sock_lost_time = now;

	// Set a timer to go off after we've been disconnected
	// for the maximum lease time.
	if ( syscall_sock_lost_tid != -1 ) {
		daemonCore->Cancel_Timer(syscall_sock_lost_tid);
		syscall_sock_lost_tid = -1;
	}
	int lease_duration = -1;
	job_ad->LookupInteger(ATTR_JOB_LEASE_DURATION,lease_duration);
	lease_duration -= now - syscall_last_rpc_time;
	if (lease_duration < 0) {
		lease_duration = 0;
	}
	syscall_sock_lost_tid = daemonCore->Register_Timer(
			lease_duration,
			(TimerHandlercpp)&JICShadow::job_lease_expired,
			"job_lease_expired",
			this );
	dprintf(D_ALWAYS,
		"Lost connection to shadow, last activity was %ld secs ago, waiting %d secs for reconnect\n",
		(now - syscall_last_rpc_time), lease_duration);

	// Close up the syscall_socket and wait for a reconnect.  
	if (syscall_sock) {
		if (syscall_sock_registered) {
			daemonCore->Cancel_Socket(syscall_sock);
			syscall_sock_registered = false;
		}
		syscall_sock->close();
	}
}

void 
JICShadow::syscall_sock_reconnect()
{
	/* 
	  This method is invoked when the syscall_sock is reconnected 
	  and we can successfully communicate with the shadow.
	*/
	
	if ( syscall_sock_lost_time > 0 ) {
		dprintf(D_ALWAYS,
			"Recovered connection to shadow after %d seconds\n",
			(int)(time(NULL) - syscall_sock_lost_time) );
	}

	syscall_sock_lost_time = 0;

	if ( syscall_sock_lost_tid != -1 ) {
		daemonCore->Cancel_Timer(syscall_sock_lost_tid);
		syscall_sock_lost_tid = -1;
	}	

	ASSERT(syscall_sock);
	if (syscall_sock_registered == false) {
		int reg_rc = daemonCore->
			Register_Socket( syscall_sock, "syscall sock to shadow",
			  (SocketHandlercpp)&JICShadow::syscall_sock_handler,
			  "JICShadow::syscall_sock_handler", this );
		if(reg_rc < 0) {
			dprintf( D_ALWAYS,
		         "Failed to register syscall socket to shadow\n" );
		} else {
			syscall_sock_registered = true;
		}
	}
}


int
JICShadow::syscall_sock_handler(Stream *)
{
	// select() triggered on the syscall_sock.  this should never happen as
	// there is no async I/O on the syscall_sock in the starter.  so what
	// likely happened is the operating system closed the socket due to 
	// sockopt keepalives failing.  so all we do here is try to update
	// the shadow - should that fail, updateShadow() will do
	// all the right stuff like invoking syscall_sock_disconnect() etc.
	dprintf(D_ALWAYS,
		"Connection to shadow may be lost, will test by sending whoami request.\n");
	char buff[32];
	if ( REMOTE_CONDOR_whoami( sizeof(buff), buff ) <= 0 ) {
		// Failed to send whoami request to the shadow.  Since the shadow
		// never returns failure for this pseudo call (if our buffer is
		// large enough), we assume
		// we failed to communicate with the shadow.  Close up the
		// socket and wait for a reconnect.
		syscall_sock_disconnect();
	}
	return KEEP_STREAM;
}


void
JICShadow::job_lease_expired( int /* timerID */ ) const
{
	/* 
	  This method is invoked by a daemoncore timer, which is set
	  to fire ATTR_JOB_LEASE_DURATION seconds after the last activity
	  on the syscall_sock
	*/

	dprintf( D_ALWAYS, "No reconnect from shadow for %d seconds, aborting job execution!\n", (int)(time(NULL) - syscall_sock_lost_time) );

	// A few sanity checks...
	ASSERT(syscall_sock_lost_time > 0);
	if (syscall_sock) {
		ASSERT( !syscall_sock->is_connected() );
	}

	// Exit telling the startd we lost the shadow
	starter->SetShutdownExitCode(STARTER_EXIT_LOST_SHADOW_CONNECTION);
	if ( starter->RemoteShutdownFast(0) ) {
		starter->StarterExit( starter->GetShutdownExitCode() );
	}
}

bool
JICShadow::beginFileTransfer( void )
{

		// if requested in the jobad, transfer files over.  
	if( wants_file_transfer ) {
		filetrans = new FileTransfer();
	#if 1 //def HAVE_DATA_REUSE_DIR
		auto reuse_dir = starter->getDataReuseDirectory();
		if (reuse_dir) {
			filetrans->setDataReuseDirectory(*reuse_dir);
		}
	#endif

		// file transfer plugins will need to know about OAuth credentials
		const char *cred_path = getCredPath();
		if (cred_path) {
			filetrans->setCredsDir(cred_path);
		}

		std::string job_ad_path, machine_ad_path;
		formatstr(job_ad_path, "%s%c%s", starter->GetWorkingDir(0),
			DIR_DELIM_CHAR,
			JOB_AD_FILENAME);
		formatstr(machine_ad_path, "%s%c%s", starter->GetWorkingDir(0),
			DIR_DELIM_CHAR,
			MACHINE_AD_FILENAME);
		filetrans->setRuntimeAds(job_ad_path, machine_ad_path);
		dprintf(D_ALWAYS, "Set filetransfer runtime ads to %s and %s.\n", job_ad_path.c_str(), machine_ad_path.c_str());

			// In the starter, we never want to use
			// SpooledOutputFiles, because we are not reading the
			// output from the spool.  We always want to use
			// TransferOutputFiles instead.
		job_ad->Delete(ATTR_SPOOLED_OUTPUT_FILES);

		ASSERT( filetrans->Init(job_ad, false, PRIV_USER) );
		filetrans->setSecuritySession(m_filetrans_sec_session);
		filetrans->setSyscallSocket(syscall_sock);
		// "true" means want in-flight status updates
#ifdef WINDOWS
		filetrans->RegisterCallback(
				  (FileTransferHandlerCpp)&JICShadow::transferInputStatus,this,false);
#else
		filetrans->RegisterCallback(
				  (FileTransferHandlerCpp)&JICShadow::transferInputStatus,this,true);
#endif


		if ( shadow_version == NULL ) {
			dprintf( D_ALWAYS, "Can't determine shadow version for FileTransfer!\n" );
		} else {
			filetrans->setPeerVersion( *shadow_version );
		}

		if( ! filetrans->DownloadFiles(false) ) { // do not block (i.e. fork)
				// Error starting the non-blocking file transfer.  For
				// now, consider this a fatal error
			EXCEPT( "Could not initiate file transfer" );
		}

		this->file_xfer_last_alive_time = time(nullptr);
#ifndef WINDOWS
		this->file_xfer_last_alive_tid = 
			daemonCore->Register_Timer(20, 300, (TimerHandlercpp) &JICShadow::verifyXferProgressing, "verify xfer progress", this); 
#endif
		return true;
	}
		// If FileTransfer not requested, but we still need an x509 proxy, do RPC call
	else if ( wants_x509_proxy ) {
		
			// Get scratch directory path
		const char* scratch_dir = starter->GetWorkingDir(0);

			// Get source path to proxy file on the submit machine
		std::string proxy_source_path;
		job_ad->LookupString( ATTR_X509_USER_PROXY, proxy_source_path );

			// Get proxy expiration timestamp
		time_t proxy_expiration = GetDesiredDelegatedJobCredentialExpiration( job_ad );

			// Parse proxy filename
		const char *proxy_filename = condor_basename( proxy_source_path.c_str() );

			// Combine scratch dir and proxy filename to get destination proxy path on execute machine
		std::string proxy_dest_path = scratch_dir;
		proxy_dest_path += DIR_DELIM_CHAR;
		proxy_dest_path += proxy_filename;

			// Do RPC call to get delegated proxy.
			// This needs to happen with user privs so we can write to scratch dir!
		priv_state original_priv = set_user_priv();
		REMOTE_CONDOR_get_delegated_proxy( proxy_source_path.c_str(), proxy_dest_path.c_str(), proxy_expiration );
		set_priv( original_priv );

			// Update job ad with location of proxy
		job_ad->Assign( ATTR_X509_USER_PROXY, proxy_dest_path );
	}
		// no transfer wanted or started, so return false
	return false;
}


void
JICShadow::updateShadowWithPluginResults( const char * which ) {
	if(! filetrans) { return; }
	if( filetrans->getPluginResultList().size() <= 0 ) { return; }

	ClassAd updateAd;

	classad::ExprList * e = new classad::ExprList();
	for( const auto & ad : filetrans->getPluginResultList() ) {
		ClassAd * filteredAd = filterPluginResults( ad );
		if( filteredAd != NULL ) {
			e->push_back( filteredAd );
		}
	}
	std::string attributeName;
	formatstr( attributeName, "%sPluginResultList", which );
	updateAd.Insert( attributeName, e );

	updateShadow( & updateAd );
}

void 
JICShadow::verifyXferProgressing(int /*timerid*/) {
	int stall_timeout = param_integer("STARTER_FILE_XFER_STALL_TIMEOUT", 3600);
	time_t now = time(nullptr);
	if ((now - this->file_xfer_last_alive_time) > stall_timeout) {
		EXCEPT( "Input File transfer stalled for %ld seconds", now - file_xfer_last_alive_time);
	}
}

// FileTransfer callback for status messages of input transfer
int
JICShadow::transferInputStatus(FileTransfer *ftrans)
{
	// An in-progress message? (ftrans is null when we fake completion)
	if (ftrans) {
		const FileTransfer::FileTransferInfo &info = ftrans->GetInfo();
		if (info.in_progress) {
			// a status ping message. xfer is still making progress!
			this->file_xfer_last_alive_time = time(nullptr);
			return 1;
		}
	}

	dprintf(D_ZKM,"transferInputCompleted(%p) success=%d try_again=%d\n", ftrans,
		ftrans ? ftrans->GetInfo().success : -1,
		ftrans ? ftrans->GetInfo().try_again : -1) ;

#ifndef WINDOWS
	if (this->file_xfer_last_alive_tid) {
		daemonCore->Cancel_Timer(this->file_xfer_last_alive_tid);
	}
#endif

	if ( ftrans ) {
		updateShadowWithPluginResults("Input");

			// Make certain the file transfer succeeded.
		FileTransfer::FileTransferInfo ft_info = ftrans->GetInfo();
		if ( !ft_info.success ) {

		#if 1 // don't EXCEPT, keep going to transfer FailureFiles
			UnreadyReason urea = { ft_info.hold_code, ft_info.hold_subcode, "Failed to transfer files: " };
			if ( ! ft_info.try_again && ! urea.hold_code) {
				// make sure we have a valid hold code
				urea.hold_code = FILETRANSFER_HOLD_CODE::DownloadFileError;
			}
			if (ft_info.error_desc.empty()) {
				urea.message += " reason unknown.";
			} else {
				urea.message += ft_info.error_desc;
			}

			dprintf(D_ERROR, "%s\n", urea.message.c_str());

			// setupCompleted with non-success will queue a SkipJob timer to
			// start output transfer of FailureFiles
			setupCompleted(ft_info.try_again ? JOB_SHOULD_REQUEUE : JOB_SHOULD_HOLD, &urea);
			m_job_setup_done = true;
			return TRUE;
		#else
			if (job_ad->Lookup(ATTR_JOB_STARTER_LOG)) {
				// Do a failure transfer
				dprintf(D_ZKM,"JEF Doing failure transfer\n");
				dprintf_close_logs_in_directory(starter->GetWorkingDir(false), false);
				TemporaryPrivSentry sentry(PRIV_USER);
				filetrans->addFailureFile(SANDBOX_STARTER_LOG_FILENAME);
				priv_state saved_priv = set_user_priv();
				filetrans->UploadFailureFiles( true );
			}

			if(!ft_info.try_again) {
					// Put the job on hold.
				ASSERT(ft_info.hold_code != 0);
				notifyStarterError(ft_info.error_desc.c_str(), true,
				                   ft_info.hold_code,ft_info.hold_subcode);
			}

			std::string message {"Failed to transfer files: "};
			if (ft_info.error_desc.empty()) {
				message += " reason unknown.";
			} else {
				message += ft_info.error_desc;
			}

			EXCEPT("%s", message.c_str());
		#endif
		}


		// It's not enought to for the FTO to believe that the transfer
		// of a checkpoint succeeded if that checkpoint wasn't transferred
		// by CEDAR (because our file-transfer plugins don't do integrity).
		std::string checkpointDestination;
		if( job_ad->LookupString( ATTR_JOB_CHECKPOINT_DESTINATION, checkpointDestination ) ) {
			// We only generate MANIFEST files if the checkpoint wasn't
			// stored to the spool, which is exactly the case in which
			// we want to do this manual integrity check.

			// Due to a shortcoming in the FTO, we can't send files with
			// one name from the shadow to the starter with another, so
			// we have to look for any file of the form `MANIFEST\.\d\d\d\d`;
			// it is erroneous to have received more than one.

			std::string manifestFileName;
			const char * currentFile = nullptr;
			// Should this be starter->getWorkingDir(false)?
			Directory sandboxDirectory( "." );
			while( (currentFile = sandboxDirectory.Next()) ) {
				if( -1 != manifest::getNumberFromFileName( currentFile ) ) {
					if(! manifestFileName.empty()) {
						std::string message = "Found more than one MANIFEST file, aborting.";
						notifyStarterError( message.c_str(), true, 0, 0 );
						EXCEPT( "%s", message.c_str() );
					}
					manifestFileName = currentFile;
				}
			}

			if(! manifestFileName.empty()) {

				// This file should have been transferred via CEDAR, so this
				// check shouldn't be necessary, but it also ensures that we
				// haven't had a name collision with the job.
				if(! manifest::validateManifestFile( manifestFileName )) {
					std::string message = "Invalid MANIFEST file, aborting.";
					notifyStarterError( message.c_str(), true, 0, 0 );
					EXCEPT( "%s", message.c_str() );
				}

				std::string error;
				if(! manifest::validateFilesListedIn( manifestFileName, error )) {
					formatstr( error, "%s, aborting.", error.c_str() );
					notifyStarterError( error.c_str(), true, 0, 0 );
					EXCEPT( "%s", error.c_str() );
				}

				unlink( manifestFileName.c_str() );
			}
		}

		const char *stats = m_ft_info.tcp_stats.c_str();
		if (strlen(stats) != 0) {
			std::string full_stats = "(peer stats from starter): ";
			full_stats += stats;

			ASSERT( !shadowDisconnected() );

			if (shadow_version && shadow_version->built_since_version(8, 5, 8)) {
				REMOTE_CONDOR_dprintf_stats(full_stats.c_str());
			}
		}

			// If we transferred the executable, make sure it
			// has its execute bit set.
		bool xferExec = true;
		job_ad->LookupBool(ATTR_TRANSFER_EXECUTABLE,xferExec);

		std::string cmd;
		if (job_ad->LookupString(ATTR_JOB_CMD, cmd) && xferExec)
		{
				// if we are running as root, the files were downloaded
				// as PRIV_USER, so switch to that priv level to do chmod
			priv_state saved_priv = set_priv( PRIV_USER );

			if (chmod(condor_basename(cmd.c_str()), 0755) == -1) {
				dprintf(D_ALWAYS,
				        "warning: unable to chmod %s to "
				            "ensure execute bit is set: %s\n",
				        condor_basename(cmd.c_str()),
				        strerror(errno));
			}

			set_priv( saved_priv );
		}
	}

	// We are done with input transfer, so record the sandbox content now
	// note that if setupCompleted() runs a PREPARE_JOB hook which modifies the sandbox
	// the post hook code will need to re-do this work, which currently it does not.
	std::string dummy;
	bool want_manifest = false;
	if( job_ad->LookupString( ATTR_JOB_MANIFEST_DIR, dummy ) ||
		(job_ad->LookupBool( ATTR_JOB_MANIFEST_DESIRED, want_manifest ) && want_manifest) ) {
		recordSandboxContents( "in" );
	}

	// Now that we're done, report successful setup to the base class which tells the starter.
	// This will either queue a prepare hook. or a queue a DEFERRAL timer to launch the job
	setupCompleted(0);

	return 1;
}


bool
JICShadow::getJobAdFromShadow( void )
{
		// Instantiate a new ClassAd for the job we'll be starting,
		// and get a copy of it from the shadow.
	if( job_ad ) {
		delete job_ad;
	}
    job_ad = new ClassAd;

	if( REMOTE_CONDOR_get_job_info(job_ad) < 0 ) {
		dprintf( D_ERROR,
				 "Failed to get job info from Shadow!\n" );
		return false;
	}
	return true;
}


void
JICShadow::initShadowInfo( ClassAd* ad )
{
	ASSERT( shadow );
	if( ! shadow->initFromClassAd(ad) ) { 
		dprintf( D_ALWAYS, 
				 "Failed to initialize shadow info from ClassAd!\n" );
		return;
	}
	dprintf( D_ALWAYS, "Communicating with shadow %s\n",
			 shadow->addr() );

	if( shadow_version ) {
		delete shadow_version;
		shadow_version = NULL;
	}
	const char* tmp = shadow->version();
	if( tmp ) {
		dprintf( D_FULLDEBUG, "Shadow version: %s\n", tmp );
		shadow_version = new CondorVersionInfo( tmp, "SHADOW" );
	} else {
		dprintf( D_FULLDEBUG, "Shadow version: unknown (pre v6.3.3)\n" ); 
	}
}



bool
JICShadow::initIOProxy( void )
{
	bool want_io_proxy = false;
	std::string io_proxy_config_file;

		// the admin should have the final say over whether
		// chirp is enabled
    bool enableIOProxy = true;
	enableIOProxy = param_boolean("ENABLE_CHIRP", true);

	bool enableUpdates = false;
	enableUpdates = param_boolean("ENABLE_CHIRP_UPDATES", true);

	bool enableFiles = false;
	enableFiles = param_boolean("ENABLE_CHIRP_IO", true);

	bool enableDelayed = false;
	enableDelayed = param_boolean("ENABLE_CHIRP_DELAYED", true);

	if (!enableIOProxy || (!enableUpdates && !enableFiles && !enableDelayed)) {
		dprintf(D_ALWAYS, "ENABLE_CHIRP is false in config file, not enabling chirp\n");
		return false;
	}

	if( ! job_ad->EvaluateAttrBool( ATTR_WANT_IO_PROXY, want_io_proxy ) ) {
		want_io_proxy = false;
		dprintf( D_FULLDEBUG, "JICShadow::initIOProxy(): "
				 "Job does not define %s; setting to false\n",
				 ATTR_WANT_IO_PROXY);
	} else {
		dprintf( D_ALWAYS, "Job has %s=%s\n", ATTR_WANT_IO_PROXY,
				 want_io_proxy ? "true" : "false" );
	}
	if (!enableFiles && want_io_proxy)
	{
		dprintf(D_ALWAYS, "Starter config prevents us from enabling IO proxy.\n");
		want_io_proxy = false;
	}
	bool want_updates = want_io_proxy;
	if ( ! job_ad->EvaluateAttrBool(ATTR_WANT_REMOTE_UPDATES, want_updates) ) {
		want_updates = want_io_proxy;
		dprintf(D_FULLDEBUG, "JICShadow::initIOProxy(): "
				"Job does not define %s; setting to %s.\n",
				ATTR_WANT_REMOTE_UPDATES, want_updates ? "true" : "false" );
	} else {
		dprintf(D_ALWAYS, "Job has %s=%s\n", ATTR_WANT_REMOTE_UPDATES,
				want_updates ? "true" : "false");
	}
	if (!enableUpdates && want_updates)
	{
		dprintf(D_ALWAYS, "Starter config prevents us from enabling remote updates.\n");
		want_updates = false;
	}
	bool want_delayed = true;
	if ( ! job_ad->EvaluateAttrBool(ATTR_WANT_DELAYED_UPDATES, want_delayed) ) {
		want_delayed = true;
		dprintf(D_FULLDEBUG, "JICShadow::initIOProxy(): "
				"Job does not define %s; enabling delayed updates.\n", ATTR_WANT_DELAYED_UPDATES);
	} else {
		dprintf(D_ALWAYS, "Job has %s=%s\n", ATTR_WANT_DELAYED_UPDATES,
				want_delayed ? "true" : "false");
	}
	if (!enableDelayed && want_delayed)
	{
		dprintf(D_ALWAYS, "Starter config prevents us from enabling delayed updates.\n");
		want_delayed = false;
	}
	dprintf(D_ALWAYS, "Chirp config summary: IO %s, Updates %s, Delayed updates %s.\n",
		want_io_proxy ? "true" : "false", want_updates ? "true" : "false",
		want_delayed ? "true" : "false");
	if( want_io_proxy || want_updates || want_delayed || job_universe==CONDOR_UNIVERSE_JAVA ) {
		m_wrote_chirp_config = true;
		condor_sockaddr *bindTo = NULL;
		struct in_addr addr;
		addr.s_addr = htonl(0xac110001);
		condor_sockaddr dockerInterface(addr);

		bool wantDocker = false;
		job_ad->LookupBool(ATTR_WANT_DOCKER, wantDocker);
		std::string dockerImage;
		job_ad->LookupString(ATTR_DOCKER_IMAGE, dockerImage);
		bool hasDockerImage = ! dockerImage.empty();
		if (wantDocker || hasDockerImage) {
			bindTo = &dockerInterface;
		}

		formatstr( io_proxy_config_file, "%s%c%s" ,
				 starter->GetWorkingDir(0), DIR_DELIM_CHAR, CHIRP_CONFIG_FILENAME );
		m_chirp_config_filename = io_proxy_config_file;
		dprintf(D_FULLDEBUG, "Initializing IO proxy with config file at %s.\n", io_proxy_config_file.c_str());
		if( !io_proxy.init(this, io_proxy_config_file.c_str(), want_io_proxy, want_updates, want_delayed, bindTo) ) {
			dprintf( D_ERROR,
					 "Couldn't initialize IO Proxy.\n" );
			return false;
		}
		dprintf( D_ALWAYS, "Initialized IO Proxy.\n" );
		return true;
	}
	return false;
}


bool
JICShadow::initUserCredentials() {

	// check to see if the job needs any OAuth services (scitokens)
	// if so, call the function that does that.
	std::string services_needed;
	if (job_ad->LookupString(ATTR_OAUTH_SERVICES_NEEDED, services_needed)) {
		dprintf(D_ALWAYS, "initUserCredentials: job needs OAuth services %s\n", services_needed.c_str());
		if ( ! refreshSandboxCredentialsOAuth()) {
			return false;
		}
	}

	// check to see if the job wants to have a kerberos credential.
	bool send_credential = false; // is there a KRB credential to fetch?
	if( ! job_ad->EvaluateAttrBool( ATTR_JOB_SEND_CREDENTIAL, send_credential ) ) {
		send_credential = false;
		dprintf(D_FULLDEBUG, "Job does not define " ATTR_JOB_SEND_CREDENTIAL "; Will not fetch kerberos credentials\n");
	} else {
		dprintf(D_ALWAYS, "Job has " ATTR_JOB_SEND_CREDENTIAL "=%s\n",
				 send_credential ? "true; Attempting to fetch Kerberos credentials" : "false" );
	}

	// no credentials needed?  no problem!  we're done here.
	if(!send_credential) {
		return true;
	}

	auto_free_ptr cred_dir_krb(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	if(!cred_dir_krb) {
		dprintf(D_ALWAYS, "ERROR: job has SendCredential=true but SEC_CREDENTIAL_DIRECTORY_KRB not defined!\n");
		return false;
	}

	// get username that was stored with InitUserIds
	const char * user = get_user_loginname();
#ifdef WIN32
	const char * domain = get_user_domainname();
#else
	const char * domain = uid_domain ? uid_domain : "DOMAIN";
#endif

	std::string fullusername(user);
	fullusername += "@";
	fullusername += domain;

	// remove mark on update for "mark and sweep"
	credmon_clear_mark(cred_dir_krb, user);

	// TODO: add domain to filename

	// check to see if .cc already exists
	std::string ccfilename;
	dircat(cred_dir_krb, user, ".cc", ccfilename);

	struct stat cred_stat_buf;
	int rc = stat(ccfilename.c_str(), &cred_stat_buf);

	// if the credential already exists, we should update it if
	// it's more than X seconds old.  if X is zero, we always
	// update it.  if X is negative, we never update it.
	int fresh_time = param_integer("SEC_CREDENTIAL_REFRESH_INTERVAL", -1);

	if (rc==0 && fresh_time < 0) {
		// if the credential cache already exists, we don't even need
		// to talk to the shadow.  just return success as quickly as
		// possible.
		//
		// before we do, copy the creds to the sandbox and initialize
		// the timer to monitor them.  propagate any errors.
		dprintf(D_FULLDEBUG, "CREDMON: credentials for user %s already exist in %s, and interval is %i\n",
			user, ccfilename.c_str(), fresh_time );

		rc = refreshSandboxCredentialsKRB();
		return rc;
	}

	// return success if the credential exists and has been recently
	// updated.  note that if fresh_time is zero, we'll never return
	// success here, meaning we will always update the credential.
	time_t now = time(NULL);
	if ((rc==0) && (now - cred_stat_buf.st_mtime < fresh_time)) {
		// was updated in the last X seconds, just copy existing to sandbox
		dprintf(D_FULLDEBUG, "CREDMON: credentials for user %s already exist in %s, and interval is %i\n",
			user, ccfilename.c_str(), fresh_time );

		rc = refreshSandboxCredentialsKRB();
		return rc;
	}

	dprintf(D_FULLDEBUG, "CREDMON: obtaining credentials for %s from shadow %s\n",
		fullusername.c_str(), shadow->addr() );

	unsigned char * cred = NULL;
	int credlen = 0;

	// if we don't know the shadow version, or if it is >= 8.9.7, use getUserCredential
	// if older than that use the backward compat hack where we fetch the 'password'
	// and then base64 decode it to get the krb credential.  the backward compat
	// mode only works if the shadow is not Windows, and there is a credmon running on the schedd.
	CondorVersionInfo ver(shadow->version());
	if (ver.getMajorVer() < 8 || ver.built_since_version(8, 9, 7)) {
		if (! shadow->getUserCredential(user, domain, STORE_CRED_USER_KRB, cred, credlen)) {
			dprintf(D_ALWAYS, "getUserCredential failed to get KRB cred from the shadow\n");
			return false;
		}
	} else {
		std::string credential;
		if ( ! shadow->getUserPassword(user, domain, credential)) {
			dprintf(D_ALWAYS, "getUserPassword failed to get KRB cred from the shadow\n");
			return false;
		}

		credlen = -1;
		zkm_base64_decode(credential.c_str(), &cred, &credlen);
	}
	dprintf(D_FULLDEBUG, "CREDMON: got a cred of size %d bytes\n", credlen);

	std::string ccfile;
	long long res = store_cred_blob(fullusername.c_str(), ADD_KRB_MODE, cred, credlen, NULL, ccfile);
	SecureZeroMemory(cred, credlen);
	free(cred);

	if (store_cred_failed(res, ADD_KRB_MODE)) {
		dprintf(D_ALWAYS, "CREDMON: credmon failed to store the KRB credential locally\n");
		return false;
	}

	int timeout = param_integer("CREDD_POLLING_TIMEOUT", 20);
	rc = credmon_kick_and_poll_for_ccfile(credmon_type_KRB, ccfile.c_str(), timeout);
	if(!rc) {
		dprintf(D_ALWAYS, "CREDMON: credmon failed to produce file : %s\n", ccfile.c_str());
		return false;
	}

	// this will set up the credentials in the sandbox and set a timer to
	// do it periodically.  propagate any errors.
	rc = refreshSandboxCredentialsKRB();

	return rc;
}

#if 1 //ndef WIN32
bool
JICShadow::refreshSandboxCredentialsKRB()
{
	/*
	  This method is invoked whenever we should check
	  the user credential in SEC_CREDENTIAL_DIRECTORY_KRB and
	  then, if needed, copy them to the job sandbox.
	*/

	dprintf(D_FULLDEBUG, "CREDS: in refreshSandboxCredentialsKRB()\n");

	// poor, abuse return code.  used for booleans and syscalls, with
	// opposite meanings.  assume failure.
	int rc = false;

	// the buffer
	char  *ccbuf = 0;
	size_t cclen = 0;

	std::string ccfile;
	std::string sandboxccfile;
	const char * ccfilename = NULL;
	const char * sandboxccfilename = NULL;

	// get username
	const char * user = get_user_loginname();

	// declaring at top since we use goto for error handling
	priv_state priv;

	// construct filename to stat
	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY_KRB");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: in refreshSandboxCredentialsKRB() but SEC_CREDENTIAL_DIRECTORY_KRB not defined!\n");
		rc = false;
		goto resettimer;
	}

	// stat the file
	ccfilename = dircat(cred_dir, user, ".cc", ccfile);

	struct stat syscred;
	priv = set_root_priv();
	rc = stat(ccfilename, &syscred);
	set_priv(priv);
	if (rc!=0) {
		dprintf(D_ALWAYS, "ERROR: in refreshSandboxCredentialsKRB() but %s is gone!\n", ccfilename );
		rc = false;
		goto resettimer;
	}

	// has it been updated?
	if (memcmp(&m_sandbox_creds_last_update, &syscred.st_mtime, sizeof(time_t)) == 0) {
		// no update?  no problem, we'll check again later
		rc = true;
		goto resettimer;
	}

	// read entire ccfilename as root into ccbuf
	if (!read_secure_file(ccfilename, (void**)(&ccbuf), &cclen, true)) {
		dprintf(D_ALWAYS, "ERROR: read_secure_file(%s,ccbuf,%zu) failed\n", ccfilename, cclen);
		rc = false;
		goto resettimer;
	}

	// if it has been updated, we need to deal with it.
	//
	// securely copy the cc to sandbox.
	//
	sandboxccfilename = dircat(starter->GetWorkingDir(0), user, ".cc", sandboxccfile);

	// as user, write tmp file securely
	priv = set_user_priv();
	rc = replace_secure_file(sandboxccfilename, ".tmp", ccbuf, cclen, false);
	set_priv(priv);
	if (!rc) {
		// replace_secure_file already logged this failure
		goto resettimer;
	}

	// aklog now if we decide to go that route
	// my_popen_env("aklog", KRB5CCNAME=sandbox copy of .cc)

	// store the mtime of the current copy
	memcpy(&m_sandbox_creds_last_update, &syscred.st_mtime, sizeof(time_t));

	// only need to do this once
	if ( ! getKrb5CCName()) {
		dprintf(D_ALWAYS, "CREDS: configuring job to use KRB5CCNAME %s\n", sandboxccfilename);
		setKrb5CCName(sandboxccfilename);
	}

	// if we made it here, we succeeded!
	rc = true;

resettimer:
	if(ccbuf) {
		free(ccbuf);
		ccbuf = NULL;
	}

	// rc at this point should be: true==SUCCESS, or false==FAILURE

	// set/reset timer
	int sec_cred_refresh = param_integer("SEC_CREDENTIAL_REFRESH", 300);
	if (sec_cred_refresh > 0) {
		if (m_refresh_sandbox_creds_tid == -1) {
			m_refresh_sandbox_creds_tid = daemonCore->Register_Timer(
				sec_cred_refresh,
				(TimerHandlercpp)&JICShadow::refreshSandboxCredentialsKRB_from_timer,
				"refreshSandboxCredentialsKRB",
				this );
		} else {
			daemonCore->Reset_Timer(m_refresh_sandbox_creds_tid, sec_cred_refresh);
		}
		dprintf(D_ALWAYS,
			"CREDS: will check credential again in %i seconds\n", sec_cred_refresh);
	} else {
		dprintf(D_ALWAYS, "CREDS: cred refresh is DISABLED.\n");
	}

	// return boolean value true on success
	return (rc);
}

bool
JICShadow::refreshSandboxCredentialsOAuth()
{
	/*
	  This method is invoked whenever we should fetch fresh credentials
	  from the shadow and then update them in the job sandbox.
	*/
	dprintf(D_ALWAYS, "CREDS: in refreshSandboxCredentialsMultiple()\n");

	// set/reset timer
	int sec_cred_refresh = param_integer("SEC_CREDENTIAL_REFRESH", 300);
	if (sec_cred_refresh > 0) {
		if (m_refresh_sandbox_creds_tid == -1) {
			m_refresh_sandbox_creds_tid = daemonCore->Register_Timer(
				sec_cred_refresh,
				(TimerHandlercpp)&JICShadow::refreshSandboxCredentialsOAuth_from_timer,
				"refreshSandboxCredentialsOAuth",
				this );
		} else {
			daemonCore->Reset_Timer(m_refresh_sandbox_creds_tid, sec_cred_refresh);
		}
		dprintf(D_SECURITY,
			"CREDS: will refresh credentials again in %i seconds\n", sec_cred_refresh);
	} else {
		dprintf(D_SECURITY, "CREDS: cred refresh is DISABLED.\n");
	}

	// setup .condor_creds directory in sandbox (may already exist).
	std::string sandbox_dir_name;
	dircat(starter->GetWorkingDir(0), ".condor_creds", sandbox_dir_name);

	ShadowCredDirCreator creds(*job_ad, sandbox_dir_name);
	CondorError err;
	if (!creds.GetCreds(err) || !creds.PrepareCredDir(err)) {
		return false;
	}

	// only need to do this once
	if( ! getCredPath()) {
		dprintf(D_SECURITY, "CREDS: setting env _CONDOR_CREDS to %s\n", sandbox_dir_name.c_str());
		setCredPath(sandbox_dir_name.c_str());
	}

	return true;
}
#endif  // WIN32

void
JICShadow::initMatchSecuritySession()
{
	if( !param_boolean("SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true) ) {
		return;
	}

	if( shadow_version && !shadow_version->built_since_version(7,1,3) ) {
		return;
	}

	std::string reconnect_session_id;
	std::string reconnect_session_info;
	std::string reconnect_session_key;

	std::string filetrans_session_id;
	std::string filetrans_session_info;
	std::string filetrans_session_key;

		// For possible future use.  We could set security session
		// options here if we wanted to get an effect similar to
		// security negotiation, but currently we just take the
		// defaults from the shadow.
	const char *starter_reconnect_session_info = "";
	const char *starter_filetrans_session_info = "";

	int rc = REMOTE_CONDOR_get_sec_session_info(
		starter_reconnect_session_info,
		reconnect_session_id,
		reconnect_session_info,
		reconnect_session_key,
		starter_filetrans_session_info,
		filetrans_session_id,
		filetrans_session_info,
		filetrans_session_key);

	if( rc < 0 ) {
		dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to get security "
				"session info from shadow.  Skipping creation of match "
				"security session.\n");
		return;
	}

	if( m_reconnect_sec_session ) {
			// Already have one (must be reconnecting with shadow).
			// Continue using the one we have.  We cannot destroy it
			// and create a new one, because the syscall socket is
			// already using the session key from it.
			// If a new DCShadow object was created, ensure it's using
			// the session.
		shadow->setSecSessionId(m_reconnect_sec_session);
	}
	else if( reconnect_session_id.length() ) {
		rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
			WRITE,
			reconnect_session_id.c_str(),
			reconnect_session_key.c_str(),
			reconnect_session_info.c_str(),
			AUTH_METHOD_MATCH,
			SUBMIT_SIDE_MATCHSESSION_FQU,
			NULL,
			0 /*don't expire*/,
			nullptr, false );

		if( !rc ) {
			dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create "
					"reconnect security session.  "
					"Will fall back on auto-negotiated session instead.\n");
		}
		else {
			m_reconnect_sec_session = strdup(reconnect_session_id.c_str());
			shadow->setSecSessionId(m_reconnect_sec_session);
		}
	}

	IpVerify* ipv = daemonCore->getIpVerify();
	ipv->PunchHole( DAEMON, SUBMIT_SIDE_MATCHSESSION_FQU );
	ipv->PunchHole( CLIENT_PERM, SUBMIT_SIDE_MATCHSESSION_FQU );

	if( m_filetrans_sec_session ) {
			// Already have one (must be reconnecting with shadow).
			// Destroy it and create a new one in its place, since
			// the shadow may have changed how it wants this to be
			// set up.
		daemonCore->getSecMan()->invalidateKey( m_filetrans_sec_session );
		free( m_filetrans_sec_session );
		m_filetrans_sec_session = NULL;
	}

	if( filetrans_session_id.length() ) {
		rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
			WRITE,
			filetrans_session_id.c_str(),
			filetrans_session_key.c_str(),
			filetrans_session_info.c_str(),
			AUTH_METHOD_MATCH,
			SUBMIT_SIDE_MATCHSESSION_FQU,
			shadow->addr(),
			0 /*don't expire*/,
			nullptr, false );

		if( !rc ) {
			dprintf(D_ALWAYS, "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION: failed to create file "
					"transfer security session.  "
					"Will fall back on auto-negotiated session instead.\n");
		}
		else {
			m_filetrans_sec_session = strdup(filetrans_session_id.c_str());
		}
	}
}

bool
JICShadow::receiveMachineAd( Stream *stream )
{
        bool ret_val = true;

	dprintf(D_FULLDEBUG, "Entering JICShadow::receiveMachineAd\n");
	mach_ad = new ClassAd();

	stream->decode();
	if (!getClassAd(stream, *mach_ad))
	{
		dprintf(D_ALWAYS, "Received invalid machine ad.  Discarding\n");
		delete mach_ad;
		mach_ad = NULL;
		ret_val = false;
	}
	else
	{
		dPrintAd(D_JOB, *mach_ad);
	}

	return ret_val;
}

bool JICShadow::receiveExecutionOverlayAd(Stream* stream)
{
	bool ret_val = true;

	if (job_execution_overlay_ad) {
		delete job_execution_overlay_ad;
	}
	job_execution_overlay_ad = new ClassAd();

	if (!getClassAd(stream, *job_execution_overlay_ad))
	{
		delete job_execution_overlay_ad; job_execution_overlay_ad = nullptr;
		dprintf(D_ALWAYS, "Received invalid Execution Overlay Ad.  Discarding\n");
		return false;
	}
	else
	{
		if ((job_execution_overlay_ad->size() > 0) && IsDebugLevel(D_JOB)) {
			std::string adbuf;
			dprintf(D_JOB, "Received Execution Overlay Ad:\n%s", formatAd(adbuf, *job_execution_overlay_ad, "\t"));
		} else {
			dprintf(D_FULLDEBUG, "Received Execution Overlay Ad (%d attributes)\n", (int)job_execution_overlay_ad->size());
		}
		if (job_execution_overlay_ad->size() == 0) {
			delete job_execution_overlay_ad; job_execution_overlay_ad = nullptr;
		}
	}
	return ret_val;
}

#if !defined(WINDOWS)
void
JICShadow::recordSandboxContents( const char * filename ) {

	FILE * file = starter->OpenManifestFile(filename);
	if( file == NULL ) {
		dprintf( D_ALWAYS, "recordSandboxContents(%s): failed to open manifest file : %d (%s)\n",
			filename, errno, strerror(errno) );
		return;
	}

    // The execute directory is now owned by the user and mode 0700 by default.
	TemporaryPrivSentry sentry(PRIV_USER);
	if ( get_priv_state() != PRIV_USER ) {
		dprintf( D_ERROR, "JICShadow::recordSandboxContents(%s): failed to switch to PRIV_USER\n", filename );
		return;
	}

	// The starter's CWD should only ever temporarily not be the job
	// sandbox directory, and this code shouldn't ever be called in
	// the middle of any of those temporaries, but as long as we're
	// copying from OpenManifestFile(), let's do everything right.
	std::string errMsg;
	TmpDir tmpDir;
	if (!tmpDir.Cd2TmpDir(starter->GetWorkingDir(0),errMsg)) {
		dprintf( D_ERROR, "OpenManifestFile(%s): failed to cd to job sandbox %s\n",
			filename, starter->GetWorkingDir(0));
		fclose(file);
		return;
	}

	DIR * dir = opendir(".");
	if( dir == NULL ) {
		dprintf( D_ALWAYS, "recordSandboxContents(%s): failed to open sandbox directory: %d (%s)\n",
			filename, errno, strerror(errno) );
		fclose(file);
		return;
	}

	struct dirent * e;
	while( (e = readdir(dir)) != NULL ) {
		if( strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 ) { continue; }
		fprintf( file, "%s\n", e->d_name );
	}
	closedir(dir);
	fclose(file);
}
#else
void
JICShadow::recordSandboxContents( const char * filename ) {
	ASSERT(filename != NULL);
}
#endif


//
// We could exclude everything in this function, except m_removed_output_files,
// between finishing input transfer and starting the job, but since we need
// to handle m_removed_output_files for both checkpointing and "normal"
// output transfer, and we want to make sure that code is and stays identical,
// we might as well eliminate the chance for any semantic weirdness in the
// non-checkpointing case but excluding this files at the same time we
// always have.
//
void
JICShadow::_remove_files_from_output() {
	// If we've excluded or removed a file since input transfer.
	for( const auto & filename : m_removed_output_files ) {
		filetrans->addFileToExceptionList( filename.c_str() );
	}

	// Make sure that we've excluded the files we always exclude.
	for( const auto & filename : ALWAYS_EXCLUDED_FILES ) {
		filetrans->addFileToExceptionList( filename.c_str() );
	}

	// Don't transfer the chirp config file.
	if( m_wrote_chirp_config ) {
		filetrans->addFileToExceptionList( CHIRP_CONFIG_FILENAME );
	}

	// Don't transfer the starter log if it wasn't requested.
	if( job_ad->Lookup(ATTR_JOB_STARTER_DEBUG) ) {
		if(! job_ad->Lookup(ATTR_JOB_STARTER_LOG)) {
			filetrans->addFileToExceptionList( SANDBOX_STARTER_LOG_FILENAME );
		}
		filetrans->addFileToExceptionList( SANDBOX_STARTER_LOG_FILENAME ".old" );
	}
}
