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
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
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

extern CStarter *Starter;
extern const char* JOB_WRAPPER_FAILURE_FILE;

extern const char* JOB_AD_FILENAME;
extern const char* MACHINE_AD_FILENAME;


/* OsProc class implementation */

OsProc::OsProc( ClassAd* ad )
{
    dprintf ( D_FULLDEBUG, "In OsProc::OsProc()\n" );
	JobAd = ad;
	is_suspended = false;
	is_checkpointed = false;
	num_pids = 0;
	dumped_core = false;
	m_using_priv_sep = false;
	UserProc::initialize();
}


OsProc::~OsProc()
{
}


int
OsProc::StartJob(FamilyInfo* family_info, FilesystemRemap* fs_remap=NULL)
{
	int nice_inc = 0;
	bool has_wrapper = false;

	dprintf(D_FULLDEBUG,"in OsProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in OsProc::StartJob()!\n" );
		return 0;
	}

	MyString JobName;
	if ( JobAd->LookupString( ATTR_JOB_CMD, JobName ) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				 ATTR_JOB_CMD );
		return 0;
	}

	const char* job_iwd = Starter->jic->jobRemoteIWD();
	dprintf( D_ALWAYS, "IWD: %s\n", job_iwd );

		// some operations below will require a PrivSepHelper if
		// PrivSep is enabled (if it's not, privsep_helper will be
		// NULL)
	PrivSepHelper* privsep_helper = Starter->privSepHelper();

		// // // // // // 
		// Arguments
		// // // // // // 

		// prepend the full path to this name so that we
		// don't have to rely on the PATH inside the
		// USER_JOB_WRAPPER or for exec().

    bool transfer_exe = false;
    if (!JobAd->LookupBool(ATTR_TRANSFER_EXECUTABLE, transfer_exe)) {
        transfer_exe = false;
    }

    bool preserve_rel = false;
    if (!JobAd->LookupBool(ATTR_PRESERVE_RELATIVE_EXECUTABLE, preserve_rel)) {
        preserve_rel = false;
    }

    bool relative_exe = is_relative_to_cwd(JobName.Value());

    if (relative_exe && preserve_rel && !transfer_exe) {
        dprintf(D_ALWAYS, "Preserving relative executable path: %s\n", JobName.Value());
    }
	else if ( strcmp(CONDOR_EXEC,JobName.Value()) == 0 ) {
		JobName.formatstr( "%s%c%s",
		                 Starter->GetWorkingDir(),
		                 DIR_DELIM_CHAR,
		                 CONDOR_EXEC );
    }
	else if (relative_exe && job_iwd && *job_iwd) {
		MyString full_name;
		full_name.formatstr("%s%c%s",
		                  job_iwd,
		                  DIR_DELIM_CHAR,
		                  JobName.Value());
		JobName = full_name;

	}

	if( Starter->isGridshell() ) {
			// if we're a gridshell, just try to chmod our job, since
			// globus probably transfered it for us and left it with
			// bad permissions...
		priv_state old_priv = set_user_priv();
		int retval = chmod( JobName.Value(), S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf ( D_ALWAYS, "Failed to chmod %s!\n", JobName.Value() );
			return 0;
		}
	} 

	ArgList args;

		// Since we may be adding to the argument list, we may need to deal
		// with platform-specific arg syntax in the user's args in order
		// to successfully merge them with the additional wrapper args.
	args.SetArgV1SyntaxToCurrentPlatform();

		// First, put "condor_exec" or whatever at the front of Args,
		// since that will become argv[0] of what we exec(), either
		// the wrapper or the actual job.

	if( !getArgv0() ) {
		args.AppendArg(JobName.Value());
	} else {
		args.AppendArg(getArgv0());
	}
	
		// Support USER_JOB_WRAPPER parameter...
	char *wrapper = NULL;
	if( (wrapper=param("USER_JOB_WRAPPER")) ) {

			// make certain this wrapper program exists and is executable
		if( access(wrapper,X_OK) < 0 ) {
			dprintf( D_ALWAYS, 
					 "Cannot find/execute USER_JOB_WRAPPER file %s\n",
					 wrapper );
			free( wrapper );
			return 0;
		}
		has_wrapper = true;
			// Now, we've got a valid wrapper.  We want that to become
			// "JobName" so we exec it directly, and we want to put
			// what was the JobName (with the full path) as the first
			// argument to the wrapper
		args.AppendArg(JobName.Value());
		JobName = wrapper;
		free(wrapper);
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
				return 0;
			} else {
				args.AppendArg(JobName.Value());
				JobName = parrot;
				free( parrot );
			}
		} else {
			dprintf( D_ALWAYS, "Unable to use parrot(Undefined path in config"
			" file)" );
			return 0;
		}
	}

		// Either way, we now have to add the user-specified args as
		// the rest of the Args string.
	MyString args_error;
	if(!args.AppendArgsFromClassAd(JobAd,&args_error)) {
		dprintf(D_ALWAYS, "Failed to read job arguments from JobAd.  "
				"Aborting OsProc::StartJob: %s\n",args_error.Value());
		return 0;
	}

		// // // // // // 
		// Environment 
		// // // // // // 

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;

	MyString env_errors;
	if( !Starter->GetJobEnv(JobAd,&job_env,&env_errors) ) {
		dprintf( D_ALWAYS, "Aborting OSProc::StartJob: %s\n",
				 env_errors.Value());
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

		// if we're in PrivSep mode, we won't necessarily be able to
		// open the files for the job. getStdFile will return us an
		// open FD in some situations, but otherwise will give us
		// a filename that we'll pass to the PrivSep Switchboard
		//
	bool stdin_ok;
	bool stdout_ok;
	bool stderr_ok;
	MyString privsep_stdin_name;
	MyString privsep_stdout_name;
	MyString privsep_stderr_name;
	if (privsep_helper != NULL) {
		stdin_ok = getStdFile(SFT_IN,
		                      NULL,
		                      true,
		                      "Input file",
		                      &fds[0],
		                      &privsep_stdin_name);
		stdout_ok = getStdFile(SFT_OUT,
		                       NULL,
		                       true,
		                       "Output file",
		                       &fds[1],
		                       &privsep_stdout_name);
		stderr_ok = getStdFile(SFT_ERR,
		                       NULL,
		                       true,
		                       "Error file",
		                       &fds[2],
		                       &privsep_stderr_name);
	}
	else {
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
	}

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
		MyString reniceAttr = "Renice = ";
		reniceAttr += ptmp;
		if( !JobAd->Insert( reniceAttr.Value() ) ) {
			dprintf( D_ALWAYS, "ERROR: failed to insert JOB_RENICE_INCREMENT "
				"into job ad, Aborting OsProc::StartJob...\n" );
			free( ptmp );
			return 0;
		}
			// evaluate
		if( JobAd->EvalInteger( "Renice", NULL, nice_inc ) ) {
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
			// if JOB_RENICE_INCREMENT is undefined, default to 10
		nice_inc = 10;
	}

		// in the below dprintfs, we want to skip past argv[0], which
		// is sometimes condor_exec, in the Args string. 

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string, 1);
	if( has_wrapper ) { 
			// print out exactly what we're doing so folks can debug
			// it, if they need to.
		dprintf( D_ALWAYS, "Using wrapper %s to exec %s\n", JobName.Value(), 
				 args_string.Value() );

		MyString wrapper_err;
		wrapper_err.formatstr("%s%c%s", Starter->GetWorkingDir(),
				 	DIR_DELIM_CHAR,
					JOB_WRAPPER_FAILURE_FILE);
		if( ! job_env.SetEnv("_CONDOR_WRAPPER_ERROR_FILE", wrapper_err.Value()) ) {
			dprintf( D_ALWAYS, "Failed to set _CONDOR_WRAPPER_ERROR_FILE environment variable\n");
		}
	} else {
		dprintf( D_ALWAYS, "About to exec %s %s\n", JobName.Value(),
				 args_string.Value() );
	}

	MyString path;
	path.formatstr("%s%c%s", Starter->GetWorkingDir(),
			 	DIR_DELIM_CHAR,
				MACHINE_AD_FILENAME);
	if( ! job_env.SetEnv("_CONDOR_MACHINE_AD", path.Value()) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_MACHINE_AD environment variable\n");
	}

	if( Starter->jic->wroteChirpConfig() && (! job_env.SetEnv("_CONDOR_CHIRP_CONFIG", Starter->jic->chirpConfigFilename().c_str())) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_CHIRP_CONFIG environment variable.\n");
	}

	path.formatstr("%s%c%s", Starter->GetWorkingDir(),
			 	DIR_DELIM_CHAR,
				JOB_AD_FILENAME);
	if( ! job_env.SetEnv("_CONDOR_JOB_AD", path.Value()) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_JOB_AD environment variable\n");
	}

	std::string remoteUpdate;
	param(remoteUpdate, "REMOTE_UPDATE_PREFIX", "CHIRP");
	if( ! job_env.SetEnv("_CONDOR_REMOTE_UPDATE_PREFIX", remoteUpdate) ) {
		dprintf( D_ALWAYS, "Failed to set _CONDOR_REMOTE_UPDATE_PREFIX environment variable\n");
	}

		// Grab the full environment back out of the Env object 
	if(IsFulldebug(D_FULLDEBUG)) {
		MyString env_string;
		job_env.getDelimitedStringForDisplay(&env_string);
		dprintf(D_FULLDEBUG, "Env = %s\n", env_string.Value());
	}

	// Check to see if we need to start this process paused, and if
	// so, pass the right flag to DC::Create_Process().
	int job_opt_mask = DCJOBOPT_NO_CONDOR_ENV_INHERIT;
	if (!param_boolean("JOB_INHERITS_STARTER_ENVIRONMENT",false)) {
		job_opt_mask |= DCJOBOPT_NO_ENV_INHERIT;
	}
	int suspend_job_at_exec = 0;
	JobAd->LookupBool( ATTR_SUSPEND_JOB_AT_EXEC, suspend_job_at_exec);
	if( suspend_job_at_exec ) {
		dprintf( D_FULLDEBUG, "OsProc::StartJob(): "
				 "Job wants to be suspended at exec\n" );
		job_opt_mask |= DCJOBOPT_SUSPEND_ON_EXEC;
	}

	// If there is a requested coresize for this job, enforce it.
	// It is truncated because you can't put an unsigned integer
	// into a classad. I could rewrite condor's use of ATTR_CORE_SIZE to
	// be a float, but then when that attribute is read/written to the
	// job queue log by/or shared between versions of Condor which view the
	// type of that attribute differently, calamity would arise.
	int core_size_truncated;
	size_t core_size;
	size_t *core_size_ptr = NULL;
	if ( JobAd->LookupInteger( ATTR_CORE_SIZE, core_size_truncated ) ) {
		core_size = (size_t)core_size_truncated;
		core_size_ptr = &core_size;
	}

	long rlimit_as_hard_limit = 0;
	char *rlimit_expr = param("STARTER_RLIMIT_AS");
	if (rlimit_expr) {
		classad::ClassAdParser parser;

		classad::ExprTree *tree = parser.ParseExpression(rlimit_expr);
		if (tree) {
			classad::Value val;
			long long result;

			if (EvalExprTree(tree, Starter->jic->machClassAd(), JobAd, val) && 
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
					dprintf(D_ALWAYS, "Setting job's virtual memory rlimit to %ld megabytes\n", rlimit_as_hard_limit);
			} else {
				dprintf(D_ALWAYS, "Can't evaluate STARTER_RLIMIT_AS expression %s\n", rlimit_expr);
			}
		} else {
			dprintf(D_ALWAYS, "Can't parse STARTER_RLIMIT_AS expression: %s\n", rlimit_expr);
		}
	}

	int *affinity_mask = makeCpuAffinityMask(Starter->getMySlotNumber());

