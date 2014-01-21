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
#include "java_proc.h"
#include "tool_daemon_proc.h"
#include "mpi_master_proc.h"
#include "mpi_comrade_proc.h"
#include "parallel_proc.h"
#include "vm_proc.h"
#include "my_hostname.h"
#include "internet.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "classad_command_util.h"
#include "condor_random_num.h"
#include "../condor_sysapi/sysapi.h"
#include "build_job_env.h"
#include "get_port_range.h"
#include "perm.h"
#include "filename_tools.h"
#include "directory.h"
#include "exit.h"
#include "condor_auth_x509.h"
#include "setenv.h"
#include "condor_claimid_parser.h"
#include "condor_version.h"
#include "sshd_proc.h"
#include "condor_base64.h"
#include "my_username.h"

extern "C" int get_random_int();
extern void main_shutdown_fast();

const char* JOB_AD_FILENAME = ".job.ad";
const char* MACHINE_AD_FILENAME = ".machine.ad";

#ifdef WIN32
// Note inversion of argument order...
#define realpath(path,resolved_path) _fullpath((resolved_path),(path),_MAX_PATH)
#endif


/* CStarter class implementation */

CStarter::CStarter()
{
	Execute = NULL;
	orig_cwd = NULL;
	is_gridshell = false;
	ShuttingDown = FALSE;
	jic = NULL;
	jobUniverse = CONDOR_UNIVERSE_VANILLA;
	pre_script = NULL;
	post_script = NULL;
	starter_stdin_fd = -1;
	starter_stdout_fd = -1;
	starter_stderr_fd = -1;
	deferral_tid = -1;
	suspended = false;
	m_privsep_helper = NULL;
	m_configured = false;
	m_job_environment_is_ready = false;
	m_all_jobs_done = false;
	m_deferred_job_update = false;
}


CStarter::~CStarter()
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
}


