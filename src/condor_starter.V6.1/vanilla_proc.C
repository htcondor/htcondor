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
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "syscall_numbers.h"
#include "dynuser.h"
#include "condor_config.h"
#include "domain_tools.h"

#ifdef WIN32
extern dynuser* myDynuser;
#endif

extern CStarter *Starter;

VanillaProc::VanillaProc( ClassAd *jobAd ) : OsProc( jobAd )
{
	family = NULL;
	snapshot_tid = -1;
}

VanillaProc::~VanillaProc()
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
VanillaProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// vanilla jobs, unlike standard jobs, are allowed to run 
	// shell scripts (or as is the case on NT, batch files).  so
	// edit the ad so we start up a shell, pass the executable as
	// an argument to the shell, if we are asked to run a .bat file.
#ifdef WIN32

	char systemshell[_POSIX_PATH_MAX];
	char tmp[_POSIX_PATH_MAX];

	ClassAdUnParser unp;
	string treeString;

	const char* jobtmp = Starter->jic->origJobName();
	int joblen = strlen(jobtmp);
	if ( joblen > 5 && 
			( (stricmp(".bat",&(jobtmp[joblen-4])) == 0) || 
			  (stricmp(".cmd",&(jobtmp[joblen-4])) == 0) ) ) {
		// executable name ends in .bat or .cmd

		// first change executable to be cmd.exe
		::GetSystemDirectory(systemshell,MAX_PATH);
		sprintf(tmp,"%s=\"%s\\cmd.exe\"",ATTR_JOB_CMD,systemshell);
		JobAd->InsertOrUpdate(tmp);		

		// now change arguments to include name of program cmd.exe 
		// should run

		// this little piece of code preserves the backwhacking of quotes,
		// so when it's re-inserted into the JobAd any original backwhacks are still there.
		ExprTree  *tree;
        char	  *job_args;
		char      *argstmp;
		int		  length;

		job_args = argstmp = NULL;
		
		tree = JobAd->Lookup(ATTR_JOB_ARGUMENTS);
//		if ( tree != NULL && tree->RArg() != NULL ) {
//			tree->RArg()->PrintToNewStr(&argstmp);
		if ( tree != NULL ) {
			unp.Unparse( treeString, tree );
			argstmp = new char[treeString.length( )+1];
			strcpy( argstmp, treeString.c_str( ) );
			job_args = argstmp+1;		// skip first quote
			length = strlen(job_args);
			job_args[length-1] = '\0';	// destroy last quote
		} else {
			job_args = (char*) malloc(1*sizeof(char));
			job_args[0] = '\0';
		}
		
		// also pass /Q and /C arguments to cmd.exe, to tell it we do not
		// want an interactive shell -- just run the command and exit
		sprintf ( tmp, "%s=\"/Q /C condor_exec.bat %s\"",
				  ATTR_JOB_ARGUMENTS, job_args );
		JobAd->InsertOrUpdate(tmp);		

		if ( argstmp != NULL ) {
			// this is needed if we had to call PrintToNewStr() in classads
			free(argstmp);
		} else {
			// otherwise we just need to free the byte we used to store the empty string
			free(job_args);
		}

		// finally we must rename file condor_exec to condor_exec.bat
		rename(CONDOR_EXEC,"condor_exec.bat");

		dprintf(D_FULLDEBUG,
			"Executable is .bat, so running %s\\cmd.exe %s\n",
			systemshell,tmp);

	}	// end of if executable name ends in .bat
#endif

	// call OsProc to start it up; if success, create the ProcFamily
	if (OsProc::StartJob()) {
		
		// success!  create a ProcFamily
		family = new ProcFamily(JobPid,PRIV_USER);
		ASSERT(family);

		const char* run_jobs_as = NULL;
		bool dedicated_account = false;

		run_jobs_as = get_user_loginname();

		// See if we want our family built by login if we're using
		// specific run accounts. (default is no)
		dedicated_account = param_boolean("EXECUTE_LOGIN_IS_DEDICATED", false);

		// we support running the job as other users if the user
		// is specifed in the config file, and the account's password
		// is properly stored in our credential stash.

		if (dedicated_account) {
			// set ProcFamily to find decendants via a common login name
			dprintf(D_FULLDEBUG, "Building procfamily by login \"%s\"\n",
					run_jobs_as);
			family->setFamilyLogin(run_jobs_as);
		}

		// take a snapshot of the family every 15 seconds
		snapshot_tid = daemonCore->Register_Timer(2, 15, 
			(TimerHandlercpp)&ProcFamily::takesnapshot, 
			"ProcFamily::takesnapshot", family);

		return TRUE;
	}
	return FALSE;
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );

	unsigned long max_image;
	long sys_time, user_time;
	char buf[200];

	if ( !family ) {
		// we do not have a job family beneath us 
		dprintf( D_FULLDEBUG, "Leaving VanillaProc::PublishUpdateAd(): "
				 "No job family!\n" );
		return false;
	}

		// Gather the info we care about
	family->get_cpu_usage( sys_time, user_time );
	family->get_max_imagesize( max_image );
	num_pids = family->size();

		// Publish it into the ad.
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_SYS_CPU, sys_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_USER_CPU, user_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_IMAGE_SIZE, max_image );
	ad->InsertOrUpdate( buf );

		// Now, call our parent class's version
	return OsProc::PublishUpdateAd( ad );
}


int
VanillaProc::JobCleanup(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobCleanup()\n");

	// ok, the parent exited.  make certain all decendants are dead.
	family->hardkill();

	if( snapshot_tid >= 0 ) {
		daemonCore->Cancel_Timer(snapshot_tid);
	}
	snapshot_tid = -1;

		// This will reset num_pids for us, too.
	return OsProc::JobCleanup( pid, status );
}


void
VanillaProc::Suspend()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Suspend()\n");

	// suspend the user job
	if ( family ) {
		family->suspend();
	}

	// set our flag
	is_suspended = true;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");

	// resume user job
	if ( family ) {
		family->resume();
	}

	// set our flag
	is_suspended = false;
}

bool
VanillaProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownGraceful()\n");

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

	// now softkill the parent job process.  this is exactly what
	// OsProc::ShutdownGraceful does, so call it.
	return OsProc::ShutdownGraceful();	
}

bool
VanillaProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownFast()\n");

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
	requested_exit = true;
	family->hardkill();
	return false;	// shutdown is pending, so return false
}