#if defined ( WIN32 )
    owner_profile_.update ();
    /*************************************************************
    NOTE: We currently *ONLY* support loading slot-user profiles.
    This limitation will be addressed shortly, by allowing regular 
    users to load their registry hive - Ben [2008-09-31]
    **************************************************************/
    bool load_profile = false,
         run_as_owner = false;
    JobAd->LookupBool ( ATTR_JOB_LOAD_PROFILE, load_profile );
    JobAd->LookupBool ( ATTR_JOB_RUNAS_OWNER,  run_as_owner );
    if ( load_profile && !run_as_owner ) {
        if ( owner_profile_.load () ) {
            /* publish the users environment into that of the main 

            job's environment */
            if ( !owner_profile_.environment ( job_env ) ) {
                dprintf ( D_ALWAYS, "OsProc::StartJob(): Failed to "
                    "export owner's environment.\n" );
            }            
        } else {
            dprintf ( D_ALWAYS, "OsProc::StartJob(): Failed to load "
                "owner's profile.\n" );
        }
    }
#endif

		// While we are still in user priv, print out the username
#if defined(LINUX)
	if( Starter->glexecPrivSepHelper() ) {
			// TODO: if there is some way to figure out the final username,
			// print it out here or after starting the job.
		dprintf(D_ALWAYS,"Running job via glexec\n");
	}
