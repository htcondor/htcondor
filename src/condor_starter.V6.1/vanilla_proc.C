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

VanillaProc::VanillaProc( ClassAd *jobAd ) : OsProc()
{
    dprintf ( D_FULLDEBUG, "Constructor of MPIVanillaProc::MPIVanillaProc\n" );
	family = NULL;
	filetrans = NULL;
    JobAd = jobAd;
	snapshot_tid = -1;
	shadowupdate_tid = -1;
	shadowsock = NULL;
}

VanillaProc::~VanillaProc()
{
	if (family) {
		delete family;
	}

	if (filetrans) {
		delete filetrans;
	}

	if ( snapshot_tid != -1 ) {
		daemonCore->Cancel_Timer(snapshot_tid);
		snapshot_tid = -1;
	}

	if ( shadowupdate_tid != -1 ) {
		daemonCore->Cancel_Timer(shadowupdate_tid);
		shadowupdate_tid = -1;
	}

	if ( shadowsock ) {
		delete shadowsock;
	}

}

int
VanillaProc::StartJob()
{
	char tmp[_POSIX_ARG_MAX];
	char jobtmp[_POSIX_PATH_MAX];

	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// for now, stash the executable in jobtmp, and switch the ad to 
	// say the executable is condor_exec
	jobtmp[0] = '\0';
	JobAd->LookupString(ATTR_JOB_CMD,jobtmp);
	sprintf(tmp,"%s=\"condor_exec\"",ATTR_JOB_CMD);
	JobAd->InsertOrUpdate(tmp);		

	// if requested in the jobad, transfer files over 
	char TransSock[40];
	if (JobAd->LookupString(ATTR_TRANSFER_SOCKET, TransSock) == 1) {
		char buf[ATTRLIST_MAX_EXPRESSION];

		// reset iwd of job to the starter directory
		sprintf(buf,"%s=\"%s\"",ATTR_JOB_IWD,Starter->GetWorkingDir());
		JobAd->InsertOrUpdate(buf);		

		filetrans = new FileTransfer();
		ASSERT( filetrans->Init(JobAd) );
		ASSERT( filetrans->DownloadFiles() );
	}

	// vanilla jobs, unlike standard jobs, are allowed to run 
	// shell scripts (or as is the case on NT, batch files).  so
	// edit the ad so we start up a shell, pass the executable as
	// an argument to the shell, if we are asked to run a .bat file.
#ifdef WIN32
	char argstmp[_POSIX_ARG_MAX];
	char systemshell[_POSIX_PATH_MAX];

	int joblen = strlen(jobtmp);
	if ( joblen > 5 && stricmp(".bat",&(jobtmp[joblen-4])) == 0 ) {
		// executable name ends in .bat

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
		rename("condor_exec","condor_exec.bat");

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

		// take a snapshot of the family every 15 seconds
		snapshot_tid = daemonCore->Register_Timer(2, 15, 
			(TimerHandlercpp)&ProcFamily::takesnapshot, 
			"ProcFamily::takesnapshot", family);

		// update the shadow every 20 minutes.  years of study say
		// this is the optimal value. :^).
		shadowupdate_tid = daemonCore->Register_Timer(8,(20*60)+6,
			(TimerHandlercpp)&VanillaProc::UpdateShadow,
			"VanillaProc::UpdateShadow", this);

		return TRUE;
	} else {
		// failure
		return FALSE;
	}
}

int
VanillaProc::UpdateShadow()
{
	unsigned long max_image;
	long sys_time, user_time;
	ClassAd ad;
	char buf[200];

	if ( ShadowAddr[0] == '\0' ) {
		// we do not have an address for the shadow
		return FALSE;
	}

	if ( !shadowsock ) {
		shadowsock = new SafeSock();
		ASSERT(shadowsock);
		shadowsock->connect(ShadowAddr);
	}


	family->get_cpu_usage(sys_time,user_time);
	family->get_max_imagesize(max_image);

	// create an ad
	sprintf(buf,"%s=%lu",ATTR_JOB_REMOTE_SYS_CPU,sys_time);
	ad.InsertOrUpdate(buf);
	sprintf(buf,"%s=%lu",ATTR_JOB_REMOTE_USER_CPU,user_time);
	ad.InsertOrUpdate(buf);
	sprintf(buf,"%s=%lu",ATTR_JOB_REMOTE_SYS_CPU,max_image);
	ad.InsertOrUpdate(buf);

	// send it to the shadow
	dprintf(D_FULLDEBUG,"Sending shadow update, user_time=%lu, image=%lu\n",
		user_time,max_image);
	shadowsock->snd_int(SHADOW_UPDATEINFO,FALSE);
	ad.put(*shadowsock);
	shadowsock->end_of_message();
	
	return TRUE;
}

int
VanillaProc::JobExit(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobExit()\n");

	// transfer output files back if requested job really finished
	if ( filetrans && Requested_Exit != TRUE ) {
		ASSERT( filetrans->UploadFiles() );
	}

	if ( OsProc::JobExit(pid,status) ) {
		// ok, the parent exited.  make certain all decendants are dead.
		family->hardkill();
		return 1;
	}

	return 0;
}

void
VanillaProc::Suspend()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Suspend()\n");

	// suspend any filetransfer activity
	if ( filetrans ) {
		filetrans->Suspend();
	}

	// suspend the user job
	family->suspend();

	// set our flag
	job_suspended = TRUE;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");

	// resume any filetransfer activity
	if ( filetrans ) {
		filetrans->Continue();
	}

	// resume user job
	family->resume();

	// set our flag
	job_suspended = FALSE;
}

void
VanillaProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownGraceful()\n");

	// take a snapshot before we softkill the parent job process.
	// this helps ensure that if the parent exits without killing
	// the kids, our JobExit() handler will get em all.
	family->takesnapshot();

	// now softkill the parent job process.  this is exactly what
	// OsProc::ShutdownGraceful does, so call it.
	OsProc::ShutdownGraceful();
	
}

void
VanillaProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::ShutdownFast()\n");

	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	Requested_Exit = TRUE;
	family->hardkill();
}



