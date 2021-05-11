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
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "user_proc.h"
#include "script_proc.h"
#include "starter.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "exit.h"
#include "condor_uid.h"
#include "basename.h"


extern Starter *Starter;


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

	std::string attr;

	attr = name;
	attr += ATTR_JOB_CMD;
	char* tmp = NULL;
	if( ! JobAd->LookupString( attr, &tmp ) ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				 attr.c_str() );
		return 0;
	}

		// // // // // // 
		// executable
		// // // // // // 

		// TODO: make it smart in cases we're not the gridshell and/or
		// didn't transfer files so that we don't prepend the wrong
		// path to the binary, and don't try to chmod it.
	std::string exe_path = "";
	if( tmp != NULL && !fullpath( tmp ) ) {
		exe_path += Starter->GetWorkingDir(0);
		exe_path += DIR_DELIM_CHAR;
	}

	if (tmp) {
		exe_path += tmp;
	}
	free( tmp ); 
	tmp = NULL;

	if( Starter->isGridshell() ) {
			// if we're a gridshell, chmod() the binary, since globus
			// probably transfered it for us and left it with bad
			// permissions...
		priv_state old_priv = set_user_priv();
		int retval = chmod( exe_path.c_str(), 0755 );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf( D_ALWAYS, "Failed to chmod %s: %s (errno %d)\n", 
					 exe_path.c_str(), strerror(errno), errno );
			return 0;
		}
	} 


		// // // // // // 
		// Args
		// // // // // // 

	char *args1 = NULL;
	char *args2 = NULL;
	std::string args1_attr;
	std::string args2_attr;
	args1_attr = name;
	args1_attr += ATTR_JOB_ARGUMENTS1;
	args2_attr = name;
	args2_attr += ATTR_JOB_ARGUMENTS2;

	JobAd->LookupString(args1_attr, &args1);
	JobAd->LookupString(args2_attr, &args2);

	ArgList args;

		// Since we are adding to the argument list, we may need to deal
		// with platform-specific arg syntax in the user's args in order
		// to successfully merge them with the additional args.
	args.SetArgV1SyntaxToCurrentPlatform();

		// First, put "condor_<name>script" at the front of Args,
		// since that will become argv[0] of what we exec(), either
		// the wrapper or the actual job.
	std::string arg0;
	arg0 = "condor_";
	arg0 += name;
	arg0 += "script";
	args.AppendArg(arg0);

	std::string args_error;
	if(args2 && *args2) {
		args.AppendArgsV2Raw(args2,args_error);
	}
	else if(args1 && *args1) {
		args.AppendArgsV1Raw(args1,args_error);
	}
	else {
		dprintf( D_FULLDEBUG, "neither %s nor %s could be found in JobAd\n",
				 args1_attr.c_str(), args2_attr.c_str());
	}

	free( args1 );
	free( args2 );

		// // // // // // 
		// Environment 
		// // // // // // 

	char *env1 = NULL;
	char *env2 = NULL;
	std::string env1_attr;
	std::string env2_attr;
	env1_attr = name;
	env1_attr += ATTR_JOB_ENVIRONMENT1;
	env2_attr = name;
	env2_attr += ATTR_JOB_ENVIRONMENT2;
	JobAd->LookupString( env1_attr, &env1 );
	JobAd->LookupString( env2_attr, &env2 );
			// TODO do we want to use the regular ATTR_JOB_ENVIRONMENT
			// if there's nothing specific for this script?

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;
	std::string env_errors;
	if( env2 && *env2 ) { 
		if( ! job_env.MergeFromV2Raw(env2, env_errors) ) {
			dprintf( D_ALWAYS, "Invalid %s found in JobAd (%s).  "
					 "Aborting ScriptProc::StartJob.\n",
					 env2_attr.c_str(),env_errors.c_str() );
			free( env1 );
			free( env2 );
			return 0;
		}
	}
	else if( env1 && *env1 ) { 
		if( ! job_env.MergeFromV1Raw(env1, env_errors) ) {
			dprintf( D_ALWAYS, "Invalid %s found in JobAd (%s).  "
					 "Aborting ScriptProc::StartJob.\n",
					 env1_attr.c_str(),env_errors.c_str() );
			free( env1 );
			free( env2 );
			return 0;
		}
	}

	free(env1);
	free(env2);

		// Now, let the starter publish any env vars it wants to add
	Starter->PublishToEnv( &job_env );


		// TODO: Deal with port regulation stuff?

		// Grab the full environment back out of the Env object 
	if(IsFulldebug(D_FULLDEBUG)) {
		std::string env_str;
		job_env.getDelimitedStringForDisplay( env_str);
		dprintf(D_FULLDEBUG, "%sEnv = %s\n", name, env_str.c_str() );
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

	std::string args_string;
	args.GetArgsStringForDisplay(args_string,1);
	dprintf( D_ALWAYS, "About to exec %s script: %s %s\n", 
			 name, exe_path.c_str(), 
			 args_string.c_str() );
		
	// If there is a requested coresize for this job, enforce it.
	// It is truncated because you can't put an unsigned integer
	// into a classad. I could rewrite condor's use of ATTR_CORE_SIZE to
	// be a float, but then when that attribute is read/written to the
	// job queue log by/or shared between versions of Condor which view the
	// type of that attribute differently, calamity would arise.
	int core_size_truncated;
	size_t core_size;
	size_t *core_size_ptr = NULL;
	if ( JobAd->LookupInteger(ATTR_CORE_SIZE, core_size_truncated) ) {
		core_size = (size_t)core_size_truncated;
		core_size_ptr = &core_size;
	}

	JobPid = daemonCore->Create_Process(exe_path.c_str(), 
	                                    args,
	                                    PRIV_USER_FINAL,
	                                    1,
	                                    FALSE,
	                                    FALSE,
	                                    &job_env,
	                                    Starter->jic->jobIWD(),
	                                    NULL,
	                                    NULL,
	                                    NULL,
	                                    NULL,
	                                    nice_inc,
	                                    NULL,
	                                    DCJOBOPT_NO_ENV_INHERIT,
	                                    core_size_ptr );

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = NULL;
	int create_process_errno = errno;
	if( JobPid == FALSE && errno ) {
		create_process_error = strerror( errno );
	}

	if( JobPid == FALSE ) {
		JobPid = -1;

		if( create_process_error ) {
			std::string err_msg = "Failed to execute '";
			err_msg += exe_path;
			err_msg += "'";
			if(!args_string.empty()) {
				err_msg += " with arguments ";
				err_msg += args_string;
			}
			err_msg += ": ";
			err_msg += create_process_error;
			Starter->jic->notifyStarterError( err_msg.c_str(), true, CONDOR_HOLD_CODE_FailedToCreateProcess, create_process_errno );
		}

		EXCEPT( "Create_Process(%s,%s, ...) failed",
				exe_path.c_str(), args_string.c_str() );
		return 0;
	}

	dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid );

	condor_gettimestamp( job_start_time );

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

