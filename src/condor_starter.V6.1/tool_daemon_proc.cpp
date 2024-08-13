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
#include "tool_daemon_proc.h"
#include "starter.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_distribution.h"
#include "basename.h"
#ifdef WIN32
#include "perm.h"
#endif

extern class Starter *starter;

/* ToolDaemonProc class implementation */

ToolDaemonProc::ToolDaemonProc( ClassAd *jobAd, int application_pid )
{
    dprintf( D_FULLDEBUG, "In ToolDaemonProc::ToolDaemonProc()\n" );
    JobAd = jobAd;
    job_suspended = false;
	ApplicationPid = application_pid;

}

int
ToolDaemonProc::StartJob()
{
    int i;
    int nice_inc = 0;

    dprintf( D_FULLDEBUG, "in ToolDaemonProc::StartJob()\n" );

    if( !JobAd ) {
        dprintf( D_ALWAYS, "No JobAd in ToolDaemonProc::StartJob()!\n" );
		return 0;
    }

	std::string DaemonNameStr;
	char* tmp = NULL;
	if( JobAd->LookupString( ATTR_TOOL_DAEMON_CMD, &tmp ) != 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting "
				 "ToolDaemonProc::StartJob()\n", ATTR_TOOL_DAEMON_CMD );
	    return 0;
	}

	const char* job_iwd = starter->jic->jobIWD();
	dprintf( D_ALWAYS, "IWD: %s\n", job_iwd );

	const char* base = NULL;
	base = condor_basename( tmp );
	if( starter->jic->iwdIsChanged() ) {
		formatstr( DaemonNameStr, "%s%c%s", starter->GetWorkingDir(0),
							   DIR_DELIM_CHAR, base );
	} else if( ! fullpath(tmp) ) {
		formatstr( DaemonNameStr, "%s%c%s", job_iwd, DIR_DELIM_CHAR, tmp );
	} else {
		DaemonNameStr = tmp;
	}
	const char* DaemonName = DaemonNameStr.c_str();
	free( tmp );
	tmp = NULL;
			
	dprintf( D_FULLDEBUG, "Daemon Name: %s \n", DaemonName );

		// This is something of an ugly hack.  filetransfer doesn't
		// preserve file permissions when it moves a file.  so, our
		// tool "binary" (or script, whatever it is), is sitting in
		// the starter's directory without an execute bit set.  So,
		// we've got to call chmod() so that exec() doesn't fail.
	if( starter->jic->iwdIsChanged() ) {
		priv_state old_priv = set_user_priv();
		int retval = chmod( DaemonName, S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf( D_ALWAYS, "Failed to chmod %s!\n", DaemonName );
			return 0;
		}
	}

	// compute job's renice value by evaluating the machine's
	// JOB_RENICE_INCREMENT in the context of the job ad...

    char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if( ptmp ) {
			// insert renice expr into our copy of the job ad
		if( !JobAd->AssignExpr( "Renice", ptmp ) ) {
			dprintf( D_ALWAYS, "ERROR: failed to insert JOB_RENICE_INCREMENT "
				"into job ad, Aborting ToolDaemonProc::StartJob...\n" );
			free( ptmp );
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
			// if JOB_RENICE_INCREMENT is undefined, default to 10
		nice_inc = 10;
	}

		// // // // // // 
		// Arguments
		// // // // // // 

	ArgList DaemonArgs;
	ASSERT( tmp == NULL );

	DaemonArgs.AppendArg(DaemonName);

	JobAd->LookupString( ATTR_TOOL_DAEMON_ARGS2, &tmp );
	bool args_success = true;
	std::string args_error;
	if( tmp ) {
		args_success = DaemonArgs.AppendArgsV2Raw(tmp,args_error);
		dprintf( D_FULLDEBUG, "Daemon Args: %s\n", tmp ) ;
		free( tmp );
		tmp = NULL;
	}
	else {
		JobAd->LookupString( ATTR_TOOL_DAEMON_ARGS1, &tmp );
		if( tmp ) {
			args_success = DaemonArgs.AppendArgsV1Raw(tmp,args_error);
			dprintf( D_FULLDEBUG, "Daemon Args: %s\n", tmp ) ;
			free( tmp );
			tmp = NULL;
		}
	}

	if(!args_success) {
		dprintf(D_ALWAYS, "Aborting.  Failed to read daemon args: %s\n",
				args_error.c_str());
		return 0;
	}

		// // // // // // 
		// Environment 
		// // // // // // 

	Env job_env;
	std::string env_errors;
	if( !job_env.MergeFrom(JobAd, env_errors) ) {
		dprintf( D_ALWAYS, "Failed to read environment from JobAd.  Aborting "
				 "ToolDaemonProc::StartJob: %s\n",env_errors.c_str());
		return 0;
	}

	// for now, we pass "ENV" as the address of the LASS
	// this tells the tool that the job PID will be placed
	// in the environment variable, "TDP_AP_PID"
	job_env.SetEnv("TDP_LASS_ADDRESS", "ENV");
	char pid_buf[256];
	snprintf(pid_buf, sizeof(pid_buf), "%d", ApplicationPid);
	job_env.SetEnv("TDP_AP_PID", pid_buf);

		// Now, let the starter publish any env vars it wants to into
		// the mainjob's env...
	starter->PublishToEnv( &job_env );


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

	fds[0] = openStdFile( SFT_IN,
	                      ATTR_TOOL_DAEMON_INPUT,
	                      false,
	                      "Tool Daemon Input file");

	fds[1] = openStdFile( SFT_OUT,
	                      ATTR_TOOL_DAEMON_OUTPUT,
	                      false,
	                      "Tool Daemon Output file");

	fds[2] = openStdFile( SFT_ERR,
	                      ATTR_TOOL_DAEMON_ERROR,
	                      false,
	                      "Tool Daemon Error file");

	/* Bail out if we couldn't open the std files correctly */
	if( fds[0] == -1 || fds[1] == -1 || fds[2] == -1 ) {
			/* only close ones that had been opened correctly */
		for( int fdindex=0; fdindex<=2; fdindex++ ) {
			if( fds[fdindex] >= 0 ) {
				close(fds[fdindex]);
			}
		}
		dprintf(D_ALWAYS, "Failed to open some/all of the std files...\n");
		dprintf(D_ALWAYS, "Aborting ToolDaemonProc::StartJob.\n");
		set_priv(priv); /* go back to original priv state before leaving */
		return 0;
	}


		// // // // // // 
		// Misc + Exec
		// // // // // // 

	// set up the FamilyInfo structure we will be using to track the tool
	// daemon's process family
	//
	FamilyInfo fi;
	fi.max_snapshot_interval = 15;
	m_dedicated_account = NULL;
	if (job_universe != CONDOR_UNIVERSE_LOCAL) {
		m_dedicated_account = starter->jic->getExecuteAccountIsDedicated();
	}
	if (m_dedicated_account) {
		fi.login = m_dedicated_account;
		dprintf(D_FULLDEBUG,
		        "Tracking process family by login \"%s\"\n",
		        fi.login);
	}

	std::string args_string;
	DaemonArgs.GetArgsStringForDisplay(args_string);
	dprintf( D_ALWAYS, "About to exec %s\n", args_string.c_str() ); 

	set_priv( priv );

	JobPid = daemonCore->Create_Process( DaemonName,
	                                     DaemonArgs,
	                                     PRIV_USER_FINAL,
	                                     1,
	                                     FALSE,
	                                     FALSE,
	                                     &job_env,
	                                     job_iwd,
	                                     &fi,
	                                     NULL,
	                                     fds,
	                                     NULL,
	                                     nice_inc,
	                                     NULL,
	                                     DCJOBOPT_NO_ENV_INHERIT );

		//NOTE: Create_Process() saves the errno for us if it is an
		//"interesting" error.
	char const *create_process_error = NULL;
	int create_process_errno = errno;
	if(JobPid == FALSE && errno) create_process_error = strerror(errno);

		// now close the descriptors in daemon_fds array.  our child has inherited
		// them already, so we should close them so we do not leak descriptors.

	for (i=0;i<=2;i++) {
		if( fds[i] >= 0 ) {
			close( fds[i] );
		}
	}

	if( JobPid == FALSE ) {
		JobPid = -1;
		if( create_process_error ) {
			std::string err_msg;
			formatstr( err_msg, "Failed to execute '%s': %s",
							 args_string.c_str(), create_process_error );
			starter->jic->notifyStarterError( err_msg.c_str(), true, CONDOR_HOLD_CODE::FailedToCreateProcess, create_process_errno );
		}
		EXCEPT( "Create_Process(%s, ...) failed", args_string.c_str() );
		return FALSE;

	} else {
		dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid );

		condor_gettimestamp( job_start_time );

		return TRUE;
	}
}


