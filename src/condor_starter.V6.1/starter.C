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
#include "condor_debug.h"
#include "condor_config.h"
#if defined(__GNUG__)
#pragma implementation "list.h"
#endif
#include "starter.h"
#include "script_proc.h"
#include "vanilla_proc.h"
#include "java_proc.h"
#include "tool_daemon_proc.h"
#include "mpi_master_proc.h"
#include "mpi_comrade_proc.h"
#include "parallel_proc.h"
#include "my_hostname.h"
#include "internet.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "classad_command_util.h"
#include "condor_random_num.h"
#include "../condor_sysapi/sysapi.h"
#include "build_job_env.h"

#include "perm.h"
#include "filename_tools.h"
#include "directory.h"


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
CStarter::Init( JobInfoCommunicator* my_jic, const char* orig_cwd,
				bool is_gridshell, int stdin_fd, int stdout_fd, 
				int stderr_fd )
{
	if( ! my_jic ) {
		EXCEPT( "CStarter::Init() called with no JobInfoCommunicator!" ); 
	}
	if( jic ) {
		delete( jic );
	}
	jic = my_jic;

	if( orig_cwd ) {
		this->orig_cwd = strdup( orig_cwd );
	}
	this->is_gridshell = is_gridshell;
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
		sprintf( WorkingDir, "%s", Execute );
	} else {
		sprintf( WorkingDir, "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
				 (long)daemonCore->getpid() );
	}

	// setup daemonCore handlers
	daemonCore->Register_Signal(DC_SIGSUSPEND, "DC_SIGSUSPEND", 
		(SignalHandlercpp)&CStarter::Suspend, "Suspend", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGCONTINUE, "DC_SIGCONTINUE",
		(SignalHandlercpp)&CStarter::Continue, "Continue", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGHARDKILL, "DC_SIGHARDKILL",
		(SignalHandlercpp)&CStarter::ShutdownFast, "ShutdownFast", this, 
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGSOFTKILL, "DC_SIGSOFTKILL",
		(SignalHandlercpp)&CStarter::ShutdownGraceful, "ShutdownGraceful",
		this, IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGPCKPT, "DC_SIGPCKPT",
		(SignalHandlercpp)&CStarter::PeriodicCkpt, "PeriodicCkpt", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGREMOVE, "DC_SIGREMOVE",
		(SignalHandlercpp)&CStarter::Remove, "Remove", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGHOLD, "DC_SIGHOLD",
		(SignalHandlercpp)&CStarter::Hold, "Hold", this,
		IMMEDIATE_FAMILY);
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
	DC_Exit( code );
}


void
CStarter::Config()
{
	if( Execute ) {
		free( Execute );
	}
	if( (Execute = param("EXECUTE")) == NULL ) {
		if( is_gridshell ) {
			Execute = strdup( orig_cwd );
		} else {
			EXCEPT("Execute directory not specified in config file.");
		}
	}

		// Tell our JobInfoCommunicator to reconfig, too.
	jic->config();
}


int
CStarter::ShutdownGraceful(int)
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownGraceful all jobs.\n");

		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	jic->gotShutdownGraceful();

	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		if ( job->ShutdownGraceful() ) {
			// job is completely shut down, so delete it
			JobList.DeleteCurrent();
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
		return 1;
	}	
	return 0;
}


int
CStarter::ShutdownFast(int)
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownFast all jobs.\n");

		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions (for example, disabiling file transfer if
		// we're talking to a shadow)
	if( jic ) {
		jic->gotShutdownFast();
	}

	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		if ( job->ShutdownFast() ) {
			// job is completely shut down, so delete it
			JobList.DeleteCurrent();
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
		return 1;
	}	
	return 0;
}


int
CStarter::Remove( int )
{
	bool jobRunning = false;
	UserProc *job;

	dprintf( D_ALWAYS, "Remove all jobs\n" );

		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	if( jic ) {
		jic->gotRemove();
	}

	JobList.Rewind();
	while( (job = JobList.Next()) != NULL ) {
		if( job->Remove() ) {
			// job is completely shut down, so delete it
			JobList.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
		}
	}
	ShuttingDown = TRUE;
	if (!jobRunning) {
		dprintf( D_FULLDEBUG, "Got Remove when no jobs running\n" );
		return 1;
	}	
	return 0;
}