bool
CStarter::Init( JobInfoCommunicator* my_jic, const char* original_cwd,
				bool is_gsh, int stdin_fd, int stdout_fd, 
				int stderr_fd )
{
	if( ! my_jic ) {
		EXCEPT( "CStarter::Init() called with no JobInfoCommunicator!" ); 
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
	} else {
		WorkingDir.formatstr( "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
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
		(SignalHandlercpp)&CStarter::RemoteSuspend, "RemoteSuspend",
		this);
	daemonCore->Register_Signal(DC_SIGCONTINUE, "DC_SIGCONTINUE",
		(SignalHandlercpp)&CStarter::RemoteContinue, "RemoteContinue",
		this);
	daemonCore->Register_Signal(DC_SIGHARDKILL, "DC_SIGHARDKILL",
		(SignalHandlercpp)&CStarter::RemoteShutdownFast, "RemoteShutdownFast",
		this);
	daemonCore->Register_Signal(DC_SIGSOFTKILL, "DC_SIGSOFTKILL",
		(SignalHandlercpp)&CStarter::RemoteShutdownGraceful, "RemoteShutdownGraceful",
		this);
	daemonCore->Register_Signal(DC_SIGPCKPT, "DC_SIGPCKPT",
		(SignalHandlercpp)&CStarter::RemotePeriodicCkpt, "RemotePeriodicCkpt",
		this);
	daemonCore->Register_Signal(DC_SIGREMOVE, "DC_SIGREMOVE",
		(SignalHandlercpp)&CStarter::RemoteRemove, "RemoteRemove",
		this);
	daemonCore->Register_Signal(SIGUSR1, "SIGUSR1",
		(SignalHandlercpp)&CStarter::RemoteRemove, "RemoteRemove",
		this);
	daemonCore->Register_Signal(DC_SIGHOLD, "DC_SIGHOLD",
		(SignalHandlercpp)&CStarter::RemoteHold, "RemoteHold",
		this);
	daemonCore->Register_Reaper("Reaper", (ReaperHandlercpp)&CStarter::Reaper,
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
						  (CommandHandlercpp)&CStarter::classadCommand,
						  "CStarter::classadCommand", this, WRITE );
	daemonCore->
		Register_Command( UPDATE_GSI_CRED, "UPDATE_GSI_CRED",
						  (CommandHandlercpp)&CStarter::updateX509Proxy,
						  "CStarter::updateX509Proxy", this, WRITE );
	daemonCore->
		Register_Command( DELEGATE_GSI_CRED_STARTER,
						  "DELEGATE_GSI_CRED_STARTER",
						  (CommandHandlercpp)&CStarter::updateX509Proxy,
						  "CStarter::updateX509Proxy", this, WRITE );
	daemonCore->
		Register_Command( STARTER_HOLD_JOB,
						  "STARTER_HOLD_JOB",
						  (CommandHandlercpp)&CStarter::remoteHoldCommand,
						  "CStarter::remoteHoldCommand", this, DAEMON );
	daemonCore->
		Register_Command( CREATE_JOB_OWNER_SEC_SESSION,
						  "CREATE_JOB_OWNER_SEC_SESSION",
						  (CommandHandlercpp)&CStarter::createJobOwnerSecSession,
						  "CStarter::createJobOwnerSecSession", this, DAEMON );

		// Job-owner commands are registered at READ authorization level.
		// Why?  See the explanation in createJobOwnerSecSession().
	daemonCore->
		Register_Command( START_SSHD,
						  "START_SSHD",
						  (CommandHandlercpp)&CStarter::startSSHD,
						  "CStarter::startSSHD", this, READ );
	daemonCore->
		Register_Command( STARTER_PEEK,
						"STARTER_PEEK",
						(CommandHandlercpp)&CStarter::peek,
						"CStarter::peek", this, READ );

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
CStarter::StarterExit( int code )
{
	FinalCleanup();
#if !defined(WIN32)
	if ( GetEnv( "CONDOR_GLEXEC_STARTER_CLEANUP_FLAG" ) ) {
		exitAfterGlexec( code );
	}
#endif
	DC_Exit( code );
}

void CStarter::FinalCleanup()
{
	RemoveRecoveryFile();
	removeTempExecuteDir();
}


void
CStarter::Config()
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
	if (!m_configured) {
		bool ps = privsep_enabled();
		bool gl = param_boolean("GLEXEC_JOB", false);
#if !defined(LINUX)
		dprintf(D_ALWAYS,
		        "GLEXEC_JOB not supported on this platform; "
		            "ignoring\n");
		gl = false;
#endif
		if (ps && gl) {
			EXCEPT("can't support both "
			           "PRIVSEP_ENABLED and GLEXEC_JOB");
		}
		if (ps) {
			m_privsep_helper = new CondorPrivSepHelper;
			ASSERT(m_privsep_helper != NULL);
		}
		else if (gl) {
#if defined(LINUX)
			m_privsep_helper = new GLExecPrivSepHelper;
			ASSERT(m_privsep_helper != NULL);
#endif
		}
	}

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
CStarter::RemoteShutdownGraceful( int )
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
CStarter::ShutdownGraceful( void )
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownGraceful all jobs.\n");

		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	m_job_list.Rewind();
	while ((job = m_job_list.Next()) != NULL) {
		if ( job->ShutdownGraceful() ) {
			// job is completely shut down, so delete it
			m_job_list.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
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
CStarter::RemoteShutdownFast(int)
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
CStarter::ShutdownFast( void )
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownFast all jobs.\n");
	
		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	m_job_list.Rewind();
	while ((job = m_job_list.Next()) != NULL) {
		if ( job->ShutdownFast() ) {
			// job is completely shut down, so delete it
			m_job_list.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
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
CStarter::RemoteRemove( int )
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
CStarter::Remove( ) {
	bool jobRunning = false;
	UserProc *job;

	dprintf( D_ALWAYS, "Remove all jobs\n" );

		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	m_job_list.Rewind();
	while( (job = m_job_list.Next()) != NULL ) {
		if( job->Remove() ) {
			// job is completely shut down, so delete it
			m_job_list.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
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
CStarter::RemoteHold( int )
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
CStarter::createJobOwnerSecSession( int /*cmd*/, Stream* s )
{
		// A Condor daemon on the submit side (e.g. schedd) wishes to
		// create a security session for use by a user
		// (e.g. condor_rsh_to_job).  Why doesn't the user's tool just
		// connect and create a security session directly?  Because
		// the starter is not necessarily set up to authenticate the
		// owner of the job.  So the schedd authenticates the owner of
		// the job and facilitates the creation of a security session
		// between the starter and the tool.

	MyString fqu;
	getJobOwnerFQUOrDummy(fqu);
	ASSERT( !fqu.IsEmpty() );

	MyString error_msg;
	ClassAd input;
	s->decode();
	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to read request in createJobOwnerSecSession()\n");
		return FALSE;
	}

		// In order to ensure that we are really talking to the schedd
		// that is managing this job, check that the schedd has provided
		// the correct secret claim id.
	MyString job_claim_id;
	MyString input_claim_id;
	getJobClaimId(job_claim_id);
	input.LookupString(ATTR_CLAIM_ID,input_claim_id);
	if( job_claim_id != input_claim_id || job_claim_id.IsEmpty() ) {
		dprintf(D_ALWAYS,
				"Claim ID provided to createJobOwnerSecSession does not match "
				"expected value!  Rejecting connection from %s\n",
				s->peer_description());
		return FALSE;
	}

	char *session_id = Condor_Crypt_Base::randomHexKey();
	char *session_key = Condor_Crypt_Base::randomHexKey();

	MyString session_info;
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
			session_info.Value(),
			fqu.Value(),
			NULL,
			0 );
	}
	if( rc ) {
			// get the final session parameters that were chosen
		session_info = "";
		rc = daemonCore->getSecMan()->ExportSecSessionInfo(
			session_id,
			session_info );
	}

	ClassAd response;
	response.Assign(ATTR_VERSION,CondorVersion());
	if( !rc ) {
		if( error_msg.IsEmpty() ) {
			error_msg = "Failed to create security session.";
		}
		response.Assign(ATTR_RESULT,false);
		response.Assign(ATTR_ERROR_STRING,error_msg);
		dprintf(D_ALWAYS,
				"createJobOwnerSecSession failed: %s\n", error_msg.Value());
	}
	else {
		// We use a "claim id" string to hold the security session info,
		// because it is a convenient container.

		ClaimIdParser claimid(session_id,session_info.Value(),session_key);
		response.Assign(ATTR_RESULT,true);
		response.Assign(ATTR_CLAIM_ID,claimid.claimId());
		response.Assign(ATTR_STARTER_IP_ADDR,daemonCore->publicNetworkIpAddr());

		dprintf(D_FULLDEBUG,"Created security session for job owner (%s).\n",
				fqu.Value());
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
CStarter::vMessageFailed(Stream *s,bool retry, const std::string &prefix,char const *fmt,va_list args)
{
	MyString error_msg;
	error_msg.vformatstr( fmt, args );

		// old classads cannot handle a string ending in a double quote
		// followed by a newline, so strip off any trailing newline
	error_msg.trim();

	dprintf(D_ALWAYS,"%s failed: %s\n", prefix.c_str(), error_msg.Value());

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
CStarter::SSHDRetry(Stream *s,char const *fmt,...)
{
	va_list args;
	va_start( args, fmt );
	vMessageFailed(s, true, "START_SSHD", fmt, args);
	va_end( args );

	return FALSE;
}
int
CStarter::SSHDFailed(Stream *s,char const *fmt,...)
{
	va_list args;
	va_start( args, fmt );
	vMessageFailed(s, false, "START_SSHD", fmt, args);
	va_end( args );

	return FALSE;
}

int
CStarter::PeekRetry(Stream *s,char const *fmt,...)
{
        va_list args;
        va_start( args, fmt );
        vMessageFailed(s, true, "PEEK", fmt, args);
        va_end( args );

        return FALSE;
}
int
CStarter::PeekFailed(Stream *s,char const *fmt,...)
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
	MyString &output_buffer,
	MyString *error_msg)
{
	int start = find_str_in_buffer(input_buffer,input_len,begin_marker);
	int end = find_str_in_buffer(input_buffer,input_len,end_marker);
	if( start < 0 ) {
		if( error_msg ) {
			error_msg->formatstr("Failed to find '%s' in input: %.*s",
							   begin_marker,input_len,input_buffer);
		}
		return false;
	}
	start += strlen(begin_marker);
	if( end < 0 || end < start ) {
		if( error_msg ) {
			error_msg->formatstr("Failed to find '%s' in input: %.*s",
							   end_marker,input_len,input_buffer);
		}
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
	MyString &output_buffer,
	MyString *error_msg)
{
	int start = find_str_in_buffer(input_buffer,input_len,begin_marker);
	int end = find_str_in_buffer(input_buffer,input_len,end_marker);
	if( start < 0 ) {
		if( error_msg ) {
			error_msg->formatstr("Failed to find '%s' in input: %.*s",
							   begin_marker,input_len,input_buffer);
		}
		return false;
	}
	start += strlen(begin_marker);
	if( end < 0 || end < start ) {
		if( error_msg ) {
			error_msg->formatstr("Failed to find '%s' in input: %.*s",
							   end_marker,input_len,input_buffer);
		}
		return false;
	}
	output_buffer.formatstr("%.*s",end-start,input_buffer+start);
	return true;
}

#endif

int
CStarter::peek(int /*cmd*/, Stream *sock)
{
	// This command should only be allowed by the job owner.
	MyString error_msg;
	ReliSock *s = (ReliSock*)sock;
	char const *fqu = s->getFullyQualifiedUser();
	MyString job_owner;
	getJobOwnerFQUOrDummy(job_owner);
	if( !fqu || job_owner != fqu )
	{
		dprintf(D_ALWAYS, "Unauthorized attempt to peek at job logfiles by '%s'\n", fqu ? fqu : "");
		return false;
	}

	compat_classad::ClassAd input;
	s->decode();
	if( !getClassAd(s, input) || !s->end_of_message() ) {
		dprintf(D_ALWAYS, "Failed to read request for peeking at logs.\n");
		return false;
	}

	compat_classad::ClassAd *jobad = NULL;
	compat_classad::ClassAd *machinead = NULL;
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
		return PeekRetry(s, "Rejecting request, because job not yet initialized.");
	}

	if( !m_job_environment_is_ready ) {
		// This can happen if file transfer is still in progress.
		return PeekRetry(s, "Rejecting request, because the job execution environment is not yet ready.");
	}
	if( m_all_jobs_done ) {
		return PeekFailed(s, "Rejecting request, because the job is finished.");
	}
 
	if( !jic->userPrivInitialized() ) {
		return PeekRetry(s, "Rejecting request, because job execution account not yet established.");
	}

	ssize_t max_xfer = -1;
	input.EvaluateAttrInt(ATTR_MAX_TRANSFER_BYTES, max_xfer);

	const char *jic_iwd = GetWorkingDir();
	if (!jic_iwd) return PeekFailed(s, "Unknown job remote IWD.");
	std::string iwd = jic_iwd;
	std::vector<char> real_iwd; real_iwd.reserve(MAXPATHLEN+1);
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
			classad::Value value; value.SetIntegerValue(0);
			file_expr_list.push_back(classad::Literal::MakeLiteral(value));
			file_list.push_back(out);
			off_t stdout_off = -1;
			input.EvaluateAttrInt("OutOffset", stdout_off);
			offsets_list.push_back(stdout_off);
			value.SetIntegerValue(stdout_off);
			off_expr_list.push_back(classad::Literal::MakeLiteral(value));
		}
	}
	bool transfer_stderr = false;
	if (input.EvaluateAttrBool(ATTR_JOB_ERROR, transfer_stderr) && transfer_stderr)
	{
		std::string err;
		if (jobad->EvaluateAttrString(ATTR_JOB_ERROR, err))
		{
			classad::Value value; value.SetIntegerValue(1);
			file_expr_list.push_back(classad::Literal::MakeLiteral(value));
			file_list.push_back(err);
			off_t stderr_off = -1;
			input.EvaluateAttrInt("ErrOffset", stderr_off);
			offsets_list.push_back(stderr_off);
			value.SetIntegerValue(stderr_off);
			off_expr_list.push_back(classad::Literal::MakeLiteral(value));
		}
	}

	classad::Value transfer_list_value;
	std::vector<std::string> transfer_list;
	classad_shared_ptr<classad::ExprList> transfer_list_ptr;
	if (input.EvaluateAttr("TransferFiles", transfer_list_value) && transfer_list_value.IsSListValue(transfer_list_ptr))
	{
		transfer_list.reserve(transfer_list_ptr->size());
		for (classad::ExprList::const_iterator it = transfer_list_ptr->begin();
			it != transfer_list_ptr->end();
			it++)
		{
			std::string transfer_entry;
			classad::Value transfer_value;
			if (!(*it)->Evaluate(transfer_value) || !transfer_value.IsStringValue(transfer_entry))
			{
				return PeekFailed(s, "Could not evaluate transfer list.");
			}
			transfer_list.push_back(transfer_entry);
		}
	}

	std::vector<off_t> transfer_offsets; transfer_offsets.reserve(transfer_list.size());
	for (size_t idx = 0; idx < transfer_list.size(); idx++) transfer_offsets.push_back(-1);

	if (input.EvaluateAttr("TransferOffsets", transfer_list_value) && transfer_list_value.IsSListValue(transfer_list_ptr))
	{
		size_t idx = 0;
		for (classad::ExprList::const_iterator it = transfer_list_ptr->begin();
			it != transfer_list_ptr->end() && idx < transfer_list.size();
			it++, idx++)
		{
			classad::Value transfer_value;
			off_t transfer_entry;
			if ((*it)->Evaluate(transfer_value) && transfer_value.IsIntegerValue(transfer_entry))
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
				classad::Value value; value.SetStringValue(filename);
				file_expr_list.push_back(classad::Literal::MakeLiteral(value));
				file_list.push_back(filename);
				value.SetIntegerValue(*iter2);
				off_expr_list.push_back(classad::Literal::MakeLiteral(value));
				offsets_list.push_back(*iter2);
			}
			continue;
		}
		if (jobad->EvaluateAttrString(ATTR_JOB_OUTPUT, filename) && (filename == *iter))
		{
			missing = false;
			if (!transfer_stdout)
			{
				classad::Value value; value.SetStringValue(filename);
				file_expr_list.push_back(classad::Literal::MakeLiteral(value));
				file_list.push_back(filename);
				value.SetIntegerValue(*iter2);
				off_expr_list.push_back(classad::Literal::MakeLiteral(value));
				offsets_list.push_back(*iter2);
			}
			continue;
		}
		std::string job_transfer_list;
		bool found = false;
		if (jobad->EvaluateAttrString(ATTR_TRANSFER_OUTPUT_FILES, job_transfer_list))
		{
			StringList job_sl(job_transfer_list.c_str());
			job_sl.rewind();
			const char *job_iter;
			while ((job_iter = job_sl.next()))
			{
				if (strcmp(iter->c_str(), job_iter) == 0)
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
			classad::Value value; value.SetStringValue(filename);
			file_expr_list.push_back(classad::Literal::MakeLiteral(value));
			value.SetIntegerValue(*iter2);
			off_expr_list.push_back(classad::Literal::MakeLiteral(value));
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
	std::vector<char> real_filename; real_filename.reserve(MAXPATHLEN+1);
	unsigned idx = 0;
	for (std::vector<std::string>::iterator it = file_list.begin();
		it != file_list.end() && it2 != offsets_list.end();
		it++, it2++, idx++)
	{
		size_t size = 0;
		off_t offset = *it2;

		if (it->size() && ((*it)[0] != DIR_DELIM_CHAR))
		{
			*it = iwd + DIR_DELIM_CHAR + *it;
		}
		if (!realpath(it->c_str(), &(real_filename[0])))
		{
			return PeekRetry(s, "Unable to resolve file %s to real path. (errno=%d, %s)", it->c_str(), errno, strerror(errno));
		}
		*it = &(real_filename[0]);
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
			offset = size;
			*it2 = offset;
			classad::Value value; value.SetIntegerValue(*it2);
			off_expr_list[idx] = classad::Literal::MakeLiteral(value);
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
				classad::Value value; value.SetIntegerValue(*it2);
				off_expr_list[idx] = classad::Literal::MakeLiteral(value);
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
				classad::Value value; value.SetIntegerValue(*it2);
				off_expr_list[idx] = classad::Literal::MakeLiteral(value);
			}
		}
		file_sizes_list.push_back(size);
	}
	}

	compat_classad::ClassAd reply;
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
		dprintf(D_ALWAYS, "Failed to send file count %ld for peeking at logs.\n", file_count);
	}
	return file_count == file_list.size();
}

int
CStarter::startSSHD( int /*cmd*/, Stream* s )
{
		// This command should only be allowed by the job owner.
	MyString error_msg;
	Sock *sock = (Sock*)s;
	char const *fqu = sock->getFullyQualifiedUser();
	MyString job_owner;
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
		return SSHDRetry(s,"Rejecting request, because job not yet initialized.");
	}
	if( !m_job_environment_is_ready ) {
			// This can happen if file transfer is still in progress.
			// At this stage, the sandbox might not even be owned by the user.
		return SSHDRetry(s,"Rejecting request, because the job execution environment is not yet ready.");
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

	MyString preferred_shells;
	input.LookupString(ATTR_SHELL,preferred_shells);

	MyString slot_name;
	input.LookupString(ATTR_NAME,slot_name);

	if( !jic->userPrivInitialized() ) {
		return SSHDRetry(s,"Rejecting request, because job execution account not yet established.");
	}

	MyString libexec;
	if( !param(libexec,"LIBEXEC") ) {
		return SSHDFailed(s,"LIBEXEC not defined, so cannot find condor_ssh_to_job_sshd_setup");
	}
	MyString ssh_to_job_sshd_setup;
	MyString ssh_to_job_shell_setup;
	ssh_to_job_sshd_setup.formatstr(
		"%s%ccondor_ssh_to_job_sshd_setup",libexec.Value(),DIR_DELIM_CHAR);
	ssh_to_job_shell_setup.formatstr(
		"%s%ccondor_ssh_to_job_shell_setup",libexec.Value(),DIR_DELIM_CHAR);

	if( access(ssh_to_job_sshd_setup.Value(),X_OK)!=0 ) {
		return SSHDFailed(s,"Cannot execute %s: %s",
						  ssh_to_job_sshd_setup.Value(),strerror(errno));
	}
	if( access(ssh_to_job_shell_setup.Value(),X_OK)!=0 ) {
		return SSHDFailed(s,"Cannot execute %s: %s",
						  ssh_to_job_shell_setup.Value(),strerror(errno));
	}

	MyString sshd_config_template;
	if( !param(sshd_config_template,"SSH_TO_JOB_SSHD_CONFIG_TEMPLATE") ) {
		if( param(sshd_config_template,"LIB") ) {
			sshd_config_template.formatstr_cat("%ccondor_ssh_to_job_sshd_config_template",DIR_DELIM_CHAR);
		}
		else {
			return SSHDFailed(s,"SSH_TO_JOB_SSHD_CONFIG_TEMPLATE and LIB are not defined.  At least one of them is required.");
		}
	}
	if( access(sshd_config_template.Value(),F_OK)!=0 ) {
		return SSHDFailed(s,"%s does not exist!",sshd_config_template.Value());
	}

	MyString ssh_keygen;
	MyString ssh_keygen_args;
	ArgList ssh_keygen_arglist;
	param(ssh_keygen,"SSH_TO_JOB_SSH_KEYGEN","/usr/bin/ssh-keygen");
	param(ssh_keygen_args,"SSH_TO_JOB_SSH_KEYGEN_ARGS","\"-N '' -C '' -q -f %f -t rsa\"");
	ssh_keygen_arglist.AppendArg(ssh_keygen.Value());
	if( !ssh_keygen_arglist.AppendArgsV2Quoted(ssh_keygen_args.Value(),&error_msg) ) {
		return SSHDFailed(s,
						  "SSH_TO_JOB_SSH_KEYGEN_ARGS is misconfigured: %s",
						  error_msg.Value());
	}

	MyString client_keygen_args;
	input.LookupString(ATTR_SSH_KEYGEN_ARGS,client_keygen_args);
	if( !ssh_keygen_arglist.AppendArgsV2Raw(client_keygen_args.Value(),&error_msg) ) {
		return SSHDFailed(s,
						  "Failed to produce ssh-keygen arg list: %s",
						  error_msg.Value());
	}

	MyString ssh_keygen_cmd;
	if(!ssh_keygen_arglist.GetArgsStringSystem(&ssh_keygen_cmd,0,&error_msg)) {
		return SSHDFailed(s,
						  "Failed to produce ssh-keygen command string: %s",
						  error_msg.Value());
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
	if( !GetJobEnv( jobad, &setup_env, &error_msg ) ) {
		return SSHDFailed(
			s,"Failed to get job environment: %s",error_msg.Value());
	}

	if( !slot_name.IsEmpty() ) {
		setup_env.SetEnv("_CONDOR_SLOT_NAME",slot_name.Value());
	}

    int setup_opt_mask = DCJOBOPT_NO_CONDOR_ENV_INHERIT;
    if (!param_boolean("JOB_INHERITS_STARTER_ENVIRONMENT",false)) {
        setup_opt_mask |= DCJOBOPT_NO_ENV_INHERIT;
    }

	if( !preferred_shells.IsEmpty() ) {
		dprintf(D_FULLDEBUG,
				"Checking preferred shells: %s\n",preferred_shells.Value());
		StringList shells(preferred_shells.Value(),",");
		shells.rewind();
		char *shell;
		while( (shell=shells.next()) ) {
			if( access(shell,X_OK)==0 ) {
				dprintf(D_FULLDEBUG,"Will use shell %s\n",shell);
				setup_env.SetEnv("_CONDOR_SHELL",shell);
				break;
			}
		}
	}

	ArgList setup_args;
	setup_args.AppendArg(ssh_to_job_sshd_setup.Value());
	setup_args.AppendArg(GetWorkingDir());
	setup_args.AppendArg(ssh_to_job_shell_setup.Value());
	setup_args.AppendArg(sshd_config_template.Value());
	setup_args.AppendArg(ssh_keygen_cmd.Value());

		// Would like to use my_popen here, but we need to support glexec.
		// Use the default reaper, even though it doesn't know anything
		// about this task.  We avoid needing to know the final exit status
		// by checking for a magic success string at the end of the output.
	int setup_reaper = 1;
	if( privSepHelper() ) {
		privSepHelper()->create_process(
			ssh_to_job_sshd_setup.Value(),
			setup_args,
			setup_env,
			GetWorkingDir(),
			setup_std_fds,
			NULL,
			0,
			NULL,
			setup_reaper,
			setup_opt_mask,
			NULL);
	}
	else {
		daemonCore->Create_Process(
			ssh_to_job_sshd_setup.Value(),
			setup_args,
			PRIV_USER_FINAL,
			setup_reaper,
			FALSE,
			&setup_env,
			GetWorkingDir(),
			NULL,
			NULL,
			setup_std_fds,
			NULL,
			0,
			NULL,
			setup_opt_mask);
	}

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
		error_msg.formatstr("condor_ssh_to_job_sshd_setup failed: %s",
						  setup_output);
		free( setup_output );
		return SSHDFailed(s,"%s",error_msg.Value());
	}

		// Since in privsep situations, we cannot directly access the
		// ssh key files that sshd_setup prepared, we slurp them in
		// from the pipe to sshd_setup.

	bool rc = true;
	MyString session_dir;
	if( rc ) {
		rc = extract_delimited_data(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup SSHD DIR BEGIN\n",
			"\ncondor_ssh_to_job_sshd_setup SSHD DIR END\n",
			session_dir,
			&error_msg);
	}

	MyString sshd_user;
	if( rc ) {
		rc = extract_delimited_data(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup SSHD USER BEGIN\n",
			"\ncondor_ssh_to_job_sshd_setup SSHD USER END\n",
			sshd_user,
			&error_msg);
	}

	MyString public_host_key;
	if( rc ) {
		rc = extract_delimited_data_as_base64(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup PUBLIC SERVER KEY BEGIN\n",
			"condor_ssh_to_job_sshd_setup PUBLIC SERVER KEY END\n",
			public_host_key,
			&error_msg);
	}

	MyString private_client_key;
	if( rc ) {
		rc = extract_delimited_data_as_base64(
			setup_output,
			setup_output_len,
			"condor_ssh_to_job_sshd_setup AUTHORIZED CLIENT KEY BEGIN\n",
			"condor_ssh_to_job_sshd_setup AUTHORIZED CLIENT KEY END\n",
			private_client_key,
			&error_msg);
	}

	free( setup_output );

	if( !rc ) {
		MyString msg;
		return SSHDFailed(s,
			"Failed to parse output of condor_ssh_to_job_sshd_setup: %s",
			error_msg.Value());
	}

	dprintf(D_FULLDEBUG,"StartSSHD: session_dir='%s'\n",session_dir.Value());

	MyString sshd_config_file;
	sshd_config_file.formatstr("%s%csshd_config",session_dir.Value(),DIR_DELIM_CHAR);



	MyString sshd;
	param(sshd,"SSH_TO_JOB_SSHD","/usr/sbin/sshd");
	if( access(sshd.Value(),X_OK)!=0 ) {
		return SSHDFailed(s,"Failed, because sshd not correctly configured (SSH_TO_JOB_SSHD=%s): %s.",sshd.Value(),strerror(errno));
	}

	ArgList sshd_arglist;
	MyString sshd_arg_string;
	param(sshd_arg_string,"SSH_TO_JOB_SSHD_ARGS","\"-i -e -f %f\"");
	if( !sshd_arglist.AppendArgsV2Quoted(sshd_arg_string.Value(),&error_msg) )
	{
		return SSHDFailed(s,"Invalid SSH_TO_JOB_SSHD_ARGS (%s): %s",
						  sshd_arg_string.Value(),error_msg.Value());
	}

	char **argarray = sshd_arglist.GetStringArray();
	sshd_arglist.Clear();
	for(int i=0; argarray[i]; i++) {
		char const *ptr;
		MyString new_arg;
		for(ptr=argarray[i]; *ptr; ptr++) {
			if( *ptr == '%' ) {
				ptr += 1;
				if( *ptr == '%' ) {
					new_arg += '%';
				}
				else if( *ptr == 'f' ) {
					new_arg += sshd_config_file.Value();
				}
				else {
					return SSHDFailed(s,
							"Unexpected %%%c in SSH_TO_JOB_SSHD_ARGS: %s\n",
							*ptr ? *ptr : ' ', sshd_arg_string.Value());
				}
			}
			else {
				new_arg += *ptr;
			}
		}
		sshd_arglist.AppendArg(new_arg.Value());
	}
	deleteStringArray(argarray);
	argarray = NULL;


	ClassAd sshd_ad;
	sshd_ad.CopyAttribute(ATTR_REMOTE_USER,jobad);
	sshd_ad.CopyAttribute(ATTR_JOB_RUNAS_OWNER,jobad);
	sshd_ad.Assign(ATTR_JOB_CMD,sshd.Value());
	CondorVersionInfo ver_info;
	if( !sshd_arglist.InsertArgsIntoClassAd(&sshd_ad,&ver_info,&error_msg) ) {
		return SSHDFailed(s,
			"Failed to insert args into sshd job description: %s",
			error_msg.Value());
	}
		// Since sshd clenses the user's environment, it doesn't really
		// matter what we pass here.  Instead, we depend on sshd_shell_init
		// to restore the environment that was saved by sshd_setup.
		// However, we may as well pass the desired environment.
	if( !setup_env.InsertEnvIntoClassAd(&sshd_ad,&error_msg,NULL,&ver_info) ) {
		return SSHDFailed(s,
			"Failed to insert environment into sshd job description: %s",
			error_msg.Value());
	}



		// Now send the expected ClassAd response to the caller.
		// This is the last exchange before switching the connection
		// over to the ssh protocol.
	ClassAd response;
	response.Assign(ATTR_RESULT,true);
	response.Assign(ATTR_REMOTE_USER,sshd_user);
	response.Assign(ATTR_SSH_PUBLIC_SERVER_KEY,public_host_key.Value());
	response.Assign(ATTR_SSH_PRIVATE_CLIENT_KEY,private_client_key.Value());

	s->encode();
	if( !putClassAd(s, response) || !s->end_of_message() ) {
		dprintf(D_ALWAYS,"Failed to send response to START_SSHD.\n");
		return FALSE;
	}



	MyString sshd_log_fname;
	sshd_log_fname.formatstr(
		"%s%c%s",session_dir.Value(),DIR_DELIM_CHAR,"sshd.log");


	int std[3];
	char const *std_fname[3];
	std[0] = sock->get_file_desc();
	std_fname[0] = "stdin";
	std[1] = sock->get_file_desc();
	std_fname[1] = "stdout";
	std[2] = -1;
	std_fname[2] = sshd_log_fname.Value();


	SSHDProc *proc = new SSHDProc(&sshd_ad);
	if( !proc ) {
		dprintf(D_ALWAYS,"Failed to create SSHDProc.\n");
		return FALSE;
	}
	if( !proc->SshStartJob(std,std_fname) ) {
		dprintf(D_ALWAYS,"Failed to start sshd.\n");
		return FALSE;
	}
	m_job_list.Append(proc);
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
CStarter::remoteHoldCommand( int /*cmd*/, Stream* s )
{
	MyString hold_reason;
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
		dprintf(D_ALWAYS,"Failed to read message from %s in CStarter::remoteHoldCommand()\n", s->peer_description());
		return FALSE;
	}

		// Put the job on hold on the remote side.
	if( jic ) {
		jic->holdJob(hold_reason.Value(),hold_code,hold_subcode);
	}

	int reply = 1;
	s->encode();
	if( !s->put(reply) || !s->end_of_message()) {
		dprintf(D_ALWAYS,"Failed to send response to startd in CStarter::remoteHoldCommand()\n");
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
CStarter::Hold( void )
{	
	bool jobRunning = false;
	UserProc *job;

	dprintf( D_ALWAYS, "Hold all jobs\n" );

		//
		// Check if there is currently a timer registerd for a 
		// deferred job. If there is then we need to cancel it
		//
	if ( this->deferral_tid != -1 ) {
		this->removeDeferredJobs();
	}

	m_job_list.Rewind();
	while( (job = m_job_list.Next()) != NULL ) {
		if( job->Hold() ) {
			// job is completely shut down, so delete it
			m_job_list.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
		}
	}
	ShuttingDown = TRUE;
	return ( !jobRunning );
}


bool
CStarter::createTempExecuteDir( void )
{
		// Once our JobInfoCommmunicator has initialized the right
		// user for the priv_state code, we can finally make the
		// scratch execute directory for this job.

		// If we're the gridshell, for now, we're not making a temp
		// scratch dir, we're just using whatever we got from the
		// scheduler we're running under.
	if( is_gridshell ) { 
		dprintf( D_ALWAYS, "gridshell running in: \"%s\"\n", WorkingDir.Value() ); 
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

	CondorPrivSepHelper* cpsh = condorPrivSepHelper();
	if (cpsh != NULL) {
		// the privsep switchboard now ALWAYS creates directories with
		// permissions 0700.
		cpsh->initialize_sandbox(WorkingDir.Value());
		WriteAdFiles();
	} else {
		// we can only get here if we are not using *CONDOR* PrivSep.  but we
		// might be using glexec.  glexec relies on being able to read the
		// contents of the execute directory as a non-condor user, so in that
		// case, use 0755.  for all other cases, use the more-restrictive 0700.

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
		}

#if defined(LINUX)
		if(glexecPrivSepHelper()) {
			dir_perms = 0755;
		}
#endif
		if( mkdir(WorkingDir.Value(), dir_perms) < 0 ) {
			dprintf( D_FAILURE|D_ALWAYS,
			         "couldn't create dir %s: %s\n",
			         WorkingDir.Value(),
			         strerror(errno) );
			set_priv( priv );
			return false;
		}
		WriteAdFiles();
#if !defined(WIN32)
		if (use_chown) {
			priv_state p = set_root_priv();
			if (chown(WorkingDir.Value(),
			          get_user_uid(),
			          get_user_gid()) == -1)
			{
				EXCEPT("chown error on %s: %s",
				       WorkingDir.Value(),
				       strerror(errno));
			}
			set_priv(p);
		}
#endif
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
		bool ret_val = dirperm.set_acls( WorkingDir.Value() );
		if ( !ret_val ) {
			dprintf(D_ALWAYS,"UNABLE TO SET PERMISSIONS ON EXECUTE DIRECTORY\n");
			set_priv( priv );
			return false;
		}
	}
	
	// if the user wants the execute directory encrypted, 
	// go ahead and set that up now too
	
	if ( param_boolean_crufty("ENCRYPT_EXECUTE_DIRECTORY", false) ) {
		
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
					dprintf(D_FULLDEBUG, "cannot get address for EncryptionDisable()");
					efs_support = false;
				}
				FPEncryptFileA EncryptFile = (FPEncryptFileA) 
					GetProcAddress(advapi,"EncryptFileA");
				if ( !EncryptFile ) {
					dprintf(D_FULLDEBUG, "cannot get address for EncryptFile()");
					efs_support = false;
				}
			}

			if ( efs_support ) {
				wchar_t *WorkingDir_w = new wchar_t[WorkingDir.Length()+1];
				swprintf(WorkingDir_w, L"%S", WorkingDir.Value());
				EncryptionDisable(WorkingDir_w, FALSE);
				delete[] WorkingDir_w;
				
				if ( EncryptFile(WorkingDir.Value()) == 0 ) {
					dprintf(D_ALWAYS, "Could not encrypt execute directory "
							"(err=%li)\n", GetLastError());
				}

				FreeLibrary(advapi); // don't leak the dll library handle

			} else {
				// tell the user it didn't work out
				dprintf(D_ALWAYS, "ENCRYPT_EXECUTE_DIRECTORY set to True, "
						"but the Encryption" " functions are unavailable!");
			}

	} // ENCRYPT_EXECUTE_DIRECTORY is True
	

#endif /* WIN32 */

	// switch to user priv -- it's the owner of the directory we just made
	priv_state ch_p = set_user_priv();
	int chdir_result = chdir(WorkingDir.Value());
	set_priv( ch_p );

	if( chdir_result < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS, "couldn't move to %s: %s\n", WorkingDir.Value(),
				 strerror(errno) ); 
		set_priv( priv );
		return false;
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", WorkingDir.Value() );
	set_priv( priv );
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
CStarter::jobEnvironmentReady( void )
{
#if defined(LINUX)
		//
		// For the GLEXEC_JOB case, we should now be able to
		// initialize our helper object.
		//
	GLExecPrivSepHelper* gpsh = glexecPrivSepHelper();
	if (gpsh != NULL) {
		MyString proxy_path;
		if (!jic->jobClassAd()->LookupString(ATTR_X509_USER_PROXY,
		                                     proxy_path))
		{
			EXCEPT("configuration specifies use of glexec, "
			           "but job has no proxy");
		}
		const char* proxy_name = condor_basename(proxy_path.Value());
		gpsh->initialize(proxy_name, WorkingDir.Value());
	}
#endif

		//
		// Now that we are done preparing the job's environment,
		// change the sandbox ownership to the user before spawning
		// any job processes. VM universe jobs are special-cased
		// here: chowning of the sandbox occurs in the VMGahp after
		// it has had a chance to operate on the VM description
		// file(s)
		//
	if (m_privsep_helper != NULL) {
		int univ = -1;
		if (!jic->jobClassAd()->LookupInteger(ATTR_JOB_UNIVERSE, univ) ||
		    (univ != CONDOR_UNIVERSE_VM))
		{
			PrivSepError err;
			if( !m_privsep_helper->chown_sandbox_to_user(err) ) {
				jic->notifyStarterError(
					err.holdReason(),
					true,
					err.holdCode(),
					err.holdSubCode());
				EXCEPT("failed to chown sandbox to user");
			}
		}
		else if( univ == CONDOR_UNIVERSE_VM ) {
				// the vmgahp will chown the sandbox to the user
			m_privsep_helper->set_sandbox_owned_by_user();
		}
	}

	m_job_environment_is_ready = true;

		//
		// The Starter will determine when the job 
		// should be started. This method will always return 
		// immediately
		//
	this->jobWaitUntilExecuteTime( );
	return ( true );
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
CStarter::jobWaitUntilExecuteTime( void )
{
		//
		// Return value
		//
	bool ret = true;
		//
		// If this is set to true, then we'll want to abort the job
		//
	bool abort = false;
	MyString error;
	
		//
		// First check to see if the job is set to be
		// deferred until a certain time before beginning to
		// execute 
		//		
	ClassAd* jobAd = this->jic->jobClassAd();
	int deferralTime = 0;
	int deferralOffset = 0;
	int deltaT = 0;
	int deferralWindow = 0;
	if ( jobAd->LookupExpr( ATTR_DEFERRAL_TIME ) != NULL ) {
			//
		 	// Make sure that the expression evaluated and we 
		 	// got a positive integer. Otherwise we'll have to kick out
		 	//
		if ( ! jobAd->EvalInteger( ATTR_DEFERRAL_TIME, NULL, deferralTime ) ) {
			error.formatstr( "Invalid deferred execution time for Job %d.%d.",
							this->jic->jobCluster(),
							this->jic->jobProc() );
			abort = true;
		} else if ( deferralTime <= 0 ) {
			error.formatstr( "Invalid execution time '%d' for Job %d.%d.",
							deferralTime,
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
			time_t now = time(NULL);
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
			if ( jobAd->LookupExpr( ATTR_DEFERRAL_WINDOW ) != NULL &&
				 jobAd->EvalInteger( ATTR_DEFERRAL_WINDOW, NULL, deferralWindow ) ) {
				dprintf( D_FULLDEBUG, "Job %d.%d has a deferral window of "
				                      "%d seconds\n", 
							this->jic->jobCluster(),
							this->jic->jobProc(),
							deferralWindow );
			}
			deltaT = deferralTime - now;
				//
				// The time has already passed, check whether it's
				// within our window. If not then abort
				//
			if ( deltaT < 0 ) {
				if ( abs( deltaT ) > deferralWindow ) {
					error.formatstr( "Job %d.%d missed its execution time.",
								this->jic->jobCluster(),
								this->jic->jobProc() );
					abort = true;

				} else {
						//
						// Be sure to set the deltaT to zero so
						// that the timer goes right off
						//
					dprintf( D_ALWAYS, "Job %d.%d missed its execution time but "
										"is within the %d seconds window\n",
								this->jic->jobCluster(),
								this->jic->jobProc(),
								deferralWindow );
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
										(TimerHandlercpp)&CStarter::SpawnPreScript,
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
			dprintf( D_ALWAYS, "Job %d.%d deferred for %d seconds\n", 
						this->jic->jobCluster(),
						this->jic->jobProc(),
						deltaT );
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
		dprintf( D_ALWAYS, "%s Aborting.\n", error.Value() );
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
CStarter::removeDeferredJobs() {
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
		MyString error = "Failed to cancel deferred execution timer for Job ";
		error += this->jic->jobCluster();
		error += ".";
		error += this->jic->jobProc();
		EXCEPT( "%s", error.Value() );
		ret = false;
	}
	return ( ret );
}

/**
 * Start the prescript for a job, if one exists
 * If one doesn't, then we will call SpawnJob() directly
 * 
 * return true if no errors occured
 **/
void
CStarter::SpawnPreScript( void )
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
	MyString attr;

	attr = "Pre";
	attr += ATTR_JOB_CMD;
	if( jobAd->LookupString(attr.Value(), &tmp) ) {
		free( tmp );
		tmp = NULL;
		pre_script = new ScriptProc( jobAd, "Pre" );
	}

	attr = "Post";
	attr += ATTR_JOB_CMD;
	if( jobAd->LookupString(attr.Value(), &tmp) ) {
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

void CStarter::getJobOwnerFQUOrDummy(MyString &result)
{
	ClassAd *jobAd = jic ? jic->jobClassAd() : NULL;
	if( jobAd ) {
		jobAd->LookupString(ATTR_USER,result);
	}
	if( result.IsEmpty() ) {
		result = "job-owner@submit-domain";
	}
}

bool CStarter::getJobClaimId(MyString &result)
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
CStarter::SpawnJob( void )
{
		// Now that we've got all our files, we can figure out what
		// kind of job we're starting up, instantiate the appropriate
		// userproc class, and actually start the job.
	ClassAd* jobAd = jic->jobClassAd();
	if ( jobAd->LookupInteger( ATTR_JOB_UNIVERSE, jobUniverse ) < 1 ) {
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
		case CONDOR_UNIVERSE_VANILLA:
			job = new VanillaProc( jobAd );
			break;
		case CONDOR_UNIVERSE_JAVA:
			job = new JavaProc( jobAd, WorkingDir.Value() );
			break;
	    case CONDOR_UNIVERSE_PARALLEL:
			job = new ParallelProc( jobAd );
			break;
		case CONDOR_UNIVERSE_MPI: {
			int is_master = FALSE;
			if ( jobAd->LookupBool( ATTR_MPI_IS_MASTER, is_master ) < 1 ) {
				is_master = FALSE;
			}
			if ( is_master ) {
				dprintf ( D_FULLDEBUG, "Starting a MPIMasterProc\n" );
				job = new MPIMasterProc( jobAd );
			} else {
				dprintf ( D_FULLDEBUG, "Starting a MPIComradeProc\n" );
				job = new MPIComradeProc( jobAd );
			}
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
		m_job_list.Append(job);
		
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
				m_job_list.Append( tool_daemon_proc );
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
CStarter::WriteRecoveryFile( ClassAd *recovery_ad )
{
	MyString tmp_file;
	FILE *tmp_fp;

	if ( recovery_ad == NULL ) {
		return;
	}

	if ( m_recoveryFile.Length() == 0 ) {
		m_recoveryFile.formatstr( "%s%cdir_%ld.recover", Execute,
								DIR_DELIM_CHAR, (long)daemonCore->getpid() );
	}

	tmp_file.formatstr( "%s.tmp", m_recoveryFile.Value() );

	tmp_fp = safe_fcreate_replace_if_exists( tmp_file.Value(), "w" );
	if ( tmp_fp == NULL ) {
		dprintf( D_ALWAYS, "Failed to open recovery file %s\n", tmp_file.Value() );
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
		unlink( tmp_file.Value() );
		return;
	}

	if ( rotate_file( tmp_file.Value(), m_recoveryFile.Value() ) != 0 ) {
		dprintf( D_ALWAYS, "Failed to rename recovery file\n" );
		MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
		unlink( tmp_file.Value() );
	}
}

void
CStarter::RemoveRecoveryFile()
{
	if ( m_recoveryFile.Length() > 0 ) {
		MSC_SUPPRESS_WARNING_FIXME(6031) // return value of unlink ignored.
		unlink( m_recoveryFile.Value() );
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
CStarter::RemoteSuspend(int)
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
CStarter::Suspend( void ) {
	dprintf(D_ALWAYS, "Suspending all jobs.\n");

	UserProc *job;
	m_job_list.Rewind();
	while ((job = m_job_list.Next()) != NULL) {
		job->Suspend();
	}
	
		//
		// We set a flag to let us know that if any other
		// job tries to start after we recieved this Suspend call
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
CStarter::RemoteContinue(int)
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
CStarter::Continue( void )
{
	dprintf(D_ALWAYS, "Continuing all jobs.\n");

	UserProc *job;
	m_job_list.Rewind();
	while ((job = m_job_list.Next()) != NULL) {
		if (this->suspended)
		{
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
CStarter::RemotePeriodicCkpt(int)
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
CStarter::PeriodicCkpt( void )
{
	dprintf(D_ALWAYS, "Periodic Checkpointing all jobs.\n");

	if( jobUniverse != CONDOR_UNIVERSE_VM ) {
		return false;
	}

	UserProc *job;
	m_job_list.Rewind();
	while ((job = m_job_list.Next()) != NULL) {
		if( job->Ckpt() ) {

			CondorPrivSepHelper* cpsh = condorPrivSepHelper();
			if (cpsh != NULL) {
				PrivSepError err;
				if( !cpsh->chown_sandbox_to_condor(err) ) {
					jic->notifyStarterError(
						err.holdReason(),
						false,
						err.holdCode(),
						err.holdSubCode());
					dprintf(D_ALWAYS,"failed to change sandbox to condor ownership before checkpoint");
					return false;
				}
			}

			bool transfer_ok = jic->uploadWorkingFiles();

			if (cpsh != NULL) {
				PrivSepError err;
				if( !cpsh->chown_sandbox_to_user(err) ) {
					jic->notifyStarterError(
						err.holdReason(),
						true,
						err.holdCode(),
						err.holdSubCode());
					EXCEPT("failed to restore sandbox to user ownership after checkpoint");
					return false;
				}
			}

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


int
CStarter::Reaper(int pid, int exit_status)
{
	int handled_jobs = 0;
	int all_jobs = 0;
	UserProc *job;

	if( WIFSIGNALED(exit_status) ) {
		dprintf( D_ALWAYS, "Process exited, pid=%d, signal=%d\n", pid,
				 WTERMSIG(exit_status) );
	} else {
		dprintf( D_ALWAYS, "Process exited, pid=%d, status=%d\n", pid,
				 WEXITSTATUS(exit_status) );
	}

	if( pre_script && pre_script->JobReaper(pid, exit_status) ) {		
			// TODO: deal with shutdown case?!?
		
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
			// when the post script exits, we know the m_job_list is
			// going to be empty, so don't bother with any of the rest
			// of this.  instead, the starter is now able to call
			// allJobsdone() to do the final clean up stages.
		allJobsDone();
		return TRUE;
	}


	m_job_list.Rewind();
	while ((job = m_job_list.Next()) != NULL) {
		all_jobs++;
		if( job->GetJobPid()==pid && job->JobReaper(pid, exit_status) ) {
			handled_jobs++;
			m_job_list.DeleteCurrent();
			m_reaped_job_list.Append(job);
		}
	}

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
				dprintf(D_ALWAYS, "Returning from CStarter::JobReaper()\n");
				return 0;
			}
		}
	}

	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		StarterExit(0);
	}

	return 0;
}


bool
CStarter::allJobsDone( void )
{
	m_all_jobs_done = true;
	bool bRet=false;

		// now that all user processes are complete, change the
		// sandbox ownership back over to condor. if this is a VM
		// universe job, this chown will have already been
		// performed by the VMGahp, since it does some post-
		// processing on files in the sandbox
	if (m_privsep_helper != NULL) {
		if (jobUniverse != CONDOR_UNIVERSE_VM) {
			PrivSepError err;
			if( !m_privsep_helper->chown_sandbox_to_condor(err) ) {
				jic->notifyStarterError(
					err.holdReason(),
					false,
					err.holdCode(),
					err.holdSubCode());
				EXCEPT("failed to chown sandbox to condor after job completed");
			}
		}
	}

		// No more jobs, notify our JobInfoCommunicator.
	if (jic->allJobsDone()) {
			// JIC::allJobsDone returned true: we're ready to move on.
		bRet=transferOutput();
	}
	
	if (m_deferred_job_update){
		jic->notifyJobExit( -1, JOB_SHOULD_REQUEUE, 0 );
	}
		// JIC::allJobsDonbRete() returned false: propagate that so we
		// halt the cleanup process and wait for external events.
	return bRet;
}


bool
CStarter::transferOutput( void )
{
	UserProc *job;
	bool transient_failure = false;

	if (jic->transferOutput(transient_failure) == false) {

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

		m_reaped_job_list.Rewind();
		while ((job = m_reaped_job_list.Next()) != NULL) {
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
CStarter::cleanupJobs( void )
{
		// Now that we're done with HOOK_JOB_EXIT and transfering
		// files, we can finally go through the m_reaped_job_list and
		// call JobExit() on all the procs in there.
	UserProc *job;
	m_reaped_job_list.Rewind();
	while( (job = m_reaped_job_list.Next()) != NULL) {
		if( job->JobExit() ) {
			m_reaped_job_list.DeleteCurrent();
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
		// No more jobs, all cleanup done, notify our JIC
	jic->allJobsGone();
	return true;
}


bool
CStarter::publishUpdateAd( ClassAd* ad )
{
	return publishJobInfoAd(&m_job_list, ad);
}


bool
CStarter::publishJobExitAd( ClassAd* ad )
{
	return publishJobInfoAd(&m_reaped_job_list, ad);
}


bool
CStarter::publishJobInfoAd(List<UserProc>* proc_list, ClassAd* ad)
{
		// Iterate through all our UserProcs and have those publish,
		// as well.  This method is virtual, so we'll get all the
		// goodies from derived classes, as well.  If any of them put
		// info into the ad, return true.  Otherwise, return false.
	bool found_one = false;
	if( pre_script && pre_script->PublishUpdateAd(ad) ) {
		found_one = true;
	}
	
	// Update the state.
	if (m_deferred_job_update)
	{
		MyString buf;
		buf.formatstr( "%s=\"Exited\"", ATTR_JOB_STATE );
	}
	
	UserProc *job;
	proc_list->Rewind();
	while ((job = proc_list->Next()) != NULL) {
		if( job->PublishUpdateAd(ad) ) {
			found_one = true;
		}
	}
	if( post_script && post_script->PublishUpdateAd(ad) ) {
		found_one = true;
	}
	return found_one;
}


bool
CStarter::publishPreScriptUpdateAd( ClassAd* ad )
{
	if( pre_script && pre_script->PublishUpdateAd(ad) ) {
		return true;
	}
	return false;
}


bool
CStarter::publishPostScriptUpdateAd( ClassAd* ad )
{
	if( post_script && post_script->PublishUpdateAd(ad) ) {
		return true;
	}
	return false;
}

bool
CStarter::GetJobEnv( ClassAd *jobad, Env *job_env, MyString *env_errors )
{
	char *env_str = param( "STARTER_JOB_ENVIRONMENT" );

	ASSERT( jobad );
	ASSERT( job_env );
	if( !job_env->MergeFromV1RawOrV2Quoted(env_str,env_errors) ) {
		if( env_errors ) {
			env_errors->formatstr_cat(
				" The full value for STARTER_JOB_ENVIRONMENT: %s\n",env_str);
		}
		free(env_str);
		return false;
	}
	free(env_str);

	if(!job_env->MergeFrom(jobad,env_errors)) {
		if( env_errors ) {
			env_errors->formatstr_cat(
				" (This error was from the environment string in the job "
				"ClassAd.)");
		}
		return false;
	}

		// Now, let the starter publish any env vars it wants to into
		// the mainjob's env...
	PublishToEnv( job_env );
	return true;
}	

void
CStarter::PublishToEnv( Env* proc_env )
{
	ASSERT(proc_env);
	if( pre_script ) {
		pre_script->PublishToEnv( proc_env );
	}
		// we don't have to worry about post, since it's going to run
		// after everything else, so there's not going to be any info
		// about it to pass until it's already done.

		// used by sshd_shell_setup script to cd to the job working dir
	proc_env->SetEnv("_CONDOR_JOB_IWD",jic->jobRemoteIWD());

	MyString job_pids;
	UserProc* uproc;
	m_job_list.Rewind();
	while ((uproc = m_job_list.Next()) != NULL) {
		uproc->PublishToEnv( proc_env );

		if( ! job_pids.IsEmpty() ) {
			job_pids += " ";
		}
		job_pids.formatstr_cat("%d",uproc->GetJobPid());		
	}
		// put the pid of the job in the environment, used by sshd and hooks
	proc_env->SetEnv("_CONDOR_JOB_PIDS",job_pids);

	m_reaped_job_list.Rewind();
	while ((uproc = m_reaped_job_list.Next()) != NULL) {
		uproc->PublishToEnv( proc_env );
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
	MyString base;
	base = "_";
	base += myDistro->GetUc();
	base += '_';
 
	MyString env_name;

		// if there are non-fungible assigned resources (i.e. GPUs) pass those assignments down in the environment
		// we look through all machine resource names looking for an attribute Assigned*, if we find one
		// then we publish it's value in the environment as _CONDOR_Assigned*,  so for instance, if there is
		// an AssignedGPU attribute in the machine add, there will be a _CONDOR_AssignedGPU environment
		// variable with the same value.
	ClassAd * mad = jic->machClassAd();
	if (mad) {
		MyString restags;
		if (mad->LookupString(ATTR_MACHINE_RESOURCES, restags)) {
			StringList tags(restags.c_str());
			tags.rewind();
			const char *tag;
			while ((tag = tags.next())) {
				MyString attr("Assigned"); attr += tag;
				MyString assigned;
				if (mad->LookupString(attr.c_str(), assigned)) {
					env_name = base;
					env_name += attr;
					proc_env->SetEnv( env_name.Value(), assigned.c_str() );

					// also allow a configured alternate environment name
					MyString param_name("ENVIRONMENT_NAME_FOR_"); param_name += attr;
					if (param(env_name, param_name.c_str())) {
						if ( ! env_name.empty()) {
							proc_env->SetEnv( env_name.Value(), assigned.c_str() );
						}
					}
				}
			}
		}
	}

		// path to the output ad, if any
	const char* output_ad = jic->getOutputAdFile();
	if( output_ad && !(output_ad[0] == '-' && output_ad[1] == '\0') ) {
		env_name = base.Value();
		env_name += "OUTPUT_CLASSAD";
		proc_env->SetEnv( env_name.Value(), output_ad );
	}
	
		// job scratch space
	env_name = base.Value();
	env_name += "SCRATCH_DIR";
	proc_env->SetEnv( env_name.Value(), GetWorkingDir() );

		// slot identifier
	env_name = base.Value();
	env_name += "SLOT";
	
	proc_env->SetEnv(env_name.Value(), getMySlotName());

		// pass through the pidfamily ancestor env vars this process
		// currently has to the job.

		// port regulation stuff.  assume the outgoing port range.
	int low, high;
	if (get_port_range (TRUE, &low, &high) == TRUE) {
		MyString tmp_port_number;

		tmp_port_number = high;
		env_name = base.Value();
		env_name += "HIGHPORT";
		proc_env->SetEnv( env_name.Value(), tmp_port_number.Value() );

		tmp_port_number = low;
		env_name = base.Value();
		env_name += "LOWPORT";
		proc_env->SetEnv( env_name.Value(), tmp_port_number.Value() );
    }

		// set environment variables for temporary directories
		// Condor will clean these up on job exits, and there's
		// no chance of file collisions with other running slots

	proc_env->SetEnv("TMPDIR", GetWorkingDir());
	proc_env->SetEnv("TEMP", GetWorkingDir()); // Windows
	proc_env->SetEnv("TMP", GetWorkingDir()); // Windows
}


int
CStarter::getMySlotNumber( void )
{
	
	char *logappend = param("STARTER_LOG");		
	char const *tmp = NULL;
		
	int slot_number = 0; // default to 0, let our caller decide how to 
						 // interpret that.  
			
	if ( logappend ) {
			// We currently use the extension of the starter log file
			// name to determine which slot we are.  Strange.
		char const *log_basename = condor_basename(logappend);
		MyString prefix;

		char* resource_prefix = param("STARTD_RESOURCE_PREFIX");
		if( resource_prefix ) {
			prefix.formatstr(".%s",resource_prefix);
			free( resource_prefix );
		}
		else {
			prefix = ".slot";
		}

		tmp = strstr(log_basename, prefix.Value());
		if ( tmp ) {				
			prefix += "%d";
			if ( sscanf(tmp, prefix.Value(), &slot_number) < 1 ) {
				// if we couldn't parse it, leave it at 0.
				slot_number = 0;
			}
		} 

		free(logappend);
	}

	return slot_number;
}

MyString
CStarter::getMySlotName(void) {
	
	char *logappend = param("STARTER_LOG");		
	const char *tmp = NULL;
		
	MyString slotName = "";
			
	if ( logappend ) {
			// We currently use the extension of the starter log file
			// name to determine which slot we are.  Strange.
		char const *log_basename = condor_basename(logappend);
		MyString prefix;

		char* resource_prefix = param("STARTD_RESOURCE_PREFIX");
		if( resource_prefix ) {
			prefix.formatstr(".%s",resource_prefix);
			free( resource_prefix );
		}
		else {
			prefix = ".slot";
		}

		tmp = strstr(log_basename, prefix.Value());
		if ( tmp ) {				
			slotName = (tmp + 1); // skip the .
		} 

		free(logappend);
	}

	return slotName;
}


void
CStarter::closeSavedStdin( void )
{
	if( starter_stdin_fd > -1 ) {
		close( starter_stdin_fd );
		starter_stdin_fd = -1;
	}
}


void
CStarter::closeSavedStdout( void )
{
	if( starter_stdout_fd > -1 ) {
		close( starter_stdout_fd );
		starter_stdout_fd = -1;
	}
}


void
CStarter::closeSavedStderr( void )
{
	if( starter_stderr_fd > -1 ) {
		close( starter_stderr_fd );
		starter_stderr_fd = -1;
	}
}


int
CStarter::classadCommand( int, Stream* s )
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
		MyString err_msg = "Starter does not support command (";
		err_msg += tmp;
		err_msg += ')';
		sendErrorReply( s, tmp, CA_INVALID_REQUEST, err_msg.Value() );
		return FALSE;
	}
	return TRUE;
}


int 
CStarter::updateX509Proxy( int cmd, Stream* s )
{
	ASSERT(s);
	ReliSock* rsock = (ReliSock*)s;
	ASSERT(jic);
	return jic->updateX509Proxy(cmd,rsock) ? TRUE : FALSE;
}


bool
CStarter::removeTempExecuteDir( void )
{
	if( is_gridshell ) {
			// we didn't make our own directory, so just bail early
		return true;
	}

	MyString dir_name = "dir_";
	dir_name += (int)daemonCore->getpid();

#if !defined(WIN32)
	if (condorPrivSepHelper() != NULL) {
		MyString path_name;
		path_name.formatstr("%s/%s", Execute, dir_name.Value());
		if (!privsep_remove_dir(path_name.Value())) {
			dprintf(D_ALWAYS,
			        "privsep_remove_dir failed for %s\n",
			        path_name.Value());
			return false;
		}
		return true;
	}
#endif

		// Remove the directory from all possible chroots.
	pair_strings_vector root_dirs = root_dir_list();
	bool has_failed = false;
	for (pair_strings_vector::const_iterator it=root_dirs.begin(); it != root_dirs.end(); ++it) {
		const char *full_execute_dir = dirscat(it->second.c_str(), Execute);
		if (!full_execute_dir) {
			continue;
		}
		Directory execute_dir( full_execute_dir, PRIV_ROOT );
		if ( execute_dir.Find_Named_Entry( dir_name.Value() ) ) {

			// since we chdir()'d to the execute directory, we can't
			// delete it until we get out (at least on WIN32). So lets
			// just chdir() to EXECUTE so we're sure we can remove it. 
			if (chdir(Execute)) {
				dprintf(D_ALWAYS, "Error: chdir(%s) failed: %s\n", Execute, strerror(errno));
			}

			dprintf( D_FULLDEBUG, "Removing %s%c%s\n", Execute,
					 DIR_DELIM_CHAR, dir_name.Value() );
			if (!execute_dir.Remove_Current_File()) {
				has_failed = true;
			}
		}
		delete [] full_execute_dir;
	}
	return !has_failed;
}

#if !defined(WIN32)
void
CStarter::exitAfterGlexec( int code )
{
	// tell Daemon Core to uninitialize its process family tracking
	// subsystem. this will make sure that we tell our ProcD to exit,
	// if we started one
	daemonCore->Proc_Family_Cleanup();

	// now we blow away the directory that the startd set up for us
	// using glexec. this directory will be the parent directory of
	// EXECUTE. we first "cd /", so that our working directory
	// is not in the directory we're trying to delete
	if (chdir( "/" )) {
		dprintf(D_ALWAYS, "Error: chdir(\"/\") failed: %s\n", strerror(errno));
	}
	char* glexec_dir_path = condor_dirname( Execute );
	ASSERT( glexec_dir_path );
	Directory glexec_dir( glexec_dir_path );
	glexec_dir.Remove_Entire_Directory();
	rmdir( glexec_dir_path );
	free( glexec_dir_path );

	// all done
	exit( code );
}
#endif

bool
CStarter::WriteAdFiles()
{

	ClassAd* ad;
	const char* dir = this->GetWorkingDir();
	MyString ad_str, filename;
	FILE* fp;
	bool ret_val = true;

	// Write the job ad first
	ad = this->jic->jobClassAd();
	if (ad != NULL)
	{
		filename.formatstr("%s%c%s", dir, DIR_DELIM_CHAR, JOB_AD_FILENAME);
		fp = safe_fopen_wrapper_follow(filename.Value(), "w");
		if (!fp)
		{
			dprintf(D_ALWAYS, "Failed to open \"%s\" for to write job ad: "
						"%s (errno %d)\n", filename.Value(),
						strerror(errno), errno);
			ret_val = false;
		}
		else
		{
			fPrintAd(fp, *ad, true);
			fclose(fp);
		}
	}
	else
	{
		// If there is no job ad, this is a problem
		ret_val = false;
	}

	// Write the machine ad
	ad = this->jic->machClassAd();
	if (ad != NULL)
	{
		filename.formatstr("%s%c%s", dir, DIR_DELIM_CHAR, MACHINE_AD_FILENAME);
		fp = safe_fopen_wrapper_follow(filename.Value(), "w");
		if (!fp)
		{
			dprintf(D_ALWAYS, "Failed to open \"%s\" for to write machine "
						"ad: %s (errno %d)\n", filename.Value(),
					strerror(errno), errno);
			ret_val = false;
		}
		else
		{
			fPrintAd(fp, *ad, true);
			fclose(fp);
		}
	}

	return ret_val;
}