bool
ToolDaemonProc::JobReaper(int pid, int status)
{
    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::JobReaper()\n" );

		// If the tool exited, we want to shutdown everything.
    if (JobPid == pid) {
		if (daemonCore->Kill_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error killing process family for job cleanup\n");
		}
    } 
    return UserProc::JobReaper(pid, status);
}


// We don't have to do anything special to notify a shadow that we've
// really exited.
bool
ToolDaemonProc::JobExit( void )
{
    return true;
}


void
ToolDaemonProc::Suspend()
{
		dprintf(D_FULLDEBUG,"in ToolDaemonProc::Suspend()\n");

		// suspend the tool daemon job
	if ( JobPid != -1 ) {
		if (daemonCore->Suspend_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error suspending process family\n");
		}
	}

        // set our flag
	job_suspended = true;
}

void
ToolDaemonProc::Continue()
{
	dprintf(D_FULLDEBUG,"in ToolDaemonProc::Continue()\n");
	
	// resume user job
	if ( JobPid != -1 && job_suspended ) {
		if (daemonCore->Continue_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS, "error continuing process family\n");
		}
	}

        // set our flag
	job_suspended = false;
}


bool
ToolDaemonProc::ShutdownGraceful()
{
  dprintf(D_FULLDEBUG,"in ToolDaemonProc::ShutdownGraceful()\n");

	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

    // WE USED TO...
    //
    // take a snapshot before we softkill the parent job process.
    // this helps ensure that if the parent exits without killing
    // the kids, our JobExit() handler will get em all.
    //
    // TODO: should we explicitly call out to the procd here to tell
    // it to take a snapshot???

	// now softkill the parent job process.
    if( job_suspended ) {
        Continue();
    }
//    requested_exit = true;
    daemonCore->Send_Signal( JobPid, soft_kill_sig );
    return false;	// return false says shutdown is pending	
}

bool
ToolDaemonProc::ShutdownFast()
{
  	dprintf(D_FULLDEBUG,"in ToolDaemonProc::ShutdownFast()\n");

	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}  

    // We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
//    requested_exit = true;

	if (daemonCore->Kill_Family(JobPid) == FALSE) {
		dprintf(D_ALWAYS,
		        "error killing process family for fast shutdown\n");
	}

    return false;	// return false says shutdown is pending
}


bool
ToolDaemonProc::Remove()
{
	if ( job_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, rm_kill_sig);
	return false;	// return false says shutdown is pending	
}


bool
ToolDaemonProc::Hold()
{
	if ( job_suspended ) {
		Continue();
	}
	requested_exit = true;
	daemonCore->Send_Signal(JobPid, hold_kill_sig);
	return false;	// return false says shutdown is pending	
}


bool
ToolDaemonProc::PublishUpdateAd( ClassAd* /*ad*/ ) 
{
    // Nothing special for us to do.
    return true;
}
