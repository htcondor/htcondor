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
#include "directory.h"

extern CStarter *Starter;

VanillaProc::VanillaProc( ClassAd *jobAd ) : OsProc()
{
	family = NULL;
	filetrans = NULL;
    JobAd = jobAd;
	snapshot_tid = -1;
	shadowupdate_tid = -1;
	shadowsock = NULL;
	TransferAtVacate = false;
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
VanillaProc::TransferCompleted(FileTransfer *ftrans)
{
	char tmp[_POSIX_ARG_MAX];

	// Make certain the file transfer succeeded.  It is 
	// completely wrong to ASSERT here if it failed since
	// we are a _multi_ starter -- it is just a quick hack
	// to get WinNT out the door.
	if ( ftrans ) {
		ASSERT( (ftrans->GetInfo()).success );
	}

	// vanilla jobs, unlike standard jobs, are allowed to run 
	// shell scripts (or as is the case on NT, batch files).  so
	// edit the ad so we start up a shell, pass the executable as
	// an argument to the shell, if we are asked to run a .bat file.
#ifdef WIN32
	char argstmp[_POSIX_ARG_MAX];
	char systemshell[_POSIX_PATH_MAX];

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
		sprintf(nobody_login,"condor-run-dir_%d",daemonCore->getpid());
		// set ProcFamily to find decendants via a common login name
		family->setFamilyLogin(nobody_login);
#endif

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
		// return FALSE;

		// DREADFUL HACK:  for now, if we fail to start the job,
		// do an EXCEPT.  Of course this is wrong for a multi-starter,
		// this is just a quick hack to get the first pass at WinNT
		// Condor out the door.
		EXCEPT("Failed to start job");

	}	

	return FALSE;
}

int
VanillaProc::StartJob()
{
	int ret_value;
	char tmp[_POSIX_ARG_MAX];
	int change_iwd = true;

	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

		/* setup value for TransferAtVacate and also determine if
		   we should change our working directory */
	tmp[0] = '\0';
	JobAd->LookupString(ATTR_TRANSFER_FILES,tmp);
		// if set to "ALWAYS", then set TransferAtVacate to true
	switch ( tmp[0] ) {
	case 'a':
	case 'A':
		TransferAtVacate = true;
		break;
	case 'n':  /* for "Never" */
	case 'N':
		change_iwd = false;  // It's true otherwise...
		break;
	}

	// for now, stash the executable in jobtmp, and switch the ad to 
	// say the executable is condor_exec
	jobtmp[0] = '\0';
	JobAd->LookupString(ATTR_JOB_CMD,jobtmp);

		// if requested in the jobad, transfer files over.  
	if ( change_iwd ) {
		// reset iwd of job to the starter directory
		sprintf(tmp,"%s=\"%s\"",ATTR_JOB_IWD,Starter->GetWorkingDir());
		JobAd->InsertOrUpdate(tmp);		
	
			// rename executable to 'condor_exec'
		sprintf(tmp,"%s=\"%s\"",ATTR_JOB_CMD,CONDOR_EXEC);
		JobAd->InsertOrUpdate(tmp);		

		filetrans = new FileTransfer();
		ASSERT( filetrans->Init(JobAd, false, PRIV_USER) );
		filetrans->RegisterCallback(
				  (FileTransferHandler)&VanillaProc::TransferCompleted,this);
		ret_value = filetrans->DownloadFiles(false); // do not block

	} else {
			/* no file transfer desired, thus the file transfer is "done".
			   We assume that transfer_files == Never means that we want 
			   to live in the submit directory, so we DON'T change the 
			   ATTR_JOB_CMD or the ATTR_JOB_IWD.  This is important 
			   to MPI!  -MEY 12-8-1999  */
		ret_value = TransferCompleted(NULL);
	}

	return ret_value;
}


int
VanillaProc::UpdateShadow()
{
	ClassAd ad;

	dprintf( D_FULLDEBUG, "Entering VanillaProc::UpdateShadow()\n" );

	if ( ShadowAddr[0] == '\0' ) {
		// we do not have an address for the shadow
		dprintf( D_FULLDEBUG, "Leaving VanillaProc::UpdateShadow(): "
				 "No ShadowAddr!\n" );
		return FALSE;
	}

	if ( !shadowsock ) {
		shadowsock = new SafeSock();
		ASSERT( shadowsock );
		shadowsock->connect( ShadowAddr );
	}

		// Publish all the info we care about into the ad.  This
		// method is virtual, so we'll get all the goodies from
		// derived classes, as well.
	PublishUpdateAd( &ad );

		// Send it to the shadow
	shadowsock->snd_int( SHADOW_UPDATEINFO, FALSE );
	ad.put( *shadowsock );
	shadowsock->end_of_message();
	
	dprintf( D_FULLDEBUG, "Leaving VanillaProc::UpdateShadow(): success\n" );

	return TRUE;
}



bool
VanillaProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "In VanillaProc::PublishUpdateAd()\n" );

	unsigned long max_image;
	long sys_time, user_time;
	unsigned int execsz = 0;
	char buf[200];

	// if there is a filetrans object, then let's send the current
	// size of the starter execute directory back to the shadow.  this
	// way the ATTR_DISK_USAGE will be updated, and we won't end
	// up on a machine without enough local disk space.
	if ( filetrans ) {
		Directory starter_dir( Starter->GetWorkingDir(), PRIV_USER );
		execsz = starter_dir.GetDirectorySize();
		sprintf( buf, "%s=%u", ATTR_DISK_USAGE, (execsz+1023)/1024 ); 
		ad->InsertOrUpdate( buf );
	}

	if ( !family ) {
		// we do not have a job family beneath us 
		dprintf( D_FULLDEBUG, "Leaving VanillaProc::PublishUpdateAd(): "
				 "No job family!\n" );
		return false;
	}

		// Gather the info we care about
	family->get_cpu_usage( sys_time, user_time );
	family->get_max_imagesize( max_image );

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
VanillaProc::JobExit(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobExit()\n");

	// ok, the parent exited.  make certain all decendants are dead.
	family->hardkill();

	daemonCore->Cancel_Timer(snapshot_tid);
	daemonCore->Cancel_Timer(shadowupdate_tid);
	delete shadowsock;
	snapshot_tid = -1;
	shadowupdate_tid = -1;
	shadowsock = NULL;

	// transfer output files back if requested job really finished.
	// may as well do this in the foreground, 
	// since we do not want to be interrupted by anything short of a hardkill.
	if ( filetrans && 
		((Requested_Exit != TRUE) || TransferAtVacate) ) {
		// The user job may have created files only readable by the user,
		// so set_user_priv here.
		// true if job exited on its own
		bool final_transfer = (Requested_Exit != TRUE);	
		priv_state saved_priv = set_user_priv();
		// this will block
		ASSERT( filetrans->UploadFiles(true, final_transfer) );	

		set_priv(saved_priv);
	}

	if ( OsProc::JobExit(pid,status) ) {
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

	// resume any filetransfer activity
	if ( filetrans ) {
		filetrans->Continue();
	}

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


	// Despite what the user wants, do not transfer back any files 
	// on a ShutdownFast.
	TransferAtVacate = false;	
	
	Requested_Exit = TRUE;
	family->hardkill();
	return false;	// shutdown is pending, so return false
}



