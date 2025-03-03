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
#include "condor_config.h"
#include "starter.h"
#include "script_proc.h"
#include "vanilla_proc.h"
#include "docker_proc.h"
#include "remote_proc.h"
#include "java_proc.h"
#include "tool_daemon_proc.h"
#include "parallel_proc.h"
#include "vm_proc.h"
#include "my_hostname.h"
#include "internet.h"
#include "util_lib_proto.h"
#include "condor_attributes.h"
#include "classad_command_util.h"
#include "condor_random_num.h"
#include "../condor_sysapi/sysapi.h"
#include "build_job_env.h"
#include "get_port_range.h"
#include "perm.h"
#include "filename_tools.h"
#include "directory.h"
#include "tmp_dir.h"
#include "exit.h"
#include "setenv.h"
#include "condor_claimid_parser.h"
#include "condor_version.h"
#include "sshd_proc.h"
#include "condor_base64.h"
#include "my_username.h"
#include "condor_regex.h"
#include "starter_util.h"
#ifdef HAVE_DATA_REUSE_DIR
#include "data_reuse.h"
#endif
#include "authentication.h"
#include "to_string_si_units.h"
#include "guidance.h"
#include "dc_coroutines.h"

extern void main_shutdown_fast();

const char* JOB_AD_FILENAME = ".job.ad";
const char* JOB_EXECUTION_OVERLAY_AD_FILENAME = ".execution_overlay.ad";
const char* MACHINE_AD_FILENAME = ".machine.ad";
extern const char* JOB_WRAPPER_FAILURE_FILE;

#ifdef WIN32
// Note inversion of argument order...
#define realpath(path,resolved_path) _fullpath((resolved_path),(path),_MAX_PATH)
#endif


/* Starter class implementation */

Starter::Starter() : 
	jic(nullptr),
	m_deferred_job_update(false),
	job_exit_status(0),
	dirMonitor(nullptr),
	jobUniverse(CONDOR_UNIVERSE_VANILLA),
	Execute(nullptr),
	orig_cwd(nullptr),
	is_gridshell(false),
	m_workingDirExists(false),
	has_encrypted_working_dir(false),
	ShuttingDown(FALSE),
	starter_stdin_fd(-1),
	starter_stdout_fd(-1),
	starter_stderr_fd(-1),
	suspended(false),
	deferral_tid(-1),
	pre_script(nullptr),
	post_script(nullptr),
	m_configured(false),
	m_job_environment_is_ready(false),
	m_all_jobs_done(false),
	m_shutdown_exit_code(STARTER_EXIT_NORMAL)
{
}


Starter::~Starter()
{
	if( Execute ) {
		free(Execute);
	}
	if( orig_cwd ) {
		free(orig_cwd);
	}
	if( jic ) {
		delete jic;
	}
	if( pre_script ) {
		delete( pre_script );
	}
	if( post_script ) {
		delete( post_script );
	}
	if( dirMonitor ) {
		delete( dirMonitor );
	}
}


bool
Starter::Init( JobInfoCommunicator* my_jic, const char* original_cwd,
				bool is_gsh, int stdin_fd, int stdout_fd, 
				int stderr_fd )
{
	if( ! my_jic ) {
		EXCEPT( "Starter::Init() called with no JobInfoCommunicator!" ); 
	}
	if( jic ) {
		delete( jic );
	}
	jic = my_jic;

	if( original_cwd ) {
		this->orig_cwd = strdup( original_cwd );
	}
	this->is_gridshell = is_gsh;
	starter_stdin_fd = stdin_fd;
	starter_stdout_fd = stdout_fd;
	starter_stderr_fd = stderr_fd;

	Config();


		// Now that we know what Execute is, we can figure out what
		// directory the starter will be working in and save that,
		// since we'll want this info a lot while we initialize and
		// figure things out.

	if( is_gridshell ) {
			// For now, the gridshell doesn't need its own special
			// scratch directory, we're just going to use whatever
			// EXECUTE is, or our CWD if that's not defined...
		WorkingDir = Execute;
		m_workingDirExists = true;
	} else {
		formatstr( WorkingDir, "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
				 (long)daemonCore->getpid() );
	}

		//
		// We have switched all of these to call the "Remote"
		// version of the appropriate method. The difference between the 
		// "Remote" versions and the regular version is that the
		// the JIC is only notified of an action in the "Remote" methods
		// The Remote versions will call the regular method after
		// calling the appropriate JIC method
		//
	daemonCore->Register_Signal(DC_SIGSUSPEND, "DC_SIGSUSPEND", 
		(SignalHandlercpp)&Starter::RemoteSuspend, "RemoteSuspend",
		this);
	daemonCore->Register_Signal(DC_SIGCONTINUE, "DC_SIGCONTINUE",
		(SignalHandlercpp)&Starter::RemoteContinue, "RemoteContinue",
		this);
	daemonCore->Register_Signal(DC_SIGHARDKILL, "DC_SIGHARDKILL",
		(SignalHandlercpp)&Starter::RemoteShutdownFast, "RemoteShutdownFast",
		this);
	daemonCore->Register_Signal(DC_SIGSOFTKILL, "DC_SIGSOFTKILL",
		(SignalHandlercpp)&Starter::RemoteShutdownGraceful, "RemoteShutdownGraceful",
		this);
	daemonCore->Register_Signal(DC_SIGPCKPT, "DC_SIGPCKPT",
		(SignalHandlercpp)&Starter::RemotePeriodicCkpt, "RemotePeriodicCkpt",
		this);
	daemonCore->Register_Signal(DC_SIGREMOVE, "DC_SIGREMOVE",
		(SignalHandlercpp)&Starter::RemoteRemove, "RemoteRemove",
		this);
	daemonCore->Register_Signal(SIGUSR1, "SIGUSR1",
		(SignalHandlercpp)&Starter::RemoteRemove, "RemoteRemove",
		this);
	daemonCore->Register_Signal(DC_SIGHOLD, "DC_SIGHOLD",
		(SignalHandlercpp)&Starter::RemoteHold, "RemoteHold",
		this);
	daemonCore->Register_Reaper("Reaper", (ReaperHandlercpp)&Starter::Reaper,
		"Reaper", this);

		// Register a command with DaemonCore to handle ClassAd-only
		// protocol commands.  For now, the only one we care about is
		// CA_RECONNECT_JOB, which is used if we get disconnected and
		// a new shadow comes back to try to reconnect, we're willing
		// to talk to them...  Arguably, that's shadow-specific, and
		// therefore it should live in the JICShadow, not here.
		// However, a) we might support other ClassAd-only commands in
		// the future that make more sense with non-shadow JIC's and
		// b) someday even CA_RECONNECT_JOB might have some meaning
		// for COD, who knows...
	daemonCore->
		Register_Command( CA_CMD, "CA_CMD",
						  (CommandHandlercpp)&Starter::classadCommand,
						  "Starter::classadCommand", this, WRITE );
	daemonCore->
		Register_Command( UPDATE_GSI_CRED, "UPDATE_GSI_CRED",
						  (CommandHandlercpp)&Starter::updateX509Proxy,
						  "Starter::updateX509Proxy", this, WRITE );
	daemonCore->
		Register_Command( DELEGATE_GSI_CRED_STARTER,
						  "DELEGATE_GSI_CRED_STARTER",
						  (CommandHandlercpp)&Starter::updateX509Proxy,
						  "Starter::updateX509Proxy", this, WRITE );
	daemonCore->
		Register_Command( STARTER_HOLD_JOB,
						  "STARTER_HOLD_JOB",
						  (CommandHandlercpp)&Starter::remoteHoldCommand,
						  "Starter::remoteHoldCommand", this, DAEMON );
	daemonCore->
		Register_Command( CREATE_JOB_OWNER_SEC_SESSION,
						  "CREATE_JOB_OWNER_SEC_SESSION",
						  (CommandHandlercpp)&Starter::createJobOwnerSecSession,
						  "Starter::createJobOwnerSecSession", this, DAEMON );

		// Job-owner commands are registered at READ authorization level.
		// Why?  See the explanation in createJobOwnerSecSession().
	daemonCore->
		Register_Command( START_SSHD,
						  "START_SSHD",
						  (CommandHandlercpp)&Starter::startSSHD,
						  "Starter::startSSHD", this, READ );
	daemonCore->
		Register_Command( STARTER_PEEK,
						"STARTER_PEEK",
						(CommandHandlercpp)&Starter::peek,
						"Starter::peek", this, READ );

		// initialize our JobInfoCommunicator
	if( ! jic->init() ) {
		dprintf( D_ALWAYS, 
				 "Failed to initialize JobInfoCommunicator, aborting\n" );
		return false;
	}

	// jic already assumed to be nonzero above
	//
	// also, the jic->init call above has set up the user priv state via
	// initUserPriv, so we can safely use it now, and we should so that we can
	// actually do this in the user-owned execute dir.
	priv_state rl_p = set_user_priv();
	sysapi_set_resource_limits(jic->getStackSize());
	set_priv (rl_p);

		// Now, ask our JobInfoCommunicator to setup the environment
		// where our job is going to execute.  This might include
		// doing file transfer stuff, who knows.  Whenever the JIC is
		// done, it'll call our jobEnvironmentReady() method so we can
		// actually spawn the job.
	jic->setupJobEnvironment();
	return true;
}


void
Starter::StarterExit( int code )
{
	code = FinalCleanup(code);
	if (job_requests_broken_exit && code == STARTER_EXIT_NORMAL){
		code = STARTER_EXIT_BROKEN_BY_REQUEST;
		dprintf(D_STATUS, "Job requested a broken exit code, setting code to %d\n", code);
	}
	// Once libc starts calling global destructors, we can't reliably
	// notify anyone of an EXCEPT().
	_EXCEPT_Cleanup = NULL;
	DC_Exit( code );
}

int Starter::FinalCleanup(int code)
{
#if defined(LINUX)
		// Not useful to have the volume management code trigger
		// while we are trying to cleanup.
	if (m_lvm_poll_tid >= 0) {
		daemonCore->Cancel_Timer(m_lvm_poll_tid);
	}
#endif

	RemoveRecoveryFile();
	if ( ! removeTempExecuteDir(code)) {
		if (code == STARTER_EXIT_NORMAL) {
		#ifdef WIN32
			// bit of a hack for testing purposes
			if (param_true("TREAT_REMOVE_EXEC_DIR_FAIL_AS_LV_FAIL")) {
				code = STARTER_EXIT_IMMORTAL_LVM;
			}
		#endif
		}
	}
#ifdef WIN32
	/* If we loaded the user's profile, then we should dump it now */
	if (m_owner_profile.loaded()) {
		m_owner_profile.unload ();
		// TODO: figure out how we can avoid doing this..
		m_owner_profile.destroy ();
	}
#endif
	return code;
}


void
Starter::Config()
{
	if( Execute ) {
		free( Execute );
	}
		// NOTE: We do not param for SLOTx_EXECUTE here, because it is
		// the startd's job to set _CONDOR_EXECUTE in our environment
		// and point that to the appropriate path.
	if( (Execute = param("EXECUTE")) == NULL ) {
		if( is_gridshell ) {
			Execute = strdup( orig_cwd );
		} else {
			EXCEPT("Execute directory not specified in config file.");
		}
	}

#ifdef HAVE_DATA_REUSE_DIR
	std::string reuse_dir;
	if (param(reuse_dir, "DATA_REUSE_DIRECTORY")) {
		if (!m_reuse_dir.get() || (m_reuse_dir->GetDirectory() != reuse_dir)) {
			m_reuse_dir.reset(new htcondor::DataReuseDirectory(reuse_dir, false));
		}
		m_reuse_dir->PrintInfo(true);
	} else {
		m_reuse_dir.reset();
	}
#endif

		// Tell our JobInfoCommunicator to reconfig, too.
	jic->config();

	m_configured = true;
}

/**
 * DC Shutdown Graceful Wrapper
 * We notify our JIC that we got a shutdown graceful call
 * then invoke ShutdownGraceful() which does the real work
 * 
 * @param command (not used)
 * @return true if ????, otherwise false
 */ 
int
Starter::RemoteShutdownGraceful( int )
{
	bool graceful_in_progress = false;

		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	if ( jic ) {
		graceful_in_progress = jic->isGracefulShutdown();
		jic->gotShutdownGraceful();
	}
	if ( graceful_in_progress == false ) {
		return ( this->ShutdownGraceful( ) );
	}
	else {
		return ( false );
	}
}

/**
 * Tells all the job's in the job queue to shutdown gracefully
 * If a job has already been shutdown, then we will remove it from
 * the job queue. If a job is being deferred, we will just call
 * removeDeferredJobs() to clean up any timers we may have set.
 * 
 * @return true if there are no jobs running and we can shutdown now
 */
bool
Starter::ShutdownGraceful( void )
{
	bool jobRunning = false;

	dprintf(D_ALWAYS, "ShutdownGraceful all jobs.\n");

		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	auto listit = m_job_list.begin();
	while (listit != m_job_list.end()) {
		auto *job = *listit;
		if ( job->ShutdownGraceful() ) {
			// job is completely shut down, so delete it
			listit = m_job_list.erase(listit);
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
			listit++;
		}
	}
	ShuttingDown = TRUE;
	if (!jobRunning) {
		dprintf(D_FULLDEBUG,
				"Got ShutdownGraceful when no jobs running.\n");
		return ( this->allJobsDone() );
	}
	return 0;
}

/**
 * DC Shutdown Fast Wrapper
 * We notify our JIC that we got a shutdown fast call
 * then invoke ShutdownFast() which does the real work
 *
 * @param command (not used)
 * @return true if ????, otherwise false
 */
int
Starter::RemoteShutdownFast(int)
{
	bool fast_in_progress = false;

		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions (for example, disabiling file transfer if
		// we're talking to a shadow)
	if( jic ) {
		fast_in_progress = jic->isFastShutdown();
		jic->gotShutdownFast();
	}
	if( fast_in_progress == false ) {
		return ( this->ShutdownFast( ) );
	}
	else {
		return ( false );
	}
}

/**
 * Tells all the job's in the job queue to shutdown now!
 * If a job has already been shutdown, then we will remove it from
 * the job queue. If a job is being deferred, we will just call
 * removeDeferredJobs() to clean up any timers we may have set.
 * 
 * @return true if there are no jobs running and we can shutdown now
 */
bool
Starter::ShutdownFast( void )
{
	bool jobRunning = false;

	dprintf(D_ALWAYS, "ShutdownFast all jobs.\n");
	
		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	auto listit = m_job_list.begin();
	while (listit != m_job_list.end()) {
		UserProc *job = *listit;;
		if ( job->ShutdownFast() ) {
			// job is completely shut down, so delete it
			listit = m_job_list.erase(listit);
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
			listit++;
		}
	}
	ShuttingDown = TRUE;
	if (!jobRunning) {
		dprintf(D_FULLDEBUG,
				"Got ShutdownFast when no jobs running.\n");
		return ( this->allJobsDone() );
	}
	return ( false );
}

/**
 * DC Remove Job Wrapper
 * We notify our JIC that we got a remove call
 * then invoke Remove() which does the real work
 *
 * @param command (not used)
 * @return true if ????, otherwise false
 */ 
int
Starter::RemoteRemove( int )
{
		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	if( jic ) {
		jic->gotRemove();
	}
		//
		// Like the RemoteHold() call, if we asked to remove jobs
		// and there were no jobs running, then we need to let ourselves
		// know that all hte jobs are gone
		//
	if ( this->Remove( ) ) {
		dprintf( D_FULLDEBUG, "Got Remove when no jobs running\n" );
		this->allJobsDone();
		return ( true );
	}
	return ( false );
}

/**
 * Attempts to remove all the jobs from the job queue
 * If a job has already been removed, that is if job->Remove() returns
 * true, then we know that it is safe to remove it from the job queue.
 * If a job is being deferred, we will just call removeDeferredJobs()
 * to clean up any timers we may have set.
 * 
 * @return true if all the jobs have been removed
 */
bool
Starter::Remove( ) {
	bool jobRunning = false;

	dprintf( D_ALWAYS, "Remove all jobs\n" );

		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	auto listit = m_job_list.begin();
	while (listit != m_job_list.end()) {
		UserProc *job = *listit;
		if( job->Remove() ) {
			// job is completely shut down, so delete it
			listit = m_job_list.erase(listit);
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
			listit++;
		}
	}
	ShuttingDown = TRUE;

	return ( !jobRunning );
}

/**
 * DC Hold Job Wrapper
 * We notify our JIC that we got a hold call
 * then invoke Hold() which does the real work
 * 
 * @param command (not used)
 * @return true if ????, otherwise false
 */ 
int
Starter::RemoteHold( int )
{
		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	if( jic ) {
		jic->gotHold();
	}
		//
		// If this method returns true, that means there were no
		// jobs running. We'll send out a little debug message 
		// and let ourselves know that all the jobs are done
		// Note that this used to be in the Hold() call but
		// this causes problems if the Hold is coming from ourself
		// For instance, if the OnExitHold evaluates to true for
		// a local universe job
		//
	if ( this->Hold( ) ) {
		dprintf( D_FULLDEBUG, "Got Hold when no jobs running\n" );
		this->allJobsDone();
		return ( true );
	}
	return ( false );
}

int
Starter::createJobOwnerSecSession( int /*cmd*/, Stream* s )
{
		// A Condor daemon on the submit side (e.g. schedd) wishes to
		// create a security session for use by a user
		// (e.g. condor_rsh_to_job).  Why doesn't the user's tool just
		// connect and create a security session directly?  Because
		// the starter is not necessarily set up to authenticate the
		// owner of the job.  So the schedd authenticates the owner of
		// the job and facilitates the creation of a security session
		// between the starter and the tool.

	std::string fqu;
	getJobOwnerFQUOrDummy(fqu);
	ASSERT( !fqu.empty() );

	std::string error_msg;
	ClassAd input;
	s->decode();
	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to read request in createJobOwnerSecSession()\n");
		return FALSE;
	}

		// In order to ensure that we are really talking to the schedd
		// that is managing this job, check that the schedd has provided
		// the correct secret claim id.
	std::string job_claim_id;
	std::string input_claim_id;
	getJobClaimId(job_claim_id);
	input.LookupString(ATTR_CLAIM_ID,input_claim_id);
	if( job_claim_id != input_claim_id || job_claim_id.empty() ) {
		dprintf(D_ALWAYS,
				"Claim ID provided to createJobOwnerSecSession does not match "
				"expected value!  Rejecting connection from %s\n",
				s->peer_description());
		return FALSE;
	}

	char *session_id = Condor_Crypt_Base::randomHexKey();
	char *session_key = Condor_Crypt_Base::randomHexKey(SEC_SESSION_KEY_LENGTH_V9);

	std::string session_info;
	input.LookupString(ATTR_SESSION_INFO,session_info);

		// Create an authorization rule so that the job owner can
		// access commands that the job owner should be able to
		// access.  All such commands are registered at READ level.
		// Why?  Because all such commands do a special check to make
		// sure the authenticated name matches the job owner name.
		// Therefore, to give the job owner as little power as
		// necessary, these commands are registered as READ (rather
		// than something more powerful such as DAEMON) and we leave
		// it up to the job-owner commands themselves to be more
		// selective about who gets to successfully run the command.
		// Ideally, we would have a first-class OWNER access level
		// instead.

	IpVerify* ipv = daemonCore->getSecMan()->getIpVerify();
	bool rc = ipv->PunchHole(READ, fqu);
	if( !rc ) {
		error_msg = "Starter failed to create authorization entry for job owner.";
	}

	if( rc ) {
		rc = daemonCore->getSecMan()->CreateNonNegotiatedSecuritySession(
			READ,
			session_id,
			session_key,
			session_info.c_str(),
			AUTH_METHOD_MATCH,
			fqu.c_str(),
			NULL,
			0,
			nullptr, true );
	}
	if( rc ) {
			// get the final session parameters that were chosen
		rc = daemonCore->getSecMan()->ExportSecSessionInfo(
			session_id,
			session_info );
	}

	ClassAd response;
	response.Assign(ATTR_VERSION,CondorVersion());
	if( !rc ) {
		if( error_msg.empty() ) {
			error_msg = "Failed to create security session.";
		}
		response.Assign(ATTR_RESULT,false);
		response.Assign(ATTR_ERROR_STRING,error_msg);
		dprintf(D_ALWAYS,
				"createJobOwnerSecSession failed: %s\n", error_msg.c_str());
	}
	else {
		// We use a "claim id" string to hold the security session info,
		// because it is a convenient container.

		ClaimIdParser claimid(session_id,session_info.c_str(),session_key);
		response.Assign(ATTR_RESULT,true);
		response.Assign(ATTR_CLAIM_ID,claimid.claimId());
		response.Assign(ATTR_STARTER_IP_ADDR,daemonCore->publicNetworkIpAddr());

		dprintf(D_FULLDEBUG,"Created security session for job owner (%s).\n",
				fqu.c_str());
	}

	if( !putClassAd(s, response) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,
				"createJobOwnerSecSession failed to send response\n");
	}

	free( session_id );
	free( session_key );

	return TRUE;
}

