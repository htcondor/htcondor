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
#include "condor_version.h"
#include "condor_ver_info.h"
#include "../condor_sysapi/sysapi.h"
#include "directory.h"


extern ReliSock *syscall_sock;

extern "C" int get_random_int();
extern int main_shutdown_fast();

/* CStarter class implementation */

CStarter::CStarter()
{
	Execute = NULL;
	UIDDomain = NULL;
	FSDomain = NULL;
	ShuttingDown = FALSE;
	ShadowVersion = NULL;
	shadow = NULL;
	jobAd = NULL;
	jobUniverse = CONDOR_UNIVERSE_VANILLA;
	shadowupdate_tid = -1;
	filetrans = NULL;
	transfer_at_vacate = false;
	requested_exit = false;
	wants_file_transfer = false;
}


CStarter::~CStarter()
{
	if( Execute ) {
		free(Execute);
	}
	if( UIDDomain ) {
		free(UIDDomain);
	}
	if( FSDomain ) {
		free(FSDomain);
	}
	if( ShadowVersion ) {
		delete ShadowVersion;
	}
	if( filetrans ) {
		delete filetrans;
	}
	if( jobAd ) {
		delete jobAd;
	}
	if( shadowupdate_tid != -1 && daemonCore ) {
		daemonCore->Cancel_Timer(shadowupdate_tid);
		shadowupdate_tid = -1;
	}
	if( shadow ) {
		delete shadow;
	}
}


bool
CStarter::Init( char peer[] )
{
	if( shadow ) {
		delete shadow;
	}
	shadow = new DCShadow( peer );
	ASSERT( shadow );

	dprintf(D_ALWAYS, "Submitting machine is \"%s\"\n", shadow->name());

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

	sysapi_set_resource_limits();

	return StartJob();
}

void
CStarter::Config()
{
	if (Execute) free(Execute);
	if ((Execute = param("EXECUTE")) == NULL) {
		EXCEPT("Execute directory not specified in config file.");
	}

	if( UIDDomain ) {
		free(UIDDomain);
	}
	if( ! (UIDDomain = param("UID_DOMAIN")) ) {
		EXCEPT( "UID_DOMAIN not specified in config file." );
	}

	if( FSDomain ) {
		free(FSDomain);
	}
	if( ! (FSDomain = param("FILESYSTEM_DOMAIN")) ) {
		EXCEPT( "FILESYSTEM_DOMAIN not specified in config file." );
	}
}

int
CStarter::ShutdownGraceful(int)
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownGraceful all jobs.\n");
	requested_exit = true;
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

		// Despite what the user wants, do not transfer back any files 
		// on a ShutdownFast.
   transfer_at_vacate = false;   

	dprintf(D_ALWAYS, "ShutdownFast all jobs.\n");
	requested_exit = true;
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


void
CStarter::InitShadowInfo( void )
{
	ASSERT( jobAd );
	ASSERT( shadow );

	if( ! shadow->initFromClassAd(jobAd) ) { 
		dprintf( D_ALWAYS, "Failed to initialize shadow info from job ClassAd!\n" );
		return;
	}

	if( ShadowVersion ) {
		delete ShadowVersion;
		ShadowVersion = NULL;
	}
	char* tmp = shadow->version();
	if( tmp ) {
		dprintf( D_FULLDEBUG, "Version of Shadow is %s\n", tmp );
		ShadowVersion = new CondorVersionInfo( tmp, "SHADOW" );
	} else {
		dprintf( D_FULLDEBUG, "Version of Shadow unknown (pre v6.3.3)\n" ); 
	}
}


