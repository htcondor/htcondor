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
	JobPid = Cluster = Proc = -1;
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

	if (JobAd->LookupInteger(ATTR_CLUSTER_ID, Cluster) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				ATTR_CLUSTER_ID);
		return 0;
	}

	if (JobAd->LookupInteger(ATTR_PROC_ID, Proc) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				ATTR_PROC_ID);
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

		// Support USER_JOB_WRAPPER parameter...

		// First, put "condor_exec" at the front of Args, since that
		// will become argv[0] of what we exec(), either the wrapper
		// or the actual job.
	strcpy( Args, CONDOR_EXEC );

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
	if( JobAd->LookupString(ATTR_JOB_ENVIRONMENT, &env_str) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  "
				 "Aborting OsProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
		return 0;
	}
		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;
	if( ! job_env.Merge(env_str) ) {
		dprintf( D_ALWAYS, "Invalid %s found in JobAd.  "
				 "Aborting OsProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
		return 0;
	}
		// Next, we can free the string we got back from
		// LookupString() so we don't leak any memory.
	free( env_str );

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

	if (JobAd->LookupString(ATTR_JOB_IWD, &job_iwd) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  "
				"Aborting DC_StartCondorJob.\n", ATTR_JOB_IWD);
		return 0;
	}

	// handle stdin, stdout, and stderr redirection
	int fds[3];
	int failedStdin, failedStdout, failedStderr;
	fds[0] = -1; fds[1] = -1; fds[2] = -1;
	failedStdin = 0; failedStdout = 0; failedStderr = 0;
	char filename1[_POSIX_PATH_MAX];
	char *filename;
	char infile[_POSIX_PATH_MAX];
	char outfile[_POSIX_PATH_MAX];
	char errfile[_POSIX_PATH_MAX];

		// in order to open these files we must have the user's privs:
	priv_state priv;
	priv = set_user_priv();

	if (JobAd->LookupString(ATTR_JOB_INPUT, filename1) == 1) {
		if ( !nullFile(filename1) ) {
			if( Starter->wantsFileTransfer() ) {
				filename = basename( filename1 );
			} else {
				filename = filename1;
			}
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( infile, "%s%c", job_iwd, DIR_DELIM_CHAR );
            } else {
                infile[0] = '\0';
            }
			strcat ( infile, filename );
			if ( (fds[0]=open( infile, O_RDONLY ) ) < 0 ) {
				dprintf(D_ALWAYS,"failed to open stdin file %s, errno %d\n",
						infile, errno);
				failedStdin = 1;
			}
		dprintf ( D_ALWAYS, "Input file: %s\n", infile );
		}
	} else {
	#ifndef WIN32
		if ( (fds[0]=open( "/dev/null", O_RDONLY ) ) < 0 ) {
			dprintf(D_ALWAYS, "failed to open stdin file /dev/null, errno %d\n",
				errno);
			failedStdin = 1;
		}
	#endif
	}

	if (JobAd->LookupString(ATTR_JOB_OUTPUT, filename1) == 1) {
		if ( !nullFile(filename1) ) {
			if( Starter->wantsFileTransfer() ) {
				filename = basename( filename1 );
			} else {
				filename = filename1;
			}
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( outfile, "%s%c", job_iwd, DIR_DELIM_CHAR );
            } else {
                outfile[0] = '\0';
            }
			strcat ( outfile, filename );
			if ((fds[1]=open(outfile,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
					// if failed, try again without O_TRUNC
				if ( (fds[1]=open( outfile, O_WRONLY | O_CREAT, 0666)) < 0 ) {
					dprintf(D_ALWAYS,
							"failed to open stdout file %s, errno %d\n",
							outfile, errno);
					failedStdout = 1;
				}
			}
			dprintf ( D_ALWAYS, "Output file: %s\n", outfile );
		}
	} else {
	#ifndef WIN32
		if ((fds[1]=open("/dev/null",O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
			// if failed, try again without O_TRUNC
			if ( (fds[1]=open( "/dev/null", O_WRONLY | O_CREAT, 0666)) < 0 ) {
				dprintf(D_ALWAYS, 
					"failed to open stdout file /dev/null, errno %d\n", 
					 errno);
				failedStdout = 1;
			}
		}
	#endif
	}

	if (JobAd->LookupString(ATTR_JOB_ERROR, filename1) == 1) {
		if ( !nullFile(filename1) ) {
			if( Starter->wantsFileTransfer() ) {
				filename = basename( filename1 );
			} else {
				filename = filename1;
			}
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( errfile, "%s%c", job_iwd, DIR_DELIM_CHAR );
            } else {
                errfile[0] = '\0';
            }
			strcat ( errfile, filename );
			if ((fds[2]=open( errfile,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
					// if failed, try again without O_TRUNC
				if ((fds[2]=open( errfile,O_WRONLY|O_CREAT, 0666)) < 0 ) {
					dprintf(D_ALWAYS,
							"failed to open stderr file %s, errno %d\n",
							errfile, errno);
					failedStderr = 1;
				}
			}
			dprintf ( D_ALWAYS, "Error file: %s\n", errfile );
		}
	} else {
	#ifndef WIN32
		if ((fds[2]=open("/dev/null",O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
			// if failed, try again without O_TRUNC
			if ( (fds[2]=open( "/dev/null", O_WRONLY | O_CREAT, 0666)) < 0 ) {
				dprintf(D_ALWAYS, 
						"failed to open stderr file /dev/null, errno %d\n", 
						errno);
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

		// Notify the shadow we're about to exec.
	REMOTE_CONDOR_begin_execution();

	// handle JOB_RENICE_INCREMENT
	char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if ( ptmp ) {
		nice_inc = atoi(ptmp);
		free(ptmp);
	} else {
		nice_inc = 0;
	}

		// in the below dprintfs, we want to skip past argv[0], which
		// we know will always be condor_exec, in the Args string. 
	int skip = strlen(CONDOR_EXEC) + 1;
	if( has_wrapper ) { 
			// print out exactly what we're doing so folks can debug
			// it, if they need to.
		dprintf( D_ALWAYS, "Using wrapper %s to exec %s\n", JobName, 
				 &(Args[skip]) );
	} else {
		dprintf( D_ALWAYS, "About to exec %s %s\n", JobName,
				 &(Args[skip]) );
	}

		// Grap the full environment back out of the Env object 
	env_str = job_env.getDelimitedString();

	set_priv ( priv );

	JobPid = daemonCore->Create_Process(JobName, Args, PRIV_USER_FINAL, 1,
				   FALSE, env_str, job_iwd, TRUE, NULL, fds, nice_inc,
				   DCJOBOPT_NO_ENV_INHERIT );

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
	if( sig > 0 ) {
		soft_kill_sig = sig;
	} else {
		soft_kill_sig = SIGTERM;
	}

	sig = findRmKillSig( JobAd );
	if( sig > 0 ) {
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
	bool job_exit_wants_ad = true;

	dprintf( D_FULLDEBUG, "Inside OsProc::JobExit()\n" );

	if ( requested_exit == true ) {
		reason = JOB_NOT_CKPTED;
	} else if( dumped_core ) {
		reason = JOB_COREDUMPED;
	} else {
		reason = JOB_EXITED;
	}

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
	
	if ( job_exit_wants_ad ) {
		PublishUpdateAd( &ad );
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
OsProc::renameCoreFile( void )
{
	bool rval = false;

	priv_state old_priv;

	char buf[64];
	sprintf( buf, "core.%d.%d", Cluster, Proc );

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
		Starter->addToTransferOutputFiles( buf );
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

int 
nullFile(const char *filename)
{
	// On WinNT, /dev/null is NUL
	// on UNIX, /dev/null is /dev/null
	
	// a UNIX->NT submit will result in the NT starter seeing /dev/null, so it
	// needs to recognize that /dev/null is the null file

	// an NT->NT submit will result in the NT starter seeing NUL as the null 
	// file

	// a UNIX->UNIX submit ill result in the UNIX starter seeing /dev/null as
	// the null file
	
	// NT->UNIX submits are not worried about - we don't think that anyone can
	// do them, and to make it clean we'll fix submit to always use /dev/null,
	// in the job ad, even on NT. 

	#ifdef WIN32
	if(_stricmp(filename, "NUL") == 0) {
		return 1;
	}
	#endif
	if(strcmp(filename, "/dev/null") == 0 ) {
		return 1;
	}
	return 0;
}
