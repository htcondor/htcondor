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
#include "condor_syscall_mode.h"   // moronic: must go before condor_config
#include "condor_config.h"
#if defined(__GNUG__)
#pragma implementation "list.h"
#endif
#include "starter.h"
#include "vanilla_proc.h"
#include "java_proc.h"
#include "mpi_master_proc.h"
#include "mpi_comrade_proc.h"
#include "syscall_numbers.h"
#include "my_hostname.h"
#include "internet.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "condor_random_num.h"
#include "io_proxy.h"
#include "condor_ver_info.h"


extern ReliSock *syscall_sock;

extern "C" int get_random_int();

/* CStarter class implementation */

CStarter::CStarter()
{
	InitiatingHost = NULL;
	Execute = NULL;
	UIDDomain = NULL;
	FSDomain = NULL;
	Key = get_random_int()%get_random_int();
	ShuttingDown = FALSE;
	ShadowVersion = NULL;
}

CStarter::~CStarter()
{
	if (Execute) free(Execute);
	if (UIDDomain) free(UIDDomain);
	if (FSDomain) free(FSDomain);
	if (ShadowVersion) free(ShadowVersion);
}

bool
CStarter::Init( char peer[] )
{
	InitiatingHost = peer;

	dprintf(D_ALWAYS, "Submitting machine is \"%s\"\n", InitiatingHost);

	Config();

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

	// init environment info
	char *mfhn = strnewp ( my_full_hostname() );
	REMOTE_CONDOR_register_machine_info(UIDDomain, FSDomain,
				   daemonCore->InfoCommandSinfulString(), 
				   mfhn, Key);
	delete [] mfhn;


	set_resource_limits();

	return StartJob();
}

void
CStarter::Config()
{
	if (Execute) free(Execute);
	if ((Execute = param("EXECUTE")) == NULL) {
		EXCEPT("Execute directory not specified in config file.");
	}

	if (UIDDomain) free(UIDDomain);
	if ((UIDDomain = param("UID_DOMAIN")) == NULL) {
		EXCEPT("UID_DOMAIN not specified in config file.");
	}

	if (FSDomain) free(FSDomain);
	if ((FSDomain = param("FILESYSTEM_DOMAIN")) == NULL) {
		EXCEPT("FILESYSTEM_DOMAIN not specified in config file.");
	}
}

int
CStarter::ShutdownGraceful(int)
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownGraceful all jobs.\n");
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
        // We want to get the jobAd first, make an appropriate 
        // type of starter, *then* call StartJob() on it.
    dprintf ( D_FULLDEBUG, "In CStarter::StartJob()\n" );

    ClassAd *jobAd = new ClassAd;

	if (REMOTE_CONDOR_get_job_info(jobAd) < 0) {
		dprintf(D_ALWAYS, 
				"Failed to get job info from Shadow.  Aborting StartJob.\n");
		return false;
	}

	int universe = CONDOR_UNIVERSE_STANDARD;
	if ( jobAd->LookupInteger( ATTR_JOB_UNIVERSE, universe ) < 1 ) {
		dprintf( D_ALWAYS, "Job doesn't specify universe, assuming STANDARD\n" );
	}

		// Grab the version of the Shadow from the job ad

	jobAd->LookupString(ATTR_SHADOW_VERSION,&ShadowVersion);
	if ( !ShadowVersion ) {
		dprintf(D_FULLDEBUG,"Version of Shadow unknown (pre v6.3.2)\n");
	} else {
		dprintf(D_FULLDEBUG,"Version of Shadow is %s\n",ShadowVersion);
	}

		// Now that we have the job ad, figure out what the owner
		// should be and initialize our priv_state code:
	if( ! InitUserPriv(jobAd) ) {
		dprintf( D_ALWAYS, "ERROR: Failed to determine what user "
				 "to run this job as, aborting\n" );
		return false;
	}

		// Now that we have the right user for priv_state code, we can
		// finally make the scratch execute directory for this job.

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

	sprintf( WorkingDir, "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
			 daemonCore->getpid() );
	if( mkdir(WorkingDir, 0777) < 0 ) {
		dprintf( D_ALWAYS, "couldn't create dir %s: %s\n", WorkingDir,
				 strerror(errno) );
		return false;
	}

#ifdef WIN32
		// On NT, we've got to manually set the acls, too.
	{
		perm dirperm;
		dirperm.init(nobody_login);
		int ret_val = dirperm.set_acls( WorkingDir );
		if ( ret_val < 0 ) {
			dprintf(D_ALWAYS,"UNABLE TO SET PERMISSIONS ON EXECUTE DIRECTORY");
			return false;
		}
	}
