/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
#include "condor_version.h"

#include "starter.h"
#include "jic_shadow.h"

#include "NTsenders.h"
#include "syscall_numbers.h"
#include "my_hostname.h"
#include "internet.h"
#include "basename.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "directory.h"
#include "nullfile.h"

extern CStarter *Starter;
ReliSock *syscall_sock = NULL;


JICShadow::JICShadow( char* shadow_sinful ) : JobInfoCommunicator()
{
	if( ! shadow_sinful ) {
		EXCEPT( "Trying to instantiate JICShadow with no shadow address!" );
	}
	shadow_addr = strdup( shadow_sinful );

	shadow = NULL;
	shadow_version = NULL;
	filetrans = NULL;
	shadowupdate_tid = -1;
	
	uid_domain = NULL;
	fs_domain = NULL;

	transfer_at_vacate = false;
	wants_file_transfer = false;

		// now we need to try to inherit the syscall sock from the startd
	Stream **socks = daemonCore->GetInheritedSocks();
	if (socks[0] == NULL || 
		socks[1] != NULL || 
		socks[0]->type() != Stream::reli_sock) 
	{
		dprintf(D_ALWAYS, "Failed to inherit remote system call socket.\n");
		DC_Exit(1);
	}
	syscall_sock = (ReliSock *)socks[0];
		/* Set a timeout on remote system calls.  This is needed in
		   case the user job exits in the middle of a remote system
		   call, leaving the shadow blocked.  -Jim B. */
	syscall_sock->timeout(300);
}


