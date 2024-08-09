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
#include "env.h"
#include "user_proc.h"
#include "os_proc.h"
#include "starter.h"
#include "condor_daemon_core.h"

#ifdef HAVE_SCM_RIGHTS_PASSFD
#include "shared_port_scm_rights.h"
#include "fdpass.h"
#endif

#include "condor_attributes.h"
#include "classad_helpers.h"
#include "sig_name.h"
#include "exit.h"
#include "condor_uid.h"
#include "filename_tools.h"
#include "my_popen.h"
#ifdef WIN32
#include "perm.h"
#include "profile.WINDOWS.h"
#include "access_desktop.WINDOWS.h"
#endif
#include "classad_oldnew.h"
#include "singularity.h"
#include "find_child_proc.h"
#include "ToE.h"

#include <sstream>
#include <charconv>

extern class Starter *Starter;
extern const char* JOB_WRAPPER_FAILURE_FILE;

extern const char* JOB_AD_FILENAME;
extern const char* MACHINE_AD_FILENAME;


int singExecPid = -1;
ReliSock *sns = 0;

/* OsProc class implementation */

OsProc::OsProc( ClassAd* ad ) : howCode(-1), cgroupActive(false)
{
    dprintf ( D_FULLDEBUG, "In OsProc::OsProc()\n" );
	JobAd = ad;
	is_suspended = false;
	is_checkpointed = false;
	num_pids = 0;
	dumped_core = false;
	job_not_started = false;
	m_using_priv_sep = false;
	singReaperId = -1;
	UserProc::initialize();
}


OsProc::~OsProc()
{
	if ( daemonCore && daemonCore->SocketIsRegistered(&sshListener)) {
		daemonCore->Cancel_Socket(&sshListener);
	}
}


bool
OsProc::canonicalizeJobPath(/* not const */ std::string &JobName, const char *job_iwd) {
		// prepend the full path to this name so that we
		// don't have to rely on the PATH inside the
		// USER_JOB_WRAPPER or for exec().

    bool transfer_exe = true;
    JobAd->LookupBool(ATTR_TRANSFER_EXECUTABLE, transfer_exe);

    bool preserve_rel = false;
    if (!JobAd->LookupBool(ATTR_PRESERVE_RELATIVE_EXECUTABLE, preserve_rel)) {
        preserve_rel = false;
    }

    bool relative_exe = !fullpath(JobName.c_str()) && (JobName.length() > 0);

	if (this->ShouldConvertCmdToAbsolutePath()) {
		if (relative_exe && preserve_rel && !transfer_exe) {
			dprintf(D_ALWAYS, "Preserving relative executable path: %s\n", JobName.c_str());
		}
		else if ( Starter->jic->usingFileTransfer() && transfer_exe ) {
			formatstr( JobName, "%s%c%s",
					Starter->GetWorkingDir(0),
					DIR_DELIM_CHAR,
					condor_basename(JobName.c_str()) );
		}
		else if (relative_exe && job_iwd && *job_iwd) {
			std::string full_name;
			formatstr(full_name, "%s%c%s",
					job_iwd,
					DIR_DELIM_CHAR,
					JobName.c_str());
			JobName = full_name;
		}
	}
	return true;
}

