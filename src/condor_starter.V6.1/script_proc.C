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
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "user_proc.h"
#include "script_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "exit.h"
#include "condor_uid.h"


extern CStarter *Starter;


/* ScriptProc class implementation */

ScriptProc::ScriptProc( ClassAd* ad, const char* proc_name )
{
    dprintf ( D_FULLDEBUG, "In ScriptProc::ScriptProc()\n" );
	if( proc_name ) {
		name = strdup( proc_name );
	} else {
		EXCEPT( "Can't instantiate a ScriptProc without a name!" );
	}
	JobAd = ad;
	is_suspended = false;
	UserProc::initialize();
}


ScriptProc::~ScriptProc()
{
		// Nothing special yet...
}


int
ScriptProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in ScriptProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in ScriptProc::StartJob()!\n" );
		return 0;
	}

	MyString attr;

	attr = name;
	attr += ATTR_JOB_CMD;
	char* tmp = NULL;
	if( ! JobAd->LookupString( attr.GetCStr(), &tmp ) ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				 attr.GetCStr() );
		return 0;
	}

		// // // // // // 
		// executable
		// // // // // // 

		// TODO: make it smart in cases we're not the gridshell and/or
		// didn't transfer files so that we don't prepend the wrong
		// path to the binary, and don't try to chmod it.
	MyString exe_path = Starter->GetWorkingDir();
	exe_path += DIR_DELIM_CHAR;
	exe_path += tmp;
	free( tmp ); 
	tmp = NULL;

	if( Starter->isGridshell() ) {
			// if we're a gridshell, chmod() the binary, since globus
			// probably transfered it for us and left it with bad
			// permissions...
		priv_state old_priv = set_user_priv();
		int retval = chmod( exe_path.GetCStr(), 0755 );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf( D_ALWAYS, "Failed to chmod %s: %s (errno %d)\n", 
					 exe_path.GetCStr(), strerror(errno), errno );
			return 0;
		}
	} 


		// // // // // // 
		// Args
		// // // // // // 

	attr = name;
	attr += ATTR_JOB_ARGUMENTS;
	if( ! JobAd->LookupString(attr.GetCStr(), &tmp) ) {
		dprintf( D_FULLDEBUG, "%s not found in JobAd\n" );
	}

	MyString args;

		// First, put "condor_<name>script" at the front of Args,
		// since that will become argv[0] of what we exec(), either
		// the wrapper or the actual job.
	args = "condor_";
	args += name;
	args += "script ";

		// This variable is used to keep track of the position
		// of the arguments immediately following argv[0].
	int skip = args.Length();

		// If specified, append the real arguments now.
	if( tmp ) {
		args += tmp;
		free( tmp );
		tmp = NULL;
	}


		// // // // // // 
		// Environment 
		// // // // // // 

	attr = name;
	attr += ATTR_JOB_ENVIRONMENT;
	JobAd->LookupString( attr.GetCStr(), &tmp );
			// TODO do we want to use the regular ATTR_JOB_ENVIRONMENT
			// if there's nothing specific for this script?

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;
	if( tmp ) { 
		if( ! job_env.Merge(tmp) ) {
			dprintf( D_ALWAYS, "Invalid %s found in JobAd.  "
					 "Aborting ScriptProc::StartJob.\n", attr.GetCStr() );  
			return 0;
		}
			// Next, we can free the string we got back from
			// LookupString() so we don't leak any memory.
		free( tmp );
		tmp = NULL;
	}
		// Now, let the starter publish any env vars it wants to add
	Starter->PublishToEnv( &job_env );


		// TODO: Deal with port regulation stuff?

		// Grab the full environment back out of the Env object 
	char* env_str = job_env.getDelimitedString();
	dprintf(D_FULLDEBUG, "%sEnv = %s\n", name, env_str );


		// // // // // // 
		// Standard Files
		// // // // // // 

		// TODO???


		// // // // // // 
		// Misc + Exec
		// // // // // // 

		// TODO?
		// Starter->jic->notifyJobPreSpawn( name );

		// compute job's renice value by evaluating the machine's
		// JOB_RENICE_INCREMENT in the context of the job ad...
		// TODO?
	int nice_inc = 10;


		// in the below dprintfs, we want to skip past argv[0], which
		// is sometimes condor_exec, in the Args string. 
		// We rely on the "skip" variable defined above when
		// argv[0] was set according to the universe and job name.

	dprintf( D_ALWAYS, "About to exec %s script: %s %s\n", 
			 name, exe_path.GetCStr(), 
			 (skip < args.Length()) ? &(args[skip]) : "" );

	JobPid = daemonCore->Create_Process(exe_path.GetCStr(), 
				args.GetCStr(), PRIV_USER_FINAL, 1, FALSE, env_str,
				Starter->jic->jobIWD(), TRUE, NULL, NULL, nice_inc,
				DCJOBOPT_NO_ENV_INHERIT );

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = NULL;
	if( JobPid == FALSE && errno ) {
		create_process_error = strerror( errno );
	}

		// Free up memory we allocated so we don't leak.
	delete [] env_str;

	if( JobPid == FALSE ) {
		JobPid = -1;

		if( create_process_error ) {
			MyString err_msg = "Failed to execute '";
			err_msg += exe_path.GetCStr();
			err_msg += ' ';
			err_msg += args.GetCStr();
			err_msg += "': ";
			err_msg += create_process_error;
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}

		EXCEPT( "Create_Process(%s,%s, ...) failed",
				exe_path.GetCStr(), args.GetCStr() );
		return 0;
	}

	dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid );

	job_start_time.getTime();

	return 1;
}



int
ScriptProc::JobCleanup( int pid, int status )
{
	dprintf( D_FULLDEBUG, "Inside ScriptProc::JobCleanup()\n" );

	if( JobPid != pid ) {		
		return 0;
	}

	job_exit_time.getTime();

		// save the exit status for future use.
	exit_status = status;

	return 1;
}


bool
ScriptProc::JobExit( void )
{
	dprintf( D_FULLDEBUG, "Inside ScriptProc::JobExit()\n" );
	return true;
}


bool
ScriptProc::PublishUpdateAd( ClassAd* ad ) 
{
	dprintf( D_FULLDEBUG, "Inside ScriptProc::PublishUpdateAd()\n" );

		// TODO: anything interesting or specific in here?

	return UserProc::PublishUpdateAd( ad );
}


void
ScriptProc::Suspend()
{
	daemonCore->Send_Signal(JobPid, SIGSTOP);
	is_suspended = true;
}


void
ScriptProc::Continue()
{
	daemonCore->Send_Signal(JobPid, SIGCONT);
	is_suspended = false;
}


bool
ScriptProc::ShutdownGraceful()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, soft_kill_sig);
	return false;	// return false says shutdown is pending	
}


bool
ScriptProc::ShutdownFast()
{
	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, SIGKILL);
	return false;	// return false says shutdown is pending
}


bool
ScriptProc::Remove()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, rm_kill_sig);
	return false;	// return false says shutdown is pending	
}


bool
ScriptProc::Hold()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, hold_kill_sig);
	return false;	// return false says shutdown is pending	
}