int
Starter::vMessageFailed(Stream *s,bool retry, const std::string &prefix,char const *fmt,va_list args)
{
	std::string error_msg;
	vformatstr( error_msg, fmt, args );

		// old classads cannot handle a string ending in a double quote
		// followed by a newline, so strip off any trailing newline
	trim(error_msg);

	dprintf(D_ALWAYS,"%s failed: %s\n", prefix.c_str(), error_msg.c_str());

	ClassAd response;
	response.Assign(ATTR_RESULT,false);
	response.Assign(ATTR_ERROR_STRING,error_msg);
	if( retry ) {
		response.Assign(ATTR_RETRY,retry);
	}

	s->encode();
	if( !putClassAd(s, response) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to send response to %s.\n", prefix.c_str());
	}

	return FALSE;
}

int
Starter::SSHDRetry(Stream *s,char const *fmt,...)
{
	va_list args;
	va_start( args, fmt );
	vMessageFailed(s, true, "START_SSHD", fmt, args);
	va_end( args );

	return FALSE;
}
int
Starter::SSHDFailed(Stream *s,char const *fmt,...)
{
	va_list args;
	va_start( args, fmt );
	vMessageFailed(s, false, "START_SSHD", fmt, args);
	va_end( args );

	return FALSE;
}

int
Starter::PeekRetry(Stream *s,char const *fmt,...)
{
        va_list args;
        va_start( args, fmt );
        vMessageFailed(s, true, "PEEK", fmt, args);
        va_end( args );

        return FALSE;
}
int
Starter::PeekFailed(Stream *s,char const *fmt,...)
{
        va_list args;
        va_start( args, fmt );
        vMessageFailed(s, false, "PEEK", fmt, args);
        va_end( args );

        return FALSE;
}

// The following functions are only needed if SSH_TO_JOB is supported.

#if HAVE_SSH_TO_JOB

// returns position of str in buffer or -1 if not found
static int find_str_in_buffer(
	char const *buffer,
	int buffer_len,
	char const *str)
{
	int str_len = strlen(str);
    int i;
	for(i=0; i+str_len <= buffer_len; i++) {
		if( memcmp(buffer+i,str,str_len)==0 ) {
			return i;
		}
	}
	return -1;
}

static bool extract_delimited_data_as_base64(
	char const *input_buffer,
	int input_len,
	char const *begin_marker,
	char const *end_marker,
	std::string &output_buffer,
	std::string &error_msg)
{
	int start = find_str_in_buffer(input_buffer,input_len,begin_marker);
	int end = find_str_in_buffer(input_buffer,input_len,end_marker);
	if( start < 0 ) {
		formatstr(error_msg,"Failed to find '%s' in input: %.*s",
							begin_marker,input_len,input_buffer);
		return false;
	}
	start += strlen(begin_marker);
	if( end < 0 || end < start ) {
		formatstr(error_msg,"Failed to find '%s' in input: %.*s",
							end_marker,input_len,input_buffer);
		return false;
	}
	char *encoded = condor_base64_encode((unsigned char const *)input_buffer+start,end-start);
	output_buffer = encoded;
	free(encoded);
	return true;
}


static bool extract_delimited_data(
	char const *input_buffer,
	int input_len,
	char const *begin_marker,
	char const *end_marker,
	std::string &output_buffer,
	std::string &error_msg)
{
	int start = find_str_in_buffer(input_buffer,input_len,begin_marker);
	int end = find_str_in_buffer(input_buffer,input_len,end_marker);
	if( start < 0 ) {
		formatstr(error_msg,"Failed to find '%s' in input: %.*s",
							begin_marker,input_len,input_buffer);
		return false;
	}
	start += strlen(begin_marker);
	if( end < 0 || end < start ) {
		formatstr(error_msg,"Failed to find '%s' in input: %.*s",
							end_marker,input_len,input_buffer);
		return false;
	}
	formatstr(output_buffer,"%.*s",end-start,input_buffer+start);
	return true;
}

#endif

int
Starter::peek(int /*cmd*/, Stream *sock)
{
	// This command should only be allowed by the job owner.
	ReliSock *s = (ReliSock*)sock;
	char const *fqu = s->getFullyQualifiedUser();
	std::string job_owner;
	getJobOwnerFQUOrDummy(job_owner);
	if( !fqu || job_owner != fqu )
	{
		dprintf(D_ALWAYS, "Unauthorized attempt to peek at job logfiles by '%s'\n", fqu ? fqu : "");
		return false;
	}

	ClassAd input;
	s->decode();
	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS, "Failed to read request for peeking at logs.\n");
		return false;
	}

	ClassAd *jobad = NULL;
	ClassAd *machinead = NULL;
        if( jic )
	{
                jobad = jic->jobClassAd();
		machinead = jic->machClassAd();
        }

	bool enabled = param_boolean("ENABLE_PEEK_JOB",true,true,machinead,jobad);
	if( !enabled ) {
		return PeekFailed(s, "Rejecting request, because ENABLE_PEEK_JOB=false");
	}

	if( !jic || !jobad ) {
		return PeekRetry(s, "Retrying request, because job not yet initialized.");
	}

	if( !m_job_environment_is_ready ) {
		// This can happen if file transfer is still in progress.
		return PeekRetry(s, "Retrying request, because the job execution environment is not yet ready.");
	}
	if( m_all_jobs_done ) {
		return PeekFailed(s, "Rejecting request, because the job is finished.");
	}
 
	if( !jic->userPrivInitialized() ) {
		return PeekRetry(s, "Retrying request, because job execution account not yet established.");
	}

	ssize_t max_xfer = -1;
	input.EvaluateAttrInt(ATTR_MAX_TRANSFER_BYTES, max_xfer);

	const char *jic_iwd = GetWorkingDir(0);
	if (!jic_iwd) return PeekFailed(s, "Unknown job remote IWD.");
	std::string iwd = jic_iwd;
	char real_iwd[MAXPATHLEN+1];
	if (!realpath(iwd.c_str(), &real_iwd[0]))
	{
		return PeekFailed(s, "Unable to resolve IWD %s to real path. (errno=%d, %s)", iwd.c_str(), errno, strerror(errno));
	}
	iwd = &real_iwd[0];

	// Ok, sanity checks pass.  Let's iterate through the files requested and make sure they are in the
	// stagein / stageout list.
	//
	std::vector<ExprTree*> file_expr_list;
	std::vector<ExprTree*> off_expr_list;
	std::vector<std::string> file_list;
	std::vector<off_t> offsets_list;

	// Allow the user to request stdout/err explicitly instead of by name;
	// this is done because HTCondor will sometimes rewrite the location of the stdout/err
	// and we don't want to have to replicate that logic in the client.
	bool transfer_stdout = false;
	if (input.EvaluateAttrBool(ATTR_JOB_OUTPUT, transfer_stdout) && transfer_stdout)
	{
		std::string out;
		if (jobad->EvaluateAttrString(ATTR_JOB_OUTPUT, out))
		{
			file_expr_list.push_back(classad::Literal::MakeInteger(0));
			file_list.push_back(out);
			off_t stdout_off = -1;
			input.EvaluateAttrInt("OutOffset", stdout_off);
			offsets_list.push_back(stdout_off);
			off_expr_list.push_back(classad::Literal::MakeInteger(0));
		}
	}
	bool transfer_stderr = false;
	if (input.EvaluateAttrBool(ATTR_JOB_ERROR, transfer_stderr) && transfer_stderr)
	{
		std::string err;
		if (jobad->EvaluateAttrString(ATTR_JOB_ERROR, err))
		{
			file_expr_list.push_back(classad::Literal::MakeInteger(1));
			file_list.push_back(err);
			off_t stderr_off = -1;
			input.EvaluateAttrInt("ErrOffset", stderr_off);
			offsets_list.push_back(stderr_off);
			off_expr_list.push_back(classad::Literal::MakeInteger(stderr_off));
		}
	}

	classad::Value transfer_list_value;
	std::vector<std::string> transfer_list;
	const classad::ExprList * plst;
	if (input.EvaluateAttr("TransferFiles", transfer_list_value) && transfer_list_value.IsListValue(plst))
	{
		transfer_list.reserve(plst->size());
		for (auto it : *plst)
		{
			std::string transfer_entry;
			if (!ExprTreeIsLiteralString(it, transfer_entry))
			{
				return PeekFailed(s, "Could not evaluate transfer list.");
			}
			transfer_list.push_back(transfer_entry);
		}
	}

	std::vector<off_t> transfer_offsets; transfer_offsets.reserve(transfer_list.size());
	for (size_t idx = 0; idx < transfer_list.size(); idx++) transfer_offsets.push_back(-1);

	if (input.EvaluateAttr("TransferOffsets", transfer_list_value) && transfer_list_value.IsListValue(plst))
	{
		size_t idx = 0;
		for (auto it : *plst) 
		{
			long long transfer_entry;
			if (ExprTreeIsLiteralNumber(it, transfer_entry))
			{
				transfer_offsets[idx] = transfer_entry;
			}
		}
	}

	bool missing = false;
	std::vector<off_t>::const_iterator iter2 = transfer_offsets.begin();
	for (std::vector<std::string>::const_iterator iter = transfer_list.begin();
		!missing && (iter != transfer_list.end()) && (iter2 != transfer_offsets.end());
		iter++, iter2++)
	{
		missing = true;
		std::string filename;
		if (jobad->EvaluateAttrString(ATTR_JOB_ERROR, filename) && (filename == *iter))
		{
			missing = false;
			if (!transfer_stderr)
			{
				file_expr_list.push_back(classad::Literal::MakeString(filename));
				file_list.push_back(filename);
				off_expr_list.push_back(classad::Literal::MakeInteger(*iter2));
				offsets_list.push_back(*iter2);
			}
			continue;
		}
		if (jobad->EvaluateAttrString(ATTR_JOB_OUTPUT, filename) && (filename == *iter))
		{
			missing = false;
			if (!transfer_stdout)
			{
				file_expr_list.push_back(classad::Literal::MakeString(filename));
				file_list.push_back(filename);
				off_expr_list.push_back(classad::Literal::MakeInteger(*iter2));
				offsets_list.push_back(*iter2);
			}
			continue;
		}
		std::string job_transfer_list;
		bool found = false;
		if (jobad->EvaluateAttrString(ATTR_TRANSFER_OUTPUT_FILES, job_transfer_list))
		{
			for (const auto& job_iter: StringTokenIterator(job_transfer_list))
			{
				if (strcmp(iter->c_str(), job_iter.c_str()) == 0)
				{
					filename = job_iter;
					found = true;
					break;
				}
			}
		}
		if (found)
		{
			missing = false;
			file_expr_list.push_back(classad::Literal::MakeString(filename));
			off_expr_list.push_back(classad::Literal::MakeInteger(*iter2));
			file_list.push_back(*iter);
			offsets_list.push_back(*iter2);
		}
		else
		{
			return PeekFailed(s, "File %s requested but not authorized to be sent.\n", iter->c_str());
		}
	}

	// Calculate the offsets and sizes we think we are able to do
	std::vector<size_t> file_sizes_list; file_sizes_list.reserve(file_list.size());
	{
	ssize_t remaining = max_xfer;
	TemporaryPrivSentry sentry(PRIV_USER);
	std::vector<off_t>::iterator it2 = offsets_list.begin();
	char real_filename[MAXPATHLEN+1];
	unsigned idx = 0;
	for (std::vector<std::string>::iterator it = file_list.begin();
		it != file_list.end() && it2 != offsets_list.end();
		it++, it2++, idx++)
	{
		size_t size = 0;
		off_t offset = *it2;

		if ( it->size() && !fullpath(it->c_str()) )
		{
			*it = iwd + DIR_DELIM_CHAR + *it;
		}
		if (!realpath(it->c_str(), real_filename))
		{
			return PeekRetry(s, "Unable to resolve file %s to real path. (errno=%d, %s)", it->c_str(), errno, strerror(errno));
		}
		*it = real_filename;
		if (it->substr(0, iwd.size()) != iwd)
		{
			return PeekFailed(s, "Invalid symlink or filename (%s) outside sandbox (%s)", it->c_str(), iwd.c_str());
		}
		dprintf(D_FULLDEBUG, "Peeking at %s inside sandbox %s.\n", it->c_str(), iwd.c_str());

		int fd = safe_open_wrapper(it->c_str(), O_RDONLY);
		struct stat stat_buf;
		if (fd < 0)
		{
			dprintf(D_ALWAYS, "Cannot open file %s for stat / peeking at logs (errno=%d, %s).\n", it->c_str(), errno, strerror(errno));
		}
		else if (fstat(fd, &stat_buf) < 0)
		{
			dprintf(D_ALWAYS, "Cannot stat file %s for peeking at logs.\n", it->c_str());
			close(fd);
		}
		else
		{
			size = stat_buf.st_size;
			close(fd);
		}
		if (offset > 0 && size < static_cast<size_t>(offset))
		{
			offset = (int)size;
			*it2 = offset;
			off_expr_list[idx] = classad::Literal::MakeInteger(*it2);
		}
		if (remaining >= 0)
		{
			if (offset < 0)
			{
				remaining -= size;
				if (remaining < 0)
				{
					size += remaining;
					*it2 = -remaining;
					remaining = 0;
				}
				else
				{
					*it2 = 0;
				}
				off_expr_list[idx] = classad::Literal::MakeInteger(*it2);
			}
			else
			{
				if (remaining > static_cast<ssize_t>(size) - offset)
				{
					size -= offset;
					remaining -= size;
				}
				else
				{
					size = remaining;
					remaining = 0;
				}
			}
		}
		else
		{
			size = -1;
			if (*it2 < 0)
			{
				*it2 = 0;
				off_expr_list[idx] = classad::Literal::MakeInteger(*it2);
			}
		}
		file_sizes_list.push_back(size);
	}
	}

	ClassAd reply;
	reply.InsertAttr(ATTR_RESULT, true);
	classad::ExprTree *list = classad::ExprList::MakeExprList(file_expr_list);
	if (!reply.Insert("TransferFiles", list))
	{
		return PeekFailed(s, "Failed to add transfer files list.\n");
	}
	list = classad::ExprList::MakeExprList(off_expr_list);
	if (!reply.Insert("TransferOffsets", list))
	{
		return PeekFailed(s, "Failed to add transfer offsets list.\n");
	}
	dPrintAd(D_FULLDEBUG, reply);

	s->encode();
	// From here on out, *always* send the same number of files as specified by
	// the response ad, even if it means putting an empty file.
	if (!putClassAd(s, reply) || !s->end_of_message()) {
		dprintf(D_ALWAYS, "Failed to send read request response for peeking at logs.\n");
		return false;
	}

	size_t file_count = 0;
	{
	TemporaryPrivSentry sentry(PRIV_USER);
	std::vector<off_t>::const_iterator it2 = offsets_list.begin();
	std::vector<size_t>::const_iterator it3 = file_sizes_list.begin();
	for (std::vector<std::string>::const_iterator it = file_list.begin();
		it != file_list.end() && it2 != offsets_list.end() && it3 != file_sizes_list.end();
		it++, it2++, it3++)
	{
		filesize_t size = -1;

		int fd = safe_open_wrapper(it->c_str(), O_RDONLY);
		if (fd < 0)
		{
			dprintf(D_ALWAYS, "Cannot open file %s for peeking at logs (errno=%d, %s).\n", it->c_str(), errno, strerror(errno));
			s->put_empty_file(&size);
			continue;
		}
		
		s->put_file(&size, fd, *it2, *it3);
		close(fd);
		if (size < 0 && size != PUT_FILE_MAX_BYTES_EXCEEDED)
		{
			dprintf(D_ALWAYS, "Failed to send file %s for peeking at logs (%ld).\n", it->c_str(), (long)size);
			continue;
		}
		file_count++;
	}
	}
	if (!s->code(file_count) || !s->end_of_message())
	{
		dprintf(D_ALWAYS, "Failed to send file count %zu for peeking at logs.\n", file_count);
	}
	return file_count == file_list.size();
}