int
OsProc::StartJob(FamilyInfo* family_info, FilesystemRemap* fs_remap=NULL)
{
	int nice_inc = 0;
	bool has_wrapper = false;

	dprintf(D_FULLDEBUG,"in OsProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in OsProc::StartJob()!\n" );
		job_not_started = true;
		return 0;
	}

	std::string JobName;
	if ( JobAd->LookupString( ATTR_JOB_CMD, JobName ) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n",
				 ATTR_JOB_CMD );
		job_not_started = true;
		return 0;
	}

	const char* job_iwd = Starter->jic->jobRemoteIWD();
	dprintf( D_ALWAYS, "IWD: %s\n", job_iwd );

		// // // // // //
		// Arguments
		// // // // // //
	this->canonicalizeJobPath(JobName, job_iwd);

	if( Starter->isGridshell() ) {
			// if we're a gridshell, just try to chmod our job, since
			// globus probably transfered it for us and left it with
			// bad permissions...
		priv_state old_priv = set_user_priv();
		int retval = chmod( JobName.c_str(), S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf ( D_ALWAYS, "Failed to chmod %s!\n", JobName.c_str() );
			return 0;
		}
	}

	ArgList args;

		// Since we may be adding to the argument list, we may need to deal
		// with platform-specific arg syntax in the user's args in order
		// to successfully merge them with the additional wrapper args.
	args.SetArgV1SyntaxToCurrentPlatform();

	std::string wrapper;
	has_wrapper = param(wrapper, "USER_JOB_WRAPPER");

	if (JobName.length() > 0) {
		if( !getArgv0() || has_wrapper ) {
			args.AppendArg(JobName.c_str());
		} else {
			args.AppendArg(getArgv0());
		}
	}
	
		// Support USE_PARROT 
	bool use_parrot = false;
	if( JobAd->LookupBool( ATTR_USE_PARROT, use_parrot) ) {
			// Check for parrot executable
		char *parrot = NULL;
		if( (parrot=param("PARROT")) ) {
			if( access(parrot,X_OK) < 0 ) {
				dprintf( D_ALWAYS, "Unable to use parrot(Cannot find/execute "
					"at %s(%s)).\n", parrot, strerror(errno) );
				free( parrot );
				job_not_started = true;
				return 0;
			} else {
				args.AppendArg(JobName.c_str());
				JobName = parrot;
				free( parrot );
			}
		} else {
			dprintf( D_ALWAYS, "Unable to use parrot(Undefined path in config"
			" file)" );
			job_not_started = true;
			return 0;
		}
	}

		// Either way, we now have to add the user-specified args as
		// the rest of the Args string.
	std::string args_error;
	if(!args.AppendArgsFromClassAd(JobAd,args_error)) {
		dprintf(D_ALWAYS, "Failed to read job arguments from JobAd.  "
				"Aborting OsProc::StartJob: %s\n",args_error.c_str());
		job_not_started = true;
		return 0;
	}

		// // // // // // 
		// Environment 
		// // // // // // 

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;

	std::string env_errors;
	if( !Starter->GetJobEnv(JobAd,&job_env, env_errors) ) {
		dprintf( D_ALWAYS, "Aborting OSProc::StartJob: %s\n",
				 env_errors.c_str());
		job_not_started = true;
		return 0;
	}


		// // // // // // 
		// Standard Files
		// // // // // // 

	// handle stdin, stdout, and stderr redirection
	int fds[3];
		// initialize these to -2 to mean they're not specified.
		// -1 will be treated as an error.
	fds[0] = -2; fds[1] = -2; fds[2] = -2;

		// in order to open these files we must have the user's privs:
	priv_state priv;
	priv = set_user_priv();

	bool stdin_ok;
	bool stdout_ok;
	bool stderr_ok;
	fds[0] = openStdFile( SFT_IN,
	                      NULL,
	                      true,
	                      "Input file");
	stdin_ok = (fds[0] != -1);
	fds[1] = openStdFile( SFT_OUT,
	                      NULL,
	                      true,
	                      "Output file");
	stdout_ok = (fds[1] != -1);
	fds[2] = openStdFile( SFT_ERR,
	                      NULL,
	                      true,
	                      "Error file");
	stderr_ok = (fds[2] != -1);

	/* Bail out if we couldn't open the std files correctly */
	if( !stdin_ok || !stdout_ok || !stderr_ok ) {
		/* only close ones that had been opened correctly */
		for ( int i = 0; i <= 2; i++ ) {
			if ( fds[i] >= 0 ) {
				daemonCore->Close_FD ( fds[i] );
			}
		}
		dprintf(D_ALWAYS, "Failed to open some/all of the std files...\n");
		dprintf(D_ALWAYS, "Aborting OsProc::StartJob.\n");
		set_priv(priv); /* go back to original priv state before leaving */
		job_not_started = true;
		return 0;
	}

		// // // // // // 
		// Misc + Exec
		// // // // // // 

	if( !ThisProcRunsAlongsideMainProc() ) {
		Starter->jic->notifyJobPreSpawn();
	}

	// compute job's renice value by evaluating the machine's
	// JOB_RENICE_INCREMENT in the context of the job ad...

    char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if( ptmp ) {
			// insert renice expr into our copy of the job ad
		if( !JobAd->AssignExpr( "Renice", ptmp ) ) {
			dprintf( D_ALWAYS, "ERROR: failed to insert JOB_RENICE_INCREMENT "
				"into job ad, Aborting OsProc::StartJob...\n" );
			free( ptmp );
			job_not_started = true;
			return 0;
		}
			// evaluate
		if( JobAd->LookupInteger( "Renice", nice_inc ) ) {
			dprintf( D_ALWAYS, "Renice expr \"%s\" evaluated to %d\n",
					 ptmp, nice_inc );
		} else {
			dprintf( D_ALWAYS, "WARNING: job renice expr (\"%s\") doesn't "
					 "eval to int!  Using default of 10...\n", ptmp );
			nice_inc = 10;
		}

			// enforce valid ranges for nice_inc
		if( nice_inc < 0 ) {
			dprintf( D_FULLDEBUG, "WARNING: job renice value (%d) is too "
					 "low: adjusted to 0\n", nice_inc );
			nice_inc = 0;
		}
		else if( nice_inc > 19 ) {
			dprintf( D_FULLDEBUG, "WARNING: job renice value (%d) is too "
					 "high: adjusted to 19\n", nice_inc );
			nice_inc = 19;
		}

		ASSERT( ptmp );
		free( ptmp );
		ptmp = NULL;
	} else {
			// if JOB_RENICE_INCREMENT is undefined, default to 0
		nice_inc = 0;
	}

		// Grab the full environment back out of the Env object 
	if(IsFulldebug(D_FULLDEBUG)) {
		std::string env_string;
		job_env.getDelimitedStringForDisplay( env_string);
		dprintf(D_FULLDEBUG, "Env = %s\n", env_string.c_str());
	}

	// Stash the environment in the manifest directory, if desired.
	std::string manifest_dir = "_condor_manifest";
	bool want_manifest = false;
	if( JobAd->LookupString( ATTR_JOB_MANIFEST_DIR, manifest_dir ) ||
	   (JobAd->LookupBool( ATTR_JOB_MANIFEST_DESIRED, want_manifest ) && want_manifest) ) {
		const std::string f = "environment";
		FILE * file = Starter->OpenManifestFile( f.c_str() );
		if( file != NULL ) {
			std::string env_string;
			job_env.getDelimitedStringForDisplay( env_string);

			fprintf( file, "%s\n", env_string.c_str());
			fclose(file);
		} else {
			dprintf( D_ALWAYS, "Failed to open environment log %s: %d (%s)\n", f.c_str(), errno, strerror(errno) );
		}
	}

	// Check to see if we need to start this process paused, and if
	// so, pass the right flag to DC::Create_Process().
	int job_opt_mask = DCJOBOPT_NO_CONDOR_ENV_INHERIT;
	bool inherit_starter_env = false;
	auto_free_ptr envlist(param("JOB_INHERITS_STARTER_ENVIRONMENT"));
	if (envlist && ! string_is_boolean_param(envlist, inherit_starter_env)) {
		WhiteBlackEnvFilter filter(envlist);
		job_env.Import(filter);
		inherit_starter_env = false; // make sure that CreateProcess doesn't inherit again
	}
	if ( ! inherit_starter_env) {
		job_opt_mask |= DCJOBOPT_NO_ENV_INHERIT;
	}
	bool suspend_job_at_exec = false;
	JobAd->LookupBool( ATTR_SUSPEND_JOB_AT_EXEC, suspend_job_at_exec);
	if( suspend_job_at_exec ) {
		dprintf( D_FULLDEBUG, "OsProc::StartJob(): "
				 "Job wants to be suspended at exec\n" );
		job_opt_mask |= DCJOBOPT_SUSPEND_ON_EXEC;
	}

	// If there is a requested coresize for this job, enforce it.
	// Convert negative and very large values to RLIM_INFINITY, meaning
	// no size limit.
	// RLIM_INFINITY is unsigned, but its value and type size vary.
	long long core_size_ad;
	size_t core_size;
	size_t *core_size_ptr = NULL;
