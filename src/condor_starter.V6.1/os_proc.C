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
#include "condor_distribution.h"
#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;


/* OsProc class implementation */

OsProc::OsProc( ClassAd* ad )
{
    dprintf ( D_FULLDEBUG, "In OsProc::OsProc()\n" );
	JobAd = ad;
	JobPid = -1;
	exit_status = -1;
	requested_exit = false;
	job_suspended = FALSE;
	num_pids = 0;
	dumped_core = false;
	job_iwd = NULL;
}

OsProc::~OsProc()
{
	if( job_iwd ) {
		free( job_iwd );
	}
}


int
OsProc::StartJob()
{
	int i;
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

	initKillSigs();

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
		int retval = chmod( JobName, S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf ( D_ALWAYS, "Failed to chmod %s!\n",JobName );
			return 0;
		}
	}

	char Args[_POSIX_ARG_MAX];
	char tmp[_POSIX_ARG_MAX];

	if (JobAd->LookupString(ATTR_JOB_ARGUMENTS, tmp) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  "
				"Aborting OsProc::StartJob.\n", ATTR_JOB_ARGUMENTS);
		return 0;
	}

		// First, put "condor_exec" at the front of Args, since that
		// will become argv[0] of what we exec(), either the wrapper
		// or the actual job.

		// The Java universe cannot tolerate an incorrect argv[0].
		// For Java, set it correctly.  In a future version, we
		// may consider removing the CONDOR_EXEC feature entirely.

	int universe;
	if ( JobAd->LookupInteger( ATTR_JOB_UNIVERSE, universe ) < 1 ) {
		universe = CONDOR_UNIVERSE_VANILLA;
	}

	if(universe==CONDOR_UNIVERSE_JAVA) {
		strcpy( Args, JobName );
	} else {
		strcpy( Args, CONDOR_EXEC );
	}

		// This variable is used to keep track of the position
		// of the arguments immediately following argv[0].

	int skip = strlen(Args)+1;

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
		strcat( Args, " " );
		strcat( Args, JobName );
		strcpy( JobName, wrapper );
		free(wrapper);
	} 
		// Either way, we now have to add the user-specified args as
		// the rest of the Args string.
	if ( tmp[0] != '\0' ) {
		strcat( Args, " " );
		strcat( Args, tmp );
	}

		// // // // // // 
		// Environment 
		// // // // // // 

	char* env_str = NULL;
	JobAd->LookupString( ATTR_JOB_ENVIRONMENT, &env_str );

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;
	if( env_str ) { 
		if( ! job_env.Merge(env_str) ) {
			dprintf( D_ALWAYS, "Invalid %s found in JobAd.  "
					 "Aborting OsProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
			return 0;
		}
			// Next, we can free the string we got back from
			// LookupString() so we don't leak any memory.
		free( env_str );
	}

	// Now, add some env vars the user job might want to see:
	char	envName[256];
	sprintf( envName, "%s_SCRATCH_DIR", myDistro->GetUc() );
	job_env.Put( envName, Starter->GetWorkingDir() );

		// Deal with port regulation stuff
	char* low = param( "LOWPORT" );
	char* high = param( "HIGHPORT" );
	if( low && high ) {
		sprintf( envName, "_%s_HIGHPORT", myDistro->Get() );
		job_env.Put( envName, high );
		sprintf( envName, "_%s_LOWPORT", myDistro->Get() );
		job_env.Put( envName, low );
		free( high );
		free( low );
	} else if( low ) {
		dprintf( D_ALWAYS, "LOWPORT is defined but HIGHPORT is not, "
				 "ignoring LOWPORT\n" );
		free( low );
	} else if( high ) {
		dprintf( D_ALWAYS, "HIGHPORT is defined but LOWPORT is not, "
				 "ignoring HIGHPORT\n" );
		free( high );
    }

		// // // // // // 
		// Standard Files
		// // // // // // 

	const char* job_iwd = Starter->jic->jobIWD();

	// handle stdin, stdout, and stderr redirection
	int fds[3];
	int failedStdin, failedStdout, failedStderr;
	fds[0] = -1; fds[1] = -1; fds[2] = -1;
	failedStdin = 0; failedStdout = 0; failedStderr = 0;
	const char* filename = NULL;

		// in order to open these files we must have the user's privs:
	priv_state priv;
	priv = set_user_priv();

	filename = Starter->jic->jobInputFilename();
	if( filename ) {
		if( (fds[0]=open(filename, O_RDONLY)) < 0 ) {
			failedStdin = 1;
			char const *errno_str = strerror( errno );
			MyString err_msg;
			err_msg = "Failed to open standard input file '";
			err_msg += filename;
			err_msg += "': ";
			err_msg += errno_str;
			err_msg += " (errno ";
			err_msg += errno;
			err_msg += ')';
			dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}
		dprintf( D_ALWAYS, "Input file: %s\n", filename );
	} else { 
	#ifndef WIN32
		if( (fds[0]=open("/dev/null", O_RDONLY) ) < 0 ) {
			dprintf( D_ALWAYS,
					 "failed to open stdin file /dev/null, errno %d\n",
					 errno );
			failedStdin = 1;
		}
	#endif
	}

	filename = Starter->jic->jobOutputFilename();
	if( filename ) {
		if( (fds[1]=open(filename,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
				// if failed, try again without O_TRUNC
			if( (fds[1]=open( filename, O_WRONLY|O_CREAT, 0666)) < 0 ) {
				failedStdout = 1;
				char const *errno_str = strerror( errno );
				MyString err_msg;
				err_msg = "Failed to open standard output file '";
				err_msg += filename;
				err_msg += "': ";
				err_msg += errno_str;
				err_msg += " (errno ";
				err_msg += errno;
				err_msg += ')';
				dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
				Starter->jic->notifyStarterError( err_msg.Value(), true );
			}
		}
		dprintf( D_ALWAYS, "Output file: %s\n", filename );
	} else {
    #ifndef WIN32
		if( (fds[1]=open("/dev/null",O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
				// if failed, try again without O_TRUNC
			if( (fds[1]=open( "/dev/null", O_WRONLY | O_CREAT, 0666)) < 0 ) {
				dprintf( D_ALWAYS, 
						 "failed to open stdout file /dev/null, errno %d\n", 
						 errno );
				failedStdout = 1;
			}
		}
    #endif
	}

	filename = Starter->jic->jobErrorFilename();
	if( filename ) {
		if( (fds[2]=open(filename,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
				// if failed, try again without O_TRUNC
			if( (fds[2]=open(filename,O_WRONLY|O_CREAT, 0666)) < 0 ) {
				failedStderr = 1;
				char const *errno_str = strerror( errno );
				MyString err_msg;
				err_msg = "Failed to open standard error file '";
				err_msg += filename;
				err_msg += "': ";
				err_msg += errno_str;
				err_msg += " (errno ";
				err_msg += errno;
				err_msg += ')';
				dprintf( D_ALWAYS, "%s\n", err_msg.Value() );
				Starter->jic->notifyStarterError( err_msg.Value(), true );
			}
		}
		dprintf ( D_ALWAYS, "Error file: %s\n", filename );
	} else {
	#ifndef WIN32
		if( (fds[2]=open("/dev/null",O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
			// if failed, try again without O_TRUNC
			if( (fds[2]=open( "/dev/null", O_WRONLY | O_CREAT, 0666)) < 0 ) {
				dprintf( D_ALWAYS, 
						 "failed to open stderr file /dev/null, errno %d\n", 
						 errno );
				failedStderr = 1;
			}
		}
	#endif
	}


	/* Bail out if we couldn't open the std files correctly */
	if ( failedStdin || failedStdout || failedStderr ) {
		/* only close ones that had been opened correctly */
		if (fds[0] != -1) {
			close(fds[0]);
		}
		if (fds[1] != -1) {
			close(fds[1]);
		}
		if (fds[2] != -1) {
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

	// handle JOB_RENICE_INCREMENT
	char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if ( ptmp ) {
		nice_inc = atoi(ptmp);
		free(ptmp);
	} else {
		nice_inc = 0;
	}

		// in the below dprintfs, we want to skip past argv[0], which
		// is sometimes condor_exec, in the Args string. 
		// We rely on the "skip" variable defined above when
		// argv[0] was set according to the universe and job name.

	if( has_wrapper ) { 
			// print out exactly what we're doing so folks can debug
			// it, if they need to.
		dprintf( D_ALWAYS, "Using wrapper %s to exec %s\n", JobName, 
				 &(Args[skip]) );
	} else {
		if (skip < (int)strlen(Args)){
			/* some arguments exist, so skip and print them out */
			dprintf( D_ALWAYS, "About to exec %s %s\n", JobName,
				 &(Args[skip]) );
		} else {
			/* no arguments exist, so just print out executable */
			dprintf( D_ALWAYS, "About to exec %s\n", JobName);
		}
	}

		// Grap the full environment back out of the Env object 
	env_str = job_env.getDelimitedString();

	set_priv ( priv );

	JobPid = daemonCore->Create_Process(JobName, Args, PRIV_USER_FINAL, 1,
				   FALSE, env_str, (char*)job_iwd, TRUE, NULL, fds, nice_inc,
				   DCJOBOPT_NO_ENV_INHERIT );

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = NULL;
	if(JobPid == FALSE && errno) create_process_error = strerror(errno);

	// now close the descriptors in fds array.  our child has inherited
	// them already, so we should close them so we do not leak descriptors.
	for (i=0;i<3;i++) {
		if ( fds[i] != -1 ) {
			close(fds[i]);
		}
	}

		// Free up memory we allocated so we don't leak.
	delete [] env_str;

	if ( JobPid == FALSE ) {
		JobPid = -1;

		if(create_process_error) {
			MyString err_msg = "Failed to execute '";
			err_msg += JobName;
			err_msg += ' ';
			err_msg += Args;
			err_msg += "': ";
			err_msg += create_process_error;
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}

		EXCEPT("Create_Process(%s,%s, ...) failed",
			JobName, Args );
		return 0;
	}

	num_pids++;

	dprintf(D_ALWAYS,"Create_Process succeeded, pid=%d\n",JobPid);

	return 1;
}


void
OsProc::initKillSigs( void )
{
	int sig;

	sig = findSoftKillSig( JobAd );
	if( sig >= 0 ) {
		soft_kill_sig = sig;
	} else {
		soft_kill_sig = SIGTERM;
	}

	sig = findRmKillSig( JobAd );
	if( sig >= 0 ) {
		rm_kill_sig = sig;
	} else {
		rm_kill_sig = SIGTERM;
	}

	const char* tmp = signalName( soft_kill_sig );
	dprintf( D_FULLDEBUG, "KillSignal: %d (%s)\n", soft_kill_sig, 
			 tmp ? tmp : "Unknown" );

	tmp = signalName( rm_kill_sig );
	dprintf( D_FULLDEBUG, "RmKillSignal: %d (%s)\n", rm_kill_sig, 
			 tmp ? tmp : "Unknown" );
}


int
OsProc::JobCleanup( int pid, int status )
{
	dprintf( D_FULLDEBUG, "Inside OsProc::JobCleanup()\n" );

	if( JobPid != pid ) {		
		return 0;
	}

		// save the exit status for future use.
	exit_status = status;

		// clear out num_pids... everything under this process should
		// now be gone
	num_pids = 0;

		// check to see if the job dropped a core file.  if so, we
		// want to rename that file so that it won't clobber other
		// core files back at the submit site.  
	if( renameCoreFile() ) {
		dumped_core = true;
	}

	return 1;
}


bool
OsProc::JobExit( void )
{
	int reason;	

	dprintf( D_FULLDEBUG, "Inside OsProc::JobExit()\n" );

	if ( requested_exit == true ) {
		reason = JOB_NOT_CKPTED;
	} else if( dumped_core ) {
		reason = JOB_COREDUMPED;
	} else {
		reason = JOB_EXITED;
	}

	return Starter->jic->notifyJobExit( exit_status, reason, this );
}


bool
OsProc::renameCoreFile( void )
{
	bool rval = false;

	priv_state old_priv;

	char buf[64];
	sprintf( buf, "core.%d.%d", Starter->jic->jobCluster(), 
			 Starter->jic->jobProc() );

	MyString old_name( job_iwd );
	MyString new_name( job_iwd );

	old_name += DIR_DELIM_CHAR;
	old_name += "core";

	new_name += DIR_DELIM_CHAR;
	new_name += buf;

		// we need to do this rename as the user...
	errno = 0;
	old_priv = set_user_priv();
	if( rename(old_name.Value(), new_name.Value()) >= 0 ) {
		rval = true;
	}
	set_priv( old_priv );

	if( rval ) {
			// make sure it'll get transfered back, too.
		Starter->jic->addToOutputFiles( buf );
		dprintf( D_FULLDEBUG, "Found core file, renamed to %s\n", buf );
	} else if( errno != ENOENT ) {
		dprintf( D_ALWAYS, "Failed to rename(%s,%s): errno %d (%s)\n",
				 old_name.Value(), new_name.Value(), errno,
				 strerror(errno) );
	}

	return rval;
}


void
OsProc::Suspend()
{
	daemonCore->Send_Signal(JobPid, SIGSTOP);
	job_suspended = TRUE;
}

void
OsProc::Continue()
{
	daemonCore->Send_Signal(JobPid, SIGCONT);
	job_suspended = FALSE;
}

bool
OsProc::ShutdownGraceful()
{
	if ( job_suspended == TRUE )
		Continue();

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
OsProc::PublishUpdateAd( ClassAd* ad ) 
{
	dprintf( D_FULLDEBUG, "Inside OsProc::PublishUpdateAd()\n" );
	char buf[128];

	if( exit_status >= 0 ) {
		sprintf( buf, "%s=\"Exited\"", ATTR_JOB_STATE );
	} else if( job_suspended ) {
		sprintf( buf, "%s=\"Suspended\"", ATTR_JOB_STATE );
	} else {
		sprintf( buf, "%s=\"Running\"", ATTR_JOB_STATE );
	}
	ad->Insert( buf );

	sprintf( buf, "%s=%d", ATTR_NUM_PIDS, num_pids );
	ad->Insert( buf );

	if( exit_status >= 0 ) {
			/*
			  If we have the exit status, we want to parse it and set
			  some attributes which describe the status in a platform
			  independent way.  This way, we're sure we're analyzing
			  the status integer with the platform-specific macros
			  where it came from, instead of assuming that WIFEXITED()
			  and friends will work correctly on a status integer we
			  got back from a different platform.
			*/
		if( WIFSIGNALED(exit_status) ) {
			sprintf( buf, "%s = TRUE", ATTR_ON_EXIT_BY_SIGNAL );
			ad->Insert( buf );
			sprintf( buf, "%s = %d", ATTR_ON_EXIT_SIGNAL, 
					 WTERMSIG(exit_status) );
			ad->Insert( buf );
			sprintf( buf, "%s = \"died on %s\"", ATTR_EXIT_REASON,
					 daemonCore->GetExceptionString(WTERMSIG(exit_status)) );
			ad->Insert( buf );
		} else {
			sprintf( buf, "%s = FALSE", ATTR_ON_EXIT_BY_SIGNAL );
			ad->Insert( buf );
			sprintf( buf, "%s = %d", ATTR_ON_EXIT_CODE, 
					 WEXITSTATUS(exit_status) );
			ad->Insert( buf );
		}
		if( dumped_core ) {
			sprintf( buf, "%s = True", ATTR_JOB_CORE_DUMPED );
			ad->Insert( buf );
		} // should we put in ATTR_JOB_CORE_DUMPED = false if not?
	}
	return true;
}
