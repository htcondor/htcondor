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
//#include "condor_sys.h"
#include "condor_syscall_mode.h"
#include "exit.h"
#include "vanilla_proc.h"
#include "starter.h"
#include "syscall_numbers.h"

extern CStarter *Starter;



/* 
 * Vanilla class implementation.  It just uses procfamily to do
 * its dirty work.
 */

VanillaProc::VanillaProc()
{
	family = NULL;
	filetrans = NULL;
}

VanillaProc::~VanillaProc()
{
	if (family) {
		delete family;
	}

	if (filetrans) {
		delete filetrans;
	}
}

int
VanillaProc::StartJob()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::StartJob()\n");

	// TEMPORARY HACK
	if (JobAd) delete JobAd;
	JobAd = new ClassAd();

	if (REMOTE_syscall(CONDOR_get_job_info, JobAd) < 0) {
		dprintf(D_ALWAYS, 
				"Failed to get job info from Shadow.  Aborting StartJob.\n");
		return 0;
	}
	// end of temporary hack


	// if requested in the jobad, transfer files over 
	char TransSock[40];
	if (JobAd->LookupString("TRANSFER_SOCKET", TransSock) == 1) {
		char buf[ATTRLIST_MAX_EXPRESSION];

		// reset iwd of job to the starter directory
		sprintf(buf,"%s=\"%s\"",ATTR_JOB_IWD,Starter->GetWorkingDir());
		JobAd->InsertOrUpdate(buf);

		filetrans = new FileTransfer();
		ASSERT( filetrans->Init(JobAd) );
		ASSERT( filetrans->DownloadFiles() );
	}


	// call OsProc to start it up; if success, create the ProcFamily
	if (OsProc::StartJob()) {
		// success!
		family = new ProcFamily(JobPid,PRIV_USER);
		ASSERT(family);
		return 1;
	} else {
		// failure
		return 0;
	}
}

int
VanillaProc::JobExit(int pid, int status)
{
	dprintf(D_FULLDEBUG,"in VanillaProc::JobExit()\n");

	// transfer output files back if requested
	if ( filetrans ) {
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

	family->suspend();
	job_suspended = TRUE;
}

void
VanillaProc::Continue()
{
	dprintf(D_FULLDEBUG,"in VanillaProc::Continue()\n");

	family->resume();
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