#if !defined(WIN32)
	if ( JobAd->LookupInteger( ATTR_CORE_SIZE, core_size_ad ) ) {
		if ( core_size_ad < 0 ) {
			core_size = RLIM_INFINITY;
		} else {
			core_size = (size_t)core_size_ad;
		}
	} else {
		// if ATTR_CORE_SIZE is unset, assume 0
		core_size = 0;
	}
	core_size_ptr = &core_size;
#endif // !defined(WIN32)

	long rlimit_as_hard_limit = 0;
	char *rlimit_expr = param("STARTER_RLIMIT_AS");
	if (rlimit_expr) {
		classad::ClassAdParser parser;

		classad::ExprTree *tree = parser.ParseExpression(rlimit_expr);
		if (tree) {
			classad::Value val;
			long long result = 0L;

			if (EvalExprToNumber(tree, Starter->jic->machClassAd(), JobAd, val) &&
				val.IsIntegerValue(result)) {
					result *= 1024 * 1024; // convert to megabytes
					rlimit_as_hard_limit = (long)result; // truncate for Create_Process
					if (result > rlimit_as_hard_limit) {
						// if truncation to long results in a change in the value, then
						// the requested limit must be > 2 GB and we are on a 32 bit platform
						// in that case, the requested limit is > than what the process can get anyway
						// so just don't set a limit.
						rlimit_as_hard_limit = 0;
					}
					if (rlimit_as_hard_limit > 0) {
						dprintf(D_ALWAYS, "Setting job's virtual memory rlimit to %ld megabytes\n", rlimit_as_hard_limit);
					}
			} else {
				dprintf(D_ALWAYS, "Can't evaluate STARTER_RLIMIT_AS expression %s\n", rlimit_expr);
			}
			delete tree;
		} else {
			dprintf(D_ALWAYS, "Can't parse STARTER_RLIMIT_AS expression: %s\n", rlimit_expr);
		}
		free( rlimit_expr );
	}

	int *affinity_mask = makeCpuAffinityMask(Starter->getMySlotNumber());

#ifdef WIN32
	// if we loaded a slot user profile, import environment variables from it
	bool load_profile = false;
	bool run_as_owner = false;
	JobAd->LookupBool ( ATTR_JOB_LOAD_PROFILE, load_profile );
	JobAd->LookupBool ( ATTR_JOB_RUNAS_OWNER,  run_as_owner );
	if (load_profile && !run_as_owner) {
		const char * username = get_user_loginname();
		/* publish the users environment into that of the main job's environment */
		if (!OwnerProfile::environment(job_env, priv_state_get_handle(), username)) {
			dprintf(D_ALWAYS, "Failed to export environment for %s into the job.\n",
				username ? username : "<null>");
		}
	}
#endif

		// While we are still in user priv, print out the username
