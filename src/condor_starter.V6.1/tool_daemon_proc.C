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
#include "tool_daemon_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_uid.h"
#include "condor_distribution.h"
#include "basename.h"
#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;

/* ToolDaemonProc class implementation */

ToolDaemonProc::ToolDaemonProc( ClassAd *jobAd, int application_pid )
{
    dprintf( D_FULLDEBUG, "In ToolDaemonProc::ToolDaemonProc()\n" );
	family = NULL;
    JobAd = jobAd;
    job_suspended = false;
	ApplicationPid = application_pid;
	snapshot_tid = -1;

}


ToolDaemonProc::~ToolDaemonProc()
{
	if (family) {
		delete family;
	}
	if ( snapshot_tid != -1 ) {
		daemonCore->Cancel_Timer(snapshot_tid);
		snapshot_tid = -1;
	}
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

	MyString DaemonNameStr;
	char* tmp = NULL;
	if( JobAd->LookupString( ATTR_TOOL_DAEMON_CMD, &tmp ) != 1 ) {
	    dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting "
				 "ToolDaemonProc::StartJob()\n", ATTR_TOOL_DAEMON_CMD );
	    return 0;
	}

	const char* job_iwd = Starter->jic->jobIWD();
	dprintf( D_ALWAYS, "IWD: %s\n", job_iwd );

	char* base = NULL;
	base = basename( tmp );
	if( Starter->jic->iwdIsChanged() ) {
		DaemonNameStr.sprintf( "%s%c%s", Starter->GetWorkingDir(),
							   DIR_DELIM_CHAR, base );
	} else if( ! fullpath(tmp) ) {
		DaemonNameStr.sprintf( "%s%c%s", job_iwd, DIR_DELIM_CHAR, tmp );
	} else {
		DaemonNameStr = tmp;
	}
	const char* DaemonName = DaemonNameStr.Value();
	free( tmp );
	tmp = NULL;
			
	dprintf( D_FULLDEBUG, "Daemon Name: %s \n", DaemonName );

		// This is something of an ugly hack.  filetransfer doesn't
		// preserve file permissions when it moves a file.  so, our
		// tool "binary" (or script, whatever it is), is sitting in
		// the starter's directory without an execute bit set.  So,
		// we've got to call chmod() so that exec() doesn't fail.
	if( Starter->jic->iwdIsChanged() ) {
		priv_state old_priv = set_user_priv();
		int retval = chmod( DaemonName, S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf( D_ALWAYS, "Failed to chmod %s!\n", DaemonName );
			return 0;
		}
	}


		// // // // // // 
		// Arguments
		// // // // // // 

	char DaemonArgs[_POSIX_ARG_MAX];
	ASSERT( tmp == NULL );

	JobAd->LookupString( ATTR_TOOL_DAEMON_ARGS, &tmp );
	if( tmp ) {
		snprintf( DaemonArgs, _POSIX_ARG_MAX, "%s %s", DaemonName, tmp );
		dprintf( D_FULLDEBUG, "Daemon Args: %s\n", tmp ) ;
		free( tmp );
		tmp = NULL;
	} else {
		snprintf( DaemonArgs, _POSIX_ARG_MAX, "%s", DaemonName );
	}


		// // // // // // 
		// Environment 
		// // // // // // 

	char* env_str = NULL;
	if( JobAd->LookupString(ATTR_JOB_ENVIRONMENT, &env_str) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting "
				 "ToolDaemonProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
		return 0;
	}

		// Now, instantiate an Env object so we can manipulate the
		// environment as needed.
	Env job_env;

	if( ! job_env.Merge(env_str) ) {
		dprintf( D_ALWAYS, "Invalid %s found in JobAd.  Aborting "
				 "ToolDaemonProc::StartJob.\n", ATTR_JOB_ENVIRONMENT );  
		return 0;
	}
		// Next, we can free the string we got back from
		// LookupString() so we don't leak any memory.
	free( env_str );

	// for now, we pass "ENV" as the address of the LASS
	// this tells the tool that the job PID will be placed
	// in the environment variable, "TDP_AP_PID"
	job_env.Put("TDP_LASS_ADDRESS", "ENV");
	char pid_buf[256];
	sprintf(pid_buf, "%d", ApplicationPid);
	job_env.Put("TDP_AP_PID", pid_buf);

		// Now, let the starter publish any env vars it wants to into
		// the mainjob's env...
	Starter->PublishToEnv( &job_env );


		// // // // // // 
		// Standard Files
		// // // // // // 

	// handle stdin, stdout, and stderr redirection
	int fds[3];
		// initialize these to -2 to mean they're not specified.
		// -1 will be treated as an error.
	fds[0] = -2; fds[1] = -2; fds[2] = -2;
	bool foo;

		// in order to open these files we must have the user's privs:
	priv_state priv;
	priv = set_user_priv();

	fds[0] = openStdFile( SFT_IN, ATTR_TOOL_DAEMON_INPUT, false, foo,
						  "Tool Daemon Input file" );

	fds[1] = openStdFile( SFT_OUT, ATTR_TOOL_DAEMON_OUTPUT, false, foo,
						  "Tool Daemon Output file" );

	fds[2] = openStdFile( SFT_ERR, ATTR_TOOL_DAEMON_ERROR, false, foo,
						  "Tool Daemon Error file" );

	/* Bail out if we couldn't open the std files correctly */
	if( fds[0] == -1 || fds[1] == -1 || fds[2] == -1 ) {
			/* only close ones that had been opened correctly */
		for( int i=0; i<=2; i++ ) {
			if( fds[i] >= 0 ) {
				close(fds[i]);
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

	char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if ( ptmp ) {
		nice_inc = atoi(ptmp);
		free(ptmp);
	} else {
		nice_inc = 0;
	}

	dprintf( D_ALWAYS, "About to exec %s\n", DaemonArgs ); 

	// Grab the full environment back out of the Env object 
	env_str = job_env.getDelimitedString();

	set_priv( priv );

	JobPid = daemonCore->
		Create_Process( DaemonName, DaemonArgs,
						PRIV_USER_FINAL, 1, FALSE, env_str, job_iwd, TRUE,
						NULL, fds, nice_inc, DCJOBOPT_NO_ENV_INHERIT );

		//NOTE: Create_Process() saves the errno for us if it is an
		//"interesting" error.
	char const *create_process_error = NULL;
	if(JobPid == FALSE && errno) create_process_error = strerror(errno);

	// free memory so we don't leak
	delete[] env_str;

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
			MyString err_msg;
			err_msg.sprintf( "Failed to execute '%s': %s",
							 DaemonArgs, create_process_error );
			Starter->jic->notifyStarterError( err_msg.Value(), true );
		}
		EXCEPT( "Create_Process(%s, ...) failed", DaemonArgs );
		return FALSE;

	} else {
		dprintf( D_ALWAYS, "Create_Process succeeded, pid=%d\n", JobPid );

		job_start_time.getTime();

		// success!  create a ProcFamily
		family = new ProcFamily(JobPid,PRIV_USER);
		ASSERT(family);

#ifdef WIN32
		// we only support running jobs as user nobody for the first pass
		char nobody_login[60];
		//sprintf(nobody_login,"condor-run-dir_%d",daemonCore->getpid());
		sprintf(nobody_login,"condor-run-%d",daemonCore->getpid());
		// set ProcFamily to find decendants via a common login name
		family->setFamilyLogin(nobody_login);
#endif

		// take a snapshot of the family every 15 seconds
		snapshot_tid = daemonCore->Register_Timer(2, 15, 
			(TimerHandlercpp)&ProcFamily::takesnapshot, 
			"ProcFamily::takesnapshot", family);
		return TRUE;
	}
}