int
Starter::startSSHD( int /*cmd*/, Stream* s )
{
		// This command should only be allowed by the job owner.
	std::string error_msg;
	Sock *sock = (Sock*)s;
	char const *fqu = sock->getFullyQualifiedUser();
	std::string job_owner;
	getJobOwnerFQUOrDummy(job_owner);
	if( !fqu || job_owner != fqu ) {
		dprintf(D_ALWAYS,"Unauthorized attempt to start sshd by '%s'\n",
				fqu ? fqu : "");
		return FALSE;
	}

	ClassAd input;
	s->decode();
	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to read request in START_SSHD.\n");
		return FALSE;
	}

#if !defined(HAVE_SSH_TO_JOB)
	return SSHDFailed(s,"This version of condor_starter does not support ssh access.");
#else

	ClassAd *jobad = NULL;
	ClassAd *machinead = NULL;
	if( jic ) {
		jobad = jic->jobClassAd();
		machinead = jic->machClassAd();
	}

	bool enabled = param_boolean("ENABLE_SSH_TO_JOB",true,true,machinead,jobad);
	if( !enabled ) {
		return SSHDFailed(s,"Rejecting request, because ENABLE_SSH_TO_JOB=false");
	}

	if( !jic || !jobad ) {
		return SSHDRetry(s,"Retrying request, because job not yet initialized.");
	}
	if( !m_job_environment_is_ready ) {
			// This can happen if file transfer is still in progress.
			// At this stage, the sandbox might not even be owned by the user.
		return SSHDRetry(s,"Retrying request, because the job execution environment is not yet ready.");
	}
	if( m_all_jobs_done ) {
		return SSHDFailed(s,"Rejecting request, because the job is finished.");
	}
	if( suspended ) {
			// Better to reject them with a clear explanation rather
			// than to allow them to connect and then immediately
			// suspend them without any explanation.
		return SSHDRetry(s,"This slot is currently suspended.");
	}

	std::string preferred_shells;
	input.LookupString(ATTR_SHELL,preferred_shells);

	std::string slot_name;
	input.LookupString(ATTR_NAME,slot_name);

	if( !jic->userPrivInitialized() ) {
		return SSHDRetry(s,"Retrying request, because job execution account not yet established.");
	}

	std::string libexec;
	if( !param(libexec,"LIBEXEC") ) {
		return SSHDFailed(s,"LIBEXEC not defined, so cannot find condor_ssh_to_job_sshd_setup");
	}
	std::string ssh_to_job_sshd_setup;
	std::string ssh_to_job_shell_setup;
	formatstr(ssh_to_job_sshd_setup,
		"%s%ccondor_ssh_to_job_sshd_setup",libexec.c_str(),DIR_DELIM_CHAR);
	formatstr(ssh_to_job_shell_setup,
		"%s%ccondor_ssh_to_job_shell_setup",libexec.c_str(),DIR_DELIM_CHAR);

	if( access(ssh_to_job_sshd_setup.c_str(),X_OK)!=0 ) {
		return SSHDFailed(s,"Cannot execute %s: %s",
						  ssh_to_job_sshd_setup.c_str(),strerror(errno));
	}
	if( access(ssh_to_job_shell_setup.c_str(),X_OK)!=0 ) {
		return SSHDFailed(s,"Cannot execute %s: %s",
						  ssh_to_job_shell_setup.c_str(),strerror(errno));
	}

	std::string sshd_config_template;
	if( !param(sshd_config_template,"SSH_TO_JOB_SSHD_CONFIG_TEMPLATE") ) {
		if( param(sshd_config_template,"LIB") ) {
			formatstr_cat(sshd_config_template,"%ccondor_ssh_to_job_sshd_config_template",DIR_DELIM_CHAR);
		}
		else {
			return SSHDFailed(s,"SSH_TO_JOB_SSHD_CONFIG_TEMPLATE and LIB are not defined.  At least one of them is required.");
		}
	}
	if( access(sshd_config_template.c_str(),F_OK)!=0 ) {
		return SSHDFailed(s,"%s does not exist!",sshd_config_template.c_str());
	}

	std::string ssh_keygen;
	std::string ssh_keygen_args;
	ArgList ssh_keygen_arglist;
	param(ssh_keygen,"SSH_TO_JOB_SSH_KEYGEN","/usr/bin/ssh-keygen");
	param(ssh_keygen_args,"SSH_TO_JOB_SSH_KEYGEN_ARGS","\"-N '' -C '' -q -f %f -t rsa\"");
	ssh_keygen_arglist.AppendArg(ssh_keygen);
	if( !ssh_keygen_arglist.AppendArgsV2Quoted(ssh_keygen_args.c_str(), error_msg) ) {
		return SSHDFailed(s,
						  "SSH_TO_JOB_SSH_KEYGEN_ARGS is misconfigured: %s",
						  error_msg.c_str());
	}

	std::string client_keygen_args;
	input.LookupString(ATTR_SSH_KEYGEN_ARGS,client_keygen_args);
	if( !ssh_keygen_arglist.AppendArgsV2Raw(client_keygen_args.c_str(), error_msg) ) {
		return SSHDFailed(s,
						  "Failed to produce ssh-keygen arg list: %s",
						  error_msg.c_str());
	}

	std::string ssh_keygen_cmd;
	if(!ssh_keygen_arglist.GetArgsStringSystem( ssh_keygen_cmd,0)) {
		return SSHDFailed(s, "Failed to produce ssh-keygen command string" );
	}

	int setup_pipe_fds[2];
	setup_pipe_fds[0] = -1;
	setup_pipe_fds[1] = -1;
	if( !daemonCore->Create_Pipe(setup_pipe_fds) ) {
		return SSHDFailed(
			s,"Failed to create pipe for condor_ssh_to_job_sshd_setup.");
	}
	int setup_std_fds[3];
	setup_std_fds[0] = 0;
	setup_std_fds[1] = setup_pipe_fds[1]; // write end of pipe
	setup_std_fds[2] = setup_pipe_fds[1];

		// Pass the job environment to the sshd setup script.  It will
		// save the environment so that it can be restored when the
		// ssh session starts and our shell setup script is called.
	Env setup_env;
	if( !GetJobEnv( jobad, &setup_env,  error_msg ) ) {
		return SSHDFailed(
			s,"Failed to get job environment: %s",error_msg.c_str());
	}

	if( !slot_name.empty() ) {
		setup_env.SetEnv("_CONDOR_SLOT_NAME",slot_name.c_str());
	}

	int setup_opt_mask = DCJOBOPT_NO_CONDOR_ENV_INHERIT;
	bool inherit_starter_env = false;
	auto_free_ptr envlist(param("JOB_INHERITS_STARTER_ENVIRONMENT"));
	if (envlist && ! string_is_boolean_param(envlist, inherit_starter_env)) {
		WhiteBlackEnvFilter filter(envlist);
		setup_env.Import(filter);
		inherit_starter_env = false; // make sure that CreateProcess doesn't inherit again
	}
	if ( ! inherit_starter_env) {
		setup_opt_mask |= DCJOBOPT_NO_ENV_INHERIT;
	}
		// Use LD_PRELOAD to force an implementation of getpwnam
		// into the setup process 
#ifdef LINUX
	if(param_boolean("CONDOR_SSH_TO_JOB_FAKE_PASSWD_ENTRY", true)) {
		std::string lib;
		param(lib, "LIB");
		std::string getpwnampath = lib + "/libgetpwnam.so";
		if (access(getpwnampath.c_str(), F_OK) == 0) {
			dprintf(D_ALWAYS, "Setting LD_PRELOAD=%s for sshd\n", getpwnampath.c_str());
			setup_env.SetEnv("LD_PRELOAD", getpwnampath.c_str());
		} else {
			dprintf(D_ALWAYS, "Not setting LD_PRELOAD=%s for sshd, as file does not exist\n", getpwnampath.c_str());
		}
	}
#endif

	if( !preferred_shells.empty() ) {
		dprintf(D_FULLDEBUG,
				"Checking preferred shells: %s\n",preferred_shells.c_str());
		for (const auto& shell: StringTokenIterator(preferred_shells, ",")) {
			if( access(shell.c_str(), X_OK)==0 ) {
				dprintf(D_FULLDEBUG, "Will use shell %s\n", shell.c_str());
				setup_env.SetEnv("_CONDOR_SHELL", shell.c_str());
				break;
			}
		}
	}

	ArgList setup_args;
	setup_args.AppendArg(ssh_to_job_sshd_setup.c_str());
	setup_args.AppendArg(GetWorkingDir(0));
	setup_args.AppendArg(ssh_to_job_shell_setup.c_str());
	setup_args.AppendArg(sshd_config_template.c_str());
	setup_args.AppendArg(ssh_keygen_cmd.c_str());

		// Would like to use my_popen here, but we needed to support glexec.
		// Use the default reaper, even though it doesn't know anything
		// about this task.  We avoid needing to know the final exit status
		// by checking for a magic success string at the end of the output.
	int setup_reaper = 1;
	daemonCore->Create_Process(
		ssh_to_job_sshd_setup.c_str(),
		setup_args,
		PRIV_USER_FINAL,
		setup_reaper,
		FALSE,
		FALSE,
		&setup_env,
		GetWorkingDir(0),
		NULL,
		NULL,
		setup_std_fds,
		NULL,
		0,
		NULL,
		setup_opt_mask);

	daemonCore->Close_Pipe(setup_pipe_fds[1]); // write-end of pipe

		// NOTE: the output from the script may contain binary data,
		// so we can't just slurp it in as a C string.
	char *setup_output = NULL;
	int setup_output_len = 0;
	char pipe_buf[1024];
	while( true ) {
		int n = daemonCore->Read_Pipe(setup_pipe_fds[0],pipe_buf,1024);
		if( n <= 0 ) {
			break;
		}
		char *old_setup_output = setup_output;
		setup_output = (char *)realloc(setup_output,setup_output_len+n+1);
		if( !setup_output ) {
			free( old_setup_output );
			daemonCore->Close_Pipe(setup_pipe_fds[0]); // read-end of pipe
			return SSHDFailed(s,"Out of memory");
		}
		memcpy(setup_output+setup_output_len,pipe_buf,n);
		setup_output_len += n;
			// for easier debugging, append a null to the end,
			// so we can try to print the buffer as a string
		setup_output[setup_output_len] = '\0';
	}

	daemonCore->Close_Pipe(setup_pipe_fds[0]); // read-end of pipe

		// look for magic success string
	if( find_str_in_buffer(setup_output,setup_output_len,"condor_ssh_to_job_sshd_setup SUCCESS") < 0 ) {
		formatstr(error_msg, "condor_ssh_to_job_sshd_setup failed: %s",
						  setup_output);
		free( setup_output );
		return SSHDFailed(s,"%s",error_msg.c_str());
	}

		// Since in privsep situations, we cannot directly access the
		// ssh key files that sshd_setup prepared, we slurp them in
		// from the pipe to sshd_setup.

	bool rc = true;
	std::string session_dir;
	if( rc ) {
		rc = extract_delimited_data(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup SSHD DIR BEGIN\n",
			"\ncondor_ssh_to_job_sshd_setup SSHD DIR END\n",
			session_dir,
			error_msg);
	}

	std::string sshd_user;
	if( rc ) {
		rc = extract_delimited_data(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup SSHD USER BEGIN\n",
			"\ncondor_ssh_to_job_sshd_setup SSHD USER END\n",
			sshd_user,
			error_msg);
	}

	std::string public_host_key;
	if( rc ) {
		rc = extract_delimited_data_as_base64(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup PUBLIC SERVER KEY BEGIN\n",
			"condor_ssh_to_job_sshd_setup PUBLIC SERVER KEY END\n",
			public_host_key,
			error_msg);
	}

	std::string private_client_key;
	if( rc ) {
		rc = extract_delimited_data_as_base64(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup AUTHORIZED CLIENT KEY BEGIN\n",
			"condor_ssh_to_job_sshd_setup AUTHORIZED CLIENT KEY END\n",
			private_client_key,
			error_msg);
	}

	free( setup_output );

	if( !rc ) {
		return SSHDFailed(s,
			"Failed to parse output of condor_ssh_to_job_sshd_setup: %s",
			error_msg.c_str());
	}

	dprintf(D_FULLDEBUG,"StartSSHD: session_dir='%s'\n",session_dir.c_str());

	std::string sshd_config_file;
	formatstr(sshd_config_file,"%s%csshd_config",session_dir.c_str(),DIR_DELIM_CHAR);



	std::string sshd;
	param(sshd,"SSH_TO_JOB_SSHD","/usr/sbin/sshd");
	if( access(sshd.c_str(),X_OK)!=0 ) {
		return SSHDFailed(s,"Failed, because sshd not correctly configured (SSH_TO_JOB_SSHD=%s): %s.",sshd.c_str(),strerror(errno));
	}

	ArgList sshd_arglist;
	std::string sshd_arg_string;
	param(sshd_arg_string,"SSH_TO_JOB_SSHD_ARGS","\"-i -e -f %f\"");
	if( !sshd_arglist.AppendArgsV2Quoted(sshd_arg_string.c_str(), error_msg) )
	{
		return SSHDFailed(s,"Invalid SSH_TO_JOB_SSHD_ARGS (%s): %s",
						  sshd_arg_string.c_str(),error_msg.c_str());
	}

	char **argarray = sshd_arglist.GetStringArray();
	sshd_arglist.Clear();
	for(int i=0; argarray[i]; i++) {
		char const *ptr;
		std::string new_arg;
		for(ptr=argarray[i]; *ptr; ptr++) {
			if( *ptr == '%' ) {
				ptr += 1;
				if( *ptr == '%' ) {
					new_arg += '%';
				}
				else if( *ptr == 'f' ) {
					new_arg += sshd_config_file;
				}
				else {
					deleteStringArray(argarray);
					return SSHDFailed(s,
							"Unexpected %%%c in SSH_TO_JOB_SSHD_ARGS: %s\n",
							*ptr ? *ptr : ' ', sshd_arg_string.c_str());
				}
			}
			else {
				new_arg += *ptr;
			}
		}
		sshd_arglist.AppendArg(new_arg.c_str());
	}
	deleteStringArray(argarray);
	argarray = NULL;


	ClassAd *sshd_ad = new ClassAd(*jobad);
	sshd_ad->Assign(ATTR_JOB_CMD,sshd);
	CondorVersionInfo ver_info;
	if( !sshd_arglist.InsertArgsIntoClassAd(sshd_ad,&ver_info, error_msg) ) {
		return SSHDFailed(s,
			"Failed to insert args into sshd job description: %s",
			error_msg.c_str());
	}
		// Since sshd clenses the user's environment, it doesn't really
		// matter what we pass here.  Instead, we depend on sshd_shell_init
		// to restore the environment that was saved by sshd_setup.
		// However, we may as well pass the desired environment.

#ifdef LINUX
	if( !setup_env.InsertEnvIntoClassAd(*sshd_ad, error_msg) ) {
		return SSHDFailed(s,
			"Failed to insert environment into sshd job description: %s",
			error_msg.c_str());
	}
#endif



		// Now send the expected ClassAd response to the caller.
		// This is the last exchange before switching the connection
		// over to the ssh protocol.
	ClassAd response;
	response.Assign(ATTR_RESULT,true);
	response.Assign(ATTR_REMOTE_USER,sshd_user);
	response.Assign(ATTR_SSH_PUBLIC_SERVER_KEY,public_host_key);
	response.Assign(ATTR_SSH_PRIVATE_CLIENT_KEY,private_client_key);

	s->encode();
	if( !putClassAd(s, response) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to send response to START_SSHD.\n");
		return FALSE;
	}



	std::string sshd_log_fname;
	formatstr(sshd_log_fname,"%s%c%s",session_dir.c_str(),DIR_DELIM_CHAR,"sshd.log");


	int std[3];
	char const *std_fname[3];
	std[0] = sock->get_file_desc();
	std_fname[0] = "stdin";
	std[1] = sock->get_file_desc();
	std_fname[1] = "stdout";
	std[2] = -1;
	std_fname[2] = sshd_log_fname.c_str();


	SSHDProc *proc = new SSHDProc(sshd_ad, session_dir, true);
	if( !proc ) {
		dprintf(D_ALWAYS,"Failed to create SSHDProc.\n");
		return FALSE;
	}
	if( !proc->SshStartJob(std,std_fname) ) {
		dprintf(D_ALWAYS,"Failed to start sshd.\n");
		delete proc;
		return FALSE;
	}
	m_job_list.emplace_back(proc);
	if( this->suspended ) {
		proc->Suspend();
	}

	return TRUE;