#if defined(LINUX)
	std::stringstream ss2;
	ss2 << Starter->GetExecuteDir() << DIR_DELIM_CHAR << "dir_" << getpid();
	std::string execute_dir = ss2.str();
	htcondor::Singularity::result sing_result; 
	int orig_args_len = args.Count();

	if (SupportsPIDNamespace()) {
		sing_result = htcondor::Singularity::setup(*Starter->jic->machClassAd(), *JobAd, JobName, args, job_iwd ? job_iwd : "", execute_dir, job_env);
	} else {
		sing_result = htcondor::Singularity::DISABLE;
	}

	if (sing_result == htcondor::Singularity::SUCCESS) {
		dprintf(D_ALWAYS, "Running job via singularity.\n");
		bool ssh_enabled = param_boolean("ENABLE_SSH_TO_JOB",true,true,Starter->jic->machClassAd(),JobAd);
		if( ssh_enabled ) {
			SetupSingularitySsh();
		}

		if (fs_remap) {
			dprintf(D_ALWAYS, "Disabling filesystem remapping; singularity will perform these features.\n");
			fs_remap = NULL;
		}
		if (family_info && family_info->want_pid_namespace) {
			dprintf(D_FULLDEBUG, "PID namespaces cannot be enabled for singularity jobs.\n");
			job_not_started = true;
			free(affinity_mask);
			return 0;
		}

		if (param_boolean("SINGULARITY_RUN_TEST_BEFORE_JOB", true)) {
			std::string singErrorMessage;
			bool result = htcondor::Singularity::runTest(JobName, args, orig_args_len, job_env, singErrorMessage);
			if (!result) {
				free(affinity_mask);
				job_not_started = true;
				std::string starterErrorMessage = "Singularity test failed:";
				starterErrorMessage += singErrorMessage;
				EXCEPT("%s", starterErrorMessage.c_str());
				return 0;
			}
		}


	} else if (sing_result == htcondor::Singularity::FAILURE) {
		dprintf(D_ALWAYS, "Singularity enabled but setup failed; failing job.\n");
		job_not_started = true;
		free(affinity_mask);
		return 0;
	} 
#else
	if( false ) {
	}
#endif
	else {
		char const *username = NULL;
		char const *how = "";
		username = get_user_loginname();
		if( !username ) {
			username = "same uid as parent: personal condor";
		}
		dprintf(D_ALWAYS,"Running job %sas user %s\n",how,username);
	}
		// Support USER_JOB_WRAPPER parameter...
	if( has_wrapper ) {

			// make certain this wrapper program exists and is executable
		if( access(wrapper.c_str(),X_OK) < 0 ) {
			dprintf( D_ALWAYS, 
					 "Cannot find/execute USER_JOB_WRAPPER file %s\n",
					 wrapper.c_str() );
			job_not_started = true;
			free(affinity_mask);
			return 0;
		}
			// Now, we've got a valid wrapper.  We want that to become
			// "JobName" so we exec it directly. We also insert the
			// wrapper filename at the front of args. As a result,
			// the executable being wrapped is now argv[1] and so forth.
		args.InsertArg(wrapper.c_str(),0);
		JobName = wrapper;
	}
		// in the below dprintfs, we want to skip past argv[0], which
		// is sometimes condor_exec, in the Args string. 

	std::string args_string;
	args.GetArgsStringForDisplay(args_string, 1);
	if( has_wrapper ) {
			// print out exactly what we're doing so folks can debug
			// it, if they need to.
		dprintf( D_ALWAYS, "Using wrapper %s to exec %s\n", JobName.c_str(),
				 args_string.c_str() );
	} else {
		dprintf( D_ALWAYS, "About to exec %s %s\n", JobName.c_str(),
				 args_string.c_str() );
	}



	set_priv ( priv );

    // use this to return more detailed and reliable error message info
    // from create-process operation.
    std::string create_process_err_msg;

	//
	// The following two commented-out Create_Process() calls were
	// left in as examples of (respectively) that bad old way,
	// the bad new way (in case you actually need to set all of
	// the default arguments), and the good new way (in case you
	// don't, in which case it's both shorter and clearer).
	//
	/*
	JobPid = daemonCore->Create_Process( JobName.c_str(),
	                                     args,
	                                     PRIV_USER_FINAL,
	                                     1,
	                                     FALSE,
	                                     FALSE,
	                                     &job_env,
	                                     job_iwd,
	                                     family_info,
	                                     NULL,
	                                     fds,
	                                     NULL,
	                                     nice_inc,
	                                     NULL,
	                                     job_opt_mask,
	                                     core_size_ptr,
	                                     affinity_mask,
	                                     NULL,
	                                     create_process_err_msg,
	                                     fs_remap,
	                                     rlimit_as_hard_limit);
	*/
	/*
	JobPid = daemonCore->CreateProcessNew( JobName.c_str(),
	                                     args,
	                                     {
	                                     PRIV_USER_FINAL,
	                                     1,
	                                     FALSE,
	                                     FALSE,
	                                     &job_env,
	                                     job_iwd,
	                                     family_info,
	                                     NULL,
	                                     fds,
	                                     NULL,
	                                     nice_inc,
	                                     NULL,
	                                     job_opt_mask,
	                                     core_size_ptr,
	                                     affinity_mask,
	                                     NULL,
	                                     create_process_err_msg,
	                                     fs_remap,
	                                     rlimit_as_hard_limit
	                                     }
	                                     );
	*/

	OptionalCreateProcessArgs cpArgs(create_process_err_msg);
	JobPid = daemonCore->CreateProcessNew( JobName, args,
		 cpArgs.priv(PRIV_USER_FINAL)
		.wantCommandPort(FALSE).wantUDPCommandPort(FALSE)
		.env(&job_env).cwd(job_iwd).familyInfo(family_info)
		.std(fds).niceInc(nice_inc).jobOptMask(job_opt_mask)
		.coreHardLimit(core_size_ptr).affinityMask(affinity_mask)
		.remap(fs_remap)
		.asHardLimit(rlimit_as_hard_limit)
	);

	// Create_Process() saves the errno for us if it is an "interesting" error.
	int create_process_errno = errno;

    // This executes for the daemon core create-process logic
    if ((FALSE == JobPid) && (0 != create_process_errno)) {
        if (create_process_err_msg != "") create_process_err_msg += " ";
        std::string errbuf;
        formatstr(errbuf, "(errno=%d: '%s')", create_process_errno, strerror(create_process_errno));
        create_process_err_msg += errbuf;
    }

	free( affinity_mask );

	// now close the descriptors in fds array.  our child has inherited
	// them already, so we should close them so we do not leak descriptors.
	// NOTE, we want to use a special method to close the starter's
	// versions, if that's what we're using, so we don't think we've
	// still got those available in other parts of the code for any
	// reason.
	for ( int i = 0; i <= 2; i++ ) {
		if ( fds[i] >= 0 ) {
			daemonCore->Close_FD ( fds[i] );
		}
	}

	if ( JobPid == FALSE ) {
		JobPid = -1;

		if(!create_process_err_msg.empty()) {

			// if the reason Create_Process failed was that registering
			// a family with the ProcD failed, it is indicative of a
			// problem regarding this execute machine, not the job. in
			// this case, we'll want to EXCEPT instead of telling the
			// Shadow to put the job on hold. there are probably other
			// error conditions where EXCEPTing would be more appropriate
			// as well...
			//
			if (create_process_errno == DaemonCore::ERRNO_REGISTRATION_FAILED) {
				EXCEPT("Create_Process failed to register the job with the ProcD");
			}

			std::string err_msg = "Failed to execute '";
			err_msg += JobName;
			err_msg += "'";
			if(!args_string.empty()) {
				err_msg += " with arguments ";
				err_msg += args_string;
			}
			err_msg += ": ";
			err_msg += create_process_err_msg;
			if( !ThisProcRunsAlongsideMainProc() ) {
				Starter->jic->notifyStarterError( err_msg.c_str(),
			    	                              true,
			        	                          CONDOR_HOLD_CODE::FailedToCreateProcess,
			            	                      create_process_errno );
			}
		}

		dprintf(D_ALWAYS,"Create_Process(%s,%s, ...) failed: %s\n",
			JobName.c_str(), args_string.c_str(), create_process_err_msg.c_str());
		job_not_started = true;
		return 0;
	}

	num_pids++;

	dprintf(D_ALWAYS,"Create_Process succeeded, pid=%d\n",JobPid);

	if (family_info->cgroup_active) {
		this->cgroupActive = true;
	}
	condor_gettimestamp( job_start_time );

	return 1;
}

