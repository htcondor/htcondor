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
#include "user_proc.h"
#include "os_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
//#include "condor_sys.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "exit.h"

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)    */

/* OsProc class implementation */

OsProc::OsProc()
{
	JobAd = NULL;
	JobPid = Cluster = Proc = -1;
	job_suspended = FALSE;
}

OsProc::~OsProc()
{
	if (JobAd) {
		delete JobAd;
	}
}

int
OsProc::StartJob()
{
	int i;

	dprintf(D_FULLDEBUG,"in OsProc::StartJob()\n");

#if 0
	if (JobAd) delete JobAd;
	JobAd = new ClassAd();

	if (REMOTE_syscall(CONDOR_get_job_info, JobAd) < 0) {
		dprintf(D_ALWAYS, 
				"Failed to get job info from Shadow.  Aborting StartJob.\n");
		return 0;
	}
#endif

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

	char JobName[40];
	sprintf(JobName, "condor_exec");
	if (REMOTE_syscall(CONDOR_get_executable, JobName) < 0) {
		dprintf(D_ALWAYS, "Failed to get/store job executable from Shadow."
				"  Aborting StartJob.\n");
		return 0;
	}

	// get job universe

	// init_user_ids
	char Executable[_POSIX_PATH_MAX];
	if (JobAd->LookupString(ATTR_JOB_CMD, Executable) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting DC_StartCondorJob.\n",
				ATTR_JOB_CMD);
		return 0;
	}

	char Args[_POSIX_PATH_MAX];
	if (JobAd->LookupString(ATTR_JOB_ARGUMENTS, Args) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting DC_StartCondorJob.\n",
				ATTR_JOB_CMD);
		return 0;
	}

	char Env[ATTRLIST_MAX_EXPRESSION];
	if (JobAd->LookupString(ATTR_JOB_ENVIRONMENT, Env) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting DC_StartCondorJob.\n",
				ATTR_JOB_ENVIRONMENT);
		return 0;
	}

	char Cwd[_POSIX_PATH_MAX];
	if (JobAd->LookupString(ATTR_JOB_IWD, Cwd) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  Aborting DC_StartCondorJob.\n",
				ATTR_JOB_IWD);
		return 0;
	}

	// handle stdin, stdout, and stderr redirection
	char filename[_POSIX_PATH_MAX];
	int fds[3];
	fds[0] = -1; fds[1] = -1; fds[2] = -1;

	if (JobAd->LookupString(ATTR_JOB_INPUT, filename) == 1) {
		if ( strcmp(filename,"NUL") != 0 &&
			(fds[0]=open(filename,O_RDONLY,0)) < 0 ) {
			dprintf(D_ALWAYS,"failed to open stdin file %s\n",
				filename);
		}
	}

	if (JobAd->LookupString(ATTR_JOB_OUTPUT, filename) == 1) {
		if ( strcmp(filename,"NUL") != 0 &&
			(fds[1]=open(filename,O_WRONLY | O_CREAT,666)) < 0 ) {
			dprintf(D_ALWAYS,"failed to open stdout file %s\n",
				filename);
		}
	}

	if (JobAd->LookupString(ATTR_JOB_ERROR, filename) == 1) {
		if ( strcmp(filename,"NUL") != 0 &&
			(fds[2]=open(filename,O_WRONLY | O_CREAT,666)) < 0 ) {
			dprintf(D_ALWAYS,"failed to open stderr file %s\n",
				filename);
		}
	}

	JobPid = daemonCore->Create_Process(JobName, Args, PRIV_USER_FINAL, 1,
									  FALSE, Env, NULL, TRUE, NULL, fds );

	// now close the descriptors in fds array.  our child has inherited
	// them already, so we should close them so we do not leak descriptors.
	for (i=0;i<3;i++) {
		if ( fds[i] != -1 ) close(i);
	}

	if ( JobPid == FALSE ) {
		dprintf(D_ALWAYS,"ERROR Create_Process(%s,%s, ...) failed\n",
			JobName, Args );
		return 0;
	}

	dprintf(D_ALWAYS,"Create_Process succeeded, pid=%d\n",JobPid);

	return 1;
}

int
OsProc::JobExit(int pid, int status)
{
	int reason;	

	if (JobPid == pid) {		
		if ( Requested_Exit == TRUE ) {
			reason = JOB_NOT_CKPTED;
		} else {
			reason = JOB_EXITED;
		}

		if (REMOTE_syscall(CONDOR_job_exit, status, reason) < 0) {
			dprintf(D_ALWAYS, "Failed to send job exit status to Shadow.\n");
		}
		return 1;
	}

	return 0;
}

void
OsProc::Suspend()
{
	daemonCore->Send_Signal(JobPid, DC_SIGSTOP);
	job_suspended = TRUE;
}

void
OsProc::Continue()
{
	daemonCore->Send_Signal(JobPid, DC_SIGCONT);
	job_suspended = FALSE;
}

void
OsProc::ShutdownGraceful()
{
	if ( job_suspended == TRUE )
		Continue();

	Requested_Exit = TRUE;
	daemonCore->Send_Signal(JobPid, DC_SIGTERM);
		
}

void
OsProc::ShutdownFast()
{
	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	Requested_Exit = TRUE;
	daemonCore->Send_Signal(JobPid, DC_SIGKILL);
}
