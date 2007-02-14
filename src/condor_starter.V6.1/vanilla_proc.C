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
		// also pass /Q and /C arguments to cmd.exe, to tell it we do not
		// want an interactive shell -- just run the command and exit

		ArgList args;
		MyString arg_errors;

			// Since we are adding to the argument list, we may need to deal
			// with platform-specific arg syntax in the user's args in order
			// to successfully merge them with the additional args.
		args.SetArgV1SyntaxToCurrentPlatform();

		args.AppendArg("/Q");
		args.AppendArg("/C");
		args.AppendArg("condor_exec.bat");

		if(!args.AppendArgsFromClassAd(JobAd,&arg_errors) ||
		   !args.InsertArgsIntoClassAd(JobAd,NULL,&arg_errors)) {
			dprintf(D_ALWAYS,"ERROR: failed to get args from job ad: %s\n",
					arg_errors.Value());
			return FALSE;
		}

		// finally we must rename file condor_exec to condor_exec.bat
		rename(CONDOR_EXEC,"condor_exec.bat");

		if(DebugFlags & D_FULLDEBUG) {
			MyString args_desc;
			args.GetArgsStringForDisplay(&args_desc);
			dprintf(D_FULLDEBUG,
					"Executable is .bat, so running %s\\cmd.exe %s\n",
					systemshell,args_desc.Value());
		}

	}	// end of if executable name ends in .bat
#endif

	// set up a FamilyInfo structure to tell OsProc to register a family
	// with the ProcD in its call to DaemonCore::Create_Process
	//
	FamilyInfo fi;

	// take snapshots at no more than 15 seconds in between, by default
	//
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	// use login-based family tracking if EXECUTE_LOGIN_IS_DEDICATED is
	// set and we're not starting a local universe job
	//
	fi.login = NULL;
	bool dedicated_account = false;
	if (job_universe != CONDOR_UNIVERSE_LOCAL) {
		dedicated_account = param_boolean("EXECUTE_LOGIN_IS_DEDICATED",
		                                  false);
	}
	if (dedicated_account) {
		fi.login = get_user_loginname();
		dprintf(D_FULLDEBUG,
		        "Tracking process family by login \"%s\"\n",
		        fi.login);
	}

	// have OsProc start the job
	//
	return OsProc::StartJob(&fi);
}


bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );

	ProcFamilyUsage usage;
	if (daemonCore->Get_Family_Usage(JobPid, usage) == FALSE) {

		dprintf(D_ALWAYS,
		        "error getting family usage in VanillaProc::PublishUpdateAd()\n");
		return false;
	}

		// Publish the info we care about into the ad.
	char buf[200];
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_SYS_CPU, usage.sys_cpu_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_JOB_REMOTE_USER_CPU, usage.user_cpu_time );
	ad->InsertOrUpdate( buf );
	sprintf( buf, "%s=%lu", ATTR_IMAGE_SIZE, usage.max_image_size );
	ad->InsertOrUpdate( buf );

		// Update our knowledge of how many processes the job has
	num_pids = usage.num_procs;

		// Now, call our parent class's version
	return OsProc::PublishUpdateAd( ad );
}


int
VanillaProc::JobCleanup(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobCleanup()\n");

		// This will reset num_pids for us, too.
	return OsProc::JobCleanup( pid, status );
}


void
VanillaProc::Suspend()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Suspend()\n");
	
	// suspend the user job
	if (JobPid != -1) {
		if (daemonCore->Suspend_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error suspending family in VanillaProc::Suspend()\n");
		}
	}
	
	// set our flag
	is_suspended = true;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");
	
	// resume user job
	if (JobPid != -1) {
		if (daemonCore->Continue_Family(JobPid) == FALSE) {
			dprintf(D_ALWAYS,
			        "error continuing family in VanillaProc::Continue()\n");
		}
	}

	// set our flag
	is_suspended = false;
}

bool
VanillaProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownGraceful()\n");
	
	if ( JobPid == -1 ) {
		// there is no process family yet, probably because we are still
		// transferring files.  just return true to say we're all done,
		// and that way the starter class will simply delete us and the
		// FileTransfer destructor will clean up.
		return true;
	}

	// WE USED TO.....
	//
	// take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	//
	// TODO: should we make an explicit call to the procd here to tell
	// it to take a snapshot???

	// now softkill the parent job process.  this is exactly what
	// OsProc::ShutdownGraceful does, so call it.
	//
	return OsProc::ShutdownGraceful();	
}

bool
VanillaProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownFast()\n");
	
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
	requested_exit = true;

	// FINDOUT: when we are being shutdown fast, do we still update the
	// shadow with the latest usage information about the job???
	//
	daemonCore->Kill_Family(JobPid);

	return false;	// shutdown is pending, so return false
}