bool
OsProc::JobReaper( int pid, int status )
{
	dprintf( D_FULLDEBUG, "Inside OsProc::JobReaper()\n" );

	if (JobPid == pid) {
		// Only write ToE tags for the actual job process.
		if(! ThisProcRunsAlongsideMainProc()) {
			// Write the appropriate ToE tag if the process exited
			// of its own accord.
			if(! requested_exit) {
				// Store for the post-script's environment.
				this->howCode = ToE::OfItsOwnAccord;

				// This ClassAd gets delete()d by toe when toe goes out of
				// scope, because Insert() transfers ownership.
				classad::ClassAd * tag = new classad::ClassAd();
				tag->InsertAttr( "Who", ToE::itself );
				tag->InsertAttr( "How", ToE::strings[ToE::OfItsOwnAccord] );
				tag->InsertAttr( "HowCode", ToE::OfItsOwnAccord );
				struct timeval exitTime;
				condor_gettimestamp( exitTime );
				tag->InsertAttr( "When", (long long)exitTime.tv_sec );

				if(WIFSIGNALED(status)) {
					tag->InsertAttr( ATTR_ON_EXIT_BY_SIGNAL, true );
					tag->InsertAttr( ATTR_ON_EXIT_SIGNAL, WTERMSIG(status));
				} else {
					tag->InsertAttr( ATTR_ON_EXIT_BY_SIGNAL, false );
					tag->InsertAttr( ATTR_ON_EXIT_CODE, WEXITSTATUS(status));
				}

				classad::ClassAd toe;
				toe.Insert( ATTR_JOB_TOE, tag );

				std::string jobAdFileName;
				formatstr( jobAdFileName, "%s/.job.ad",
					Starter->GetWorkingDir(0) );
				ToE::writeTag( & toe, jobAdFileName );

				// Update the schedd's copy of the job ad.
				ClassAd updateAd( toe );
				Starter->publishUpdateAd( & updateAd );
				Starter->jic->periodicJobUpdate( & updateAd );
			} else {
				// If we didn't write a ToE, check to see if the startd did.
				std::string jobAdFileName;
				formatstr( jobAdFileName, "%s/.job.ad",
					Starter->GetWorkingDir(0) );
				FILE * f = safe_fopen_wrapper_follow( jobAdFileName.c_str(), "r" );
				if(! f) {
					dprintf( D_ALWAYS, "Failed to open .job.ad, can't forward ToE tag.\n" );
				} else {
					int error;
					bool isEof;
					classad::ClassAd jobAd;
					if( InsertFromFile( f, jobAd, isEof, error ) ) {
						classad::ClassAd * tag =
							dynamic_cast<classad::ClassAd *>(jobAd.Lookup(ATTR_JOB_TOE));
						if( tag ) {
							// Store for the post-script's environment.
							tag->EvaluateAttrInt( "HowCode", this->howCode );

							// Don't let jobAd delete tag; toe will delete
							// when it goes out of scope.
							jobAd.Remove(ATTR_JOB_TOE);

							classad::ClassAd toe;
							toe.Insert(ATTR_JOB_TOE, tag );

							// Update the schedd's copy of the job ad.
							ClassAd updateAd( toe );
							Starter->publishUpdateAd( & updateAd );
							Starter->jic->periodicJobUpdate( & updateAd );
						}
					}
				}
			}
		}

			// clear out num_pids... everything under this process
			// should now be gone
		num_pids = 0;

			// check to see if the job dropped a core file.  if so, we
			// want to rename that file so that it won't clobber other
			// core files back at the submit site.
		checkCoreFile();
	}
	return UserProc::JobReaper(pid, status);
}