bool
CStarter::RegisterStarterInfo( void )
{
	int rval;

	if( ! shadow ) {
		EXCEPT( "RegisterStarterInfo called with NULL DCShadow object" );
	}

		// If the shadow is older than 6.3.3, we need to use the
		// CONDOR_register_machine_info method, which sends a bunch of
		// strings over the wire.  If we're 6.3.3 or later, we can use
		// CONDOR_register_starter_info, which just sends a ClassAd
		// with all the relevent info.
	if( ShadowVersion && ShadowVersion->built_since_version(6,3,3) ) {
		ClassAd* starter_info = new ClassAd;
		char *tmp = NULL;
		char* tmp_val = NULL;
		int size;

		size = strlen(UIDDomain) + strlen(ATTR_UID_DOMAIN) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_UID_DOMAIN, UIDDomain );
		starter_info->Insert( tmp );
		free( tmp );

		size = strlen(FSDomain) + strlen(ATTR_FILE_SYSTEM_DOMAIN) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_FILE_SYSTEM_DOMAIN, FSDomain ); 
		starter_info->Insert( tmp );
		free( tmp );

		tmp_val = my_full_hostname();
		size = strlen(tmp_val) + strlen(ATTR_MACHINE) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_MACHINE, tmp_val );
		starter_info->Insert( tmp );
		free( tmp );

		tmp_val = daemonCore->InfoCommandSinfulString();
 		size = strlen(tmp_val) + strlen(ATTR_STARTER_IP_ADDR) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_STARTER_IP_ADDR, tmp_val );
		starter_info->Insert( tmp );
		free( tmp );

		tmp_val = CondorVersion();
 		size = strlen(tmp_val) + strlen(ATTR_VERSION) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_VERSION, tmp_val );
		starter_info->Insert( tmp );
		free( tmp );

		tmp_val = param( "ARCH" );
		size = strlen(tmp_val) + strlen(ATTR_ARCH) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_ARCH, tmp_val );
		starter_info->Insert( tmp );
		free( tmp );
		free( tmp_val );

		tmp_val = param( "OPSYS" );
		size = strlen(tmp_val) + strlen(ATTR_OPSYS) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_OPSYS, tmp_val );
		starter_info->Insert( tmp );
		free( tmp );
		free( tmp_val );

		tmp_val = param( "CKPT_SERVER_HOST" );
		if( tmp_val ) {
			size = strlen(tmp_val) + strlen(ATTR_CKPT_SERVER) + 5; 
			tmp = (char*) malloc( size * sizeof(char) );
			sprintf( tmp, "%s=\"%s\"", ATTR_CKPT_SERVER, tmp_val ); 
			starter_info->Insert( tmp );
			free( tmp );
			free( tmp_val );
		}

		rval = REMOTE_CONDOR_register_starter_info( starter_info );
		delete( starter_info );

	} else {
			// We've got to use the old method.
		char *mfhn = strnewp ( my_full_hostname() );
		rval = REMOTE_CONDOR_register_machine_info( UIDDomain,
			     FSDomain, daemonCore->InfoCommandSinfulString(), 
				 mfhn, 0 );
		delete [] mfhn;
	}
	if( rval < 0 ) {
		return false;
	}
	return true;
}


bool
CStarter::StartJob()
{
        // We want to get the jobAd first, make an appropriate 
        // type of starter, *then* call StartJob() on it.
    dprintf ( D_FULLDEBUG, "In CStarter::StartJob()\n" );

		// Instantiate a new ClassAd for the job we'll be starting,
		// and get a copy of it from the shadow.
	if( jobAd ) {
		delete jobAd;
	}
    jobAd = new ClassAd;
	if (REMOTE_CONDOR_get_job_info(jobAd) < 0) {
		dprintf(D_FAILURE|D_ALWAYS, 
				"Failed to get job info from Shadow.  Aborting StartJob.\n");
		return false;
	}

		// For debugging, see if there's a special attribute in the
		// job ad that sends us into an infinite loop, waiting for
		// someone to attach with a debugger
	int starter_should_wait = 0;
	jobAd->LookupInteger( ATTR_STARTER_WAIT_FOR_DEBUG,
						  starter_should_wait );
	if( starter_should_wait ) {
		dprintf( D_ALWAYS, "Job requested starter should wait for "
				 "debugger with %s=%d, going into infinite loop\n",
				 ATTR_STARTER_WAIT_FOR_DEBUG, starter_should_wait );
		while( starter_should_wait );
	}

	if ( jobAd->LookupInteger( ATTR_JOB_UNIVERSE, jobUniverse ) < 1 ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
	}

		// Grab the version of the Shadow from the job ad
	InitShadowInfo();

		// Now that we know what version of the shadow we're talking
		// to, we can register information about ourselves with the
		// shadow in a method that it understands.  
	RegisterStarterInfo();

		// Now that we have the job ad, figure out what the owner
		// should be and initialize our priv_state code:
	if( ! InitUserPriv() ) {
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
			 (long)daemonCore->getpid() );
	if( mkdir(WorkingDir, 0777) < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS, "couldn't create dir %s: %s\n", 
				 WorkingDir, strerror(errno) );
		return false;
	}

