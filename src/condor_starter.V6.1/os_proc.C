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
#include "condor_classad.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "env.h"
#include "user_proc.h"
#include "os_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "classad_helpers.h"
#include "sig_name.h"
#include "exit.h"
#include "condor_uid.h"
#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;


/* OsProc class implementation */

OsProc::OsProc( ClassAd* ad )
{
    dprintf ( D_FULLDEBUG, "In OsProc::OsProc()\n" );
	JobAd = ad;
	is_suspended = false;
	num_pids = 0;
	dumped_core = false;
	UserProc::initialize();
}


OsProc::~OsProc()
{
}


int
OsProc::StartJob()
{
	int nice_inc = 0;
	bool has_wrapper = false;

	dprintf(D_FULLDEBUG,"in OsProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in OsProc::StartJob()!\n" );
		return 0;
	}

	char JobName[_POSIX_PATH_MAX];
	if ( JobAd->LookupString( ATTR_JOB_CMD, JobName ) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				 ATTR_JOB_CMD );
		return 0;
	}

		// // // // // // 
		// Arguments
		// // // // // // 

	// if name is condor_exec, we transferred it, so make certain
	// it is executable.
	if ( strcmp(CONDOR_EXEC,JobName) == 0 ) {
			// also, prepend the full path to this name so that we
			// don't have to rely on the PATH inside the
			// USER_JOB_WRAPPER or for exec().
		sprintf( JobName, "%s%c%s", Starter->GetWorkingDir(),
				 DIR_DELIM_CHAR, CONDOR_EXEC );

		priv_state old_priv = set_user_priv();
		int retval = chmod( JobName, 0755 );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf ( D_ALWAYS, "Failed to chmod %s!\n",JobName );
			return 0;
		}
	}

	if( Starter->isGridshell() ) {
			// if we're a gridshell, just try to chmod our job, since
			// globus probably transfered it for us and left it with
			// bad permissions...
		priv_state old_priv = set_user_priv();
		int retval = chmod( JobName, S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf ( D_ALWAYS, "Failed to chmod %s!\n", JobName );
			return 0;
		}
	} 

	ArgList args;

		// Since we may be adding to the argument list, we may need to deal
		// with platform-specific arg syntax in the user's args in order
		// to successfully merge them with the additional wrapper args.
	args.SetArgV1SyntaxToCurrentPlatform();

		// First, put "condor_exec" at the front of Args, since that
		// will become argv[0] of what we exec(), either the wrapper
		// or the actual job.

		// The Java universe cannot tolerate an incorrect argv[0].
		// For Java, set it correctly.  In a future version, we
		// may consider removing the CONDOR_EXEC feature entirely.
	if( job_universe==CONDOR_UNIVERSE_JAVA ) {
		args.AppendArg(JobName);
	} else {
		args.AppendArg(CONDOR_EXEC);
	}

		// Support USER_JOB_WRAPPER parameter...

	char *wrapper = NULL;
	if( (wrapper=param("USER_JOB_WRAPPER")) ) {
			// make certain this wrapper program exists and is executable
		if( access(wrapper,X_OK) < 0 ) {
			dprintf( D_ALWAYS, 
					 "Cannot find/execute USER_JOB_WRAPPER file %s\n",
					 wrapper );
			return 0;
		}
		has_wrapper = true;
			// Now, we've got a valid wrapper.  We want that to become
			// "JobName" so we exec it directly, and we want to put
			// what was the JobName (with the full path) as the first
			// argument to the wrapper
		args.AppendArg(JobName);
		strcpy( JobName, wrapper );
		free(wrapper);
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

	char *env_str = param( "STARTER_JOB_ENVIRONMENT" );

	MyString env_errors;
	if( ! job_env.MergeFromV1RawOrV2Quoted(env_str,&env_errors) ) {
		dprintf( D_ALWAYS, "Aborting OSProc::StartJob: "
				 "%s\nThe full value for STARTER_JOB_ENVIRONMENT: "
				 "%s\n",env_errors.Value(),env_str);
		free(env_str);
		return 0;
	}
	free(env_str);

	if(!job_env.MergeFrom(JobAd,&env_errors)) {
		dprintf( D_ALWAYS, "Invalid environment found in JobAd.  "
		         "Aborting OsProc::StartJob: %s\n",
		         env_errors.Value());
		return 0;
	}

		// Now, let the starter publish any env vars it wants to into
		// the mainjob's env...
	Starter->PublishToEnv( &job_env );


		// // // // // // 
		// Standard Files
		// // // // // // 

	const char* job_iwd = Starter->jic->jobRemoteIWD();
	dprintf( D_ALWAYS, "IWD: %s\n", job_iwd );

	// handle stdin, stdout, and stderr redirection
	int fds[3];
		// initialize these to -2 to mean they're not specified.
		// -1 will be treated as an error.
	fds[0] = -2; fds[1] = -2; fds[2] = -2;
	bool starter_stdin = false;
	bool starter_stdout = false;
	bool starter_stderr = false;

		// in order to open these files we must have the user's privs:
	priv_state priv;
	priv = set_user_priv();

	fds[0] = openStdFile( SFT_IN, NULL, true, starter_stdin,
						  "Input file" );

	fds[1] = openStdFile( SFT_OUT, NULL, true, starter_stdout,
						  "Output file" );

	fds[2] = openStdFile( SFT_ERR, NULL, true, starter_stderr,
						  "Error file" );

	/* Bail out if we couldn't open the std files correctly */
	if( fds[0] == -1 || fds[1] == -1 || fds[2] == -1 ) {
			/* only close ones that had been opened correctly */
		if( fds[0] >= 0 && !starter_stdin ) {
			close(fds[0]);
		}
		if( fds[1] >= 0 && !starter_stdout ) {
			close(fds[1]);
		}
		if( fds[2] >= 0 && !starter_stderr ) {
			close(fds[2]);
		}
		dprintf(D_ALWAYS, "Failed to open some/all of the std files...\n");
		dprintf(D_ALWAYS, "Aborting OsProc::StartJob.\n");
		set_priv(priv); /* go back to original priv state before leaving */
		return 0;
	}

		// // // // // // 
		// Misc + Exec
		// // // // // // 

	Starter->jic->notifyJobPreSpawn();

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
	int start_arg = has_wrapper ? 2 : 1;
	args.GetArgsStringForDisplay(&args_string,start_arg);
	if( has_wrapper ) { 
			// print out exactly what we're doing so folks can debug
			// it, if they need to.
		dprintf( D_ALWAYS, "Using wrapper %s to exec %s\n", JobName, 
				 args_string.Value() );
	} else {
		dprintf( D_ALWAYS, "About to exec %s %s\n", JobName,
				 args_string.Value() );
	}

		// Grab the full environment back out of the Env object 
	if(DebugFlags & D_FULLDEBUG) {
		MyString env_str;
		job_env.getDelimitedStringForDisplay(&env_str);
		dprintf(D_FULLDEBUG, "Env = %s\n", env_str.Value());
	}

	// Check to see if we need to start this process paused, and if
	// so, pass the right flag to DC::Create_Process().
	int job_opt_mask = DCJOBOPT_NO_ENV_INHERIT;
	int suspend_job_at_exec = 0;
	JobAd->LookupBool( ATTR_SUSPEND_JOB_AT_EXEC, suspend_job_at_exec);
	if( suspend_job_at_exec ) {
		dprintf( D_FULLDEBUG, "OsProc::StartJob(): "
				 "Job wants to be suspended at exec\n" );
		job_opt_mask |= DCJOBOPT_SUSPEND_ON_EXEC;
	}

	set_priv ( priv );

	JobPid = daemonCore->Create_Process( JobName, args, PRIV_USER_FINAL,
					     1, FALSE, &job_env, job_iwd, TRUE,
					     NULL, fds, nice_inc, job_opt_mask );

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = NULL;
	if(JobPid == FALSE && errno) create_process_error = strerror(errno);

	// now close the descriptors in fds array.  our child has inherited
	// them already, so we should close them so we do not leak descriptors.
	// NOTE, we want to use a special method to close the starter's
	// versions, if that's what we're using, so we don't think we've
	// still got those available in other parts of the code for any
	// reason.
	if( starter_stdin ) {
		Starter->closeSavedStdin();
	} else if ( fds[0] != -1 ) {
		close(fds[0]);
	}
	if( starter_stdout ) {
		Starter->closeSavedStdout();
	} else if ( fds[1] != -1 ) {
		close(fds[1]);
	}
	if( starter_stderr ) {
		Starter->closeSavedStderr();
	} else if ( fds[2] != -1 ) {
		close(fds[2]);
	}

	if ( JobPid == FALSE ) {
		JobPid = -1;

		if(create_process_error) {
			MyString err_msg = "Failed to execute '";
			err_msg += JobName;
			err_msg += ' ';
			err_msg += args_string.Value();
			err_msg += "': ";
			err_msg += create_process_error;
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}

		EXCEPT("Create_Process(%s,%s, ...) failed",
			JobName, args_string.Value() );
		return 0;
	}

	num_pids++;

	dprintf(D_ALWAYS,"Create_Process succeeded, pid=%d\n",JobPid);

	job_start_time.getTime();

	return 1;
}


