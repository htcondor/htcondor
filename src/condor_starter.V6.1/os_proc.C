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
#include "user_proc.h"
#include "os_proc.h"
#include "starter.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "exit.h"
#include "condor_uid.h"
#ifdef WIN32
#include "perm.h"
#endif

extern CStarter *Starter;

/* OsProc class implementation */

OsProc::OsProc()
{
    dprintf ( D_FULLDEBUG, "In OsProc::OsProc()\n" );
	JobAd = NULL;
	JobPid = Cluster = Proc = -1;
	job_suspended = FALSE;
	ShadowAddr[0] = '\0';
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
	int nice_inc = 0;

	dprintf(D_FULLDEBUG,"in OsProc::StartJob()\n");

	if ( !JobAd ) {
		dprintf ( D_ALWAYS, "No JobAd in OsProc::StartJob()!\n" );
		return 0;
	}

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

	char JobName[_POSIX_PATH_MAX];
	if ( JobAd->LookupString( ATTR_JOB_CMD, JobName ) != 1 ) {
		dprintf( D_ALWAYS, "%s not found in JobAd.  Aborting StartJob.\n", 
				 ATTR_JOB_CMD );
		return 0;
	}

	// get sinfullstring of our shadow, if shadow told us
	JobAd->LookupString(ATTR_MY_ADDRESS, ShadowAddr);

	if( ShadowAddr[0] ) {
		dprintf( D_FULLDEBUG, "In job ad: %s = %s\n",
				 ATTR_MY_ADDRESS, ShadowAddr );
	} else {
		dprintf( D_FULLDEBUG, "Can't find %s in job ad!\n",
				 ATTR_MY_ADDRESS );
	}

	// if name is condor_exec, we transferred it, so make certain
	// it is executable.
	if ( strcmp(CONDOR_EXEC,JobName) == 0 ) {
		priv_state old_priv = set_user_priv();
		int retval = chmod( JobName, S_IRWXU | S_IRWXO | S_IRWXG );
		set_priv( old_priv );
		if( retval < 0 ) {
			dprintf ( D_ALWAYS, "Failed to chmod %s!\n",JobName );
			return 0;
		}
	}

	char Args[_POSIX_ARG_MAX];
	char tmp[_POSIX_ARG_MAX];

	if (JobAd->LookupString(ATTR_JOB_ARGUMENTS, tmp) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  "
				"Aborting DC_StartCondorJob.\n", ATTR_JOB_ARGUMENTS);
		return 0;
	}
		// for now, simply prepend "condor_exec" to the args - this
		// becomes argv[0].  This should be made smarter later....
	strcpy ( Args, CONDOR_EXEC );
	strcat ( Args, " " );
	strcat ( Args, tmp );

	char Env[ATTRLIST_MAX_EXPRESSION];
	if (JobAd->LookupString(ATTR_JOB_ENVIRONMENT, Env) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  "
				"Aborting DC_StartCondorJob.\n", ATTR_JOB_ENVIRONMENT);
		return 0;
	}

	char Cwd[_POSIX_PATH_MAX];
	if (JobAd->LookupString(ATTR_JOB_IWD, Cwd) != 1) {
		dprintf(D_ALWAYS, "%s not found in JobAd.  "
				"Aborting DC_StartCondorJob.\n", ATTR_JOB_IWD);
		return 0;
	}

	// handle stdin, stdout, and stderr redirection
	int fds[3];
	fds[0] = -1; fds[1] = -1; fds[2] = -1;
	char filename[_POSIX_PATH_MAX];
	char infile[_POSIX_PATH_MAX];
	char outfile[_POSIX_PATH_MAX];
	char errfile[_POSIX_PATH_MAX];

		// in order to open these files we must have the user's privs:
	priv_state priv;
	priv = set_user_priv();

	if (JobAd->LookupString(ATTR_JOB_INPUT, filename) == 1) {
		if ( strcmp(filename,"NUL") != 0 ) {
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( infile, "%s%c", Cwd, DIR_DELIM_CHAR );
            } else {
                infile[0] = '\0';
            }
			strcat ( infile, filename );
			if ( (fds[0]=open( infile, O_RDONLY ) ) < 0 ) {
				dprintf(D_ALWAYS,"failed to open stdin file %s, errno %d\n",
						infile, errno);
			}
		dprintf ( D_ALWAYS, "Input file: %s\n", infile );
		}
	}

	if (JobAd->LookupString(ATTR_JOB_OUTPUT, filename) == 1) {
		if ( strcmp(filename,"NUL") != 0 ) {
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( outfile, "%s%c", Cwd, DIR_DELIM_CHAR );
            } else {
                outfile[0] = '\0';
            }
			strcat ( outfile, filename );
			if ((fds[1]=open(outfile,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
					// if failed, try again without O_TRUNC
				if ( (fds[1]=open( outfile, O_WRONLY | O_CREAT, 0666)) < 0 ) {
					dprintf(D_ALWAYS,
							"failed to open stdout file %s, errno %d\n",
							outfile, errno);
				}
			}
			dprintf ( D_ALWAYS, "Output file: %s\n", outfile );
		}
	}

	if (JobAd->LookupString(ATTR_JOB_ERROR, filename) == 1) {
		if ( strcmp(filename,"NUL") != 0 ) {
            if ( filename[0] != '/' ) {  // prepend full path
                sprintf( errfile, "%s%c", Cwd, DIR_DELIM_CHAR );
            } else {
                errfile[0] = '\0';
            }
			strcat ( errfile, filename );
			if ((fds[2]=open( errfile,O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0 ) {
					// if failed, try again without O_TRUNC
				if ((fds[2]=open( errfile,O_WRONLY|O_CREAT, 0666)) < 0 ) {
					dprintf(D_ALWAYS,
							"failed to open stderr file %s, errno %d\n",
							errfile, errno);
				}
			}
			dprintf ( D_ALWAYS, "Error file: %s\n", errfile );
		}
	}

	// handle JOB_RENICE_INCREMENT
	char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if ( ptmp ) {
		nice_inc = atoi(ptmp);
		free(ptmp);
	} else {
		nice_inc = 0;
	}

	// in below dprintf, display &(Args[11]) to skip past argv[0] 
	// which we know will always be condor_exec
	dprintf(D_ALWAYS,"About to exec %s %s\n",JobName,
		&(Args[strlen(CONDOR_EXEC)]));

	set_priv ( priv );

	JobPid = daemonCore->Create_Process(JobName, Args, PRIV_USER_FINAL, 1,
				   FALSE, Env, Cwd, TRUE, NULL, fds, nice_inc );

	// now close the descriptors in fds array.  our child has inherited
	// them already, so we should close them so we do not leak descriptors.
	for (i=0;i<3;i++) {
		if ( fds[i] != -1 ) {
			close(fds[i]);
		}
	}

	if ( JobPid == FALSE ) {
		JobPid = -1;
		EXCEPT("ERROR Create_Process(%s,%s, ...) failed",
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

	dprintf( D_FULLDEBUG, "Inside OsProc::JobExit()\n" );

	if (JobPid == pid) {		
		if ( Requested_Exit == TRUE ) {
			reason = JOB_NOT_CKPTED;
		} else {
			reason = JOB_EXITED;
		}

		ClassAd ad;
		PublishUpdateAd( &ad );

		if( REMOTE_CONDOR_job_exit(status, reason, &ad) < 0 ) {   
			dprintf( D_ALWAYS, 
					 "Failed to send job exit status to Shadow.\n" );
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

bool
OsProc::ShutdownGraceful()
{
	if ( job_suspended == TRUE )
		Continue();

	Requested_Exit = TRUE;
	daemonCore->Send_Signal(JobPid, DC_SIGTERM);
	return false;	// return false says shutdown is pending	
}

bool
OsProc::ShutdownFast()
{
	// We purposely do not do a SIGCONT here, since there is no sense
	// in potentially swapping the job back into memory if our next
	// step is to hard kill it.
	Requested_Exit = TRUE;
	daemonCore->Send_Signal(JobPid, DC_SIGKILL);
	return false;	// return false says shutdown is pending
}


bool
OsProc::PublishUpdateAd( ClassAd* ad ) 
{
	dprintf( D_FULLDEBUG, "Inside OsProc::PublishUpdateAd()\n" );
		// Nothing special for us to do.
	return true;
}