#endif
}

/**
 * Hold Job Command (sent by startd)
 * Unlike the DC signal, this includes a hold reason and hold code.
 * Also unlike the DC signal, this _puts_ the job on hold, rather than
 * just passively informing the JIC of the hold.
 * 
 * @param command (not used)
 * @return true if ????, otherwise false
 */ 
int 
Starter::remoteHoldCommand( int /*cmd*/, Stream* s )
{
	std::string hold_reason;
	int hold_code;
	int hold_subcode;
	int soft;

	s->decode();
	if( !s->get(hold_reason) ||
		!s->get(hold_code) ||
		!s->get(hold_subcode) ||
		!s->get(soft) ||
		!s->end_of_message() )
	{
		dprintf(D_ALWAYS,"Failed to read message from %s in Starter::remoteHoldCommand()\n", s->peer_description());
		return FALSE;
	}

	int reply = 1;
	s->encode();
	if( !s->put(reply) || !s->end_of_message()) {
		dprintf(D_ALWAYS,"Failed to send response to startd in Starter::remoteHoldCommand()\n");
	}

	if (hold_code >= CONDOR_HOLD_CODE::VacateBase) {
		m_vacateReason = hold_reason;
		m_vacateCode =hold_code;
		m_vacateSubcode = hold_subcode;
		if (soft) {
			return this->RemoteShutdownGraceful(0);
		} else {
			return this->RemoteShutdownFast(0);
		}
	}

		// Put the job on hold on the remote side.
	if( jic ) {
		jic->holdJob(hold_reason.c_str(),hold_code,hold_subcode);
	}

	if( !soft ) {
		return this->ShutdownFast();
	}

		//
		// If this method returns true, that means there were no
		// jobs running. We'll send out a little debug message 
		// and let ourselves know that all the jobs are done
		// Note that this used to be in the Hold() call but
		// this causes problems if the Hold is coming from ourself
		// For instance, if the OnExitHold evaluates to true for
		// a local universe job
		//
	if ( this->Hold( ) ) {
		dprintf( D_FULLDEBUG, "Got Hold when no jobs running\n" );
		this->allJobsDone();
		return ( true );
	}
	return ( false );
}

/**
 * Attempts to put all the jobs on hold
 * If a job has already been put on hold, then the job is removed
 * from the queue. If a job is being deferred, we will just
 * call removeDeferredJobs() to clean up any timers we may have set.
 * 
 * @return true if all the jobs have been put on hold & removed from the queue
 */
bool
Starter::Hold( void )
{	
	bool jobRunning = false;

	dprintf( D_ALWAYS, "Hold all jobs\n" );

		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	auto listit = m_job_list.begin();
	while (listit != m_job_list.end()) {
		UserProc *job = *listit;;
		if( job->Hold() ) {
			// job is completely shut down, so delete it
			listit = m_job_list.erase(listit);
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
			listit++;
		}
	}
	ShuttingDown = TRUE;
	return ( !jobRunning );
}

#ifdef WIN32
bool Starter::loadUserRegistry(const ClassAd * JobAd)
{
	m_owner_profile.update ();
	std::string username(m_owner_profile.username());

	/*************************************************************
	NOTE: We currently *ONLY* support loading slot-user profiles.
	This limitation will be addressed shortly, by allowing regular
	users to load their registry hive - Ben [2008-09-31]
	**************************************************************/
	bool run_as_owner = false;
	JobAd->LookupBool ( ATTR_JOB_RUNAS_OWNER,  run_as_owner );
	if (run_as_owner) {
		return true;
	}

	bool load_profile = has_encrypted_working_dir;
	if ( ! load_profile) {
		// If we don't *need* to load the user registry let the job decide that it wants to
		JobAd->LookupBool(ATTR_JOB_LOAD_PROFILE, load_profile);
	}

	// load the slot user registry if it is wanted or needed. This can take about 30 seconds to load
	if (load_profile) {
		dprintf(D_FULLDEBUG, "Loading registry hives for %s\n", username.c_str());
		if ( ! m_owner_profile.load()) {
			dprintf(D_ALWAYS, "Failed to load registry hives for %s\n", username.c_str());
			return false;
		} else {
			dprintf(D_ALWAYS, "Loaded Registry hives for %s\n", username.c_str());
		}
	}

	return true;
}
#endif

