/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#if defined(__GNUG__)
#pragma implementation "list.h"
#endif
#include "starter.h"
#include "vanilla_proc.h"
#include "java_proc.h"
#include "mpi_master_proc.h"
#include "mpi_comrade_proc.h"
#include "parallel_master_proc.h"
#include "parallel_comrade_proc.h"
#include "my_hostname.h"
#include "internet.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "condor_random_num.h"
#include "../condor_sysapi/sysapi.h"


extern "C" int get_random_int();
extern int main_shutdown_fast();

/* CStarter class implementation */

CStarter::CStarter()
{
	Execute = NULL;
	ShuttingDown = FALSE;
	jic = NULL;
	jobUniverse = CONDOR_UNIVERSE_VANILLA;

}


CStarter::~CStarter()
{
	if( Execute ) {
		free(Execute);
	}
	if( jic ) {
		delete jic;
	}
}


bool
CStarter::Init( JobInfoCommunicator* my_jic )
{
	if( ! my_jic ) {
		EXCEPT( "CStarter::Init() called with no JobInfoCommunicator!" ); 
	}
	if( jic ) {
		delete( jic );
	}
	jic = my_jic;

	Config();

		// Now that we know what Execute is, we can figure out what
		// directory the starter will be working in and save that,
		// since we'll want this info a lot while we initialize and
		// figure things out.
	sprintf( WorkingDir, "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
			 (long)daemonCore->getpid() );


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
	daemonCore->Register_Reaper("Reaper", (ReaperHandlercpp)&CStarter::Reaper,
		"Reaper", this);

	sysapi_set_resource_limits();

		// initialize our JobInfoCommunicator
	if( ! jic->init() ) {
		dprintf( D_ALWAYS, 
				 "Failed to initialize JobInfoCommunicator, aborting\n" );
		return false;
	}

		// try to spawn our job
	return StartJob();
}


void
CStarter::Config()
{
	if( Execute ) {
		free( Execute );
	}
	if( (Execute = param("EXECUTE")) == NULL ) {
		EXCEPT("Execute directory not specified in config file.");
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


bool
CStarter::StartJob()
{
    dprintf ( D_FULLDEBUG, "In CStarter::StartJob()\n" );

	ClassAd* jobAd = jic->jobClassAd();

	if ( jobAd->LookupInteger( ATTR_JOB_UNIVERSE, jobUniverse ) < 1 ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
	}

		// Now, ask our JobInfoCommunicator to setup the environment
		// where our job is going to execute.  This might include
		// doing file transfer stuff, who knows.  Whenever the JIC is
		// done, it'll call our jobEnvironmentReady() method so we can
		// actually spawn the job.
	jic->setupJobEnvironment();

	return true;
}


bool
CStarter::createTempExecuteDir( void )
{
		// Once our JobInfoCommmunicator has initialized the right
		// user for the priv_state code, we can finally make the
		// scratch execute directory for this job.

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
		perm dirperm;
		const char * nobody_login = get_user_loginname();
		ASSERT(nobody_login);
		dirperm.init(nobody_login);
		int ret_val = dirperm.set_acls( WorkingDir );
		if ( ret_val < 0 ) {
			dprintf(D_ALWAYS,"UNABLE TO SET PERMISSIONS ON EXECUTE DIRECTORY");
			set_priv( priv );
			return false;
		}
	}
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
		// Now that we've got all our files, we can figure out what
		// kind of job we're starting up, instantiate the appropriate
		// userproc class, and actually start the job.

	dprintf( D_ALWAYS, "Starting a %s universe job with ID: %d.%d\n",
			 CondorUniverseName(jobUniverse), jic->jobCluster(),
			 jic->jobProc() );

	ClassAd* jobAd = jic->jobClassAd();

	UserProc *job;
	switch ( jobUniverse )  
	{
		case CONDOR_UNIVERSE_VANILLA:
			job = new VanillaProc( jobAd );
			break;
		case CONDOR_UNIVERSE_JAVA:
			job = new JavaProc( jobAd, WorkingDir );
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
		case CONDOR_UNIVERSE_PARALLEL: {
			int is_master = FALSE;
			if ( jobAd->LookupBool(ATTR_PARALLEL_IS_MASTER, is_master) < 1 ) {
				is_master = FALSE;
			}
			if ( is_master ) {
				dprintf ( D_FULLDEBUG, "Starting a ParallelMasterProc\n" );
				job = new ParallelMasterProc( jobAd );
			} else {
				dprintf ( D_FULLDEBUG, "Starting a ParallelComradeProc\n" );
				job = new ParallelComradeProc( jobAd );
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
		dprintf( D_ALWAYS, "Job exited, pid=%d, signal=%d\n", pid,
				 WTERMSIG(exit_status) );
	} else {
		dprintf( D_ALWAYS, "Job exited, pid=%d, status=%d\n", pid,
				 WEXITSTATUS(exit_status) );
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

			// No more jobs, notify our JobInfoCommunicator
		jic->allJobsDone();

			// Now that we're done transfering files and/or doing all
			// our cleanup, we can finally go through the
			// CleanedUpJobList and call JobExit() on all the procs in
			// there.
		CleanedUpJobList.Rewind();
		while( (job = CleanedUpJobList.Next()) != NULL) {
			job->JobExit();
			CleanedUpJobList.DeleteCurrent();
			delete job;
		}
			// No more jobs, all cleanup done, notify our JIC
		jic->allJobsGone();
	}

	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		DC_Exit(0);
	}
	return 0;
}


bool
CStarter::publishUpdateAd( ClassAd* ad )
{

		// Iterate through all our UserProcs and have those publish,
		// as well.  This method is virtual, so we'll get all the
		// goodies from derived classes, as well.  If any of them put
		// info into the ad, return true.  Otherwise, return false.
	bool found_one = false;
	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		if( job->PublishUpdateAd(ad) ) {
			found_one = true;
		}
	}
	return found_one;
}