bool
OsProc::JobExit( void )
{
	int reason;	

	dprintf( D_FULLDEBUG, "Inside OsProc::JobExit()\n" );

	if( requested_exit == true ) {
		if( Starter->jic->hadHold() || Starter->jic->hadRemove() ) {
			reason = JOB_KILLED;
		} else {
			reason = JOB_SHOULD_REQUEUE;
		}
	} else if( dumped_core ) {
		reason = JOB_COREDUMPED;
	} else if( job_not_started ) {
		reason = JOB_NOT_STARTED;
	} else {
		reason = JOB_EXITED;
	}

#if defined ( WIN32 )

    priv_state old = set_user_priv ();
    HANDLE user_token = priv_state_get_handle ();
    ASSERT ( user_token );
    
    // Check USE_VISIBLE_DESKTOP in condor_config.  If set to TRUE,
    // then removed our users priveleges from the visible desktop.
	if (param_boolean_crufty("USE_VISIBLE_DESKTOP", false)) {
        /* at this point we can revoke the user's access to the visible desktop */
        RevokeDesktopAccess ( user_token );
    }

    set_priv ( old );

#endif

	return Starter->jic->notifyJobExit( exit_status, reason, this );
}



void
OsProc::checkCoreFile( void )
{
	// We rename any core file we find to core.<cluster>.<proc>.
	// We add the core file if present to the list of output files
	// (that happens in renameCoreFile)

	// Since Linux now writes out "core.pid" by default, we should
	// search for that.  Try the JobPid of our first child:
	std::string name_with_pid;
	formatstr( name_with_pid, "core.%d", JobPid );

	std::string new_name;
	formatstr( new_name, "core.%d.%d",
	                  Starter->jic->jobCluster(), 
	                  Starter->jic->jobProc() );

	if( renameCoreFile(name_with_pid.c_str(), new_name.c_str()) ) {
		// great, we found it, renameCoreFile() took care of
		// everything we need to do... we're done.
		return;
	}

	// Now, just see if there's a file called "core"
	if( renameCoreFile("core", new_name.c_str()) ) {
		return;
	}

	/*
	  maybe we should check for other possible pids (either by
	  using a Directory object to scan all the files in the iwd,
	  or by using the procfamily to test all the known pids under
	  the job).  also, you can configure linux to drop core files
	  with other possible values, not just pid.  however, it gets
	  complicated and it becomes harder and harder to tell if the
	  core files belong to this job or not in the case of a shared
	  file system where we might be running in a directory shared
	  by many jobs...  for now, the above is good enough and will
	  catch most of the problems we're having.
	*/
}


bool
OsProc::renameCoreFile( const char* old_name, const char* new_name )
{
	bool rval = false;
	int t_errno = 0;

	std::string old_full;
	std::string new_full;
	const char* job_iwd = Starter->jic->jobIWD();
	formatstr( old_full, "%s%c%s", job_iwd, DIR_DELIM_CHAR, old_name );
	formatstr( new_full, "%s%c%s", job_iwd, DIR_DELIM_CHAR, new_name );

	priv_state old_priv;

		// we need to do this rename as the user...
	errno = 0;
	old_priv = set_user_priv();
	int ret = rename(old_full.c_str(), new_full.c_str());
	if( ret != 0 ) {
			// rename failed
		t_errno = errno; // grab errno right away
		rval = false;
	} else { 
			// rename succeeded
		rval = true;
   	}
	set_priv( old_priv );

	if( rval ) {
		dprintf( D_FULLDEBUG, "Found core file '%s', renamed to '%s'\n",
				 old_name, new_name );
		if( dumped_core ) {
			EXCEPT( "IMPOSSIBLE: inside OsProc::renameCoreFile and "
					"dumped_core is already TRUE" );
		}
		dumped_core = true;
			// make sure it'll get transfered back, too.
		Starter->jic->addToOutputFiles( new_name );
	} else if( t_errno != ENOENT ) {
		dprintf( D_ALWAYS, "Failed to rename(%s,%s): errno %d (%s)\n",
				 old_full.c_str(), new_full.c_str(), t_errno,
				 strerror(t_errno) );
	}
	return rval;
}


void
OsProc::Suspend()
{
	daemonCore->Send_Signal(JobPid, SIGSTOP);
	is_suspended = true;
}

void
OsProc::Continue()
{
	if (is_suspended)
	{
	  daemonCore->Send_Signal(JobPid, SIGCONT);
	  is_suspended = false;
	}
}