bool
Starter::createTempExecuteDir( void )
{
		// Once our JobInfoCommmunicator has initialized the right
		// user for the priv_state code, we can finally make the
		// scratch execute directory for this job.

		// If we're the gridshell, for now, we're not making a temp
		// scratch dir, we're just using whatever we got from the
		// scheduler we're running under.
	if( is_gridshell ) { 
		dprintf( D_ALWAYS, "gridshell running in: \"%s\"\n", WorkingDir.c_str() ); 
		return true;
	}

		// On Unix, we stat() the execute directory to determine how
		// the sandbox directory should be created. If it is world-
		// writable, we do a mkdir() as the user. If it is not, we do
		// the mkdir() as condor then chown() it over to the user.
		//
		// On NT (at least for now), we should be in Condor priv
		// because the execute directory on NT is not world writable.
#ifndef WIN32
		// UNIX
	bool use_chown = false;
	if (can_switch_ids()) {
		struct stat st;
		if (stat(Execute, &st) == -1) {
			EXCEPT("stat failed on %s: %s",
			       Execute,
			       strerror(errno));
		}
		if (!(st.st_mode & S_IWOTH)) {
			use_chown = true;
		}
	}
	priv_state priv;
	if (use_chown) {
		priv = set_condor_priv();
	}
	else {
		priv = set_user_priv();
	}
#else
		// WIN32
	priv_state priv = set_condor_priv();
#endif

	int dir_perms = 0700;

	// Parameter JOB_EXECDIR_PERMISSIONS can be user / group / world and
	// defines permissions on execute directory (subject to umask)
	char *who = param("JOB_EXECDIR_PERMISSIONS");
	if(who != NULL)	{
		if(!strcasecmp(who, "user"))
			dir_perms = 0700;
		else if(!strcasecmp(who, "group"))
			dir_perms = 0750;
		else if(!strcasecmp(who, "world"))
			dir_perms = 0755;
		free(who);

		if( mkdir(WorkingDir.c_str(), dir_perms) < 0 ) {
			dprintf( D_ERROR,
			         "couldn't create dir %s: %s\n",
			         WorkingDir.c_str(),
			         strerror(errno) );
			set_priv( priv );
			return false;
		}
	}

	// Check if EP encrypt job execute dir is disabled and job requested encryption
	if (param_boolean("DISABLE_EXECUTE_DIRECTORY_ENCRYPTION", false)) {
		bool requested = false;
		auto * ad = jic ? jic->jobClassAd() : nullptr;
		if (ad && ad->LookupBool(ATTR_ENCRYPT_EXECUTE_DIRECTORY, requested) && requested) {
			dprintf(D_ERROR,
			        "Error: Execution Point has disabled encryption for execute directories and matched job requested encryption!\n");
			return false;
		}
	}

#ifdef WIN32
		// On NT, we've got to manually set the acls, too.
	{
		// fix up any goofy /'s in our path since
		// some of these Win32 calls might not like
		// them.
		canonicalize_dir_delimiters(WorkingDir);

		perm dirperm;
		const char * nobody_login = get_user_loginname();
		ASSERT(nobody_login);
		dirperm.init(nobody_login);
		bool ret_val = dirperm.set_acls( WorkingDir.c_str() );
		if ( !ret_val ) {
			dprintf(D_ALWAYS,"UNABLE TO SET PERMISSIONS ON EXECUTE DIRECTORY\n");
			set_priv( priv );
			return false;
		}
	
		// if the admin or the user wants the execute directory encrypted,
		// go ahead and set that up now too
		bool encrypt_execdir = param_boolean_crufty("ENCRYPT_EXECUTE_DIRECTORY", false);
		if (!encrypt_execdir && jic && jic->jobClassAd()) {
			jic->jobClassAd()->LookupBool(ATTR_ENCRYPT_EXECUTE_DIRECTORY,encrypt_execdir);
		}
		if (encrypt_execdir) {
		
			// dynamically load our encryption functions to preserve 
			// compatability with NT4 :(
			
			typedef BOOL (WINAPI *FPEncryptionDisable)(LPCWSTR,BOOL);
			typedef BOOL (WINAPI *FPEncryptFileA)(LPCSTR);
			bool efs_support = true;
			
			HINSTANCE advapi = LoadLibrary("ADVAPI32.dll");
			if ( !advapi ) {
				dprintf(D_FULLDEBUG, "Can't load advapi32.dll\n");
				efs_support = false;
			} else {
				FPEncryptionDisable EncryptionDisable = (FPEncryptionDisable) 
					GetProcAddress(advapi,"EncryptionDisable");
				if ( !EncryptionDisable ) {
					dprintf(D_FULLDEBUG, "cannot get address for EncryptionDisable()\n");
					efs_support = false;
				}
				FPEncryptFileA EncryptFile = (FPEncryptFileA) 
					GetProcAddress(advapi,"EncryptFileA");
				if ( !EncryptFile ) {
					dprintf(D_FULLDEBUG, "cannot get address for EncryptFile()\n");
					efs_support = false;
				}
			}

			if ( efs_support ) {
				size_t cch = WorkingDir.length()+1;
				wchar_t *WorkingDir_w = new wchar_t[cch];
				swprintf_s(WorkingDir_w, cch, L"%S", WorkingDir.c_str());

				EncryptionDisable(WorkingDir_w, FALSE);
				
				if ( EncryptFile(WorkingDir.c_str()) == 0 ) {
					dprintf(D_ALWAYS, "Could not encrypt execute directory (err=%li)\n", GetLastError());
				} else {
					has_encrypted_working_dir = true;
					dprintf(D_ALWAYS, "Encrypting execute directory \"%s\" to user %s\n", WorkingDir.c_str(), nobody_login);
				}

				delete[] WorkingDir_w;
				FreeLibrary(advapi); // don't leak the dll library handle

			} else {
				// tell the user it didn't work out
				dprintf(D_ALWAYS, "ENCRYPT_EXECUTE_DIRECTORY set to True, but the Encryption functions are unavailable!");
			}

		} // ENCRYPT_EXECUTE_DIRECTORY is True
	}

	// We might need to load registry hives for encrypted execute dir to work
	// so give the jic a chance to do that now. Also if the job has load_profile = true
	// this is where we honor that.  Note that a registry create/load can take multiple seconds
	loadUserRegistry(jic->jobClassAd());

#endif /* WIN32 */

#ifdef LINUX
	const char *lvm_vg = getenv("CONDOR_LVM_VG");
	const char *lv_size = getenv("CONDOR_LVM_LV_SIZE_KB");
	const char *lv_name = getenv("CONDOR_LVM_LV_NAME");
	if (lvm_vg && lv_size && lv_name) {
		const char *thinpool = getenv("CONDOR_LVM_THINPOOL");
		bool lvm_setup_successful = false;
		const char *thin_provision_val = getenv("CONDOR_LVM_THIN_PROVISION");
		const char *encrypt_val = getenv("CONDOR_LVM_ENCRYPT");
		bool thin_provision = strcasecmp(thin_provision_val?thin_provision_val:"", "true") == MATCH;
		bool encrypt_execdir = strcasecmp(encrypt_val?encrypt_val:"", "true") == MATCH;

		try {
			m_lvm_lv_size_kb = std::stol(lv_size);
		} catch (...) {
			m_lvm_lv_size_kb = -1;
		}
		if (thin_provision && ! thinpool) { m_lvm_lv_size_kb = -1; }
		if ( ! thin_provision && thinpool) { m_lvm_lv_size_kb = -1; }

		bool hide_mount = true;
		if ( ! VolumeManager::CheckHideMount(jic->jobClassAd(), jic->machClassAd(), hide_mount)) {
			m_lvm_lv_size_kb = -1;
		}

		if (m_lvm_lv_size_kb > 0) {
			CondorError err;
			m_lv_handle.reset(new VolumeManager::Handle(lvm_vg, lv_name, thinpool, encrypt_execdir));
			if ( ! m_lv_handle) {
				dprintf(D_ERROR, "Failed to create new LV handle (Out of Memory)\n");
			} else if ( ! m_lv_handle->SetupLV(WorkingDir, m_lvm_lv_size_kb, hide_mount, err)) {
				dprintf(D_ERROR, "Failed to setup LVM filesystem for job: %s\n", err.getFullText().c_str());
			} else if (m_lv_handle->SetPermission(dir_perms) != 0) {
				dprintf(D_ERROR, "Failed to chmod(%o) for LV mountpoint (%d): %s\n", dir_perms, errno, strerror(errno));
			} else {
				lvm_setup_successful = true;
				m_lvm_poll_tid = daemonCore->Register_Timer(10, 10,
					(TimerHandlercpp)&Starter::CheckLVUsage,
					"check disk usage", this);
			}
		}
		if ( ! lvm_setup_successful) {
			m_lv_handle.reset(); //This calls handle destructor and cleans up any partial setup
			return false;
		}
		dirMonitor = new StatExecDirMonitor();
		has_encrypted_working_dir = m_lv_handle->IsEncrypted();
	} else {
		// Linux && no LVM
		dirMonitor = new ManualExecDirMonitor(WorkingDir);
	}
#else /* Non-Linux OS*/
	dirMonitor = new ManualExecDirMonitor(WorkingDir);
#endif // LINUX

	if ( ! dirMonitor || ! dirMonitor->IsValid()) {
		dprintf(D_ERROR, "Failed to initialize job working directory monitor object: %s\n",
		                 dirMonitor ? "Out of memory" : "Failed initialization");
		return false;
	}

	dprintf_open_logs_in_directory(WorkingDir.c_str());

	// now we can finally write .machine.ad and .job.ad into the sandbox
	WriteAdFiles();

#if !defined(WIN32)
	if (use_chown) {
		priv_state p = set_root_priv();
		if (chown(WorkingDir.c_str(),
					get_user_uid(),
					get_user_gid()) == -1)
		{
			EXCEPT("chown error on %s: %s",
					WorkingDir.c_str(),
					strerror(errno));
		}
		set_priv(p);
	}
#endif

	// switch to user priv -- it's the owner of the directory we just made
	priv_state ch_p = set_user_priv();
	int chdir_result = chdir(WorkingDir.c_str());
	set_priv( ch_p );

	if( chdir_result < 0 ) {
		dprintf( D_ERROR, "couldn't move to %s: %s\n", WorkingDir.c_str(),
				 strerror(errno) ); 
		set_priv( priv );
		return false;
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", WorkingDir.c_str() );
	set_priv( priv );
	m_workingDirExists = true;
	return true;
}

/*
 * When input file transfer or other setup tasks fail, will enter this method
 * to begin reporting the failure.  Reporting might include transferring FailureFiles
 **/
int
Starter::jobEnvironmentCannotReady(int status, const struct UnreadyReason & urea)
{
	// record these for later
	m_setupStatus = status;
	m_urea = urea;

	// we've already decided to start the job, so just exit here
	if (this->deferral_tid != -1) {
		dprintf(D_ALWAYS, "ERROR: deferral timer already queued when jobEnvironmentCannotReady\n");
		return 0;
	}

	// Ask the AP what to do.
	std::ignore = daemonCore->Register_Timer(
		0, 0,
		[this](int /* timerID */) -> void {
			Starter::requestGuidanceJobEnvironmentUnready(this);
		},
		"ask AP what to do"
	);

	return true;
}

/**
 * After any file transfers are complete, will enter this method
 * to setup anything else that needs to happen in the Starter
 * before starting a job
 *
 * @return true
 **/
int
Starter::jobEnvironmentReady( void )
{
	m_job_environment_is_ready = true;

	// Ask the AP what to do.
	std::ignore = daemonCore->Register_Timer(
		0, 0,
		[this](int /* timerID */) -> void {
		    Starter::requestGuidanceJobEnvironmentReady(this);
		},
		"ask AP what to do"
	);

	return ( true );
}


bool
Starter::skipJobImmediately() {
	//
	// Now we will register a callback that will
	// call the function to actually execute the job
	// If there wasn't a deferral time then the job will
	// be started right away. We store the timer id so that
	// if a suspend comes in, we can cancel the job from being
	// executed
	//
	this->deferral_tid = daemonCore->Register_Timer(0,
		(TimerHandlercpp)&Starter::SkipJobs,
		"SkipJobs",
		this );

	//
	// Make sure our timer callback registered properly
	//
	if( this->deferral_tid < 0 ) {
		EXCEPT( "Can't register SkipJob DaemonCore timer" );
	}
	dprintf( D_ALWAYS, "Skipping execution of Job %d.%d because of setup failure.\n",
			this->jic->jobCluster(),
			this->jic->jobProc() );

    return true;
}


/**
 * Calculate whether we need to wait until a certain time
 * before executing the job.
 * 
 * Currently the user can specify in their job submission file 
 * a UTC timestamp of when the job should be deferred until.
 * The following example would have the Starter attempt
 * to execute the job on Friday 10.14.2005 at 12:00:00
 * 
 * 	DeferralTime = 1129309200
 * 
 * The starter will check to see if this DeferralTime is 
 * not in the past, and if it is it can be given a window in seconds
 * to say how far in the past we are willing to run a job
 * 
 * There is also an additional time offset parameter that can
 * be stuffed into the job ad by the Shadow to specify the clock
 * difference between itself and this Starter. When this offset
 * is subtracted for our current time, we can ensure that we will
 * execute at the Shadow's proper time, not what we think the current
 * time is. This offset will be in seconds.
 * 
 * @return
 **/
bool
Starter::jobWaitUntilExecuteTime( void )
{
		//
		// Return value
		//
	bool ret = true;
		//
		// If this is set to true, then we'll want to abort the job
		//
	bool abort = false;
	std::string error;
	
		//
		// First check to see if the job is set to be
		// deferred until a certain time before beginning to
		// execute 
		//		
	ClassAd* jobAd = this->jic->jobClassAd();
	time_t deferralTime = 0;
	int deferralOffset = 0;
	time_t deltaT = 0;
	time_t deferralWindow = 0;
	if ( jobAd->LookupExpr( ATTR_DEFERRAL_TIME ) != NULL ) {
			//
		 	// Make sure that the expression evaluated and we 
		 	// got a positive integer. Otherwise we'll have to kick out
		 	//
		if ( ! jobAd->LookupInteger( ATTR_DEFERRAL_TIME, deferralTime ) ) {
			formatstr( error, "Invalid deferred execution time for Job %d.%d.",
							this->jic->jobCluster(),
							this->jic->jobProc() );
			abort = true;
		} else if ( deferralTime <= 0 ) {
			formatstr( error, "Invalid execution time '%lld' for Job %d.%d.",
							(long long)deferralTime,
							this->jic->jobCluster(),
							this->jic->jobProc() );
			abort = true;
 
		} else {
				//
				// It was valid, so we need to figure out what the time difference
				// between the deferral time and our current time is. There
				// are two scenarios that can occur in this situation:
				//
				//  1) The deferral time still hasn't arrived, so we'll need
				//     to set the trigger to hit us up in the delta time
				//	2) The deferral time has passed, meaning we're late, and
				//     the job has missed its window. We will not execute it
				//		
			time_t now = time(nullptr);
				//
				// We can also be passed a offset value
				// This is from the Shadow who has determined that
				// our clock is different from theirs
				// Thus, we will just need to subtract this offset from
				// our currrent time measurement
				//
			if ( jobAd->LookupInteger( ATTR_DEFERRAL_OFFSET, deferralOffset ) ) {
				dprintf( D_FULLDEBUG, "Job %d.%d deferral time offset by "
				                      "%d seconds\n", 
							this->jic->jobCluster(),
							this->jic->jobProc(),
							deferralOffset );
				now -= deferralOffset;
			}
				//
				// Along with an offset we can be given a window range
				// to say how much leeway we will allow a late job to have
				// So if the deferralTime is less than the currenTime,
				// but within this window, we'll still run the job
				//
			if ( jobAd->LookupInteger( ATTR_DEFERRAL_WINDOW, deferralWindow ) ) {
				dprintf( D_FULLDEBUG, "Job %d.%d has a deferral window of "
				                      "%lld seconds\n", 
							this->jic->jobCluster(),
							this->jic->jobProc(),
							(long long)deferralWindow );
			}
			deltaT = deferralTime - now;
				//
				// The time has already passed, check whether it's
				// within our window. If not then abort
				//
			if ( deltaT < 0 ) {
				if ( abs( deltaT ) > deferralWindow ) {
					formatstr( error, "Job %d.%d missed its execution time.",
								this->jic->jobCluster(),
								this->jic->jobProc() );
					abort = true;

				} else {
						//
						// Be sure to set the deltaT to zero so
						// that the timer goes right off
						//
					dprintf( D_ALWAYS, "Job %d.%d missed its execution time but "
										"is within the %lld seconds window\n",
								this->jic->jobCluster(),
								this->jic->jobProc(),
								(long long)deferralWindow );
					deltaT = 0;
				}
			} // if deltaT < 0
		}	
	}
	
		//
		// Start the job timer
		//
	if ( ! abort ) {
			//
			// Quick sanity check
			// Make sure another timer isn't already registered
			//
		ASSERT( this->deferral_tid == -1 );
		
			//
			// Now we will register a callback that will
			// call the function to actually execute the job
			// If there wasn't a deferral time then the job will 
			// be started right away. We store the timer id so that
			// if a suspend comes in, we can cancel the job from being
			// executed
			//
		this->deferral_tid = daemonCore->Register_Timer(
										deltaT,
										(TimerHandlercpp)&Starter::SpawnPreScript,
										"deferred job start",
										this );
			//
			// Make sure our timer callback registered properly
			//
		if( this->deferral_tid < 0 ) {
			EXCEPT( "Can't register Deferred Execution DaemonCore timer" );
		}
			//
			// Our job will start in the future
			//
		if ( deltaT > 0 ) { 
			dprintf( D_ALWAYS, "Job %d.%d deferred for %lld seconds\n", 
						this->jic->jobCluster(),
						this->jic->jobProc(),
						(long long)deltaT );
			//
			// Our job will start right away!
			//
		} else {
			dprintf( D_ALWAYS, "Job %d.%d set to execute immediately\n",
						this->jic->jobCluster(),
						this->jic->jobProc() );
		}
		
		//
		// Aborting the job!
		// We are not going to start the job so we'll let the jic know
		//
	} else {
			//
			// Hack!
			// I want to send back that the job missed its time
			// and that the schedd needs to decide what to do with
			// the job. But the only way to do this is if you
			// have a UserProc object. So we're going to make
			// a quick on here and then send back the exit error
			//
			// This is suppose to be temporary until we have some kind
			// of error handling in place for jobs that never started
			// Andy Pavlo - 01.24.2006 - pavlo@cs.wisc.edu
			//
		dprintf( D_ALWAYS, "%s Aborting.\n", error.c_str() );
		OsProc proc( jobAd );
		proc.JobReaper( -1, JOB_MISSED_DEFERRAL_TIME );
		this->jic->notifyJobExit( -1, JOB_MISSED_DEFERRAL_TIME, &proc );
		this->allJobsDone();
		ret = false;
	}

	return ( ret );
}

/**
 * If we need to remove all our jobs, this method can
 * be called to remove any jobs that are currently being
 * deferred. All a deferral means is that there is a timer
 * that has been registered to wakeup when its time to
 * execute the job. So we just need to cancel the timer
 * 
 * @return true if the deferred job was removed successfully
 **/
bool
Starter::removeDeferredJobs() {
	bool ret = true;
	
	if ( this->deferral_tid == -1 ) {
		return ( ret );
	}
	
	m_deferred_job_update = true;
	
		//
		// Attempt to cancel the the timer
		//
	if ( daemonCore->Cancel_Timer( this->deferral_tid ) >= 0 ) {
		dprintf( D_FULLDEBUG, "Cancelled time deferred execution for "
							  "Job %d.%d\n", 
					this->jic->jobCluster(),
					this->jic->jobProc() );
		this->deferral_tid = -1;

	} else {
			//
			// We failed to cancel the timer!
			// This is bad because our job might execute when it shouldn't have
			//
		EXCEPT( "Failed to cancel deferred execution timer for Job %d.%d",
		        this->jic->jobCluster(), this->jic->jobProc() );
		ret = false;
	}
	return ( ret );
}

/**
 * Start the prescript for a job, if one exists
 * If one doesn't, then we will call SpawnJob() directly
 * 
 **/
void
Starter::SpawnPreScript( int /* timerID */ )
{
		//
		// Unset the deferral timer so that we know that no job
		// is waiting to be spawned
		//
	if ( this->deferral_tid != -1 ) {
		this->deferral_tid = -1;
	}
	
		// first, see if we're going to need any pre and post scripts
	ClassAd* jobAd = jic->jobClassAd();
	char* tmp = NULL;
	std::string attr;

	attr = "Pre";
	attr += ATTR_JOB_CMD;
	if( jobAd->LookupString(attr, &tmp) ) {
		free( tmp );
		tmp = NULL;
		pre_script = new ScriptProc( jobAd, "Pre" );
	}

	attr = "Post";
	attr += ATTR_JOB_CMD;
	if( jobAd->LookupString(attr, &tmp) ) {
		free( tmp );
		tmp = NULL;
		post_script = new ScriptProc( jobAd, "Post" );
	}

	if( pre_script ) {
			// if there's a pre script, try to run it now

		if( pre_script->StartJob() ) {
				// if it's running, all we can do is return to
				// DaemonCore and wait for the it to exit.  the
				// reaper will then do the right thing
			return;
		} else {
			dprintf( D_ALWAYS, "Failed to start prescript, exiting\n" );
				// TODO notify the JIC somehow?
			main_shutdown_fast();
			return;
		}
	}

		// if there's no pre-script, we can go directly to trying to
		// spawn the main job. if we fail to spawn the job we exit
	if( ! SpawnJob() ) {
		dprintf( D_ALWAYS, "Failed to start main job, exiting.\n" );
		main_shutdown_fast();
	}
}

/**
* Timer handler to Skip job execution because we failed setup
* used instead of the deferral timer that executes SpawnPreScript above
* return true if no errors occured
**/
void
Starter::SkipJobs( int /* timerID */ )
{
	//
	// Unset the deferral timer so that we know that no job
	// is waiting to be spawned
	//
	if ( this->deferral_tid != -1 ) {
		this->deferral_tid = -1;
	}

	if (m_setupStatus != 0) {
		jic->setJobFailed(); // so we transfer FailureFiles instead of output files
	}
	// this starts output transfer of FailureFiles
	allJobsDone();
}


void Starter::getJobOwnerFQUOrDummy(std::string &result) const
{
	ClassAd *jobAd = jic ? jic->jobClassAd() : NULL;
	if( jobAd ) {
		jobAd->LookupString(ATTR_USER,result);
	}
	if( result.empty() ) {
		result = "job-owner@submit-domain";
	}
}

bool Starter::getJobClaimId(std::string &result) const
{
	ClassAd *jobAd = jic ? jic->jobClassAd() : NULL;
	if( jobAd ) {
		return jobAd->LookupString(ATTR_CLAIM_ID,result);
	}
	return false;
}

/**
 * 
 * 
 * 
 **/
int
Starter::SpawnJob( void )
{
		// Now that we've got all our files, we can figure out what
		// kind of job we're starting up, instantiate the appropriate
		// userproc class, and actually start the job.
	ClassAd* jobAd = jic->jobClassAd();
	ClassAd * mad = jic->machClassAd();

	if ( ! jobAd->LookupInteger( ATTR_JOB_UNIVERSE, jobUniverse ) || jobUniverse < 1 ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
	}
	dprintf( D_ALWAYS, "Starting a %s universe job with ID: %d.%d\n",
			 CondorUniverseName(jobUniverse), jic->jobCluster(),
			 jic->jobProc() );

	UserProc *job;
	switch ( jobUniverse )  
	{
		case CONDOR_UNIVERSE_LOCAL:
		case CONDOR_UNIVERSE_VANILLA: {
			bool wantDocker = false;
			jobAd->LookupBool( ATTR_WANT_DOCKER, wantDocker );
			bool wantContainer = false;
			jobAd->LookupBool( ATTR_WANT_CONTAINER, wantContainer );

			bool wantDockerRepo = false;
			bool wantSandboxImage = false;
			bool wantSIF = false;
			jobAd->LookupBool(ATTR_WANT_DOCKER_IMAGE, wantDockerRepo);
			jobAd->LookupBool(ATTR_WANT_SANDBOX_IMAGE, wantSandboxImage);
			jobAd->LookupBool(ATTR_WANT_SIF, wantSIF);

			std::string container_image;
			jobAd->LookupString(ATTR_CONTAINER_IMAGE, container_image);
			std::string docker_image;
			jobAd->LookupString(ATTR_DOCKER_IMAGE, docker_image);

			bool hasDocker = false;
			mad->LookupBool(ATTR_HAS_DOCKER, hasDocker);

			// If they give us a docker repo and we have a working
			// docker runtime, use docker universe
			if (wantContainer && wantDockerRepo && hasDocker) {
				wantDocker = true;
			}

			// But if the job doesn't want to run in a container, see
			// if we want to force it into one.
			if (!wantContainer && !wantDocker) {
				if (param_boolean("USE_DEFAULT_CONTAINER", false, false, mad, jobAd)) {
					dprintf(D_ALWAYS, "USE_DEFAULT_CONTAINER evaluates to true, containerizing job\n");
					std::string default_container_image;
					if (param_eval_string(default_container_image, "DEFAULT_CONTAINER_IMAGE", "", mad, jobAd)) {
						dprintf(D_ALWAYS, "... putting job into container image %s\n", default_container_image.c_str());
						if (default_container_image.starts_with("docker:") && hasDocker) {
							wantDocker = true;
							jobAd->Assign(ATTR_DOCKER_IMAGE, default_container_image);
							jobAd->Assign(ATTR_WANT_DOCKER_IMAGE, true);
						} else {
							wantContainer = true;
							jobAd->Assign(ATTR_CONTAINER_IMAGE, default_container_image);
							if (default_container_image.ends_with(".sif")) {
								jobAd->Assign(ATTR_WANT_SIF, true);
							} else {
								jobAd->Assign(ATTR_WANT_SANDBOX_IMAGE, true);
							}
						}
					} else {
						dprintf(D_ALWAYS, "... but DEFAULT_CONTAINER_IMAGE doesn't evaluate to a string, skippping containerizing\n");
					}
				}
			}

			std::string remote_cmd;
			bool wantRemote = param(remote_cmd, "STARTER_REMOTE_CMD");

			if( wantDocker ) {
				job = new DockerProc( jobAd );
			} else if ( wantRemote ) {
				job = new RemoteProc( jobAd );
			} else {
				job = new VanillaProc( jobAd );
			}
			} break;
		case CONDOR_UNIVERSE_JAVA:
			job = new JavaProc( jobAd, WorkingDir.c_str() );
			break;
	    case CONDOR_UNIVERSE_PARALLEL:
			job = new ParallelProc( jobAd );
			break;
		case CONDOR_UNIVERSE_MPI: {
			EXCEPT("MPI Universe is no longer supported");
			break;
		}
		case CONDOR_UNIVERSE_VM:
			job = new VMProc( jobAd );
			ASSERT(job);
			break;
		default:
			dprintf( D_ALWAYS, "Starter doesn't support universe %d (%s)\n",
					 jobUniverse, CondorUniverseName(jobUniverse) ); 
			return FALSE;
	} /* switch */

	if (job->StartJob()) {
		m_job_list.emplace_back(job);
		
			//
			// If the Starter received a Suspend call while the
			// job was being deferred or preparing, we need to 
			// suspend it right away.
			//
			// I am not sure if it's ok to do this right after
			// calling StartJob() or if the job should never be 
			// allowed to start at all. Is it a big problem if
			// the job is allowed to execute a little bit before
			// they get suspended?
			//
		if ( this->suspended ) {
			this->Suspend( );
		}

		// Now, see if we also need to start up a ToolDaemon
		// for this job.
		char* tool_daemon_name = NULL;
		jobAd->LookupString( ATTR_TOOL_DAEMON_CMD,
							 &tool_daemon_name );
		if( tool_daemon_name ) {
				// we need to start a tool daemon for this job
			ToolDaemonProc* tool_daemon_proc;
			tool_daemon_proc = new ToolDaemonProc( jobAd, job->GetJobPid() );

			if( tool_daemon_proc->StartJob() ) {
				m_job_list.emplace_back( tool_daemon_proc );
				dprintf( D_FULLDEBUG, "ToolDaemonProc added to m_job_list\n");
					//
					// If the Starter received a Suspend call while the
					// job was being deferred or preparing, we need to 
					// suspend it right away.
					//
					// Although a single Suspend() call would have caught
					// both the job and the Tool Daemon, I decided 
					// to have it call Suspend as soon as the job
					// is started. See comments from above
					//
				if ( this->suspended ) {
					this->Suspend( );
				}
			} else {
				dprintf( D_ALWAYS, "Failed to start ToolDaemonProc!\n");
				delete tool_daemon_proc;
			}
			free( tool_daemon_name );
		}

			// let our JobInfoCommunicator know the job was started.
		jic->allJobsSpawned();
		return TRUE;
	} else {
		delete job;
			// Until this starter is supporting something more
			// complex, if we failed to start the job, we're done.
		dprintf( D_ALWAYS, "Failed to start job, exiting\n" );
		main_shutdown_fast();
		return FALSE;
	}
}

void
Starter::WriteRecoveryFile( ClassAd *recovery_ad )
{
	std::string tmp_file;
	FILE *tmp_fp;

	if ( recovery_ad == NULL ) {
		return;
	}

	if ( m_recoveryFile.length() == 0 ) {
		formatstr( m_recoveryFile, "%s%cdir_%ld.recover", Execute,
								DIR_DELIM_CHAR, (long)daemonCore->getpid() );
	}

	formatstr( tmp_file, "%s.tmp", m_recoveryFile.c_str() );

	tmp_fp = safe_fcreate_replace_if_exists( tmp_file.c_str(), "w" );
	if ( tmp_fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open recovery file %s\n", tmp_file.c_str() );
		return;
	}

	if ( fPrintAd( tmp_fp, *recovery_ad ) == FALSE ) {
		dprintf( D_ALWAYS, "Failed to write recovery file\n" );
		fclose( tmp_fp );
		return;
	}

	if ( fclose( tmp_fp ) != 0 ) {
		dprintf( D_ALWAYS, "Failed close recovery file\n" );
		MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
		unlink( tmp_file.c_str() );
		return;
	}

	if ( rotate_file( tmp_file.c_str(), m_recoveryFile.c_str() ) != 0 ) {
		dprintf( D_ALWAYS, "Failed to rename recovery file\n" );
		MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
		unlink( tmp_file.c_str() );
	}
}

void
Starter::RemoveRecoveryFile()
{
	if ( m_recoveryFile.length() > 0 ) {
		MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
		unlink( m_recoveryFile.c_str() );
		m_recoveryFile = "";
	}
}

/**
 * DC Suspend Job Wrapper
 * We notify our JIC that we got a suspend call
 * then invoke Suspend() which does the real work
 * 
 * @param command (not used)
 * @return true if the jobs were suspended
 */ 
int
Starter::RemoteSuspend(int)
{
	int retval = this->Suspend();

		// Notify our JobInfoCommunicator that the jobs are suspended.
		// Ideally, this would be done _before_ suspending the job, so
		// that we commit information about this change of state
		// reliably.  However, this would require additional changes
		// to do it right, because before the job is suspended, the
		// bookkeeping about the state of the job has not been updated yet,
		// so the job info communicator won't advertise the imminent
		// change of state.  For now, we have decided that it is acceptable
		// to simply log suspend events after the change of state.

	jic->Suspend();
	return retval;
}

/**
 * Suspends all the jobs in the queue. We also will set the
 * suspended flag to true so that we know that all new jobs for
 * this Starter that get executed will start suspended
 * 
 * @return true if the jobs were successfully suspended
 */
bool
Starter::Suspend( void ) {
	dprintf(D_ALWAYS, "Suspending all jobs.\n");

	for (auto *job: m_job_list) {
		job->Suspend();
	}
	
		//
		// We set a flag to let us know that if any other
		// job tries to start after we received this Suspend call
		// then they should also be suspended.
		// This can happen if a job was being deferred and when
		// the timer triggers we don't want to let it execute 
		// if the Starter was asked to suspend all jobs
		//
	this->suspended = true;

	return ( true );
}

/**
 * DC Continue Job Wrapper
 * We notify our JIC that we got a continue call
 * then invoke Continue() which does the real work
 * 
 * @param command (not used)
 * @return true if jobs were unsuspended
 */ 
int
Starter::RemoteContinue(int)
{
	int retval = this->Continue();

		// Notify our JobInfoCommunicator that the job is being continued.
		// Ideally, this would be done _before_ unsuspending the job, so
		// that we commit information about this change of state
		// reliably.  However, this would require additional changes
		// to do it right, because before the job is unsuspended, the
		// bookkeeping about the state of the job has not been updated yet,
		// so the job info communicator won't advertise the imminent
		// change of state.  For now, we have decided that it is acceptable
		// to simply log unsuspend events after the change of state.

	jic->Continue();
	return retval;
}

/**
 * Unsuspends all the jobs in the queue. We also unset
 * the suspended flag so that all new jobs that get started
 * are allowed to execute right away
 * 
 * @return true if the jobs were successfully continued
 */
bool
Starter::Continue( void )
{
	dprintf(D_ALWAYS, "Continuing all jobs.\n");

	for (auto *job: m_job_list) {
		if (this->suspended) {
		  job->Continue();
		}
	}
	
		//
		// We can unset the suspended flag 
		// This will allow for a job that was being deferred when
		// the suspend call came in to execute right away when the 
		// deferral timer trips
		//
	this->suspended = false;

	return ( true );
}

/**
 * DC Periodic Checkpoint Job Wrapper
 * Currently only vm universe is supported.
 *
 * @param command (not used)
 * @return true if jobs were checkpointed
 */ 
int
Starter::RemotePeriodicCkpt(int)
{
	return ( this->PeriodicCkpt( ) );
}

/**
 * Periodically checkpoints all the jobs in the queue. 
 * Currently only vm universe is supported.
 * 
 * @return true if the jobs were successfully checkpointed
 */
bool
Starter::PeriodicCkpt( void )
{
	dprintf(D_ALWAYS, "Periodic Checkpointing all jobs.\n");

	bool wantCheckpoint = false;
	if( jobUniverse == CONDOR_UNIVERSE_VM ) {
		wantCheckpoint = 1;
	} else if( jobUniverse == CONDOR_UNIVERSE_VANILLA ) {
		ClassAd * jobAd = jic->jobClassAd();
		jobAd->LookupBool( ATTR_WANT_CHECKPOINT_SIGNAL, wantCheckpoint );
	}
	if( ! wantCheckpoint ) { return false; }

	for (auto *job: m_job_list) {
		if( job->Ckpt() ) {

			bool transfer_ok = jic->uploadWorkingFiles();

			// checkpoint files are successfully generated
			// We need to upload those files by using condor file transfer.
			if( transfer_ok == false ) {
				// Failed to transfer ckpt files
				// What should we do?
				// For now, do nothing.
				dprintf(D_ALWAYS, "Periodic Checkpointing failed.\n");

				// notify ckpt failed
				job->CkptDone(false);
			}else {
				// sending checkpoint files succeeds.
				dprintf(D_ALWAYS, "Periodic Checkpointing is done.\n");

				// update job classAd
				jic->updateCkptInfo();	

				// notify ckpt succeeded.
				job->CkptDone(true);

			}
		}
	}

	return true;
}

void copyProcList( std::vector<UserProc *> & from, std::vector<UserProc *> & to ) {
	to.clear();
	for (auto *job: from) {
		to.emplace_back( job );
	}
}


int
Starter::Reaper(int pid, int exit_status)
{
	int handled_jobs = 0;
	int all_jobs = 0;

	if( WIFSIGNALED(exit_status) ) {
		dprintf( D_ALWAYS, "Process exited, pid=%d, signal=%d\n", pid,
				 WTERMSIG(exit_status) );
	} else {
		dprintf( D_ALWAYS, "Process exited, pid=%d, status=%d\n", pid,
				 WEXITSTATUS(exit_status) );
	}

	if( pre_script && pre_script->JobReaper(pid, exit_status) ) {
		bool exitStatusSpecified = false;
		int desiredExitStatus = computeDesiredExitStatus( "Pre", this->jic->jobClassAd(), & exitStatusSpecified );
		if( exitStatusSpecified && exit_status != desiredExitStatus ) {
			dprintf( D_ALWAYS, "Pre script failed, putting job on hold.\n" );

			ClassAd updateAd;
			publishUpdateAd( & updateAd );
			CopyAttribute( ATTR_ON_EXIT_CODE, updateAd, "PreExitCode", updateAd );
			jic->periodicJobUpdate( & updateAd );

			// This kills the shadow, which should cause us to catch a
			// SIGQUIT from the startd in short order...
			jic->holdJob( "Pre script failed.",
				CONDOR_HOLD_CODE::PreScriptFailed,
				0 );

			// ... but we might as well do what the SIGQUIT handler does
			// here and call main_shutdown_fast().  Maybe someday we'll
			// fix main_shutdown_fast(), or write a similar function, that
			// won't write spurious errors to the log.
			main_shutdown_fast();
			return FALSE;
		}

			// when the pre script exits, we know the m_job_list is
			// going to be empty, so don't bother with any of the rest
			// of this.  instead, the starter is now able to call
			// SpawnJob() to launch the main job.
		pre_script = NULL; // done with pre-script
		if( ! SpawnJob() ) {
			dprintf( D_ALWAYS, "Failed to start main job, exiting\n" );
			main_shutdown_fast();
			return FALSE;
		}
		return TRUE;
	}

	if( post_script && post_script->JobReaper(pid, exit_status) ) {
		bool exitStatusSpecified = false;
		int desiredExitStatus = computeDesiredExitStatus( "Post", this->jic->jobClassAd(), & exitStatusSpecified );
		if( exitStatusSpecified && exit_status != desiredExitStatus ) {
			dprintf( D_ALWAYS, "Post script failed, putting job on hold.\n" );

			ClassAd updateAd;
			publishUpdateAd( & updateAd );
			CopyAttribute( ATTR_ON_EXIT_CODE, updateAd, "PostExitCode", updateAd );
			jic->periodicJobUpdate( & updateAd );

			// This kills the shadow, which should cause us to catch a
			// SIGQUIT from the startd in short order...
			jic->holdJob( "Post script failed.",
				CONDOR_HOLD_CODE::PostScriptFailed,
				0 );

			// ... but we might as well do what the SIGQUIT handler does
			// here and call main_shutdown_fast().  Maybe someday we'll
			// fix main_shutdown_fast(), or write a similar function, that
			// won't write spurious errors to the log.
			main_shutdown_fast();
			return FALSE;
		}

			// when the post script exits, we know the m_job_list is
			// going to be empty, so don't bother with any of the rest
			// of this.  instead, the starter is now able to call
			// allJobsdone() to do the final clean up stages.
		allJobsDone();
		return TRUE;
	}

	//
	// The ToE tag code, via Starter::publishUpdateAd() -- and in the
	// future, maybe other features via and/or means in the future --
	// calls Rewind() on m_job_list as well.  Rather than fixing only
	// the ToE tag code and leaving this landmine, copy m_job_list
	// before calling any of the JobReaper()s.  Because of this problem,
	// we know that none of the other existing JobReaper() methods
	// look at m_job_list or m_reaped_job_list, since they'd have the
	// same problem, so it's safe to save updating those for the end.
	//

	// Is there a good reason to have neither assignment nor copy ctor?
	std::vector<UserProc *> stable_job_list;
	std::vector<UserProc *> stable_reaped_job_list;

	copyProcList( m_job_list, stable_job_list );
	copyProcList( m_reaped_job_list, stable_reaped_job_list );

	auto listit = stable_job_list.begin();
	while (listit != stable_job_list.end()) {
		auto *job = *listit;
		all_jobs++;
		if( job->GetJobPid()==pid && job->JobReaper(pid, exit_status) ) {
			handled_jobs++;
			listit = stable_job_list.erase(listit);
			stable_reaped_job_list.emplace_back(job);
		} else {
			listit++;
		}
	}

	copyProcList( stable_reaped_job_list, m_reaped_job_list );
	copyProcList( stable_job_list, m_job_list );

	dprintf( D_FULLDEBUG, "Reaper: all=%d handled=%d ShuttingDown=%d\n",
			 all_jobs, handled_jobs, ShuttingDown );

	if( handled_jobs == 0 ) {
		dprintf( D_ALWAYS, "unhandled job exit: pid=%d, status=%d\n",
				 pid, exit_status );
	}
	if( all_jobs - handled_jobs == 0 ) {
		if( post_script ) {
				// if there's a post script, we have to call it now,
				// and wait for it to exit before we do anything else
				// of interest.
			post_script->StartJob();
			return TRUE;
		} else {
				// if there's no post script, we're basically done.
				// so, we can directly call allJobsDone() to do final
				// cleanup.
			if( !allJobsDone() ) {
				dprintf(D_ALWAYS, "Returning from Starter::JobReaper()\n");
				return 0;
			}
		}
	}

	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		StarterExit(GetShutdownExitCode());
	}

	return 0;
}