#endif /* WIN32 */

	if( chdir(WorkingDir) < 0 ) {
		dprintf( D_ALWAYS, "couldn't move to %s: %s\n", WorkingDir,
				 strerror(errno) ); 
		return false;
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", WorkingDir );

	int want_io_proxy = 0;
	char io_proxy_config_file[_POSIX_PATH_MAX];

	if( jobAd->LookupBool( ATTR_WANT_IO_PROXY, want_io_proxy ) < 1 ) {
		dprintf( D_FULLDEBUG, "StartJob: Job does not define %s\n", 
				 ATTR_WANT_IO_PROXY );
		want_io_proxy = 0;
	} else {
		dprintf( D_ALWAYS, "StartJob: Job has %s=%s\n", ATTR_WANT_IO_PROXY, 
				 want_io_proxy ? "true" : "false" );
	}

	if( want_io_proxy || universe==CONDOR_UNIVERSE_JAVA ) {
		sprintf(io_proxy_config_file,"%s%cchirp.config",WorkingDir,DIR_DELIM_CHAR);
		if(!io_proxy.init(io_proxy_config_file)) {
			dprintf(D_ALWAYS,"StartJob: Couldn't initialize proxy.\n");
			return false;
		} else {
			dprintf(D_ALWAYS,"StartJob: Initialized IO Proxy.\n");
		}
	}
		
		// Return to our old priv state
	set_priv ( priv );

    // printAdToFile( jobAd, "/tmp/starter_ad" );
	if( DebugFlags & D_JOB ) {
		dprintf( D_JOB, "*** Job ClassAd ***\n" );  
		jobAd->dPrint( D_JOB );
        dprintf( D_JOB, "--- End of ClassAd ---\n" );
	}

		// Now that the scratch dir is setup, we can figure out what
		// kind of job we're starting up, instantiate the appropriate
		// userproc class, and actually start the job.

	dprintf( D_ALWAYS, "Starting a %s universe job.\n",CondorUniverseName(universe));

	UserProc *job;

	switch ( universe )  
	{
		case CONDOR_UNIVERSE_VANILLA:
			job = new VanillaProc( jobAd );
			break;
		case CONDOR_UNIVERSE_JAVA:
			job = new JavaProc( jobAd, WorkingDir );
			break;
		case CONDOR_UNIVERSE_MPI: {
			int is_master = FALSE;
			dprintf ( D_FULLDEBUG, "Is master: %s\n", ATTR_MPI_IS_MASTER );
			if ( jobAd->LookupBool( ATTR_MPI_IS_MASTER, is_master ) < 1 ) {
				is_master = FALSE;
			}
			
			dprintf ( D_FULLDEBUG, "is_master : %d\n", is_master );

			if ( is_master ) {
				dprintf ( D_FULLDEBUG, "Firing up a MPIMasterProc\n" );
				job = new MPIMasterProc( jobAd );
			} else {
				dprintf ( D_FULLDEBUG, "Firing up a MPIComradeProc\n" );
				job = new MPIComradeProc( jobAd );
			}
			break;
		}
		default:
			dprintf( D_ALWAYS, "I don't support universe %d (%s)\n",universe,CondorUniverseName(universe));
			return false;
	} /* switch */

	if (job->StartJob()) {
		JobList.Append(job);		
		return true;
	} else {
		delete job;
		return false;
	}
}