int
ToolDaemonProc::JobCleanup(int pid, int status)
{
    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::JobCleanup()\n" );

		// If the tool exited, we want to shutdown everything, and
		// also return a 1 so the CStarter knows it can put us on the
		// CleanedUpJobList.
    if( JobPid == pid ) {	
	
		job_exit_time.getTime();

		family->hardkill ();

		if( snapshot_tid >= 0 ) {
			daemonCore->Cancel_Timer(snapshot_tid);
		}
		snapshot_tid = -1;

        return 1;
    } 

    return 0;
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
	if ( family ) {
		family->suspend();
	}

        // set our flag
	job_suspended = true;
}

void
ToolDaemonProc::Continue()
{
	dprintf(D_FULLDEBUG,"in ToolDaemonProc::Continue()\n");

	// resume user job
	if ( family ) {
		family->resume();
	}
        // set our flag
	job_suspended = false;
}


bool
ToolDaemonProc::ShutdownGraceful()
{
  dprintf(D_FULLDEBUG,"in ToolDaemonProc::ShutdownGraceful()\n");

	if ( !family ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

    // take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	family->takesnapshot();

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

	if ( !family ) {
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
	family->hardkill();

    return false;	// return false says shutdown is pending
}


bool
ToolDaemonProc::PublishUpdateAd( ClassAd* ad ) 
{
    dprintf( D_FULLDEBUG, "Inside ToolDaemonProc::PublishUpdateAd()\n" );
    // Nothing special for us to do.
    return true;
}