bool
Starter::allJobsDone( void )
{
	m_all_jobs_done = true;
	bool bRet=false;
	dprintf(D_ZKM, "Starter::allJobsDone()\n");

		// No more jobs, notify our JobInfoCommunicator.
	if (jic->allJobsDone()) {
			// JIC::allJobsDone returned true: we're ready to move on.
		bRet=transferOutput();
	}

	if (m_deferred_job_update){
		jic->notifyJobExit( -1, JOB_SHOULD_REQUEUE, 0 );
	}
		// JIC::allJobsDone() returned false: propagate that so we
		// halt the cleanup process and wait for external events.
	return bRet;
}


bool
Starter::transferOutput( void )
{
	bool transient_failure = false;

	dprintf(D_ZKM, "Starter::transferOutput()\n");

	if( recorded_job_exit_status ) {
		bool exitStatusSpecified = false;
		int desiredExitStatus = computeDesiredExitStatus( "", this->jic->jobClassAd(), & exitStatusSpecified );
		if( exitStatusSpecified && job_exit_status != desiredExitStatus ) {
			jic->setJobFailed();
		}
	}

    // Try to transfer output (the failure files) even if we didn't succeed
    // in job setup (e.g., transfer input files), but just ignore any
    // output-transfer failures in the case of a setup failure; we can only
    // really report one thing, and that should be the setup failure,
    // which happens as a result of calling cleanupJobs().
	if (jic->transferOutput(transient_failure) == false && m_setupStatus == 0) {

		if( transient_failure ) {
				// we will retry the transfer when (if) the shadow reconnects
			return false;
		}

		// Send, if the JIC thinks it is talking to a shadow of the right
		// version, a message about the termination of the job to the shadow in
		// the event of a file transfer failure.  The UserProc's classad which
		// has an ATTR_JOB_PID attribute is the actual job the starter ran on
		// behalf of the user. For other types of jobs like pre/post scripts,
		// that attribute is name mangled and won't be present as ATTR_JOB_PID
		// in the published ad.
		//
		// See the usage of the "name" variable in user_proc.h/cpp

		for (auto *job: m_reaped_job_list) {
			ClassAd ad;
			int pid;
			job->PublishUpdateAd(&ad);
			if (ad.LookupInteger(ATTR_JOB_PID, pid)) {
				jic->notifyJobTermination(job);
				break;
			}
		}

		jic->transferOutputMopUp();

			/*
			  there was an error with the JIC in this step.  at this
			  point, the only possible reason is if we're talking to a
			  shadow and file transfer failed to send back the files.
			  in this case, just return to DaemonCore and wait for
			  other events (like the shadow reconnecting or the startd
			  deciding the job lease expired and killing us)
			*/
		dprintf( D_ALWAYS, "JIC::transferOutput() failed, waiting for job "
				 "lease to expire or for a reconnect attempt\n" );
		return false;
	}

	jic->transferOutputMopUp();

		// If we're here, the JIC successfully transfered output.
		// We're ready to move on to the next cleanup stage.
	return cleanupJobs();
}


bool
Starter::cleanupJobs( void )
{
		// Now that we're done with HOOK_JOB_EXIT and transfering
		// files, we can finally go through the m_reaped_job_list and
		// call JobExit() on all the procs in there.
	auto listit = m_reaped_job_list.begin();
	while (listit != m_reaped_job_list.end()) {
		auto *job = *listit;
		if( job->JobExit() ) {
			listit = m_reaped_job_list.erase(listit);
			delete job;
		} else {
				// This could fail because either we're talking to a
				// shadow and got disconnected, or we're talking to
				// the schedd and failed to update the job queue.
				// TODO: make this accurate for local universe,
				// e.g. by adding a JIC method to print this?
			dprintf( D_ALWAYS, "JobExit() failed, waiting for job "
					 "lease to expire or for a reconnect attempt\n" );
			return false;
		}
	}

	// If setup failed and we should hold the job, notify the startd
	if (m_setupStatus == JOB_SHOULD_HOLD) {
		jic->notifyStarterError(m_urea.message.c_str(), true, m_urea.hold_code, m_urea.hold_subcode);
	}

		// No more jobs, all cleanup done, notify our JIC
	jic->allJobsGone();
	return true;
}


bool
Starter::publishUpdateAd( ClassAd* ad )
{
	return publishJobInfoAd(&m_job_list, ad);
}


bool
Starter::publishJobExitAd( ClassAd* ad )
{
	return publishJobInfoAd(&m_reaped_job_list, ad);
}


bool
Starter::publishJobInfoAd(std::vector<UserProc *> *proc_list, ClassAd* ad)
{
		// Iterate through all our UserProcs and have those publish,
		// as well.  This method is virtual, so we'll get all the
		// goodies from derived classes, as well.  If any of them put
		// info into the ad, return true.  Otherwise, return false.
	bool found_one = false;
	if( pre_script && pre_script->PublishUpdateAd(ad) ) {
		found_one = true;
	}
	
	for (auto *job: *proc_list) {
		if( job->PublishUpdateAd(ad) ) {
			found_one = true;
		}
	}
	if( post_script && post_script->PublishUpdateAd(ad) ) {
		found_one = true;
	}
	if (!m_vacateReason.empty()) {
		ad->Assign(ATTR_VACATE_REASON, m_vacateReason);
		ad->Assign(ATTR_VACATE_REASON_CODE, m_vacateCode);
		ad->Assign(ATTR_VACATE_REASON_SUBCODE, m_vacateSubcode);
		found_one = true;
	}
	return found_one;
}


bool
Starter::publishPreScriptUpdateAd( ClassAd* ad )
{
	if( pre_script && pre_script->PublishUpdateAd(ad) ) {
		return true;
	}
	return false;
}


bool
Starter::publishPostScriptUpdateAd( ClassAd* ad )
{
	if( post_script && post_script->PublishUpdateAd(ad) ) {
		return true;
	}
	return false;
}

