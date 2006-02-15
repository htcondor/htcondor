/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_attributes.h"
#include "job_info_communicator.h"
#include "starter.h"
#include "condor_config.h"
#include "domain_tools.h"
#include "basename.h"


extern CStarter *Starter;


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
	job_remote_iwd = NULL;
	job_output_ad_file = NULL;
	job_output_ad_is_stdout = false;
	requested_exit = false;
	had_remove = false;
	had_hold = false;
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
	if( job_remote_iwd ) {
		free( job_remote_iwd );
	}
	if( job_output_ad_file ) {
		free( job_output_ad_file );
	}
}


void
JobInfoCommunicator::setStdin( const char* path )
{
	if( job_input_name ) {
		free( job_input_name );
	}
	job_input_name = strdup( path );
}


void
JobInfoCommunicator::setStdout( const char* path )
{
	if( job_output_name ) {
		free( job_output_name );
	}
	job_output_name = strdup( path );
}


void
JobInfoCommunicator::setStderr( const char* path )
{
	if( job_error_name ) {
		free( job_error_name );
	}
	job_error_name = strdup( path );
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

bool
JobInfoCommunicator::streamInput()
{
	return false;
}

bool
JobInfoCommunicator::streamOutput()
{
	return false;
}

bool
JobInfoCommunicator::streamError()
{
	return false;
}

bool
JobInfoCommunicator::streamStdFile( const char *which )
{
	if(!strcmp(which,ATTR_JOB_INPUT)) {
		return streamInput();
	} else if(!strcmp(which,ATTR_JOB_OUTPUT)) {
		return streamOutput();
	} else if(!strcmp(which,ATTR_JOB_ERROR)) {
		return streamError();
	} else {
		return false;
	}
}


const char*
JobInfoCommunicator::jobIWD( void )
{
	return (const char*) job_iwd;
}

const char*
JobInfoCommunicator::jobRemoteIWD( void )
{
	if(!job_remote_iwd) return jobIWD();
	return (const char*) job_remote_iwd;
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


void
JobInfoCommunicator::gotRemove( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
	had_remove = true;
}


void
JobInfoCommunicator::gotHold( void )
{
		// Set our flag so we know we were asked to vacate.
	requested_exit = true;
	had_hold = true;
}


void
JobInfoCommunicator::setOutputAdFile( const char* path )
{
	if( job_output_ad_file ) {
		free( job_output_ad_file );
	}
	job_output_ad_file = strdup( path );
}


bool
JobInfoCommunicator::writeOutputAdFile( ClassAd* ad )
{
	if( ! job_output_ad_file ) {
		return false;
	}

	FILE* fp;
	if( job_output_ad_is_stdout ) {
		dprintf( D_ALWAYS, "Will write job output ClassAd to STDOUT\n" );
		fp = stdout;
	} else {
		fp = fopen( job_output_ad_file, "a" );
		if( ! fp ) {
			dprintf( D_ALWAYS, "Failed to open job output ClassAd "
					 "\"%s\": %s (errno %d)\n", job_output_ad_file, 
					 strerror(errno), errno ); 
			return false;
		} else {
			dprintf( D_ALWAYS, "Writing job output ClassAd to \"%s\"\n", 
					 job_output_ad_file );

		}
	}
		// append a delimiter?
	ad->fPrint( fp );

	if( job_output_ad_is_stdout ) {
		fflush( fp );
	} else {
		fclose( fp );
	}
	return true;
}


// This has to be called after we know what the working directory is
// going to be, so we can make sure this is a full path...
void
JobInfoCommunicator::initOutputAdFile( void )
{
	if( ! job_output_ad_file ) {
		return;
	}
	if( job_output_ad_file[0] == '-' && job_output_ad_file[1] == '\0' ) {
		job_output_ad_is_stdout = true;
	} else if( ! fullpath(job_output_ad_file) ) {
		MyString path = Starter->GetWorkingDir();
		path += DIR_DELIM_CHAR;
		path += job_output_ad_file;
		free( job_output_ad_file );
		job_output_ad_file = strdup( path.GetCStr() );
	}
	dprintf( D_ALWAYS, "Will write job output ClassAd to \"%s\"\n",
			 job_output_ad_is_stdout ? "STDOUT" : job_output_ad_file );
}



bool
JobInfoCommunicator::userPrivInitialized( void )
{
	return user_priv_is_initialized;
}

bool
JobInfoCommunicator::usingFileTransfer( void )
{
	return false;
}

bool
JobInfoCommunicator::updateX509Proxy( ReliSock *  )
{
	return false;
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

#ifdef WIN32
#include "my_username.h"
bool
JobInfoCommunicator::initUserPrivWindows( void )
{
	// Win32
	// taken origionally from OsProc::StartJob.  Here we create the
	// user and initialize user_priv.
	
	// we support running the job as other users if the user
	// is specifed in the config file, and the account's password
	// is properly stored in our credential stash.

	char *name = NULL;
	char *domain = NULL;
	bool init_priv_succeeded = true;
	bool run_as_owner = param_boolean("STARTER_ALLOW_RUNAS_OWNER",false);

	if ( run_as_owner ) {
		if( ! job_ad ) {
			EXCEPT( "initUserPrivWindows() called with no job ad!" );
		}

		bool user_wants_runas_owner = false;
		job_ad->LookupBool(ATTR_JOB_RUNAS_OWNER,user_wants_runas_owner);
		if ( user_wants_runas_owner ) {
			job_ad->LookupString(ATTR_OWNER,&name);
			job_ad->LookupString(ATTR_NT_DOMAIN,&domain);
		}
	}

	if ( !name ) {
		char vm_user[255];
		int vm_num = Starter->getMyVMNumber();

		if ( vm_num < 1 ) {
			vm_num = 1;
		}
		sprintf(vm_user, "VM%d_USER", vm_num);
		char *run_jobs_as = param(vm_user);
		if (run_jobs_as) {		
			getDomainAndName(run_jobs_as, domain, name);
				/* 
				 * name and domain are now just pointers into run_jobs_as
				 * buffer.  copy these values into their own buffer so we
				 * deallocate below.
				 */
			if ( name ) {
				name = strdup(name);
			}
			if ( domain ) {
				domain = strdup(domain);
			}
			free(run_jobs_as);
		}
	}

	if ( name ) {
		
		if (!init_user_ids(name, domain)) {

			dprintf(D_ALWAYS, "Could not initialize user_priv as \"%s\\%s\".\n"
				"\tMake sure this account's password is securely stored "
				"with condor_store_cred.\n", domain, name );
			init_priv_succeeded = false;			
		} 

	} else if ( !can_switch_ids() ) {
		char *u = my_username();
		char *d = my_domainname();

		if ( !init_user_ids(u, d) ) {
			// shouldn't happen - we always can get our own token
			dprintf(D_ALWAYS, "Could not initialize user_priv with our own token!\n");
			init_priv_succeeded = false;
		}
		free(u);
		free(d);
	} else if( ! init_user_ids("nobody", ".") ) {
		
		// just init a new nobody user; dynuser handles the rest.
		// the "." means Local Machine to LogonUser
	
		dprintf( D_ALWAYS, "ERROR: Could not initialize user_priv "
				 "as \"nobody\"\n" );
		init_priv_succeeded = false;
	
	}

	if ( name ) free(name);
	if ( domain ) free(domain);

	user_priv_is_initialized = init_priv_succeeded;
	return init_priv_succeeded;
}
#endif // WIN32

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
