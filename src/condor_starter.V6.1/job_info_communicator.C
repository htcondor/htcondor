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
#include "condor_attributes.h"
#include "job_info_communicator.h"


JobInfoCommunicator::JobInfoCommunicator()
{
	job_ad = NULL;
	job_universe = CONDOR_UNIVERSE_VANILLA;
	job_cluster = -1;
	job_proc = -1;
	job_subproc = -1;
	u_log = new LocalUserLog( this );
	orig_job_name = NULL;
	job_input_name = NULL;
	job_output_name = NULL;
	job_error_name = NULL;
	job_iwd = NULL;
	requested_exit = false;
	change_iwd = false;
	user_priv_is_initialized = false;
}


JobInfoCommunicator::~JobInfoCommunicator()
{
	if( job_ad ) {
		delete job_ad;
	}
	if( u_log ) {
		delete u_log;
	}
	if( orig_job_name ) {
		free( orig_job_name );
	}
	if( job_input_name ) {
		free( job_input_name);
	}
	if( job_output_name ) {
		free( job_output_name);
	}
	if( job_error_name ) {
		free( job_error_name);
	}
	if( job_iwd ) {
		free( job_iwd);
	}

}


const char*
JobInfoCommunicator::jobInputFilename( void )
{
	return (const char*) job_input_name;
}


const char*
JobInfoCommunicator::jobOutputFilename( void )
{
	return (const char*) job_output_name;
}


const char*
JobInfoCommunicator::jobErrorFilename( void )
{
	return (const char*) job_error_name;
}


const char*
JobInfoCommunicator::jobIWD( void )
{
	return (const char*) job_iwd;
}


const char*
JobInfoCommunicator::origJobName( void )
{
	return (const char*) orig_job_name;
}


ClassAd*
JobInfoCommunicator::jobClassAd( void )
{
	return job_ad;
}


int
JobInfoCommunicator::jobUniverse( void )
{
	return job_universe;
}


int
JobInfoCommunicator::jobCluster( void )
{
	return job_cluster;
}


int
JobInfoCommunicator::jobProc( void )
{
	return job_proc;
}


int
JobInfoCommunicator::jobSubproc( void )
{
	return job_subproc;
}


void
JobInfoCommunicator::gotShutdownFast( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
}


void
JobInfoCommunicator::gotShutdownGraceful( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
}


bool
JobInfoCommunicator::userPrivInitialized( void )
{
	return user_priv_is_initialized;
}


bool
JobInfoCommunicator::initUserPrivNoOwner( void ) 
{
		// first, bale out if we really need ATTR_OWNER...
#ifdef WIN32
	return false;
#else
		// if we're root, we need ATTR_OWNER...
	if( getuid() == 0 ) {
		return false;
	}
#endif
		// otherwise, we can't switch privs anyway, so consider
		// ourselves done. :) 
	dprintf( D_FULLDEBUG, 
			 "Starter running as '%s', no uid switching possible\n",
			 get_real_username() );
	user_priv_is_initialized = true;
	return true;
}


bool
JobInfoCommunicator::initUserPrivWindows( void )
{
	// Win32
	// taken origionally from OsProc::StartJob.  Here we create the
	// user and initialize user_priv.
	// we only support running jobs as user nobody for the first pass

	// just init a new nobody user; dynuser handles the rest.
	// the "." means Local Machine to LogonUser
	if( ! init_user_ids("nobody", ".") ) {
		dprintf( D_ALWAYS, "ERROR: Could not initialize user_priv "
				 "as \"nobody\"\n" );
		return false;
	}
	user_priv_is_initialized = true;
	return true;
}



void
JobInfoCommunicator::checkForStarterDebugging( void )
{
	if( ! job_ad ) {
		EXCEPT( "checkForStarterDebugging() called with no job ad!" );
	}

		// For debugging, see if there's a special attribute in the
		// job ad that sends us into an infinite loop, waiting for
		// someone to attach with a debugger
	int starter_should_wait = 0;
	job_ad->LookupInteger( ATTR_STARTER_WAIT_FOR_DEBUG,
						  starter_should_wait );
	if( starter_should_wait ) {
		dprintf( D_ALWAYS, "Job requested starter should wait for "
				 "debugger with %s=%d, going into infinite loop\n",
				 ATTR_STARTER_WAIT_FOR_DEBUG, starter_should_wait );
		while( starter_should_wait );
	}

		// Also, if the starter has D_JOB turned on, we want to dump
		// out the job ad to the log file...
	if( DebugFlags & D_JOB ) {
		dprintf( D_JOB, "*** Job ClassAd ***\n" );  
		job_ad->dPrint( D_JOB );
        dprintf( D_JOB, "--- End of ClassAd ---\n" );
	}
}
