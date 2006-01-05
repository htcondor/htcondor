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

	char *args1 = NULL;
	char *args2 = NULL;
	MyString args1_attr;
	MyString args2_attr;
	args1_attr = name;
	args1_attr += ATTR_JOB_ARGUMENTS1;
	args2_attr = name;
	args2_attr += ATTR_JOB_ARGUMENTS2;

	JobAd->LookupString(args1_attr.Value(), &args1);
	JobAd->LookupString(args2_attr.Value(), &args2);

	ArgList args;

		// First, put "condor_<name>script" at the front of Args,
		// since that will become argv[0] of what we exec(), either
		// the wrapper or the actual job.
	MyString arg0;
	arg0 = "condor_";
	arg0 += name;
	arg0 += "script";
	args.AppendArg(arg0.Value());

	MyString args_error;
	bool args_success = false;
	if(args2 && *args2) {
		args_success = args.AppendArgsV2Raw(args2,&args_error);
	}
	else if(args1 && *args1) {
		args_success = args.AppendArgsV1Raw(args1,&args_error);
	}
	else {
		dprintf( D_FULLDEBUG, "neither %s nor %s could be found in JobAd\n",
				 args1_attr.Value(), args2_attr.Value());
	}


		// // // // // // 
		// Environment 
		// // // // // // 

	char *env1 = NULL;
	char *env2 = NULL;
	MyString env1_attr;
	MyString env2_attr;
	env1_attr = name;
	env1_attr += ATTR_JOB_ENVIRONMENT1;
	env2_attr = name;
	env2_attr += ATTR_JOB_ENVIRONMENT2;
	JobAd->LookupString( env1_attr.Value(), &env1 );
	JobAd->LookupString( env2_attr.Value(), &env2 );
			// TODO do we want to use the regular ATTR_JOB_ENVIRONMENT
			// if there's nothing specific for this script?

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;
	MyString env_errors;
	if( env2 && *env2 ) { 
		if( ! job_env.MergeFromV2Raw(env2,&env_errors) ) {
			dprintf( D_ALWAYS, "Invalid %s found in JobAd (%s).  "
					 "Aborting ScriptProc::StartJob.\n",
					 env2_attr.GetCStr(),env_errors.Value() );  
			return 0;
		}
	}
	else if( env1 && *env1 ) { 
		if( ! job_env.MergeFromV1Raw(env1,&env_errors) ) {
			dprintf( D_ALWAYS, "Invalid %s found in JobAd (%s).  "
					 "Aborting ScriptProc::StartJob.\n",
					 env1_attr.GetCStr(),env_errors.Value() );  
			return 0;
		}
	}

	free(env1);
	free(env2);

		// Now, let the starter publish any env vars it wants to add
	Starter->PublishToEnv( &job_env );


		// TODO: Deal with port regulation stuff?

		// Grab the full environment back out of the Env object 
	if(DebugFlags & D_FULLDEBUG) {
		MyString env_str;
		job_env.getDelimitedStringForDisplay(&env_str);
		dprintf(D_FULLDEBUG, "%sEnv = %s\n", name, env_str.Value() );
	}



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

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string,1);
	dprintf( D_ALWAYS, "About to exec %s script: %s %s\n", 
			 name, exe_path.GetCStr(), 
			 args_string.Value() );

	JobPid = daemonCore->Create_Process(exe_path.GetCStr(), 
				args, PRIV_USER_FINAL, 1, FALSE, &job_env,
				Starter->jic->jobIWD(), TRUE, NULL, NULL, nice_inc,
				DCJOBOPT_NO_ENV_INHERIT );

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = NULL;
	if( JobPid == FALSE && errno ) {
		create_process_error = strerror( errno );
	}

	if( JobPid == FALSE ) {
		JobPid = -1;

		if( create_process_error ) {
			MyString err_msg = "Failed to execute '";
			err_msg += exe_path.GetCStr();
			err_msg += ' ';
			err_msg += args_string;
			err_msg += "': ";
			err_msg += create_process_error;
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}

		EXCEPT( "Create_Process(%s,%s, ...) failed",
				exe_path.GetCStr(), args_string.Value() );
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