int
OsProc::JobCleanup( int pid, int status )
{
	dprintf( D_FULLDEBUG, "Inside OsProc::JobCleanup()\n" );

	if( JobPid != pid ) {		
		return 0;
	}

	job_exit_time.getTime();

		// save the exit status for future use.
	exit_status = status;

		// clear out num_pids... everything under this process should
		// now be gone
	num_pids = 0;

		// check to see if the job dropped a core file.  if so, we
		// want to rename that file so that it won't clobber other
		// core files back at the submit site.  
	checkCoreFile();

	return 1;
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

	return Starter->jic->notifyJobExit( exit_status, reason, this );
}



void
OsProc::checkCoreFile( void )
{
	MyString new_name;
	new_name.sprintf( "core.%d.%d", Starter->jic->jobCluster(), 
					  Starter->jic->jobProc() );

		// Since Linux now writes out "core.pid" by default, we should
		// search for that.  Try the JobPid of our first child:
	MyString old_name;
	old_name.sprintf( "core.%d", JobPid );
	if( renameCoreFile(old_name.Value(), new_name.Value()) ) {
			// great, we found it, renameCoreFile() took care of
			// everything we need to do... we're done.
		return;
	}

		// Now, just see if there's a file called "core"
	if( renameCoreFile("core", new_name.Value()) ) {
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

	MyString old_full;
	MyString new_full;
	const char* job_iwd = Starter->jic->jobIWD();
	old_full.sprintf( "%s%c%s", job_iwd, DIR_DELIM_CHAR, old_name );
	new_full.sprintf( "%s%c%s", job_iwd, DIR_DELIM_CHAR, new_name );

	priv_state old_priv;

		// we need to do this rename as the user...
	errno = 0;
	old_priv = set_user_priv();
	if( rename(old_full.Value(), new_full.Value()) != 0 ) {
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
	daemonCore->Send_Signal(JobPid, SIGCONT);
	is_suspended = false;
}

bool
OsProc::ShutdownGraceful()
{
	if ( is_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, soft_kill_sig);
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

	if( exit_status >= 0 ) {
		buf.sprintf( "%s=\"Exited\"", ATTR_JOB_STATE );
	} else if( is_suspended ) {
		buf.sprintf( "%s=\"Suspended\"", ATTR_JOB_STATE );
	} else {
		buf.sprintf( "%s=\"Running\"", ATTR_JOB_STATE );
	}
	ad->Insert( buf.Value() );

	buf.sprintf( "%s=%d", ATTR_NUM_PIDS, num_pids );
	ad->Insert( buf.Value() );

	if( exit_status >= 0 ) {
		if( dumped_core ) {
			buf.sprintf( "%s = True", ATTR_JOB_CORE_DUMPED );
			ad->Insert( buf.Value() );
		} // should we put in ATTR_JOB_CORE_DUMPED = false if not?
	}

	return UserProc::PublishUpdateAd( ad );
}