int
CStarter::Hold( int )
{
	bool jobRunning = false;
	UserProc *job;

	dprintf( D_ALWAYS, "Hold all jobs\n" );

		// tell our JobInfoCommunicator about this so it can take any
		// necessary actions
	if( jic ) {
		jic->gotHold();
	}

	JobList.Rewind();
	while( (job = JobList.Next()) != NULL ) {
		if( job->Hold() ) {
			// job is completely shut down, so delete it
			JobList.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
		}
	}
	ShuttingDown = TRUE;
	if( !jobRunning ) {
		dprintf( D_FULLDEBUG, "Got Hold when no jobs running\n" );
		return 1;
	}	
	return 0;
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
		dprintf( D_ALWAYS, "gridshell running in: \"%s\"\n", WorkingDir ); 
		return true;
	}

		// On Unix, be sure we're in user priv for this.
		// But on NT (at least for now), we should be in Condor priv
		// because the execute directory on NT is not wworld writable.
#ifndef WIN32
		// UNIX
	priv_state priv = set_user_priv();
#else
		// WIN32
	priv_state priv = set_condor_priv();
#endif

	if( mkdir(WorkingDir, 0777) < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS, "couldn't create dir %s: %s\n", 
				 WorkingDir, strerror(errno) );
		set_priv( priv );
		return false;
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
		bool ret_val = dirperm.set_acls( WorkingDir );
		if ( !ret_val ) {
			dprintf(D_ALWAYS,"UNABLE TO SET PERMISSIONS ON EXECUTE DIRECTORY");
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
				wchar_t *WorkingDir_w = new wchar_t[strlen(WorkingDir)+1];
				swprintf(WorkingDir_w, L"%S", WorkingDir);
				EncryptionDisable(WorkingDir_w, FALSE);
				delete[] WorkingDir_w;
				
				if ( EncryptFile(WorkingDir) == 0 ) {
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

	if( chdir(WorkingDir) < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS, "couldn't move to %s: %s\n", WorkingDir,
				 strerror(errno) ); 
		set_priv( priv );
		return false;
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", WorkingDir );
	set_priv( priv );
	return true;
}


int
CStarter::jobEnvironmentReady( void )
{
		// first, see if we're going to need any pre and post scripts
	ClassAd* jobAd = jic->jobClassAd();
	char* tmp = NULL;
	MyString attr;

	attr = "Pre";
	attr += ATTR_JOB_CMD;
	if( jobAd->LookupString(attr.GetCStr(), &tmp) ) {
		free( tmp );
		tmp = NULL;
		pre_script = new ScriptProc( jobAd, "Pre" );
	}

	attr = "Post";
	attr += ATTR_JOB_CMD;
	if( jobAd->LookupString(attr.GetCStr(), &tmp) ) {
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
			job = new JavaProc( jobAd, WorkingDir );
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
		default:
			dprintf( D_ALWAYS, "Starter doesn't support universe %d (%s)\n",
					 jobUniverse, CondorUniverseName(jobUniverse) ); 
			return FALSE;
	} /* switch */

	if (job->StartJob()) {
		JobList.Append(job);

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
				JobList.Append( tool_daemon_proc );
				dprintf( D_FULLDEBUG, "ToolDaemonProc added to JobList\n");
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


int
CStarter::Suspend(int)
{
	dprintf(D_ALWAYS, "Suspending all jobs.\n");

	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		job->Suspend();
	}

		// notify our JobInfoCommunicator that the jobs are suspended
	jic->Suspend();

	return 0;
}


int
CStarter::Continue(int)
{
	dprintf(D_ALWAYS, "Continuing all jobs.\n");

	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		job->Continue();
	}

	// notify our JobInfoCommunicator that the job is being continued
	jic->Continue();

	return 0;
}


int
CStarter::PeriodicCkpt(int)
{
	// TODO
	return 0;
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

	if( pre_script && pre_script->JobCleanup(pid, exit_status) ) {		
			// TODO: deal with shutdown case?!?
		
			// when the pre script exits, we know the JobList is going
			// to be empty, so don't bother with any of the rest of
			// this.  instead, the starter is now able to call
			// SpawnJob() to launch the main job.
		if( ! SpawnJob() ) {
			dprintf( D_ALWAYS, "Failed to start main job, exiting\n" );
			main_shutdown_fast();
			return FALSE;
		}
		return TRUE;
	}

	if( post_script && post_script->JobCleanup(pid, exit_status) ) {		
			// when the post script exits, we know the JobList is going
			// to be empty, so don't bother with any of the rest of
			// this.  instead, the starter is now able to call
			// allJobsdone() to do the final clean up stages.
		allJobsDone();
		return TRUE;
	}


	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		all_jobs++;
		if( job->GetJobPid()==pid && job->JobCleanup(pid, exit_status) ) {
			handled_jobs++;
			JobList.DeleteCurrent();
			CleanedUpJobList.Append(job);
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
			allJobsDone();
		}
	}

	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		StarterExit(0);
	}
	return 0;
}