#ifdef WIN32
		// On NT, we've got to manually set the acls, too.
	{
		perm dirperm;
		const char * nobody_login = get_user_nobody_loginname();
		ASSERT(nobody_login);
		dirperm.init(nobody_login);
		int ret_val = dirperm.set_acls( WorkingDir );
		if ( ret_val < 0 ) {
			dprintf(D_ALWAYS,"UNABLE TO SET PERMISSIONS ON EXECUTE DIRECTORY");
			return false;
		}
	}
#endif /* WIN32 */

	if( chdir(WorkingDir) < 0 ) {
		dprintf( D_FAILURE|D_ALWAYS, "couldn't move to %s: %s\n", WorkingDir,
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

	if( want_io_proxy || jobUniverse==CONDOR_UNIVERSE_JAVA ) {
		sprintf(io_proxy_config_file,"%s%cchirp.config",WorkingDir,DIR_DELIM_CHAR);
		if(!io_proxy.init(io_proxy_config_file)) {
			dprintf(D_FAILURE|D_ALWAYS,"StartJob: Couldn't initialize proxy.\n");
			return false;
		} else {
			dprintf(D_ALWAYS,"StartJob: Initialized IO Proxy.\n");
		}
	}
		
		// Return to our old priv state
	set_priv ( priv );

	if( DebugFlags & D_JOB ) {
		dprintf( D_JOB, "*** Job ClassAd ***\n" );  
		jobAd->dPrint( D_JOB );
        dprintf( D_JOB, "--- End of ClassAd ---\n" );
	}

		// Now that the scratch dir is setup, we can deal with
		// transfering files (if needed).
	if( BeginFileTransfer() ) {
			// We started a file transfer, so we'll just have to
			// return to DaemonCore and wait for our callback.
		return true;
	} else {
			// There were no files to transfer, so we can pretend the
			// transfer just finished and try to spawn the job now. 
		return TransferCompleted( NULL );
	}
}


bool
CStarter::BeginFileTransfer( void )
{
	char tmp[_POSIX_ARG_MAX];
	int change_iwd = true;
	wants_file_transfer = true;

		/* setup value for transfer_at_vacate and also determine if 
		   we should change our working directory */
	tmp[0] = '\0';
	jobAd->LookupString(ATTR_TRANSFER_FILES,tmp);
		// if set to "ALWAYS", then set transfer_at_vacate to true
	switch ( tmp[0] ) {
	case 'a':
	case 'A':
		transfer_at_vacate = true;
		break;
	case 'n':  /* for "Never" */
	case 'N':
		change_iwd = false;  // It's true otherwise...
		wants_file_transfer = false;
		break;
	}

		// for now, stash the executable in OrigJobName, and switch
		// the ad to say the executable is condor_exec
	OrigJobName[0] = '\0';
	jobAd->LookupString(ATTR_JOB_CMD,OrigJobName);

		// if requested in the jobad, transfer files over.  
	if ( change_iwd ) {
		// reset iwd of job to the starter directory
		sprintf( tmp, "%s=\"%s\"", ATTR_JOB_IWD, GetWorkingDir() );
		jobAd->InsertOrUpdate(tmp);		

		// Only rename the executable if it is transferred.
		int xferExec;
		if(jobAd->LookupBool(ATTR_TRANSFER_EXECUTABLE,xferExec)!=1) {
			xferExec = 1;
		} else {
			xferExec = 0;
		}

		if(xferExec) {
			sprintf(tmp,"%s=\"%s\"",ATTR_JOB_CMD,CONDOR_EXEC);
			dprintf(D_FULLDEBUG, "Changing the executable\n");
			jobAd->InsertOrUpdate(tmp);
		}

		filetrans = new FileTransfer();
		ASSERT( filetrans->Init(jobAd, false, PRIV_USER) );
		filetrans->RegisterCallback(
				  (FileTransferHandler)&CStarter::TransferCompleted,this);
		if( ! filetrans->DownloadFiles(false) ) { // do not block
				// Error starting the non-blocking file transfer.  For
				// now, consider this a fatal error
			EXCEPT( "Could not initiate file transfer" );
		}
		return true;
	}
		/* no file transfer desired, thus the file transfer is "done".
		   We assume that transfer_files == Never means that we want 
		   to live in the submit directory, so we DON'T change the 
		   ATTR_JOB_CMD or the ATTR_JOB_IWD.  This is important 
		   to MPI!  -MEY 12-8-1999  */
	return false;
}


int
CStarter::TransferCompleted( FileTransfer *ftrans )
{
		// Now that we've got all our files, we can figure out what
		// kind of job we're starting up, instantiate the appropriate
		// userproc class, and actually start the job.

		// Make certain the file transfer succeeded.  
		// Until "multi-starter" has meaning, it's ok to EXCEPT here,
		// since there's nothing else for us to do.
	if ( ftrans &&  !((ftrans->GetInfo()).success) ) {
		EXCEPT( "Failed to transfer files" );
	}

	dprintf( D_ALWAYS, "Starting a %s universe job.\n",
			 CondorUniverseName(jobUniverse) );

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
		default:
			dprintf( D_ALWAYS, "Starter doesn't support universe %d (%s)\n",
					 jobUniverse, CondorUniverseName(jobUniverse) ); 
			return FALSE;
	} /* switch */

	if (job->StartJob()) {
		JobList.Append(job);

			// Start a timer to update the shadow
		startUpdateTimer();
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
CStarter::startUpdateTimer( void )
{
	if( shadowupdate_tid >= 0 ) {
			// already registered the timer...
		return;
	}

	char* tmp = param( "STARTER_UPDATE_INTERVAL" );
	int update_interval = 0;
		// years of careful study show: 20 minutes... :)
	int def_update_interval = (20*60);
	int initial_interval = 8;
	if( tmp ) {
		update_interval = atoi( tmp );
		if( ! update_interval ) {
			dprintf( D_ALWAYS, "Invalid STARTER_UPDATE_INTERVAL: "
					 "\"%s\", using default value (%d) instead\n",
					 tmp, def_update_interval );
		}
		free( tmp );
	}
	if( ! update_interval ) {
		update_interval = def_update_interval;
	}
	if( update_interval < initial_interval ) {
		initial_interval = update_interval;
	}
	shadowupdate_tid = daemonCore->
		Register_Timer(initial_interval, update_interval,
					   (TimerHandlercpp)&CStarter::PeriodicShadowUpdate,
					   "CStarter::PeriodicShadowUpdate", this);
	if( shadowupdate_tid < 0 ) {
		EXCEPT( "Can't register DC Timer!" );
	}
}

	
void
CStarter::addToTransferOutputFiles( const char* filename )
{
	if( ! filetrans ) {
		return;
	}
	filetrans->addOutputFile( filename );
}


bool
CStarter::InitUserPriv( void )
{

#ifndef WIN32
	// Unix

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
			bool shadow_is_old = true;
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
				if( ShadowVersion && 
					ShadowVersion->built_since_version(6,3,3) ) {
						shadow_is_old = false;
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
						 "passwd file, SOFT_UID_DOMAIN is True, but the "
						 "condor_shadow on the submitting host is too old "
						 "to support SOFT_UID_DOMAIN.  You must upgrade "
						 "Condor on the submitting host to at least "
						 "version 6.3.3.\n", owner ); 
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
						 "passwd file, SOFT_UID_DOMAIN is True, but the "
						 "condor_shadow on the submitting host cannot "
						 "support SOFT_UID_DOMAIN.  You must upgrade "
						 "Condor on the submitting host to at least "
						 "version 6.3.3.\n", owner );
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
		dprintf( D_FULLDEBUG, "Submit host is in different UidDomain\n" ); 
		if( ! init_user_ids("nobody") ) { 
			dprintf( D_ALWAYS, "ERROR: Could not initialize user_priv "
					 "as \"nobody\"\n" );
			free( owner );
			return false;
		} else {
			dprintf( D_FULLDEBUG, "Initialized user_priv as \"nobody\"\n" );
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

	// suspend any filetransfer activity
	if ( filetrans ) {
		filetrans->Suspend();
	}
	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		job->Suspend();
	}
		// Now that everything is suspended, we want to send another
		// update to the shadow to let it know the job state.  We want
		// to confirm the update gets there on this important state
		// change, to pass in "true" to UpdateShadow() for that.
	UpdateShadow( true );
	return 0;
}

int
CStarter::Continue(int)
{
	dprintf(D_ALWAYS, "Continuing all jobs.\n");

	// resume any filetransfer activity
	if ( filetrans ) {
		filetrans->Continue();
	}

	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		job->Continue();
	}
		// Now that everything is running again, we want to send
		// another update to the shadow to let it know the job state.
		// We want to confirm the update gets there on this important
		// state change, to pass in "true" to UpdateShadow() for that.
	UpdateShadow( true );

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
			// No more jobs, we can stop updating the shadow
		if( shadowupdate_tid >= 0 ) {
			daemonCore->Cancel_Timer(shadowupdate_tid);
			shadowupdate_tid = -1;
		}

			// transfer output files back if requested job really
			// finished.  may as well do this in the foreground,
			// since we do not want to be interrupted by anything
			// short of a hardkill. 
		if( filetrans && 
			((requested_exit == false) || transfer_at_vacate) ) {
				// The user job may have created files only readable
				// by the user, so set_user_priv here.
				// true if job exited on its own
			bool final_transfer = (requested_exit == false);	
			priv_state saved_priv = set_user_priv();
				// this will block
			ASSERT( filetrans->UploadFiles(true, final_transfer) );	

			set_priv(saved_priv);
		}

			// Now that we're done transfering files and doing all our
			// cleanup, we can finally go through the CleanedUpJobList
			// and call JobExit() on all the procs in there.
		CleanedUpJobList.Rewind();
		while( (job = CleanedUpJobList.Next()) != NULL) {
			job->JobExit();
			CleanedUpJobList.DeleteCurrent();
			delete job;
		}
	}

	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		DC_Exit(0);
	}
	return 0;
}