FILE *
Starter::OpenManifestFile( const char * filename, bool add_to_output )
{
	// We should be passed in a filename that is a relavtive path
	ASSERT(filename != NULL);
	ASSERT(!IS_ANY_DIR_DELIM_CHAR(filename[0]));

	// Makes no sense if we don't have a job info communicator or job ad
	const ClassAd *job_ad = NULL;
	if (jic) {
		job_ad = jic->getJobAd();
	}
	if (!job_ad) {
		dprintf( D_ERROR, "OpenManifestFile(%s): invoked without a job classad\n",
			filename);
		return NULL;
	}

	// Confirm we really are in user priv before continuing; perhaps
	// we are still in condor priv if, for insatnce, thus method is invoked
	// before initializing the user ids.
	TemporaryPrivSentry sentry(PRIV_USER);
	if ( get_priv_state() != PRIV_USER ) {
		dprintf( D_ERROR, "OpenManifestFile(%s): failed to switch to PRIV_USER\n",
			filename);
		return NULL;

	}

	// The rest of this method assumes we are in the job sandbox,
	// so set cwd to the sandbox (but reset the cwd when we return)
	std::string errMsg;
	TmpDir tmpDir;
	if (!tmpDir.Cd2TmpDir(GetWorkingDir(0),errMsg)) {
		dprintf( D_ERROR, "OpenManifestFile(%s): failed to cd to job sandbox %s\n",
			filename, GetWorkingDir(0));
		return NULL;

	}

	std::string dirname = "_condor_manifest";
	int cluster, proc;
	if( job_ad->LookupInteger( ATTR_CLUSTER_ID, cluster ) && job_ad->LookupInteger( ATTR_PROC_ID, proc ) ) {
		formatstr( dirname, "%d_%d_manifest", cluster, proc );
	}
	job_ad->LookupString( ATTR_JOB_MANIFEST_DIR, dirname );
	int r = mkdir( dirname.c_str(), 0700 );
	if (r < 0 && errno != 17) {
		dprintf( D_ERROR, "OpenManifestFile(%s): failed to make directory %s: (%d) %s\n",
			filename, dirname.c_str(), errno, strerror(errno));
		return NULL;
	}
	if( add_to_output ) { jic->addToOutputFiles( dirname.c_str() ); }
	std::string f = dirname + DIR_DELIM_CHAR + filename;

	FILE * file = fopen( f.c_str(), "w" );
	if( file == NULL ) {
		dprintf( D_ERROR, "OpenManifestFile(%s): failed to open log '%s': %d (%s)\n",
			filename, f.c_str(), errno, strerror(errno) );
		return NULL;
	}

	dprintf(D_STATUS, "Writing into manifest file '%s'\n", f.c_str() );

	return file;
}

bool
Starter::GetJobEnv( ClassAd *jobad, Env *job_env, std::string & env_errors )
{
	char *env_str = param( "STARTER_JOB_ENVIRONMENT" );

	ASSERT( jobad );
	ASSERT( job_env );
	if( !job_env->MergeFromV1RawOrV2Quoted(env_str,env_errors) ) {
		formatstr_cat(env_errors,
			" The full value for STARTER_JOB_ENVIRONMENT: %s\n",env_str);
		free(env_str);
		return false;
	}
	free(env_str);

	if(!job_env->MergeFrom(jobad,env_errors)) {
		formatstr_cat(env_errors,
				" (This error was from the environment string in the job "
				"ClassAd.)");
		return false;
	}

		// Now, let the starter publish any env vars it wants to into
		// the mainjob's env...
	PublishToEnv( job_env );
	return true;
}

// helper function
static void SetEnvironmentForAssignedRes(Env* proc_env, const char * proto, const char * assigned, const char * tag);

bool expandScratchDirInEnv(void * void_scratch_dir, const std::string & /*lhs */, std::string &rhs) {
	const char *scratch_dir = (const char *) void_scratch_dir;
	replace_str(rhs, "#CoNdOrScRaTcHdIr#", scratch_dir);
	return true;
}

void
Starter::PublishToEnv( Env* proc_env )
{
	ASSERT(proc_env);

		// Write BATCH_SYSTEM environment variable to indicate HTCondor is the batch system.
	proc_env->SetEnv("BATCH_SYSTEM","HTCondor");

	if( pre_script ) {
		pre_script->PublishToEnv( proc_env );
	}
		// we don't have to worry about post, since it's going to run
		// after everything else, so there's not going to be any info
		// about it to pass until it's already done.

		// used by sshd_shell_setup script to cd to the job working dir
	proc_env->SetEnv("_CONDOR_JOB_IWD",jic->jobRemoteIWD());

	std::string job_pids;
	for (auto *uproc: m_job_list) {
		uproc->PublishToEnv( proc_env );

		if( ! job_pids.empty() ) {
			job_pids += " ";
		}
		formatstr_cat(job_pids, "%d", uproc->GetJobPid());		
	}
		// put the pid of the job in the environment, used by sshd and hooks
	proc_env->SetEnv("_CONDOR_JOB_PIDS",job_pids);

		// put the value of BIN into the environment; used by sshd to find
		// condor_config_val. also helpful to find condor_chirp.
	std::string condorBinDir;
	param(condorBinDir,"BIN");
	if (!condorBinDir.empty()) {
		proc_env->SetEnv("_CONDOR_BIN",condorBinDir.c_str());
	}

		// put in environment variables specific to the type (universe) of job
	for (auto *uproc: m_reaped_job_list) {
		uproc->PublishToEnv( proc_env );	// a virtual method per universe
	}

	ASSERT(jic);
	ClassAd* jobAd = jic->jobClassAd();
	if( jobAd ) {
		// Probing this (file transfer) ourselves is complicated.  The JIC
		// already does it.  Steal his answer.
		bool using_file_transfer = jic->usingFileTransfer();
		build_job_env( *proc_env, *jobAd, using_file_transfer );
	} else {
		dprintf(D_ALWAYS, "Unable to find job ad for job.  Environment may be incorrect\n");
	}

		// now, stuff the starter knows about, instead of individual
		// procs under its control
	const char * base = "_CONDOR_";
 
	std::string env_name;

		// if there are non-fungible assigned resources (i.e. GPUs) pass those assignments down in the environment
		// we look through all machine resource names looking for an attribute Assigned*, if we find one
		// then we publish it's value in the environment as _CONDOR_Assigned*,  so for instance, if there is
		// an AssignedGPU attribute in the machine add, there will be a _CONDOR_AssignedGPU environment
		// variable with the same value.
	ClassAd * mad = jic->machClassAd();
	if (mad) {
		std::string restags;
		if (mad->LookupString(ATTR_MACHINE_RESOURCES, restags)) {
			std::vector<std::string> tags = split(restags);
			for (const auto& tag: tags) {
				std::string attr("Assigned"); attr += tag;

				// we need to publish Assigned resources in the environment. the rules are 
				// a bit wierd here. we publish if there are any assigned, we also always
				// publish if there is a config knob ENVIRONMENT_FOR_Assigned<tag> even if
				// there are none assigned, so long as there are any defined.
				std::string env_name;
				std::string param_name("ENVIRONMENT_FOR_"); param_name += attr;
				param(env_name, param_name.c_str());

				std::string assigned;
				bool is_assigned = mad->LookupString(attr, assigned);
				if (is_assigned || (mad->Lookup(tag) &&  ! env_name.empty())) {

					if ( ! is_assigned) {
						param_name = "ENVIRONMENT_VALUE_FOR_UN"; param_name += attr;
						param(assigned, param_name.c_str());
					}

					if ( ! env_name.empty()) {
						SetEnvironmentForAssignedRes(proc_env, env_name.c_str(), assigned.c_str(), tag.c_str());
					}

					env_name = base;
					env_name += attr;
					proc_env->SetEnv(env_name.c_str(), assigned.c_str());
				}
			}

			// NVIDIA_VISIBLE_DEVICES needs to be set to an expression evaluated against the machine ad
			// which may not be the same exact value as what we set CUDA_VISIBLE_DEVICES to
			if (contains_anycase(tags, "GPUs") && param_boolean("AUTO_SET_NVIDIA_VISIBLE_DEVICES",true)) {
				classad::Value val;
				const char * env_value = nullptr;
				if (mad->EvaluateExpr("join(\",\",evalInEachContext(strcat(\"GPU-\",DeviceUuid),AvailableGPUs))", val)
					&& val.IsStringValue(env_value) && strlen(env_value) > 0) {
					proc_env->SetEnv("NVIDIA_VISIBLE_DEVICES", env_value);
				} else {
					proc_env->SetEnv("NVIDIA_VISIBLE_DEVICES", "none");
				}
			}
		}
	}

	// put OAuth creds directory into the environment
	if (jic->getCredPath()) {
		proc_env->SetEnv("_CONDOR_CREDS", jic->getCredPath());
	}

	// kerberos credential cache (in sandbox)
	const char* krb5ccname = jic->getKrb5CCName();
	if( krb5ccname && (krb5ccname[0] != '\0') ) {
		// using env_name as env_value
		env_name = "FILE:";
		env_name += krb5ccname;
		proc_env->SetEnv( "KRB5CCNAME", env_name );
	}

		// path to the output ad, if any
	const char* output_ad = jic->getOutputAdFile();
	if( output_ad && !(output_ad[0] == '-' && output_ad[1] == '\0') ) {
		env_name = base;
		env_name += "OUTPUT_CLASSAD";
		proc_env->SetEnv( env_name.c_str(), output_ad );
	}
	
		// job scratch space
	env_name = base;
	env_name += "SCRATCH_DIR";
	proc_env->SetEnv( env_name.c_str(), GetWorkingDir(true) );

	    // Apptainer/Singlarity scratch dir
	proc_env->SetEnv("APPTAINER_CACHEDIR", GetWorkingDir(true));
	proc_env->SetEnv("SINGULARITY_CACHEDIR", GetWorkingDir(true));
		// slot identifier
	env_name = base;
	env_name += "SLOT";
	
	proc_env->SetEnv(env_name, getMySlotName());

		// pass through the pidfamily ancestor env vars this process
		// currently has to the job.

		// port regulation stuff.  assume the outgoing port range.
	int low, high;
	if (get_port_range (TRUE, &low, &high) == TRUE) {
		std::string tmp_port_number;

		tmp_port_number = std::to_string( high );
		env_name = base;
		env_name += "HIGHPORT";
		proc_env->SetEnv( env_name.c_str(), tmp_port_number.c_str() );

		tmp_port_number = std::to_string( low );
		env_name = base;
		env_name += "LOWPORT";
		proc_env->SetEnv( env_name.c_str(), tmp_port_number.c_str() );
    }

		// set environment variables for temporary directories
		// Condor will clean these up on job exits, and there's
		// no chance of file collisions with other running slots

	std::string tmpdirenv = this->tmpdir.empty() ? GetWorkingDir(true) : this->tmpdir;
	proc_env->SetEnv("TMPDIR", tmpdirenv);
	proc_env->SetEnv("TEMP",tmpdirenv);
	proc_env->SetEnv("TMP", tmpdirenv);

		// Programs built with OpenMP (including matlab, gnu sort
		// and others) look at OMP_NUM_THREADS
		// to determine how many threads to spawn.  Force this to
		// Cpus, to encourage jobs to stay within the number
		// of requested cpu cores.  But trust the user, if it has
		// already been set in the job.
		// In addition, besides just OMP_NUM_THREADS, set an environment
		// variable for each var name listed in STARTER_NUM_THREADS_ENV_VARS.

	ClassAd * mach = jic->machClassAd();
	int cpus = 0;
	if (mach) {
		mach->LookupInteger(ATTR_CPUS, cpus);
	}
	char* cpu_vars_param = param("STARTER_NUM_THREADS_ENV_VARS");
	if (cpus > 0 && cpu_vars_param) {
		std::string jobNumThreads;
		for (const auto& var: StringTokenIterator(cpu_vars_param)) {
			proc_env->GetEnv(var, jobNumThreads);
			if (jobNumThreads.length() == 0) {
				proc_env->SetEnv(var, std::to_string(cpus));
			}
		}
	}
	free(cpu_vars_param);

		// If using a job wrapper, set environment to location of
		// wrapper failure file.

	char *wrapper = param("USER_JOB_WRAPPER");
	if (wrapper) {
			// setenv only if wrapper actually exists
		if ( access(wrapper,X_OK) >= 0 ) {
			std::string wrapper_err;
			formatstr(wrapper_err, "%s%c%s", GetWorkingDir(0),
						DIR_DELIM_CHAR,
						JOB_WRAPPER_FAILURE_FILE);
			proc_env->SetEnv("_CONDOR_WRAPPER_ERROR_FILE", wrapper_err);
		}
		free(wrapper);
	}

		// Set a bunch of other env vars that used to be set
		// in OsProc::StartJob(), but we want to set them here
		// so they will also appear in ssh_to_job environments.

	std::string path;
	formatstr(path, "%s%c%s", GetWorkingDir(true),
			 	DIR_DELIM_CHAR,
				MACHINE_AD_FILENAME);
	if( ! proc_env->SetEnv("_CONDOR_MACHINE_AD", path) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_MACHINE_AD environment variable\n");
	}

	if( jic->wroteChirpConfig() && 
		(! proc_env->SetEnv("_CONDOR_CHIRP_CONFIG", jic->chirpConfigFilename())) ) 
	{
		dprintf( D_ALWAYS, "Failed to set _CONDOR_CHIRP_CONFIG environment variable.\n");
	}

	formatstr(path, "%s%c%s", GetWorkingDir(true),
			 	DIR_DELIM_CHAR,
				JOB_AD_FILENAME);
	if( ! proc_env->SetEnv("_CONDOR_JOB_AD", path) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_JOB_AD environment variable\n");
	}

	std::string remoteUpdate;
	param(remoteUpdate, "CHIRP_DELAYED_UPDATE_PREFIX", "CHIRP");
	if( ! proc_env->SetEnv("_CHIRP_DELAYED_UPDATE_PREFIX", remoteUpdate) ) {
		dprintf( D_ALWAYS, 
				"Failed to set _CHIRP_DELAYED_UPDATE_PREFIX environment variable\n");
	}

	// Many jobs need an absolute path into the scratch directory in an environment var
	// expand a magic string in an env var to the scratch dir
	proc_env->Walk(&expandScratchDirInEnv, (void *)const_cast<char *>(GetWorkingDir(true)));
}

// parse an environment prototype string of the form  key[[=/regex/replace/] key2=/regex2/replace2/]
// where the / that separates the parts of the regex can be any non-whitespace character
//
static void SetEnvironmentForAssignedRes(Env* proc_env, const char * proto, const char * assigned, const char * tag)
{
	const char * env_id_separator = ","; // maybe get this from config someday?

	// multiple prototypes are permitted.
	for (;;) {
		while (isspace(*proto)) ++proto;
		if ( ! *proto) { return; }

		std::string rhs = ""; // the value that we will (eventually) assign to the environment variable

		dprintf(D_ALWAYS | D_FULLDEBUG, "Assigned%s environment proto '%s'\n", tag, proto);

		const char * peq = strchr(proto, '=');
		if ( ! peq) {
			// special case - if no equal sign, the proto is just an environment name
			// HACK!! 
			// very special case, when proto is CUDA_VISIBLE_DEVICES, or GPU_DEVICE_ORDINAL
			// we know that we have to strip the alpha part of the GPU id, as well as any whitespace.
			if (MATCH == strcmp(proto, "CUDA_VISIBLE_DEVICES") || MATCH == strcmp(proto, "GPU_DEVICE_ORDINAL")) {
				// strip everthing but digits and , from the assigned gpus value
				for (const char * p = assigned; *p; ++p) {
					if (isdigit(*p) || *p == ',') rhs += *p;
				}
				assigned = rhs.c_str();
			}
			dprintf(D_ALWAYS | D_FULLDEBUG, "Assigned%s setting env %s=%s\n", tag, proto, assigned);
			proc_env->SetEnv(proto, assigned);
			return;
		}

		std::string env_name;
		env_name.insert(0, proto, 0, (size_t)(peq - proto));
		++peq; while(isspace(*peq)) ++peq;
		// the next character we see is the regex separator character
		// usually it will be / but it could be any non-whitespace character
		// we expect to see 3 of them. 
		char chRe = *peq;
		const char * pre = peq+1;
		const char * psub = strchr(pre, chRe);
		const char * pend = psub ? strchr(psub+1,chRe) : psub;
		if ( ! psub || ! pend ) {
			dprintf(D_ERROR, "Assigned%s environment '%s' ignored - missing replacment end marker: %s\n", tag, env_name.c_str(), peq);
			break;
		}
		// at this point if your expression is /aa/bbb/
		//                              peq----^^ ^   ^
		//                              pre-----| |   |
		//                              psub------|   |
		//                              pend----------|

		// compile the pattern of the regular expression.
		std::string pat;
		pat.insert(0, pre, 0, (psub - pre));

		//const char * errstr = NULL; int erroff= 0;
		int errcode = 0; PCRE2_SIZE erroff = 0;

		PCRE2_SPTR pat_pcre2str = reinterpret_cast<const unsigned char *>(pat.c_str());
		pcre2_code *re = pcre2_compile(pat_pcre2str, PCRE2_ZERO_TERMINATED, 0, &errcode, &erroff, NULL);
		if ( ! re) {
			dprintf(D_ERROR, "Assigned%s environment '%s' regex PCRE2 error code %d at offset %d in: %s\n",
				tag, env_name.c_str(), errcode, static_cast<int>(erroff), pat.c_str());
			break;
		}

		// HACK! special magic for CUDA_VISIBLE_DEVICES with no pattern supplied
		if (env_name == "CUDA_VISIBLE_DEVICES" && pat.empty()) {
			// strip everthing but digits and , from the assigned gpus value
			for (const char * p = assigned; *p; ++p) {
				if (isdigit(*p) || *p == ',') rhs += *p;
			}
		} else {
			pcre2_match_data * matchdata = pcre2_match_data_create_from_pattern(re, NULL);

			dprintf(D_ALWAYS | D_FULLDEBUG, "Assigned%s environment '%s' pattern: %s\n", tag, env_name.c_str(), peq);

			for (const auto& resid: StringTokenIterator(assigned)) {
				if ( ! rhs.empty()) { rhs += env_id_separator; }
				int cchresid = (int)resid.size();
				PCRE2_SPTR resid_pcre2str = reinterpret_cast<const unsigned char *>(resid.c_str());
				int status = pcre2_match(re, resid_pcre2str, static_cast<PCRE2_SIZE>(cchresid), 0, 0, matchdata, NULL);
				if (status >= 0) {
					const struct _pcre_vector { int start; int end; } * groups
						= (const struct _pcre_vector*) pcre2_get_ovector_pointer(matchdata);
					dprintf(D_ALWAYS | D_FULLDEBUG, "Assigned%s environment '%s' match at %d,%d of pattern: %s\n", tag, env_name.c_str(), groups[0].start, groups[0].end, pat.c_str());
					if (groups[0].start > 0) { rhs.append(resid, 0, groups[0].start); }
					const char * ps = psub;
					while (*++ps && ps < pend) {
						if (ps[0] == '$' && isdigit(ps[1])) {
							int ngrp = *++ps - '0';
							if (ngrp < status) { rhs.append(resid, groups[ngrp].start, groups[ngrp].end - groups[ngrp].start); }
						} else {
							rhs += *ps;
						}
					}
					if (groups[0].end < cchresid) { rhs.append(resid, groups[0].end, cchresid - groups[0].end); }
				} else {
					dprintf(D_ALWAYS | D_FULLDEBUG, "Assigned%s environment '%s' no-match of pattern: %s\n", tag, env_name.c_str(), pat.c_str());
					rhs += resid;
				}
			}
			pcre2_match_data_free(matchdata);
		}

		pcre2_code_free(re);

		proc_env->SetEnv(env_name.c_str(), rhs.c_str());

		proto = pend+1;
		if (*proto == ',') ++proto; // in case there is a comma separating the fields.
	}
}