JICShadow::~JICShadow()
{
	if( shadowupdate_tid != -1 && daemonCore ) {
		daemonCore->Cancel_Timer(shadowupdate_tid);
		shadowupdate_tid = -1;
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
	if( shadow_addr ) {
		free( shadow_addr );
	}
	if( uid_domain ) {
		free( uid_domain );
	}
	if( fs_domain ) {
		free( fs_domain );
	}
}


bool
JICShadow::init( void ) 
{ 
		// First, get a copy of the job classad by doing an RSC to the
		// shadow.  This is totally independent of the shadow version,
		// etc, and is the first step to everything else. 
	if( ! getJobAdFromShadow() ) {
		dprintf( D_ALWAYS|D_FAILURE,
				 "Failed to get job ad from shadow!\n" );
		return false;
	}

		// now that we have the job ad, see if we should go into an
		// infinite loop, waiting for someone to attach w/ the
		// debugger.
	checkForStarterDebugging();

		// Grab all the interesting stuff out of the ClassAd we need
		// to know about the job itself, like are we doing file
		// transfer, what should the std files be called, etc.
	if( ! initJobInfo() ) { 
		dprintf( D_ALWAYS|D_FAILURE,
				 "Failed to initialize job info from ClassAd!\n" );
		return false;
	}

		// Next, instantiate our DCShadow object
	if( shadow ) {
		delete shadow;
	}
	shadow = new DCShadow( shadow_addr );
	ASSERT( shadow );

		// Now, initalize our version information about the shadow
	initShadowInfo();

	dprintf( D_ALWAYS, "Starter communicating with condor_shadow %s\n",
			 shadow->addr() );
	dprintf( D_ALWAYS, "Submitting machine is \"%s\"\n", shadow->name());

		// Now that we know what version of the shadow we're talking
		// to, we can register information about ourselves with the
		// shadow in a method that it understands.
	registerStarterInfo();
	
		// Now that we have the job ad, figure out what the owner
		// should be and initialize our priv_state code:
	if( ! initUserPriv() ) {
		dprintf( D_ALWAYS, "ERROR: Failed to determine what user "
				 "to run this job as, aborting\n" );
		return false;
	}

		// Now that we have the user_priv, we can make the temp
		// execute dir
	if( ! Starter->createTempExecuteDir() ) { 
		return false;
	}

		// If the user wants it, initialize our io proxy
	initIOProxy();

		// Now that the user priv is setup and the temp execute dir
		// exists, we can initialize the LocalUserLog.  if the job
		// defines StarterUserLog, we'll write the events.  if not,
		// all attemps to log events will just be no-ops.
	if( ! u_log->initFromJobAd(job_ad) ) {
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
}


void
JICShadow::setupJobEnvironment( void )
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
	transferCompleted( NULL );
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
JICShadow::allJobsSpawned( void )
{
		// at this point, all we care about now that the jobs have
		// been spawned is 
	startUpdateTimer();
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
	updateShadow( &update_ad, true );

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
	updateShadow( &update_ad, true );
}


void
JICShadow::allJobsDone( void )
{
		// now that all the jobs are gone, we can stop our periodic
		// shadow updates.
	if( shadowupdate_tid >= 0 ) {
		daemonCore->Cancel_Timer( shadowupdate_tid );
		shadowupdate_tid = -1;
	}

		// transfer output files back if requested job really
		// finished.  may as well do this in the foreground,
		// since we do not want to be interrupted by anything
		// short of a hardkill. 
	if( filetrans && ((requested_exit == false) || transfer_at_vacate) ) {
			// true if job exited on its own
		bool final_transfer = (requested_exit == false);	

			// The user job may have created files only readable
			// by the user, so set_user_priv here.
		priv_state saved_priv = set_user_priv();

			// this will block
		ASSERT( filetrans->UploadFiles(true, final_transfer) );	

		set_priv(saved_priv);
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
JICShadow::notifyJobPreSpawn( void )
{
			// Notify the shadow we're about to exec.
	REMOTE_CONDOR_begin_execution();

		// let the LocalUserLog know so it can log if necessary.  it
		// doesn't use the ClassAd for this event at all, so it's not
		// worth the trouble of creating one and publishing anything
		// into it.
	u_log->logExecute( NULL );
}


bool
JICShadow::notifyJobExit( int exit_status, int reason, UserProc*
						  user_proc )
{
	bool job_exit_wants_ad = true;

		// protocol changed w/ v6.3.0 so the Update Ad is sent
		// with the final REMOTE_CONDOR_job_exit system call.
		// to keep things backwards compatible, do not send the 
		// ad with this system call if the shadow is older.

		// However, b/c the shadow didn't start sending it's version
		// to the starter until 6.3.2, we confuse 6.3.0 and 6.3.1
		// shadows with 6.2.X shadows that don't support the new
		// protocol.  Luckily, we never released 6.3.0 or 6.3.1 for
		// windoze, and we never released any part of the new
		// shadow/starter for Unix until 6.3.0.  So, we only have to
		// do this compatibility check on windoze, and we don't have
		// to worry about it not being able to tell the difference
		// between 6.2.X, 6.3.0, and 6.3.1, since we never released
		// 6.3.0 or 6.3.1. :) Derek <wright@cs.wisc.edu> 1/25/02

#ifdef WIN32		
	job_exit_wants_ad = false;
	CondorVersionInfo * ver = Starter->GetShadowVersion();
	if( ver && ver->built_since_version(6,3,0) ) {
		job_exit_wants_ad = true;	// new shadow; send ad
	}
#endif		

	ClassAd ad;
	ClassAd *ad_to_send;

		// We want the update ad anyway, in case we want it for the
		// LocalUserLog
	user_proc->PublishUpdateAd( &ad );

		// depending on the exit reason, we want a different event. 
	u_log->logJobExit( &ad, reason );

	if ( job_exit_wants_ad ) {
		ad_to_send = &ad;
	} else {
		dprintf( D_FULLDEBUG,
				 "Shadow is pre-v6.3.0 - not sending final update ad\n" ); 
		ad_to_send = NULL;
	}
			
	if( REMOTE_CONDOR_job_exit(exit_status, reason, ad_to_send) < 0 ) {    
		dprintf( D_ALWAYS, 
				 "Failed to send job exit status to Shadow.\n" );
		return false;
	}
	return true;
}


bool
JICShadow::notifyStarterError( const char* err_msg, bool critical )
{
	u_log->logStarterError( err_msg, critical );

	if( REMOTE_CONDOR_ulog_printf(err_msg) < 0 ) {
		dprintf( D_ALWAYS, 
				 "Failed to send starter error string to Shadow.\n" );
		return false;
	}
	return true;
}


bool
JICShadow::registerStarterInfo( void )
{
	int rval;

	if( ! shadow ) {
		EXCEPT( "registerStarterInfo called with NULL DCShadow object" );
	}

		// CRUFT!
		// If the shadow is older than 6.3.3, we need to use the
		// CONDOR_register_machine_info method, which sends a bunch of
		// strings over the wire.  If we're 6.3.3 or later, we can use
		// CONDOR_register_starter_info, which just sends a ClassAd
		// with all the relevent info.
	if( shadow_version && shadow_version->built_since_version(6,3,3) ) {
		ClassAd* starter_info = new ClassAd;
		char *tmp = NULL;
		char* tmp_val = NULL;
		int size;

		size = strlen(uid_domain) + strlen(ATTR_UID_DOMAIN) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_UID_DOMAIN, uid_domain );
		starter_info->Insert( tmp );
		free( tmp );

		size = strlen(fs_domain) + strlen(ATTR_FILE_SYSTEM_DOMAIN) + 5;
		tmp = (char*) malloc( size * sizeof(char) );
		sprintf( tmp, "%s=\"%s\"", ATTR_FILE_SYSTEM_DOMAIN, fs_domain ); 
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
		rval = REMOTE_CONDOR_register_machine_info( uid_domain,
			     fs_domain, daemonCore->InfoCommandSinfulString(), 
				 mfhn, 0 );
		delete [] mfhn;
	}
	if( rval < 0 ) {
		return false;
	}
	return true;
}


void
JICShadow::addToOutputFiles( const char* filename )
{
	if( ! filetrans ) {
		return;
	}
	filetrans->addOutputFile( filename );
}


bool
JICShadow::initUserPriv( void )
{

#ifndef WIN32
	// Unix

		// Before we go through any trouble, see if we even need
		// ATTR_OWNER to initialize user_priv.  If not, go ahead and
		// initialize it as appropriate.  
	if( initUserPrivNoOwner() ) {
		return true;
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
	
	char* owner = NULL;

	if( job_ad->LookupString( ATTR_OWNER, &owner ) != 1 ) {
		dprintf( D_ALWAYS, "ERROR: %s not found in JobAd.  Aborting.\n", 
				 ATTR_OWNER );
		return false;
	}

	if( sameUidDomain() ) {
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
				if( shadow_version && 
					shadow_version->built_since_version(6,3,3) ) {
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
			// passing NULL for the domain is ok here since this is
			// UNIX code
		if( ! init_user_ids("nobody", NULL) ) { 
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
	user_priv_is_initialized = true;
	return true;

#else

		// Windoze
	return initUserPrivWindows();

#endif
}


bool
JICShadow::initJobInfo( void ) 
{
	if( ! job_ad ) {
		EXCEPT( "JICShadow::initJobInfo() called with NULL job ad!" );
	}

		// stash the executable name in orig_job_name
	if( ! job_ad->LookupString(ATTR_JOB_CMD, &orig_job_name) ) {
		dprintf( D_ALWAYS, "Error in JICShadow::initJobInfo(): "
				 "Can't find %s in job ad\n", ATTR_JOB_CMD );
		return false;
	}

	if( job_ad->LookupInteger(ATTR_JOB_UNIVERSE, job_universe) < 1 ) {
		dprintf( D_ALWAYS, 
				 "Job doesn't specify universe, assuming VANILLA\n" ); 
		job_universe = CONDOR_UNIVERSE_VANILLA;
	}

		// figure out if we're going to be using file transfer, and
		// therefore, if we're going to want to change the job's iwd
		// and so we know what filenames to use for stdin, stdout,
		// etc. 
	char tmp[_POSIX_ARG_MAX];

	change_iwd = true;
	wants_file_transfer = true;

		/* setup value for transfer_at_vacate and also determine if 
		   we should change our working directory */
	tmp[0] = '\0';
	job_ad->LookupString( ATTR_TRANSFER_FILES, tmp );
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

		/* We assume that transfer_files == Never means that we want
		   to live in the submit directory, so we DON'T change the
		   ATTR_JOB_CMD or the ATTR_JOB_IWD.  This is important to
		   MPI!  -MEY 12-8-1999 */
	if( change_iwd ) {
			// reset iwd of job to the starter directory
		sprintf( tmp, "%s=\"%s\"", ATTR_JOB_IWD, Starter->GetWorkingDir() );
		job_ad->InsertOrUpdate( tmp );		
		job_iwd = strdup( Starter->GetWorkingDir() );
	} else {
		job_ad->LookupString( ATTR_JOB_IWD, &job_iwd );
	}
	if( ! job_iwd ) {
		dprintf( D_ALWAYS, "Can't find job's IWD, aborting\n" );
		return false;
	}

		// now that we know about file transfer and the real iwd we'll
		// be using, we can initialize the std files... 
	job_input_name = getJobStdFile( ATTR_JOB_INPUT );
	job_output_name = getJobStdFile( ATTR_JOB_OUTPUT );
	job_error_name = getJobStdFile( ATTR_JOB_ERROR );

	return true;
}


char* 
JICShadow::getJobStdFile( const char* attr_name )
{
	char* tmp = NULL;
	char* filename1 = NULL;
	char filename[_POSIX_PATH_MAX];
	filename[0] = '\0';

	if( job_ad->LookupString(attr_name, &tmp) ) {
		if ( !nullFile(tmp) ) {
			if( wants_file_transfer ) {
				filename1 = basename( tmp );
			} else {
				filename1 = tmp;
			}
            if( ! fullpath(filename1) ) {	// prepend full path
                sprintf( filename, "%s%c", job_iwd, DIR_DELIM_CHAR );
            } else {
                filename[0] = '\0';
            }
			strcat ( filename, filename1 );
		}
		free( tmp );
	}
	if( filename[0] ) { 
		return strdup( filename );
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

	if( strcmp(job_uid_domain, uid_domain) == MATCH ) {
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
	if( host_in_domain(shadow->name(), uid_domain) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: the submitting host claims to be in our "
			 "UidDomain (%s), yet its hostname (%s) does not match\n",
			 uid_domain, shadow->name() );
	return false;
}



bool
JICShadow::publishUpdateAd( ClassAd* ad )
{
	unsigned int execsz = 0;
	char buf[200];

	// if there is a filetrans object, then let's send the current
	// size of the starter execute directory back to the shadow.  this
	// way the ATTR_DISK_USAGE will be updated, and we won't end
	// up on a machine without enough local disk space.
	if ( filetrans ) {
		Directory starter_dir( Starter->GetWorkingDir(), PRIV_USER );
		execsz = starter_dir.GetDirectorySize();
		sprintf( buf, "%s=%u", ATTR_DISK_USAGE, (execsz+1023)/1024 ); 
		ad->InsertOrUpdate( buf );
	}

		// Now, get our Starter object to publish, as well.  This will
		// walk through all the UserProcs and have those publish, as
		// well.  It returns true if there was anything published,
		// false if not.
	return Starter->publishUpdateAd( ad );
}


void
JICShadow::startUpdateTimer( void )
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
					   (TimerHandlercpp)&JICShadow::periodicShadowUpdate,
					   "JICShadow::periodicShadowUpdate", this);
	if( shadowupdate_tid < 0 ) {
		EXCEPT( "Can't register DC Timer!" );
	}
}




/* 
   We can't just have our periodic timer call updateShadow() directly,
   since it passes in arguments that screw up the default bool that
   determines if we want TCP or UDP for the update.  So, the periodic
   updates call this function instead, which forces the UDP version.
*/
int
JICShadow::periodicShadowUpdate( void )
{
	if( updateShadow(NULL, false) ) {
		return TRUE;
	}
	return FALSE;
}


bool
JICShadow::updateShadow( ClassAd* update_ad, bool insure_update )
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

		// Try to send it to the shadow
	if( shadow->updateJobInfo(ad, insure_update) ) {
		dprintf( D_FULLDEBUG, "Leaving JICShadow::updateShadow(): success\n" );
		return true;
	}
	dprintf( D_FULLDEBUG, "JICShadow::updateShadow(): "
			 "failed to send update\n" );
	return false;
}


bool
JICShadow::beginFileTransfer( void )
{
	char tmp[_POSIX_PATH_MAX];

		// if requested in the jobad, transfer files over.  
	if( wants_file_transfer ) {
		// Only rename the executable if it is transferred.
		int xferExec;
		if( job_ad->LookupBool(ATTR_TRANSFER_EXECUTABLE,xferExec)!=1 ) {
			xferExec = 1;
		} else {
			xferExec = 0;
		}

		if( xferExec ) {
			sprintf( tmp, "%s=\"%s\"", ATTR_JOB_CMD,CONDOR_EXEC );
			dprintf( D_FULLDEBUG, "Changing the executable name\n" );
			job_ad->InsertOrUpdate( tmp );
		}

		filetrans = new FileTransfer();
		ASSERT( filetrans->Init(job_ad, false, PRIV_USER) );
		filetrans->RegisterCallback(
				  (FileTransferHandler)&JICShadow::transferCompleted,this );

		if( ! filetrans->DownloadFiles(false) ) { // do not block
				// Error starting the non-blocking file transfer.  For
				// now, consider this a fatal error
			EXCEPT( "Could not initiate file transfer" );
		}
		return true;
	}
		// no transfer wanted or started, so return false
	return false;
}


int
JICShadow::transferCompleted( FileTransfer *ftrans )
{
		// Make certain the file transfer succeeded.  
		// Until "multi-starter" has meaning, it's ok to EXCEPT here,
		// since there's nothing else for us to do.
	if ( ftrans &&  !((ftrans->GetInfo()).success) ) {
		EXCEPT( "Failed to transfer files" );
	}

		// Now that we're done transfering files, we can let the
		// Starter object know the execution environment is ready. 
	Starter->jobEnvironmentReady();

	return TRUE;
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
		dprintf( D_FAILURE|D_ALWAYS, 
				 "Failed to get job info from Shadow!\n" );
		return false;
	}
	return true;
}


void
JICShadow::initShadowInfo( void )
{
	ASSERT( job_ad );
	ASSERT( shadow );

	if( ! shadow->initFromClassAd(job_ad) ) { 
		dprintf( D_ALWAYS, 
				 "Failed to initialize shadow info from job ClassAd!\n" );
		return;
	}

	if( shadow_version ) {
		delete shadow_version;
		shadow_version = NULL;
	}
	char* tmp = shadow->version();
	if( tmp ) {
		dprintf( D_FULLDEBUG, "Version of Shadow is %s\n", tmp );
		shadow_version = new CondorVersionInfo( tmp, "SHADOW" );
	} else {
		dprintf( D_FULLDEBUG, "Version of Shadow unknown (pre v6.3.3)\n" ); 
	}
}



bool
JICShadow::initIOProxy( void )
{
	int want_io_proxy = 0;
	char io_proxy_config_file[_POSIX_PATH_MAX];

	if( job_ad->LookupBool( ATTR_WANT_IO_PROXY, want_io_proxy ) < 1 ) {
		dprintf( D_FULLDEBUG, "JICShadow::initIOProxy(): "
				 "Job does not define %s\n", ATTR_WANT_IO_PROXY );
		want_io_proxy = 0;
	} else {
		dprintf( D_ALWAYS, "Job has %s=%s\n", ATTR_WANT_IO_PROXY,
				 want_io_proxy ? "true" : "false" );
	}

	if( want_io_proxy || job_universe==CONDOR_UNIVERSE_JAVA ) {
		sprintf( io_proxy_config_file, "%s%cchirp.config",
				 Starter->GetWorkingDir(), DIR_DELIM_CHAR );
		if( !io_proxy.init(io_proxy_config_file) ) {
			dprintf( D_FAILURE|D_ALWAYS, 
					 "Couldn't initialize IO Proxy.\n" );
			return false;
		}
		dprintf( D_ALWAYS, "Initialized IO Proxy.\n" );
		return true;
	}
	return false;
}