bool
CStarter::SameUidDomain( void ) 
{
	char* job_uid_domain = NULL;
	bool same_domain = false;

	ASSERT( UIDDomain );
	ASSERT( shadow->name() );

	if( jobAd->LookupString( ATTR_UID_DOMAIN, &job_uid_domain ) != 1 ) {
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
			 UIDDomain );

	if( strcmp(job_uid_domain, UIDDomain) == MATCH ) {
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
	if( host_in_domain(shadow->name(), UIDDomain) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: the submitting host claims to be in our "
			 "UidDomain (%s), yet its hostname (%s) does not match\n",
			 UIDDomain, shadow->name() );
	return false;
}


bool
CStarter::PublishUpdateAd( ClassAd* ad )
{
	unsigned int execsz = 0;
	char buf[200];

	// if there is a filetrans object, then let's send the current
	// size of the starter execute directory back to the shadow.  this
	// way the ATTR_DISK_USAGE will be updated, and we won't end
	// up on a machine without enough local disk space.
	if ( filetrans ) {
		Directory starter_dir( GetWorkingDir(), PRIV_USER );
		execsz = starter_dir.GetDirectorySize();
		sprintf( buf, "%s=%u", ATTR_DISK_USAGE, (execsz+1023)/1024 ); 
		ad->InsertOrUpdate( buf );
	}
	return true;
}


/* 
   We can't just have our periodic timer call UpdateShadow() directly,
   since it passes in arguments that screw up the default bool that
   determines if we want TCP or UDP for the update.  So, the periodic
   updates call this function instead, which forces the UDP version.
*/
int
CStarter::PeriodicShadowUpdate( void )
{
	if( UpdateShadow(false) ) {
		return TRUE;
	}
	return FALSE;
}


bool
CStarter::UpdateShadow( bool insure_update )
{
	ClassAd ad;

	dprintf( D_FULLDEBUG, "Entering CStarter::UpdateShadow()\n" );

		// First, get everything out of the CStarter class.
	PublishUpdateAd( &ad );

		// Now, iterate through all our UserProcs and have those
		// publish, as well.  This method is virtual, so we'll get all
		// the goodies from derived classes, as well.
	bool found_one = false;
	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		if( job->PublishUpdateAd(&ad) ) {
			found_one = true;
		}
	}

	if( ! found_one ) {
		dprintf( D_FULLDEBUG, "CStarter::UpdateShadow(): "
				 "Didn't find any info to update!\n" );
		return false;
	}

		// Try to send it to the shadow
	if( shadow->updateJobInfo(&ad, insure_update) ) {
		dprintf( D_FULLDEBUG, "Leaving CStarter::UpdateShadow(): success\n" );
		return true;
	}
	dprintf( D_FULLDEBUG, "CStarter::UpdateShadow(): "
			 "failed to send update\n" );
	return false;
}
