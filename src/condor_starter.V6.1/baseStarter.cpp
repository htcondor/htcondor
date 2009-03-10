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

extern "C" int get_random_int();
extern int main_shutdown_fast();

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
		WorkingDir.sprintf( "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
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

	sysapi_set_resource_limits();

		// initialize our JobInfoCommunicator
	if( ! jic->init() ) {
		dprintf( D_ALWAYS, 
				 "Failed to initialize JobInfoCommunicator, aborting\n" );
		return false;
	}

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
	removeTempExecuteDir();
#if !defined(WIN32)
	if ( GetEnv( "CONDOR_GLEXEC_STARTER_CLEANUP_FLAG" ) ) {
		exitAfterGlexec( code );
	}
#endif
	DC_Exit( code );
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
		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	if ( jic ) {
		jic->gotShutdownGraceful();
	}
	return ( this->ShutdownGraceful( ) );
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
		this->allJobsDone();
		return 1;
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
		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions (for example, disabiling file transfer if
		// we're talking to a shadow)
	if( jic ) {
		jic->gotShutdownFast();
	}
	return ( this->ShutdownFast( ) );
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
		this->allJobsDone();
		return ( true );
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

	s->decode();
	if( !s->get(hold_reason) ||
		!s->get(hold_code) ||
		!s->get(hold_subcode) ||
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
		cpsh->initialize_sandbox(WorkingDir.Value());
	}
	else {
		if( mkdir(WorkingDir.Value(), 0777) < 0 ) {
			dprintf( D_FAILURE|D_ALWAYS,
			         "couldn't create dir %s: %s\n",
			         WorkingDir.Value(),
			         strerror(errno) );
			set_priv( priv );
			return false;
		}
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
	
	char* eed = param("ENCRYPT_EXECUTE_DIRECTORY");
	
	if ( eed ) {
		
		if (eed[0] == 'T' || eed[0] == 't') { // user wants encryption
			
			// dynamically load our encryption functions to preserve 
			// compatability with NT4 :(
			
			typedef BOOL (WINAPI *FPEncryptionDisable)(LPCWSTR,BOOL);
			typedef BOOL (WINAPI *FPEncryptFileA)(LPCSTR);
			bool efs_support = true;
			
			HINSTANCE advapi = LoadLibrary("ADVAPI32.dll");
			if ( !advapi ) {
				dprintf(D_FULLDEBUG, "Can't load advapi32.dll\n");
				efs_support = false;
			}
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
		
		free(eed);
		eed = NULL;

	} // ENCRYPT_EXECUTE_DIRECTORY has a value
	

#endif /* WIN32 */

	if( chdir(WorkingDir.Value()) < 0 ) {
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
			m_privsep_helper->chown_sandbox_to_user();
		}
		else if( univ == CONDOR_UNIVERSE_VM ) {
				// the vmgahp will chown the sandbox to the user
			m_privsep_helper->set_sandbox_owned_by_user();
		}
	}

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
	if ( jobAd->Lookup( ATTR_DEFERRAL_TIME ) != NULL ) {
			//
		 	// Make sure that the expression evaluated and we 
		 	// got a positive integer. Otherwise we'll have to kick out
		 	//
		if ( ! jobAd->EvalInteger( ATTR_DEFERRAL_TIME, NULL, deferralTime ) ) {
			error.sprintf( "Invalid deferred execution time for Job %d.%d.",
							this->jic->jobCluster(),
							this->jic->jobProc() );
			abort = true;
		} else if ( deferralTime <= 0 ) {
			error.sprintf( "Invalid execution time '%d' for Job %d.%d.",
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
			if ( jobAd->Lookup( ATTR_DEFERRAL_WINDOW ) != NULL &&
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
					error.sprintf( "Job %d.%d missed its execution time.",
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
		EXCEPT( error.Value() );
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
int
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
			return TRUE;
		} else {
			dprintf( D_ALWAYS, "Failed to start prescript, exiting\n" );
				// TODO notify the JIC somehow?
			main_shutdown_fast();
			return FALSE;
		}
	}

		// if there's no pre-script, we can go directly to trying to
		// spawn the main job
	return SpawnJob();
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
		job->Continue();
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
				cpsh->chown_sandbox_to_condor();
			}

			bool transfer_ok = jic->uploadWorkingFiles();

			if (cpsh != NULL) {
				cpsh->chown_sandbox_to_user();
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
		// now that all user processes are complete, change the
		// sandbox ownership back over to condor. if this is a VM
		// universe job, this chown will have already been
		// performed by the VMGahp, since it does some post-
		// processing on files in the sandbox
	if (m_privsep_helper != NULL) {
		if (jobUniverse != CONDOR_UNIVERSE_VM) {
			m_privsep_helper->chown_sandbox_to_condor();
		}
	}

		// No more jobs, notify our JobInfoCommunicator.
	if (jic->allJobsDone()) {
			// JIC::allJobsDone returned true: we're ready to move on.
		return transferOutput();
	}
		// JIC::allJobsDone() returned false: propagate that so we
		// halt the cleanup process and wait for external events.
	return false;
}


bool
CStarter::transferOutput( void )
{
	if (!jic->transferOutput()) {
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

	UserProc* uproc;
	m_job_list.Rewind();
	while ((uproc = m_job_list.Next()) != NULL) {
		uproc->PublishToEnv( proc_env );
	}
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
	int slot = getMySlotNumber();
	if (!slot) {
		slot = 1;
	}
	proc_env->SetEnv(env_name.Value(), slot);

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
			prefix.sprintf(".%s",resource_prefix);
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
		path_name.sprintf("%s/%s", Execute, dir_name.Value());
		if (!privsep_remove_dir(path_name.Value())) {
			dprintf(D_ALWAYS,
			        "privsep_remove_dir failed for %s\n",
			        path_name.Value());
			return false;
		}
		return true;
	}
#endif

	Directory execute_dir( Execute, PRIV_ROOT );
	if ( execute_dir.Find_Named_Entry( dir_name.Value() ) ) {

		// since we chdir()'d to the execute directory, we can't
		// delete it until we get out (at least on WIN32). So lets
		// just chdir() to EXECUTE so we're sure we can remove it. 
		chdir(Execute);

		dprintf( D_FULLDEBUG, "Removing %s%c%s\n", Execute,
				 DIR_DELIM_CHAR, dir_name.Value() );
		return execute_dir.Remove_Current_File();
	}
	return true;
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
	chdir( "/" );
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