bool
OsProc::ShutdownGraceful()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	if ( findRmKillSig(JobAd) != -1 ) {
		daemonCore->Send_Signal(JobPid, rm_kill_sig);
	} else {
		bool sent = daemonCore->Send_Signal(JobPid, soft_kill_sig);
		if (!sent) {
			dprintf(D_ALWAYS, "Send (softkill) signal failed, retrying...\n");
			sleep(1);
			sent = daemonCore->Send_Signal(JobPid, soft_kill_sig);
			if (!sent) {
				dprintf(D_ALWAYS, "Send (softkill) signal failed twice, hardkill will fire after timeout\n");
			} else {
				dprintf(D_ALWAYS, "Send (softkill) signal worked the second time\n");
			}
		}
	}
	return false;	// return false says shutdown is pending	
}


bool
OsProc::ShutdownFast()
{
	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, SIGKILL);
	return false;	// return false says shutdown is pending
}


bool
OsProc::Remove()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, rm_kill_sig);
	return false;	// return false says shutdown is pending	
}


bool
OsProc::Hold()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, hold_kill_sig);
	return false;	// return false says shutdown is pending	
}


bool
OsProc::PublishUpdateAd( ClassAd* ad ) 
{
	std::string buf;

	if (m_proc_exited) {
		buf = "Exited";
	} else if( is_checkpointed ) {
		buf = "Checkpointed";
	} else if( is_suspended ) {
		buf = "Suspended";
	} else {
		buf = "Running";
	}
	ad->Assign( ATTR_JOB_STATE, buf );

	ad->Assign( ATTR_NUM_PIDS, num_pids );

	if (m_proc_exited) {
		if( dumped_core ) {
			ad->Assign( ATTR_JOB_CORE_DUMPED, true );
		} // should we put in ATTR_JOB_CORE_DUMPED = false if not?
	}

	if (cgroupActive) {
		ad->Assign(ATTR_CGROUP_ENFORCED, true );
	} 
	return UserProc::PublishUpdateAd( ad );
}

void
OsProc::PublishToEnv( Env * proc_env ) {
	UserProc::PublishToEnv( proc_env );

	if( howCode != -1 ) {
		proc_env->SetEnv( "_CONDOR_HOW_CODE", std::to_string( howCode ) );
	}
}

int *
OsProc::makeCpuAffinityMask(int slotId) {
	bool useAffinity = param_boolean("ENFORCE_CPU_AFFINITY", false);

	if (!useAffinity) {
		dprintf(D_FULLDEBUG, "ENFORCE_CPU_AFFINITY not true, not setting affinity\n");	
		return NULL;
	}

	// slotID of 0 means "no slot".  Punt.
	if (slotId < 1) {
		return NULL;
	}

	char *affinityParamResult = param("STARTD_ASSIGNED_AFFINITY");
	if (!affinityParamResult) {
		std::string affinityParam;
		formatstr(affinityParam, "SLOT%d_CPU_AFFINITY", slotId);
		affinityParamResult = param(affinityParam.c_str());
	}

	if (affinityParamResult == NULL) {
		// No specific cpu, assume one-to-one mapping from slotid
		// to cpu affinity

		dprintf(D_FULLDEBUG, "Setting cpu affinity to %d\n", slotId - 1);	
		int *mask = (int *) malloc(sizeof(int) * 2);
		ASSERT( mask != NULL );
		mask[0] = 2;
		mask[1] = slotId - 1; // slots start at 1, cpus at 0
		return mask;
	}

	std::vector<std::string> cpus = split(affinityParamResult);

	if (cpus.size() < 1) {
		dprintf(D_ALWAYS, "Could not parse affinity string %s, not setting affinity\n", affinityParamResult);
		free(affinityParamResult);
		return NULL;
	}

	int *mask = (int *) malloc(sizeof(int) * (cpus.size() + 1));
	if ( ! mask)
		return mask;

	mask[0] = cpus.size() + 1;
	int index = 1;
	for (const auto& cpu: cpus) {
		mask[index++] = atoi(cpu.c_str());
	}

	free(affinityParamResult);
	return mask;
}

void
OsProc::SetupSingularitySsh() {
#ifdef LINUX
	
	static bool first_time = true;
	if (first_time) {
		first_time = false;
	} else {
		return;
	}
	// First, create a unix domain socket that we can listen on
	int uds = socket(AF_UNIX, SOCK_STREAM, 0);
	if (uds < 0) {
		dprintf(D_ALWAYS, "Cannot create unix domain socket for singularity ssh_to_job\n");
		return;
	}

	// stuff the unix domain socket into a reli sock
	sshListener.close();
	sshListener.assignDomainSocket(uds);

	// and bind it to a filename in the scratch directory
	struct sockaddr_un pipe_addr;
	memset(&pipe_addr, 0, sizeof(pipe_addr));
	pipe_addr.sun_family = AF_UNIX;
	unsigned pipe_addr_len;

	std::string workingDir = Starter->GetWorkingDir(0);
	std::string pipeName = workingDir + "/.docker_sock";	

	strncpy(pipe_addr.sun_path, pipeName.c_str(), sizeof(pipe_addr.sun_path)-1);
	pipe_addr_len = SUN_LEN(&pipe_addr);

	{
	TemporaryPrivSentry sentry(PRIV_USER);
	int rc = bind(uds, (struct sockaddr *)&pipe_addr, pipe_addr_len);
	if (rc < 0) {
		dprintf(D_ALWAYS, "Cannot bind unix domain socket at %s for singularity ssh_to_job: %d\n", pipeName.c_str(), errno);
		return;
	}
	}

	listen(uds, 50);
	sshListener._state = Sock::sock_special;
	sshListener._special_state = ReliSock::relisock_listen;

	// now register this socket so we get called when connected to
	int rc;
	rc = daemonCore->Register_Socket(
		&sshListener,
		"Singularity sshd listener",
		(SocketHandlercpp)&OsProc::AcceptSingSshClient,
		"OsProc::AcceptSingSshClient",
		this);
	ASSERT( rc >= 0 );


	// and a reaper 
	singReaperId = daemonCore->Register_Reaper("SingEnterReaper", (ReaperHandlercpp)&OsProc::SingEnterReaper,
		"ExecReaper", this);

#else
	// Shut the compiler up
	//(void)execReaperId;
#endif
}

