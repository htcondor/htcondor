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
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "syscall_numbers.h"

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
	char argstmp[_POSIX_ARG_MAX];
	char systemshell[_POSIX_PATH_MAX];
	char tmp[_POSIX_PATH_MAX];

	char* jobtmp = Starter->GetOrigJobName();
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
		if (JobAd->LookupString(ATTR_JOB_ARGUMENTS,argstmp) != 1) {
			argstmp[0] = '\0';
		}
		// also pass /Q and /C arguments to cmd.exe, to tell it we do not
		// want an interactive shell -- just run the command and exit
		sprintf ( tmp, "%s=\"/Q /C condor_exec.bat %s\"",
				  ATTR_JOB_ARGUMENTS, argstmp );
		JobAd->InsertOrUpdate(tmp);		

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
	job_suspended = TRUE;
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
	job_suspended = FALSE;
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



