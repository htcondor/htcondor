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
#if defined(__GNUG__)
#pragma implementation "list.h"
#endif
#include "starter.h"
#include "vanilla_proc.h"
#include "mpi_master_proc.h"
#include "mpi_comrade_proc.h"
#include "syscall_numbers.h"
#include "my_hostname.h"
#include "condor_string.h"  // for strnewp
#include "condor_attributes.h"
#include "condor_random_num.h"

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
		(SignalHandlercpp)&Suspend, "Suspend", this, IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGCONTINUE, "DC_SIGCONTINUE",
		(SignalHandlercpp)&Continue, "Continue", this, IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGHARDKILL, "DC_SIGHARDKILL",
		(SignalHandlercpp)&ShutdownFast, "ShutdownFast", this, 
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGSOFTKILL, "DC_SIGSOFTKILL",
		(SignalHandlercpp)&ShutdownGraceful, "ShutdownGraceful", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Signal(DC_SIGPCKPT, "DC_SIGPCKPT",
		(SignalHandlercpp)&PeriodicCkpt, "PeriodicCkpt", this,
		IMMEDIATE_FAMILY);
	daemonCore->Register_Reaper("Reaper", (ReaperHandlercpp)&Reaper, "Reaper",
		this);

	// move to working directory
	sprintf(WorkingDir, "%s%cdir_%ld", Execute, DIR_DELIM_CHAR, 
			daemonCore->getpid());
	if (mkdir(WorkingDir,0777) < 0) {
		EXCEPT("mkdir(%s)", WorkingDir);
	}
	if (chdir(WorkingDir) < 0) {
		EXCEPT("chdir(%s)", WorkingDir);
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", WorkingDir );

	// init environment info
	char *mfhn = strnewp ( my_full_hostname() );
	REMOTE_syscall(CONDOR_register_machine_info, UIDDomain, FSDomain,
				   daemonCore->InfoCommandSinfulString(), 
				   mfhn, Key);
	delete [] mfhn;


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
		if ( job->ShutdownGraceful() ) {
			// job is completely shut down, so delete it
			JobList.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
		}
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
		if ( job->ShutdownFast() ) {
			// job is completely shut down, so delete it
			JobList.DeleteCurrent();
			delete job;
		} else {
			// job shutdown is pending, so just set our flag
			jobRunning = true;
		}
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
        // We want to get the jobAd first, make an appropriate 
        // type of starter, *then* call StartJob() on it.
    dprintf ( D_FULLDEBUG, "In CStarter::StartJob()\n" );

    ClassAd *jobAd = new ClassAd;

	if (REMOTE_syscall(CONDOR_get_job_info, jobAd) < 0) {
		dprintf(D_ALWAYS, 
				"Failed to get job info from Shadow.  Aborting StartJob.\n");
		return;
	}

    // printAdToFile( jobAd, "/tmp/starter_ad" );

    int universe = STANDARD;
    if ( jobAd->LookupInteger( ATTR_JOB_UNIVERSE, universe ) < 1 ) {
        dprintf( D_ALWAYS, "No universe attr. in jobAd!\n" );
    }

    UserProc *job;

    switch ( universe )  
    {
        case VANILLA:
        case STANDARD: { 
            dprintf ( D_FULLDEBUG, "Firing up a VanillaProc\n" );
            job = new VanillaProc( jobAd );
            break;
        }
        case MPI: {
            int is_master = FALSE;
            dprintf ( D_FULLDEBUG, "Is master: %s\n", ATTR_MPI_IS_MASTER );
            if ( jobAd->LookupBool( ATTR_MPI_IS_MASTER, is_master ) < 1 ) {
                is_master = FALSE;
            }
            
            dprintf ( D_FULLDEBUG, "is_master : %d\n", is_master );

            if ( is_master ) {
                dprintf ( D_FULLDEBUG, "Firing up a MPIMasterProc\n" );
                job = new MPIMasterProc( jobAd );
            } else {
                dprintf ( D_FULLDEBUG, "Firing up a MPIComradeProc\n" );
                job = new MPIComradeProc( jobAd );
            }
            break;
        }
        case PVM: {
            dprintf ( D_ALWAYS, "PVM not (yet) supported\n" );
            return;
        }
        default: {
            dprintf ( D_ALWAYS, "What the heck kinda universe is %d?\n", 
                      universe );
            return;
        }
    } /* switch */

	if (job->StartJob()) {
		JobList.Append(job);		
	} else {
		delete job;

		// DREADFUL HACK:  for now, if we fail to start the job,
		// do an EXCEPT.  Of course this is wrong for a multi-starter,
		// this is just a quick hack to get the first pass at WinNT
		// Condor out the door.
		EXCEPT("Failed to start job");
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

int CStarter::printAdToFile(ClassAd *ad, char* JobHistoryFileName) {

    FILE* LogFile=fopen(JobHistoryFileName,"a");
    if ( !LogFile ) {
        dprintf(D_ALWAYS,"ERROR saving to history file; cannot open%s\n", 
                JobHistoryFileName);
        return false;
    }
    if (!ad->fPrint(LogFile)) {
        dprintf(D_ALWAYS, "ERROR in Scheduler::LogMatchEnd - failed to "
                "write classad to log file %s\n", JobHistoryFileName);
        fclose(LogFile);
        return false;
    }
    fprintf(LogFile,"***\n");   // separator
    fclose(LogFile);
    return true;
}