int
OsProc::AcceptSingSshClient(Stream *stream) {
#ifdef LINUX
        int fds[3];
        sns = ((ReliSock*)stream)->accept();

		if (sns == nullptr) {
			dprintf(D_ALWAYS, "Could not accept new connection from ssh_to_job client %d: %s\n", errno, strerror(errno));

			// We have seen this error on production clusters, not sure what causes it.  To be safe
			// let's cancel the socket, so we won't get caught in an infinite loop if the socket stays hot
			daemonCore->Cancel_Socket(stream);
			return KEEP_STREAM;
		}
        dprintf(D_ALWAYS, "Accepted new connection from ssh client for container job\n");

        fds[0] = fdpass_recv(sns->get_file_desc());
        fds[1] = fdpass_recv(sns->get_file_desc());
        fds[2] = fdpass_recv(sns->get_file_desc());

	// we have the pid of the singularity process, need pid of the job
	// sometimes this is the direct child of singularity, sometimes singularity
	// runs an init-like process and the job is the grandchild of singularity

	// if a grandkid exists, use that, otherwise child, otherwise sing itself

	int pid = findChildProc(JobPid);
	if (pid == -1) {
		pid = JobPid; // hope for the best
	}
	if (findChildProc(pid) > 0)
		pid = findChildProc(pid);

	ArgList args;
	args.AppendArg("/usr/bin/nsenter");
	args.AppendArg("-t"); // target pid
	char buf[32];
	{ auto [p, ec] = std::to_chars(buf, buf + sizeof(buf) - 1, pid); *p = '\0';}
	args.AppendArg(buf); // pid of running job

	// get_user_[ug]id only works if uids has been initted
	// which isn't the case in a personal condor (and can't be)
	// If we get an error from those, assume personal condor
	// and pass in our uid/gid

	int uid = (int) get_user_uid();
	if (uid < 0) uid = getuid();

	args.AppendArg("-S");
	{ auto [p, ec] = std::to_chars(buf, buf + sizeof(buf) - 1, uid); *p = '\0';}
	args.AppendArg(buf);

	int gid = (int) get_user_gid();
	if (gid < 0) gid = getgid();

	args.AppendArg("-G");
	{ auto [p, ec] = std::to_chars(buf, buf + sizeof(buf) - 1, gid); *p = '\0';}
	args.AppendArg(buf);

	bool setuid = param_boolean("SINGULARITY_IS_SETUID", true);
	if (setuid) {
		// The default case where singularity is using a setuid wrapper
		args.AppendArg("-m"); // mount namespace
		args.AppendArg("-i"); // ipc namespace
		args.AppendArg("-p"); // pid namespace
		args.AppendArg("-r"); // root directory
		args.AppendArg("-w"); // cwd is container's

	} else {
		args.AppendArg("-U"); // enter only the User namespace
		args.AppendArg("-r"); // chroot
		args.AppendArg("-preserve-credentials");
	}

	Env env;
	std::string env_errors;
	if (!Starter->GetJobEnv(JobAd,&env, env_errors)) {
		dprintf(D_ALWAYS, "Warning -- cannot put environment into singularity job: %s\n", env_errors.c_str());
	}

	std::string target_dir;
	bool has_target = htcondor::Singularity::hasTargetDir(*JobAd, target_dir);

	if (has_target) {
		htcondor::Singularity::retargetEnvs(env, target_dir, "");
		// nsenter itself uses _CONDOR_SCRATCH_DIR to decide where the
		// home directory is, so leave this one behind, set to the
		// old non-prefixed value
		env.SetEnv("_CONDOR_SCRATCH_DIR", target_dir);
	}

	std::string bin_dir;
	param(bin_dir, "BIN");
	if (bin_dir.empty()) bin_dir = "/usr/bin";
	bin_dir += "/condor_nsenter";

	singExecPid = daemonCore->Create_Process(
		bin_dir.c_str(),
		args,
		setuid ? PRIV_ROOT : PRIV_USER,
		singReaperId,
		FALSE,
		FALSE,
		&env,
		".",
		NULL,
		NULL,
		fds);

        dprintf(D_ALWAYS, "singularity enter_ns returned pid %d\n", singExecPid);

#else
		(void)stream;	// shut the compiler up
#endif
return KEEP_STREAM;
}

int 
OsProc::SingEnterReaper( int pid, int status ) {
	dprintf(D_FULLDEBUG, "OsProc:singEnterReaper called with pid %d status = %d\n", pid, status);
		
	if (pid == singExecPid) {
		delete sns;
		return false;
	} else {
		dprintf(D_ALWAYS, "OsProc::singEnterReaper called for unknown pid %d\n", pid);
	}
	
	return true;
}
