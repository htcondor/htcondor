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
#include "condor_debug.h"
#include "condor_syscall_mode.h"   // moronic: must go before condor_config
#include "condor_config.h"
//#include "condor_sys.h"
#if defined(__GNUG__)
#pragma implementation "list.h"
#endif
#include "starter.h"
#include "vanilla_proc.h"
#include "syscall_numbers.h"

static char *_FileName_ = __FILE__; /* Used by EXCEPT (see except.h)    */

extern "C" int get_random_int();

/* CStarter class implementation */

CStarter::CStarter()
{
	InitiatingHost = NULL;
	Execute = NULL;
	UIDDomain = NULL;
	FSDomain = NULL;
	Key = get_random_int()%get_random_int();
	ShuttingDown = FALSE;
}

CStarter::~CStarter()
{
	if (Execute) free(Execute);
	if (UIDDomain) free(UIDDomain);
	if (FSDomain) free(FSDomain);
}

void
CStarter::Init(char peer[])
{
	InitiatingHost = peer;

	dprintf(D_ALWAYS, "Submitting machine is \"%s\"\n", InitiatingHost);

	Config();

	// setup daemonCore handlers
	daemonCore->Register_Signal(DC_SIGSUSPEND, "DC_SIGSUSPEND", 
		(SignalHandlercpp)Suspend, "Suspend", this, IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGCONTINUE, "DC_SIGCONTINUE",
		(SignalHandlercpp)Continue, "Continue", this, IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGHARDKILL, "DC_SIGHARDKILL",
		(SignalHandlercpp)ShutdownFast, "ShutdownFast", this, 
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGSOFTKILL, "DC_SIGSOFTKILL",
		(SignalHandlercpp)ShutdownGraceful, "ShutdownGraceful", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGPCKPT, "DC_SIGPCKPT",
		(SignalHandlercpp)PeriodicCkpt, "PeriodicCkpt", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Reaper("Reaper", (ReaperHandlercpp)Reaper, "Reaper",
		this);

	// move to working directory
	sprintf(WorkingDir, "%s%cdir_%d", Execute, DIR_DELIM_CHAR, daemonCore->getpid());
	if (mkdir(WorkingDir,0777) < 0) {
		EXCEPT("mkdir(%s)", WorkingDir);
	}
	if (chdir(WorkingDir) < 0) {
		EXCEPT("chdir(%s)", WorkingDir);
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", WorkingDir );

	// init environment info
	REMOTE_syscall(CONDOR_register_machine_info, UIDDomain, FSDomain,
				   daemonCore->InfoCommandSinfulString(), Key);

	set_resource_limits();

	StartJob();
}

void
CStarter::Config()
{
	if (Execute) free(Execute);
	if ((Execute = param("EXECUTE")) == NULL) {
		EXCEPT("Execute directory not specified in config file.");
	}

	if (UIDDomain) free(UIDDomain);
	if ((UIDDomain = param("UID_DOMAIN")) == NULL) {
		EXCEPT("UID_DOMAIN not specified in config file.");
	}

	if (FSDomain) free(FSDomain);
	if ((FSDomain = param("FILESYSTEM_DOMAIN")) == NULL) {
		EXCEPT("FILESYSTEM_DOMAIN not specified in config file.");
	}
}

int
CStarter::ShutdownGraceful(int)
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownGraceful all jobs.\n");
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		jobRunning = true;
		job->ShutdownGraceful();
	}
	ShuttingDown = TRUE;
	if (!jobRunning) {
		dprintf(D_FULLDEBUG, 
				"Got ShutdownGraceful when no jobs running.\n");
		return 1;
	}	
	return 0;
}

int
CStarter::ShutdownFast(int)
{
	bool jobRunning = false;
	UserProc *job;

	dprintf(D_ALWAYS, "ShutdownFast all jobs.\n");
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		jobRunning = true;
		job->ShutdownFast();
	}
	ShuttingDown = TRUE;
	if (!jobRunning) {
		dprintf(D_FULLDEBUG, 
				"Got ShutdownFast when no jobs running.\n");
		return 1;
	}	
	return 0;
}

void
CStarter::StartJob()
{
	UserProc *job = new VanillaProc();
	if (job->StartJob()) {
		JobList.Append(job);		
	} else {
		delete job;
	}
}

int
CStarter::Suspend(int)
{
	dprintf(D_ALWAYS, "Suspending all jobs.\n");
	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		job->Suspend();
	}
	return 0;
}

int
CStarter::Continue(int)
{
	dprintf(D_ALWAYS, "Continuing all jobs.\n");
	UserProc *job;
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		job->Continue();
	}
	return 0;
}

int
CStarter::PeriodicCkpt(int)
{
	// TODO
	return 0;
}

int
CStarter::Reaper(int pid, int exit_status)
{
	int handled_jobs = 0;
	int all_jobs = 0;
	UserProc *job;

	dprintf(D_ALWAYS,"Job exited, pid=%d, status=%d\n",pid,exit_status);
	JobList.Rewind();
	while ((job = JobList.Next()) != NULL) {
		all_jobs++;
		if (job->GetJobPid()==pid && job->JobExit(pid, exit_status)) {
			handled_jobs++;
			JobList.DeleteCurrent();
			delete job;
		}
	}
	dprintf(D_FULLDEBUG,"Reaper: all=%d handled=%d ShuttingDown=%d\n",
		all_jobs,handled_jobs,ShuttingDown);
	if (handled_jobs == 0) {
		dprintf(D_ALWAYS, "unhandled job exit: pid=%d, status=%d\n", pid,
				exit_status);
	}
	if ( ShuttingDown && (all_jobs - handled_jobs == 0) ) {
		dprintf(D_ALWAYS,"Last process exited, now Starter is exiting\n");
		DC_Exit(0);
	}
	return 0;
}