bool
CStarter::InitUserPriv( ClassAd* jobAd )
{

#ifndef WIN32
	// Unix

		// First, we decide if we're in the same UID_DOMAIN as the
		// submitting machine.  If so, we'll try to initialize
		// user_priv via ATTR_OWNER.  If there's no such user in the
		// passwd file, SOFT_UID_DOMAIN is True, and we're talking to
		// at least a 6.3.2 version of the shadow, we'll do a remote 
		// system call to ask the shadow what uid and gid we should
		// use.  If SOFT_UID_DOMAIN is False and there's no such user
		// in the password file, but the UID_DOMAIN's match, it's a
		// fatal error.  If the UID_DOMAIN's just don't match, we
		// initialize as "nobody".
	
	char* owner = NULL;

	if( jobAd->LookupString( ATTR_OWNER, &owner ) != 1 ) {
		dprintf( D_ALWAYS, "ERROR: %s not found in JobAd.  Aborting.\n", 
				 ATTR_OWNER );
		return false;
	}

	if( SameUidDomain() ) {
			// Cool, we can try to use ATTR_OWNER directly.
			// NOTE: we want to use the "quiet" version of
			// init_user_ids, since if we're dealing with a
			// "SOFT_UID_DOMAIN = True" scenario, it's entirely
			// possible this call will fail.  We don't want to fill up
			// the logs with scary and misleading error messages.
		if( ! init_user_ids_quiet(owner) ) { 
				// There's a problem, maybe SOFT_UID_DOMAIN can help.
			bool shadow_is_old = false;
			bool try_soft_uid = false;
			char* soft_uid = param( "SOFT_UID_DOMAIN" );
			if( soft_uid ) {
				if( soft_uid[0] == 'T' || soft_uid[0] == 't' ) {
					try_soft_uid = true;
				}
				free( soft_uid );
			}
			if( try_soft_uid ) {
					// first, see if the shadow is new enough to
					// support the RSC we need to do...
				if( GetShadowVersion() ) {
					CondorVersionInfo ver(GetShadowVersion(), "SHADOW");
					if( ! ver.built_since_version(6,3,2) ) {
						shadow_is_old = true;
					}
				} else {
						// Don't even know the shadow version...
					shadow_is_old = true;
				}
			} else {
					// No soft_uid_domain or it's set to False.  No
					// need to do the RSC, we can just fail.
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file and SOFT_UID_DOMAIN is False\n",
						 owner ); 
				free( owner );
				return false;
            }

				// if the shadow is old, we have to just print an error
				// message and fail, since we can't do the RSC we need
				// to find out the right uid/gid.
			if( shadow_is_old ) {
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file,\n"
						 "\t\tSOFT_UID_DOMAIN is True, but the "
						 "condor_shadow on the submitting\n"
						 "\t\thost is too old to support SOFT_UID_DOMAIN. "
						 " You must upgrade\n"
						 "\t\tCondor on the submitting host to at least "
						 "version 6.3.2.\n", owner );
				free( owner );
				return false;
			}

				// if we're here, it means that 1) the owner we want
				// isn't in the passwd file, 2) SOFT_UID_DOMAIN is
				// True, and 3) the shadow we're talking to can
				// support the CONDOR_REMOTE_get_user_info RSC.  So,
				// we'll do that call to get the uid/gid pair we need
				// and initialize user priv with that. 

			ClassAd user_info;
			if( REMOTE_CONDOR_get_user_info( &user_info ) < 0 ) {
				dprintf( D_ALWAYS, "ERROR: "
						 "REMOTE_CONDOR_get_user_info() failed\n" );
				dprintf( D_ALWAYS, "ERROR: Uid for \"%s\" not found in "
						 "passwd file,\n"
						 "\t\tSOFT_UID_DOMAIN is True, but the "
						 "condor_shadow on the submitting\n"
						 "\t\thost cannot support SOFT_UID_DOMAIN. "
						 " You must upgrade\n"
						 "\t\tCondor on the submitting host to at least "
						 "version 6.3.2.\n", owner );
				free( owner );
				return false;
			}

			int user_uid, user_gid;
			if( user_info.LookupInteger( ATTR_UID, user_uid ) != 1 ) {
				dprintf( D_ALWAYS, "user_info ClassAd does not contain %s!\n", 
						 ATTR_UID );
				free( owner );
				return false;
			}
			if( user_info.LookupInteger( ATTR_GID, user_gid ) != 1 ) {
				dprintf( D_ALWAYS, "user_info ClassAd does not contain %s!\n", 
						 ATTR_GID );
				free( owner );
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
				free( owner );
				return false;
			}
		} else {  
			dprintf( D_FULLDEBUG, "Initialized user_priv as \"%s\"\n", 
					 owner );
		}
	} else {
			// We're in the wrong UID domain, use "nobody".
		if( ! init_user_ids("nobody") ) { 
			dprintf( D_ALWAYS, "ERROR: Could not initialize user priv "
					 "as \"nobody\"\n" );
			free( owner );
			return false;
		}
	}
		// deallocate owner string so we don't leak memory.
	free( owner );

#else
	// Win32
	// taken origionally from OsProc::StartJob.  Here we create the
	// user and initialize user_priv.
	// we only support running jobs as user nobody for the first pass
	char nobody_login[60];
	// sprintf(nobody_login,"condor-run-dir_%d",daemonCore->getpid());
	sprintf(nobody_login,"condor-run-%d",daemonCore->getpid());
	init_user_nobody_loginname(nobody_login);
	init_user_ids("nobody");
#endif

	return true;
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

	dprintf(D_ALWAYS,"Job exited, pid=%d, status=%d\n",pid,exit_status);
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		all_jobs++;
		if (job->GetJobPid()==pid && job->JobExit(pid, exit_status)) {
			handled_jobs++;
			JobList.DeleteCurrent();
			delete job;
		}
	}
	dprintf(D_FULLDEBUG,"Reaper: all=%d handled=%d ShuttingDown=%d\n",
		all_jobs,handled_jobs,ShuttingDown);
	if (handled_jobs == 0) {
		dprintf(D_ALWAYS, "unhandled job exit: pid=%d, status=%d\n", pid,
				exit_status);
	}
	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		DC_Exit(0);
	}
	return 0;
}

int CStarter::printAdToFile(ClassAd *ad, char* JobHistoryFileName) {

    FILE* LogFile=fopen(JobHistoryFileName,"a");
    if ( !LogFile ) {
        dprintf(D_ALWAYS,"ERROR saving to history file; cannot open%s\n", 
                JobHistoryFileName);
        return false;
    }
    if (!ad->fPrint(LogFile)) {
        dprintf(D_ALWAYS, "ERROR in Scheduler::LogMatchEnd - failed to "
                "write classad to log file %s\n", JobHistoryFileName);
        fclose(LogFile);
        return false;
    }
    fprintf(LogFile,"***\n");   // separator
    fclose(LogFile);
    return true;
}


bool
CStarter::SameUidDomain( void ) 
{
	if( ! UIDDomain ) {
		EXCEPT( "CStarter::SameUidDomain called with NULL UIDDomain!" );
	}
	if( ! InitiatingHost ) {
		EXCEPT( "CStarter::SameUidDomain called with NULL InitiatingHost!" );
	}
	if( host_in_domain(InitiatingHost, UIDDomain) ) {
		return true;
	}
	return false;
}