#else
	if( false ) {
	}
#endif
	else {
		char const *username = NULL;
		char const *how = "";
		CondorPrivSepHelper* cpsh = Starter->condorPrivSepHelper();
		if( cpsh ) {
			username = cpsh->get_user_name();
			how = "via privsep switchboard ";
		}
		else {
			username = get_user_loginname();
		}
		if( !username ) {
			username = "(null)";
		}
		dprintf(D_ALWAYS,"Running job %sas user %s\n",how,username);
	}

	set_priv ( priv );

    // use this to return more detailed and reliable error message info
    // from create-process operation.
    MyString create_process_err_msg;

	if (privsep_helper != NULL) {
		const char* std_file_names[3] = {
			privsep_stdin_name.Value(),
			privsep_stdout_name.Value(),
			privsep_stderr_name.Value()
		};
		JobPid = privsep_helper->create_process(JobName.Value(),
		                                        args,
		                                        job_env,
		                                        job_iwd,
		                                        fds,
		                                        std_file_names,
		                                        nice_inc,
		                                        core_size_ptr,
		                                        1,
		                                        job_opt_mask,
		                                        family_info,
												affinity_mask,
												&create_process_err_msg);
	}
	else {
		JobPid = daemonCore->Create_Process( JobName.Value(),
		                                     args,
		                                     PRIV_USER_FINAL,
		                                     1,
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
                                             &create_process_err_msg,
                                             fs_remap,
											 rlimit_as_hard_limit);
	}

	// Create_Process() saves the errno for us if it is an "interesting" error.
	int create_process_errno = errno;

    // errno is 0 in the privsep case.  This executes for the daemon core create-process logic
    if ((FALSE == JobPid) && (0 != create_process_errno)) {
        if (create_process_err_msg != "") create_process_err_msg += " ";
        MyString errbuf;
        errbuf.formatstr("(errno=%d: '%s')", create_process_errno, strerror(create_process_errno));
        create_process_err_msg += errbuf;
    }

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

		if(!create_process_err_msg.IsEmpty()) {

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

			MyString err_msg = "Failed to execute '";
			err_msg += JobName;
			err_msg += "'";
			if(!args_string.IsEmpty()) {
				err_msg += " with arguments ";
				err_msg += args_string.Value();
			}
			err_msg += ": ";
			err_msg += create_process_err_msg;
			if( !ThisProcRunsAlongsideMainProc() ) {
				Starter->jic->notifyStarterError( err_msg.Value(),
			    	                              true,
			        	                          CONDOR_HOLD_CODE_FailedToCreateProcess,
			            	                      create_process_errno );
			}
		}

		dprintf(D_ALWAYS,"Create_Process(%s,%s, ...) failed: %s\n",
			JobName.Value(), args_string.Value(), create_process_err_msg.Value());
		return 0;
	}

	num_pids++;

	dprintf(D_ALWAYS,"Create_Process succeeded, pid=%d\n",JobPid);

	job_start_time.getTime();

	return 1;
}