void
CStarter::allJobsDone( void )
{
		// No more jobs, notify our JobInfoCommunicator
	static bool needs_jic_allJobsDone = true;
	if( needs_jic_allJobsDone && ! jic->allJobsDone() ) {
			/*
			  there was an error with the JIC in this step.  at this
			  point, the only possible reason is if we're talking to a
			  shadow and file transfer failed to send back the files.
			  in this case, just return to DaemonCore and wait for
			  other events (like the shadow reconnecting or the startd
			  deciding the job lease expired and killing us)
			*/

		dprintf( D_ALWAYS, "JIC::allJobsDone() failed, waiting for job "
				 "lease to expire or for a reconnect attempt\n" );
		return;
	}
	needs_jic_allJobsDone = false;

		// Now that we're done transfering files and/or doing all
		// our cleanup, we can finally go through the
		// CleanedUpJobList and call JobExit() on all the procs in
		// there.
	UserProc *job;
	CleanedUpJobList.Rewind();
	while( (job = CleanedUpJobList.Next()) != NULL) {
		if( job->JobExit() ) {
			CleanedUpJobList.DeleteCurrent();
			delete job;
		} else {
			dprintf( D_ALWAYS, "JobExit() failed, waiting for job "
					 "lease to expire or for a reconnect attempt\n" );
			return;
		}
	}
		// No more jobs, all cleanup done, notify our JIC
	jic->allJobsGone();
}


bool
CStarter::publishUpdateAd( ClassAd* ad )
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
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
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
	JobList.Rewind();
	while ((uproc = JobList.Next()) != NULL) {
		uproc->PublishToEnv( proc_env );
	}
	CleanedUpJobList.Rewind();
	while ((uproc = CleanedUpJobList.Next()) != NULL) {
		uproc->PublishToEnv( proc_env );
	}

	ASSERT(jic);
	ClassAd* jobAd = jic->jobClassAd();
	if( jobAd ) {
		// Probing this (file transfer) ourselves is complicated.  The JIC
		// already does it.  Steal his answer.
		bool using_file_transfer = jic->usingFileTransfer();
		StringMap classenv = build_job_env(*jobAd, using_file_transfer);
		classenv.startIterations();
		MyString key,value;
		while(classenv.iterate(key,value)) {
			MyString dummy;
			if( ! proc_env->getenv(key,dummy) ) {
				// Only set the variable if it wasn't already
				// set (let user settings override).
				proc_env->Put(key.Value(),value.Value());
			}
		}
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
		env_name = base.GetCStr();
		env_name += "OUTPUT_CLASSAD";
		proc_env->Put( env_name.GetCStr(), output_ad );
	}
	
		// job scratch space
	env_name = base.GetCStr();
	env_name += "SCRATCH_DIR";
	proc_env->Put( env_name.GetCStr(), GetWorkingDir() );

		// pass through the pidfamily ancestor env vars this process
		// currently has to the job.

		// port regulation stuff
	char* low = param( "LOWPORT" );
	char* high = param( "HIGHPORT" );
	if( low && high ) {
		env_name = base.GetCStr();
		env_name += "HIGHPORT";
		proc_env->Put( env_name.GetCStr(), high );

		env_name = base.GetCStr();
		env_name += "LOWPORT";
		proc_env->Put( env_name.GetCStr(), low );

		free( high );
		free( low );
	} else if( low ) {
		dprintf( D_ALWAYS, "LOWPORT is defined but HIGHPORT is not, "
				 "ignoring LOWPORT\n" );
		free( low );
	} else if( high ) {
		dprintf( D_ALWAYS, "HIGHPORT is defined but LOWPORT is not, "
				 "ignoring HIGHPORT\n" );
		free( high );
    }
}


int
CStarter::getMyVMNumber( void )
{
	
	char *logappend = param("STARTER_LOG");		
	char *tmp = NULL;
		
	int vm_number = 0; // default to 0, let our caller decide how to
					   // interpret that.  
			
	if ( logappend ) {

		// this could break if the user has ".vm" in the 
		// path to the starterlog, but considering it just looked
		// for '.' until now, I think this assumption is safe-enough.

		tmp = strstr(logappend, ".vm");
		if ( tmp ) {				
			if ( sscanf(tmp, ".vm%d", &vm_number) < 1 ) {
				// if we couldn't parse it, set it to 1.
				vm_number = 0;
			}
		} 
		free(logappend);
	}

	return vm_number;
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


bool
CStarter::removeTempExecuteDir( void )
{
	if( is_gridshell ) {
			// we didn't make our own directory, so just bail early
		return true;
	}

	MyString dir_name = "dir_";
	dir_name += (int)daemonCore->getpid();

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