int
Starter::getMySlotNumber( void )
{
	int slot_number = 0; // default to 0, let our caller decide how to interpret that.

#if 1
	std::string slot_name;
	if (param(slot_name, "STARTER_SLOT_NAME")) {
		// find the first number in the slot name, that's our slot number.
		const char * tmp = slot_name.c_str();
		while (*tmp && (*tmp < '0' || *tmp > '9')) ++tmp;
		if (*tmp) slot_number = atoi(tmp);
	} else {
		// legacy (before 8.1.5), assume that the log filename ends with our slot name.
		slot_name = this->getMySlotName();
		if ( ! slot_name.empty()) {
			std::string prefix;
			if ( ! param(prefix, "STARTD_RESOURCE_PREFIX")) {
				prefix = "slot";
			}

			const char * tmp = strstr(slot_name.c_str(), prefix.c_str());
			if (tmp) {
				prefix += "%d";
				if (sscanf(tmp, prefix.c_str(), &slot_number) < 1) {
					// if we couldn't parse it, leave it at 0.
					slot_number = 0;
				}
			}
		}
	}
#else

	char *logappend = param("STARTER_LOG");		
	char const *tmp = NULL;
		
			
	if ( logappend ) {
			// We currently use the extension of the starter log file
			// name to determine which slot we are.  Strange.
		char const *log_basename = condor_basename(logappend);
		std::string prefix;

		char* resource_prefix = param("STARTD_RESOURCE_PREFIX");
		if( resource_prefix ) {
			formatstr(prefix, ".%s", resource_prefix);
			free( resource_prefix );
		}
		else {
			prefix = ".slot";
		}

		tmp = strstr(log_basename, prefix.c_str());
		if ( tmp ) {				
			prefix += "%d";
			if ( sscanf(tmp, prefix.c_str(), &slot_number) < 1 ) {
				// if we couldn't parse it, leave it at 0.
				slot_number = 0;
			}
		} 

		free(logappend);
	}
#endif

	return slot_number;
}

std::string
Starter::getMySlotName(void)
{
	std::string slotName = "";
	if (param(slotName, "STARTER_SLOT_NAME")) {
		return slotName;
	}

	// legacy (before 8.1.5), assume that the log filename ends with our slot name.
	char *logappend = param("STARTER_LOG");
	if ( logappend ) {
			// We currently use the extension of the starter log file
			// name to determine which slot we are.  Strange.
		char const *log_basename = condor_basename(logappend);
		std::string prefix;

		char* resource_prefix = param("STARTD_RESOURCE_PREFIX");
		if( resource_prefix ) {
			formatstr(prefix, ".%s", resource_prefix);
			free( resource_prefix );
		}
		else {
			prefix = ".slot";
		}

		const char *tmp = strstr(log_basename, prefix.c_str());
		if ( tmp ) {				
			slotName = (tmp + 1); // skip the .
		} 

		free(logappend);
	}

	return slotName;
}


void
Starter::closeSavedStdin( void )
{
	if( starter_stdin_fd > -1 ) {
		close( starter_stdin_fd );
		starter_stdin_fd = -1;
	}
}


void
Starter::closeSavedStdout( void )
{
	if( starter_stdout_fd > -1 ) {
		close( starter_stdout_fd );
		starter_stdout_fd = -1;
	}
}


void
Starter::closeSavedStderr( void )
{
	if( starter_stderr_fd > -1 ) {
		close( starter_stderr_fd );
		starter_stderr_fd = -1;
	}
}


int
Starter::classadCommand( int, Stream* s ) const
{
	ClassAd ad;
	ReliSock* rsock = (ReliSock*)s;
	int cmd = 0;

	cmd = getCmdFromReliSock( rsock, &ad, false );

	switch( cmd ) {
	case FALSE:
			// error in getCmd().  it will already have dprintf()'ed
			// aobut it, so all we have to do is return
		return FALSE;
		break;

	case CA_RECONNECT_JOB:
			// hand this off to our JIC, since it will know what to do
		return jic->reconnect( rsock, &ad );
		break;

	default:
		const char* tmp = getCommandString(cmd);
		std::string err_msg = "Starter does not support command (";
		err_msg += tmp ? tmp : NULL;
		err_msg += ')';
		sendErrorReply( s, tmp, CA_INVALID_REQUEST, err_msg.c_str() );
		return FALSE;
	}
	return TRUE;
}


int 
Starter::updateX509Proxy( int cmd, Stream* s )
{
	ASSERT(s);
	ReliSock* rsock = (ReliSock*)s;
	ASSERT(jic);
	return jic->updateX509Proxy(cmd,rsock) ? TRUE : FALSE;
}


bool
Starter::removeTempExecuteDir(int& exit_code)
{
	if( is_gridshell ) {
			// we didn't make our own directory, so just bail early
		return true;
	}

	std::string dir_name = "dir_";
	dir_name += std::to_string( daemonCore->getpid() );

	bool has_failed = false;

	// since we chdir()'d to the execute directory, we can't
	// delete it until we get out (at least on WIN32). So lets
	// just chdir() to EXECUTE so we're sure we can remove it.
	if (chdir(Execute)) {
		dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n", Execute, strerror(errno));
	}

#ifdef LINUX
	if (m_lv_handle) {
		CondorError err;
		if ( ! m_lv_handle->CleanupLV(err)) {
			dprintf(D_ERROR, "Failed to cleanup LV: %s\n", err.getFullText().c_str());
			bool mark_broken = param_boolean("LVM_CLEANUP_FAILURE_MAKES_BROKEN_SLOT", false);
			if (mark_broken && exit_code < STARTER_EXIT_BROKEN_RES_FIRST) {
				if (exit_code != STARTER_EXIT_NORMAL) {
					dprintf(D_STATUS, "Upgrading exit code from %d to %d\n",
					        exit_code, STARTER_EXIT_IMMORTAL_LVM);
				}
				exit_code = STARTER_EXIT_IMMORTAL_LVM;
			}
		}
		m_lv_handle.reset(nullptr);
	}
#endif /* LINUX */

	// Remove the directory from all possible chroots.
	// On Windows, we expect the root_dir_list to have only a single entry - "/"
	std::string full_exec_dir(Execute);
	pair_strings_vector root_dirs = root_dir_list();
	for (pair_strings_vector::const_iterator it=root_dirs.begin(); it != root_dirs.end(); ++it) {
		if (it->second == "/") {
			// if the root is /, just use the execute dir.  we do this because dircat doesn't work
			// correctly on windows when cat'ing  / + c:\condor\execute
			full_exec_dir = Execute;
		} else {
			// for chroots other than the trivial one, cat the chroot to the configured execute dir
			// we don't expect to ever get here on Windows.
			// If we do get here on Windows, Find_Named_Entry will just fail to find a match
			if ( ! dircat(it->second.c_str(), Execute, full_exec_dir)) {
				continue;
			}
		}

		Directory execute_dir( full_exec_dir.c_str(), PRIV_ROOT );
		if ( execute_dir.Find_Named_Entry( dir_name.c_str() ) ) {

			int closed = dprintf_close_logs_in_directory(execute_dir.GetFullPath(), true);
			if (closed) { dprintf(D_FULLDEBUG, "Closed %d logs in %s\n", closed, execute_dir.GetFullPath()); }

			dprintf(D_FULLDEBUG, "Removing %s\n", execute_dir.GetFullPath());
			if (!execute_dir.Remove_Current_File()) {
				has_failed = true;
			}
		}
	}

	if (!has_failed) {
		m_workingDirExists = false;
	}
	return !has_failed;
}

bool
Starter::WriteAdFiles() const
{

	ClassAd* ad;
	const char* dir = this->GetWorkingDir(0);
	std::string filename;
	FILE* fp;
	bool ret_val = true;

	// Write the job ad first
	ad = this->jic->jobClassAd();
	if (ad != NULL)
	{
		formatstr(filename, "%s%c%s", dir, DIR_DELIM_CHAR, JOB_AD_FILENAME);
		fp = safe_fopen_wrapper_follow(filename.c_str(), "w");
		if (!fp)
		{
			dprintf(D_ALWAYS, "Failed to open \"%s\" for to write job ad: "
						"%s (errno %d)\n", filename.c_str(),
						strerror(errno), errno);
			ret_val = false;
		}
		else
		{
		#ifdef WIN32
			if (has_encrypted_working_dir) {
				DecryptFile(filename.c_str(), 0);
			}
		#endif
			fPrintAd(fp, *ad);
			fclose(fp);
		}
	}
	else
	{
		// If there is no job ad, this is a problem
		ret_val = false;
	}

	ad = this->jic->jobExecutionOverlayAd();
	if (ad && ad->size() > 0)
	{
		formatstr(filename, "%s%c%s", dir, DIR_DELIM_CHAR, JOB_EXECUTION_OVERLAY_AD_FILENAME);
		fp = safe_fopen_wrapper_follow(filename.c_str(), "w");
		if (!fp)
		{
			dprintf(D_ALWAYS, "Failed to open \"%s\" for to write job execution overlay ad: "
						"%s (errno %d)\n", filename.c_str(),
						strerror(errno), errno);
			ret_val = false;
		}
		else
		{
		#ifdef WIN32
			if (has_encrypted_working_dir) {
				DecryptFile(filename.c_str(), 0);
			}
		#endif
			fPrintAd(fp, *ad);
			fclose(fp);
		}
	}

	// Write the machine ad
	ad = this->jic->machClassAd();
	if (ad != NULL)
	{
		formatstr(filename, "%s%c%s", dir, DIR_DELIM_CHAR, MACHINE_AD_FILENAME);
		fp = safe_fopen_wrapper_follow(filename.c_str(), "w");
		if (!fp)
		{
			dprintf(D_ALWAYS, "Failed to open \"%s\" for to write machine "
						"ad: %s (errno %d)\n", filename.c_str(),
					strerror(errno), errno);
			ret_val = false;
		}
		else
		{
		#ifdef WIN32
			if (has_encrypted_working_dir) {
				DecryptFile(filename.c_str(), 0);
			}
		#endif
			fPrintAd(fp, *ad);
			fclose(fp);
		}
	}

	// Correct the bogus Provisioned* attributes in the job ad.
	ClassAd * machineAd = this->jic->machClassAd();
	if( machineAd ) {
		ClassAd updateAd;

		std::string machineResourcesString;
		if(machineAd->LookupString( ATTR_MACHINE_RESOURCES, machineResourcesString)) {
			updateAd.Assign( "ProvisionedResources", machineResourcesString );
			dprintf( D_FULLDEBUG, "Copied machine ad's %s to ProvisionedResources\n", ATTR_MACHINE_RESOURCES );
		} else {
			machineResourcesString = "CPUs, Disk, Memory";
		}

		for (const auto& resourceName: StringTokenIterator(machineResourcesString)) {
			std::string provisionedResourceName;
			formatstr( provisionedResourceName, "%sProvisioned", resourceName.c_str() );
			CopyAttribute( provisionedResourceName, updateAd, resourceName, *machineAd );
			dprintf( D_FULLDEBUG, "Copied machine ad's %s to job ad's %s\n", resourceName.c_str(), provisionedResourceName.c_str() );

			std::string assignedResourceName;
			formatstr( assignedResourceName, "Assigned%s", resourceName.c_str() );
			CopyAttribute( assignedResourceName, updateAd, *machineAd );
			dprintf( D_FULLDEBUG, "Copied machine ad's %s to job ad\n", assignedResourceName.c_str() );
		}

		dprintf( D_FULLDEBUG, "Updating *Provisioned and Assigned* attributes:\n" );
		dPrintAd( D_FULLDEBUG, updateAd );
		jic->periodicJobUpdate( & updateAd );
	}

	return ret_val;
}

void
Starter::RecordJobExitStatus(int status) {
	recorded_job_exit_status = true;
	job_exit_status = status;

    // "When the job exits" is usually synonymous with "when the process
    // spawned by the starter exits", but that's not the case for self-
    // checkpointing jobs, which the starter just spawns again (after
    // transferring the checkpoint) if the exit code indicates that the
    // job checkpointed successfully.  Nothing else in HTCondor currently
    // cares about this, but we've asked to track it (perhaps to see if
    // anything else in HTCondor should care).  See HTCONDOR-861.
    jic->notifyExecutionExit();
}


// Get job working directory disk usage: return bytes used & num dirs + files
DiskUsage
Starter::GetDiskUsage(bool exiting) const {

		StatExecDirMonitor* mon;
		if (exiting && (mon = dynamic_cast<StatExecDirMonitor*>(dirMonitor))) {
#ifdef LINUX
			auto * lv_handle = m_lv_handle.get();
			CondorError err;
			bool trash = true; // Hack to just statvfs in thin lv case
			if ( ! VolumeManager::GetVolumeUsage(lv_handle, mon->du.execute_size, mon->du.file_count, trash, err)) {
				dprintf(D_ERROR, "Failed to get final LV usage: %s\n", err.getFullText().c_str());
			}
#endif /* LINUX */
		}

		if (dirMonitor) { return dirMonitor->GetDiskUsage(); }
		else { return DiskUsage{0,0}; }
	}

#ifdef LINUX
void
Starter::CheckLVUsage( int /* timerID */ )
{
		// Avoid repeatedly triggering
	if (m_lvm_held_job) return;
		// Logic error?
	if (m_lvm_lv_size_kb < 0) return;

		// no handle? no point
	auto * lv_handle = m_lv_handle.get();
	if ( ! lv_handle) return;

	// When the job exceeds its disk usage, there are three possibilities:
	// 1. The backing pool has space remaining, we don't exhaust the extra allocated space (2GB by default),
	//    and this polling catches the over-usage.  In that case, the job goes on hold and everyone's happy.
	// 2. The backing pool has space remaining, the job DOES exhaust the extra allocated space, and the
	//    job gets an ENOSPC before this polling can trigger.  The job may not go on hold and the user
	//    doesn't get a reasonable indication without examining their stderr.  No one's happy.
	// 3. The backing pool fills up due to overcommits.  All writes to othe device pause until enough
	//    space is cleared up.
	//
	// In case (3), even well-behaved jobs will notice the issue; after a minute, if not enough space is
	// available we start evicting even those jobs in oroder to prevent a deadlock.
	//
	// If you really want to avoid case (2), set THINPOOL_EXTRA_SIZE_MB to a value larger than the backing pool.

	StatExecDirMonitor* monitor = static_cast<StatExecDirMonitor*>(dirMonitor);

	CondorError err;
	bool out_of_space = false;
	if ( ! VolumeManager::GetVolumeUsage(lv_handle, monitor->du.execute_size, monitor->du.file_count, out_of_space, err)) {
		dprintf(D_ALWAYS, "Failed to poll managed volume (may not put job on hold correctly): %s\n", err.getFullText().c_str());
		return;
	}

	filesize_t limit = m_lvm_lv_size_kb * 1024LL;
	//Thick provisioning check for 98% LV usage
	if ( ! m_lv_handle->IsThin()) { limit = limit * 0.98; }

	if (monitor->du.execute_size >= limit) {
		std::string hold_msg;
		std::string limit_str = to_string_byte_units(limit);
		formatstr(hold_msg, "Job has exceeded allocated disk (%s). Consider increasing the value of request_disk.",
		         limit_str.c_str());
		jic->holdJob(hold_msg.c_str(), CONDOR_HOLD_CODE::JobOutOfResources, 0);
		m_lvm_held_job = true;
	}

	// Only applies to thin provisioning of LVs
	if (out_of_space) {
		auto now = time(NULL);
		if ((m_lvm_last_space_issue > 0) && (now - m_lvm_last_space_issue > 60)) {
			dprintf(D_ALWAYS, "ERROR: Underlying thin pool is out of space and not recovering; evicting this job but not holding it.\n");
			jic->holdJob("Underlying thin pool is out of space and not recovering", 0, 0);
			m_lvm_held_job = true;
			return;
		} else if (m_lvm_last_space_issue < 0) {
			dprintf(D_ALWAYS, "WARNING: Thin pool used by startd (%s) is out of space; writes will be paused until this is resolved.\n",
				m_lv_handle->GetPool().c_str());
			m_lvm_last_space_issue = now;
		}
	} else {
		m_lvm_last_space_issue = -1;
	}
}
#endif // LINUX