bool
OsProc::JobReaper( int pid, int status )
{
	dprintf( D_FULLDEBUG, "Inside OsProc::JobReaper()\n" );

	if (JobPid == pid) {
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
			reason = JOB_NOT_CKPTED;
		}
	} else if( dumped_core ) {
		reason = JOB_COREDUMPED;
	} else {
		reason = JOB_EXITED;
	}

#if defined ( WIN32 )
    
    /* If we loaded the user's profile, then we should dump it now */
    if ( owner_profile_.loaded () ) {
        owner_profile_.unload ();
        
        /* !!!! DO NOT DO THIS IN THE FUTURE !!!! */
        owner_profile_.destroy ();
        /* !!!! DO NOT DO THIS IN THE FUTURE !!!! */
        
    }

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
	// we must act differently depending on whether PrivSep is enabled.
	// if it's not, we rename any core file we find to
	// core.<cluster>.<proc>. if PrivSep is enabled, we may not have the
	// permissions to do the rename, so we don't. in either case, we
	// add the core file if present to the list of output files
	// (that happens in renameCoreFile for the non-PrivSep case)

	// Since Linux now writes out "core.pid" by default, we should
	// search for that.  Try the JobPid of our first child:
	MyString name_with_pid;
	name_with_pid.formatstr( "core.%d", JobPid );

	if (Starter->privSepHelper() != NULL) {

#if defined(WIN32)
		EXCEPT("PrivSep not yet available on Windows");
#else
		struct stat stat_buf;
		if (stat(name_with_pid.Value(), &stat_buf) != -1) {
			Starter->jic->addToOutputFiles(name_with_pid.Value());
		}
		else if (stat("core", &stat_buf) != -1) {
			Starter->jic->addToOutputFiles("core");
		}
#endif
	}
	else {
		MyString new_name;
		new_name.formatstr( "core.%d.%d",
		                  Starter->jic->jobCluster(), 
		                  Starter->jic->jobProc() );

		if( renameCoreFile(name_with_pid.Value(), new_name.Value()) ) {
			// great, we found it, renameCoreFile() took care of
			// everything we need to do... we're done.
			return;
		}

		// Now, just see if there's a file called "core"
		if( renameCoreFile("core", new_name.Value()) ) {
			return;
		}
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

	MyString old_full;
	MyString new_full;
	const char* job_iwd = Starter->jic->jobIWD();
	old_full.formatstr( "%s%c%s", job_iwd, DIR_DELIM_CHAR, old_name );
	new_full.formatstr( "%s%c%s", job_iwd, DIR_DELIM_CHAR, new_name );

	priv_state old_priv;

		// we need to do this rename as the user...
	errno = 0;
	old_priv = set_user_priv();
	int ret = rename(old_full.Value(), new_full.Value());
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
				 old_full.Value(), new_full.Value(), t_errno,
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
		daemonCore->Send_Signal(JobPid, soft_kill_sig);
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
	dprintf( D_FULLDEBUG, "Inside OsProc::PublishUpdateAd()\n" );
	MyString buf;

	if (m_proc_exited) {
		buf.formatstr( "%s=\"Exited\"", ATTR_JOB_STATE );
	} else if( is_checkpointed ) {
		buf.formatstr( "%s=\"Checkpointed\"", ATTR_JOB_STATE );
	} else if( is_suspended ) {
		buf.formatstr( "%s=\"Suspended\"", ATTR_JOB_STATE );
	} else {
		buf.formatstr( "%s=\"Running\"", ATTR_JOB_STATE );
	}
	ad->Insert( buf.Value() );

	buf.formatstr( "%s=%d", ATTR_NUM_PIDS, num_pids );
	ad->Insert( buf.Value() );

	if (m_proc_exited) {
		if( dumped_core ) {
			buf.formatstr( "%s = True", ATTR_JOB_CORE_DUMPED );
			ad->Insert( buf.Value() );
		} // should we put in ATTR_JOB_CORE_DUMPED = false if not?
	}

	return UserProc::PublishUpdateAd( ad );
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
		MyString affinityParam;
		affinityParam.formatstr("SLOT%d_CPU_AFFINITY", slotId);
		affinityParamResult = param(affinityParam.Value());
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

	StringList cpus(affinityParamResult);

	if (cpus.number() < 1) {
		dprintf(D_ALWAYS, "Could not parse affinity string %s, not setting affinity\n", affinityParamResult);
		free(affinityParamResult);
		return NULL;
	}

	int *mask = (int *) malloc(sizeof(int) * (cpus.number() + 1));
	if ( ! mask)
		return mask;

	mask[0] = cpus.number() + 1;
	cpus.rewind();
	char *cpu;
	int index = 1;
	while ((cpu = cpus.next())) {
		mask[index++] = atoi(cpu);
	}

	free(affinityParamResult);
	return mask;
}
