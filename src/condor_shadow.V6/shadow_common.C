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

/* 
** shadow_common.C: This horrible file is an attempt to share code
** between condor_shadow.V6 and condor_shadow.jim.  BE CAREFUL WHEN
** CALLING EXCEPT OR ASSERT IN THIS FILE!  What may be a fatal error
** in the condor_shadow.V6 may be a recoverable error in
** condor_shadow.jim (for example, failing to activate a claim).  */

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "condor_ckpt_name.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_attributes.h"
#include "condor_email.h"
#include "condor_commands.h"
#include "condor_config.h"
#include "../condor_ckpt_server/server_interface.h"

#include "daemon.h"
#include "stream.h"

#include "condor_debug.h"
#include "fileno.h"
#include "exit.h"
#include "shadow.h"
#include "job_report.h"

#if !defined( WCOREDUMP )
#define  WCOREDUMP(stat)      ((stat)&WCOREFLG)
#endif

extern "C" {
	int JobIsStandard();
	void NotifyUser( char *buf, PROC *proc );
	void publishNotifyEmail( FILE* mailer, char* buf, PROC* proc );
	void HoldJob( const char* buf );
	char *d_format_time( double dsecs );
	int unlink_local_or_ckpt_server( char *file );
	int whoami( FILE *fp );
	void get_local_rusage( struct rusage *bsd_rusage );
	void handle_termination( PROC *proc, char *notification,
				int *jobstatus, char *coredir );
	int DoCleanup();
}


#if defined(NEW_PROC)
        extern PROC    *Proc;
#else
        V2_PROC *Proc;
#endif

char *IpcFile;

extern int ExitReason;
extern int condor_rm_happend;
extern int JobExitStatus;
extern char    ICkptName[];
extern char    CkptName[];
extern char    TmpCkptName[];
extern int             MyPid;
extern char    *Spool;
extern char    RCkptName[];
extern int ShadowBDate;
extern int LastRestartTime;
extern int CommittedTime;
extern int NumCkpts;
extern int NumRestarts;
extern float TotalBytesSent, TotalBytesRecvd;
extern float BytesSent, BytesRecvd;
extern ReliSock *syscall_sock;

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif

#include "condor_qmgr.h"

extern int MainSymbolExists;
extern int JobStatus;
extern char    *MailerPgm;
extern char My_UID_Domain[];

ClassAd *JobAd = NULL;			// ClassAd which describes this job
extern char *schedd, *scheddName;
extern "C" bool JobPreCkptServerScheddNameChange();

void
NotifyUser( char *buf, PROC *proc )
{
        FILE *mailer;
        char subject[ BUFSIZ ];	

        dprintf(D_FULLDEBUG, "NotifyUser() called.\n");

		sprintf( subject, "Condor Job %d.%d", 
				 proc->id.cluster, proc->id.proc );

		if( ! JobAd ) {
			dprintf( D_ALWAYS, "In NotifyUser() w/ NULL JobAd!\n" );
			return;
		}

			// email HACK for John Bent <johnbent@cs.wisc.edu>
			// added by Derek Wright <wright@cs.wisc.edu> 2005-02-20
		char* email_cc = param( "EMAIL_NOTIFICATION_CC" );
		if( email_cc ) {
			bool allows_cc = true;
			int bool_val;
			if( JobAd->LookupBool(ATTR_ALLOW_NOTIFICATION_CC, bool_val) ) {
				dprintf( D_FULLDEBUG, "Job defined %s to %s\n",
						 ATTR_ALLOW_NOTIFICATION_CC,
						 bool_val ? "TRUE" : "FALSE" );
				allows_cc = (bool)bool_val;
			} else {
				dprintf( D_FULLDEBUG, "%s not defined, assuming TRUE\n",
						 ATTR_ALLOW_NOTIFICATION_CC );
			}
			if( allows_cc ) {
				dprintf( D_FULLDEBUG, "%s is TRUE, sending email to \"%s\"\n",
						 ATTR_ALLOW_NOTIFICATION_CC, email_cc );
				mailer = email_open( email_cc, subject );
				publishNotifyEmail( mailer, buf, proc );
				email_close( mailer );
			} else {
				dprintf( D_FULLDEBUG,
						 "%s is FALSE, not sending email copy\n",
						 ATTR_ALLOW_NOTIFICATION_CC );
			}
			free( email_cc );
			email_cc = NULL;
		}

        /* If user loaded program incorrectly, always send a message. */
        if( MainSymbolExists == TRUE ) {
                switch( proc->notification ) {
                case NOTIFY_NEVER:
                        return;
                case NOTIFY_ALWAYS:
                        break;
                case NOTIFY_COMPLETE:
                        if( proc->status == COMPLETED ) {
                                break;
                        } else {
                                return;
                        }
                case NOTIFY_ERROR:
                        if( (proc->status == COMPLETED) && (WTERMSIG(JobStatus)!= 0) ) {
                                break;
                        } else {
                                return;
                        }
                default:
                        dprintf(D_ALWAYS, "Condor Job %d.%d has a notification of %d\n",
                                        proc->id.cluster, proc->id.proc, proc->notification );
                }
        }

		mailer = email_user_open(JobAd, subject);
        if( mailer == NULL ) {
                dprintf(D_ALWAYS,
                        "Shadow: Cannot notify user( %s, %s, %s )\n",
                        subject, proc->owner, "w"
                );
				return;
        }
		publishNotifyEmail( mailer, buf, proc );
		email_close(mailer);
}


void
publishNotifyEmail( FILE* mailer, char* buf, PROC* proc )
{
	double rutime, rstime, lutime, lstime;  /* remote/local user/sys times */
	double trtime, tltime;  /* Total remote/local time */
	double  real_time;
	float run_time = 0.0;
	time_t arch_time=0;	/* time_t is 8 bytes some archs and 4 bytes on other
						   archs, and this means that doing a (time_t*)
						   cast on & of a 4 byte int makes my life hell.
						   So we fix it by assigning the time we want to
						   a real time_t variable, then using ctime()
						   to convert it to a string */

        fprintf(mailer, "Your condor job " );
#if defined(NEW_PROC)
		if ( proc->args[0] )
        	fprintf(mailer, "%s %s ", proc->cmd[0], proc->args[0] );
		else
        	fprintf(mailer, "%s ", proc->cmd[0] );
#else
        fprintf(mailer, "%s %s ", proc->cmd, proc->args );
#endif

        fprintf(mailer, "%s\n\n", buf );

	job_report_display_errors( mailer );

	arch_time = proc->q_date;
	fprintf(mailer, "\nTime:\n");
        fprintf(mailer, "\tSubmitted at:        %s", ctime(&arch_time));

        if( proc->completion_date ) {
                real_time = proc->completion_date - proc->q_date;
		arch_time = proc->completion_date;
                fprintf(mailer, "\tCompleted at:        %s\n",
						ctime(&arch_time));

                fprintf(mailer, "\tReal Time:           %s\n", 
					d_format_time(real_time));

				if (JobAd) {
					JobAd->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK, run_time);
				}
				run_time += proc->completion_date - ShadowBDate;
				
				fprintf(mailer, "\tRun Time:            %s\n",
						d_format_time(run_time));

				if (CommittedTime > 0) {
					fprintf(mailer, "\tCommitted Time:      %s\n",
							d_format_time(CommittedTime));
				}

        }
        fprintf( mailer, "\n" );

#if defined(NEW_PROC)
        rutime = proc->remote_usage[0].ru_utime.tv_sec;
        rstime = proc->remote_usage[0].ru_stime.tv_sec;
        trtime = rutime + rstime;
#else
        rutime = proc->remote_usage.ru_utime.tv_sec;
        rstime = proc->remote_usage.ru_stime.tv_sec;
        trtime = rutime + rstime;
#endif

        lutime = proc->local_usage.ru_utime.tv_sec;
        lstime = proc->local_usage.ru_stime.tv_sec;
        tltime = lutime + lstime;


        fprintf(mailer, "\tRemote User Time:    %s\n", d_format_time(rutime) );
        fprintf(mailer, "\tRemote System Time:  %s\n", d_format_time(rstime) );
        fprintf(mailer, "\tTotal Remote Time:   %s\n\n", d_format_time(trtime));
        fprintf(mailer, "\tLocal User Time:     %s\n", d_format_time(lutime) );
        fprintf(mailer, "\tLocal System Time:   %s\n", d_format_time(lstime) );
        fprintf(mailer, "\tTotal Local Time:    %s\n\n", d_format_time(tltime));

        if( tltime >= 1.0 ) {
                fprintf(mailer, "\tLeveraging Factor:   %2.1f\n", trtime / tltime);
        }

        fprintf(mailer, "\tVirtual Image Size:  %d Kilobytes\n", proc->image_size);

	if (NumCkpts > 0) {
		fprintf(mailer, "\nCheckpoints written: %d\n", NumCkpts);
		fprintf(mailer, "Checkpoint restarts: %d\n", NumRestarts);
	}

		// TotalBytesSent and TotalBytesRecvd are from the shadow's
		// perspective, and we want to display the stats from the job's
		// perspective.  Note also that TotalBytesSent and TotalBytesRecvd
		// don't include our current run, so we need to include the
		// stats from our syscall_sock (if we have one) and the BytesSent
		// and BytesRecvd variables.  This is ugly and confusing, which
		// explains why I keep getting it wrong.  :-(
	float network_bytes = TotalBytesSent + BytesSent;
	if (syscall_sock) {
		network_bytes += syscall_sock->get_bytes_sent();
	}
	if (network_bytes > 0.0) {
		fprintf(mailer,"\nNetwork:\n");
		fprintf(mailer,"\t%s read\n",metric_units(network_bytes));
		network_bytes = TotalBytesRecvd + BytesRecvd;
		if (syscall_sock) {
			network_bytes += syscall_sock->get_bytes_recvd();
		}
		fprintf(mailer,"\t%s written\n",metric_units(network_bytes));
	}

	if (JobIsStandard()) {
		job_report_display_file_info( mailer, (int) run_time );
		job_report_display_calls( mailer );
	}

	email_custom_attributes( mailer, JobAd );
}


int 
JobIsStandard()
{
	int universe;

	if(!JobAd->LookupInteger(ATTR_JOB_UNIVERSE, universe)) {
		return 0;
	} else {
		return universe==CONDOR_UNIVERSE_STANDARD;
	}
}


void
HoldJob( const char* buf )
{
    char subject[ BUFSIZ ];	
	FILE *mailer;

	sprintf( subject, "Condor Job %d.%d put on hold\n", 
			 Proc->id.cluster, Proc->id.proc );

	if( ! JobAd ) {
		dprintf( D_ALWAYS, "In HoldJob() w/ NULL JobAd!\n" );
		exit( JOB_SHOULD_HOLD );
	}
	mailer = email_user_open(JobAd, subject);
	if( ! mailer ) {
			// User didn't want email, so just exit now with the right
			// value so the schedd actually puts the job on hold.
		dprintf( D_ALWAYS, "Job going into Hold state.\n");
		dprintf( D_ALWAYS, "********** Shadow Exiting(%d) **********\n",
			JOB_SHOULD_HOLD);
		exit( JOB_SHOULD_HOLD );
	}

	fprintf( mailer, "Your condor job " );
	if( Proc->args[0] ) {
		fprintf( mailer, "%s %s ", Proc->cmd[0], Proc->args[0] );
	} else {
		fprintf( mailer, "%s ", Proc->cmd[0] );
	}
	fprintf( mailer, "\nis being put on hold.\n\n" );
	fprintf( mailer, "%s", buf );
	email_close(mailer);

		// Now that the user knows why, exit with the right code. 
	dprintf( D_ALWAYS, "Job going into Hold state.\n");
	dprintf( D_ALWAYS, "********** Shadow Exiting(%d) **********\n",
		JOB_SHOULD_HOLD);
	exit( JOB_SHOULD_HOLD );
}


/* A file we want to remove (partiucarly a checkpoint file) may actually be
   stored on the checkpoint server.  We'll make sure it gets removed from
   both places. JCP
*/

int
unlink_local_or_ckpt_server( char *file )
{
        int             rval;

        dprintf( D_ALWAYS, "Trying to unlink %s\n", file);
        rval = unlink( file );
        if (rval && JobIsStandard()) {
                /* Local failed, so lets try to do it on the server, maybe we
                   should do that anyway??? */
				/* We only need to check the server for standard universe 
				   jobs. -Jim B. */
			rval = RemoveRemoteFile(Proc->owner, scheddName, file);
			if (JobPreCkptServerScheddNameChange()) {
				rval = RemoveRemoteFile(Proc->owner, NULL, file);
			}
			dprintf( D_FULLDEBUG, "Remove from ckpt server returns %d\n",
					 rval );
        }
		return rval;
}


/*
** Print an identifier saying who we are.  This function gets handed to
** dprintf().
*/
int
whoami( FILE *fp )
{
        if ((Proc) && (Proc->id.cluster || Proc->id.proc)) {
                fprintf( fp, "(%d.%d) (%d):", Proc->id.cluster, Proc->id.proc, MyPid );
        } else {
                fprintf( fp, "(?.?) (%d):", MyPid );
        }
		return 0;
}

extern "C" int SetSyscalls(){ return 0; }

/*
Convert a time value from the POSIX style "clock_t" to a BSD style
"struct timeval".
*/
void
clock_t_to_timeval( clock_t ticks, struct timeval *tv )
{
#if defined(OSF1)
        static long clock_tick = CLK_TCK;
#else
        static long clock_tick = 0;

        if( !clock_tick ) {
                clock_tick = sysconf( _SC_CLK_TCK );
        }
#endif

        tv->tv_sec = ticks / clock_tick;
        tv->tv_usec = ticks % clock_tick;
        tv->tv_usec *= 1000000 / clock_tick;
}


void
get_local_rusage( struct rusage *bsd_rusage )
{
        clock_t         user;
        clock_t         sys;
        struct tms      posix_usage;

        memset( bsd_rusage, 0, sizeof(struct rusage) );

        times( &posix_usage );
        user = posix_usage.tms_utime + posix_usage.tms_cutime;
        sys = posix_usage.tms_stime + posix_usage.tms_cstime;

        dprintf( D_ALWAYS, "user_time = %d ticks\n", user );
        dprintf( D_ALWAYS, "sys_time = %d ticks\n", sys );
        clock_t_to_timeval( user, &bsd_rusage->ru_utime );
        clock_t_to_timeval( sys, &bsd_rusage->ru_stime );
}

void
handle_termination( PROC *proc, char *notification, int *jobstatus,
			char *coredir )
{
	char buf[4096];
	int status = *jobstatus;
	char my_coredir[_POSIX_PATH_MAX];
	dprintf(D_FULLDEBUG, "handle_termination() called.\n");

	ASSERT (JobAd != NULL );

	switch( WTERMSIG(status) ) {
	 case -1: /* On Digital Unix, WTERMSIG returns -1 if we weren't
				 killed by a sig.  This is the same case as sig 0 */  
	 case 0: /* If core, bad executable -- otherwise a normal exit */
		if( WCOREDUMP(status) && WEXITSTATUS(status) == ENOEXEC ) {
			(void)sprintf( notification, "is not executable." );
			dprintf( D_ALWAYS, "Shadow: Job file not executable\n" );
			ExitReason = JOB_KILLED;
		} else if( WCOREDUMP(status) && WEXITSTATUS(status) == 0 ) {
				(void)sprintf(notification,
"was killed because it was not properly linked for execution \nwith Version 6 Condor.\n" );
				MainSymbolExists = FALSE;
				ExitReason = JOB_KILLED;
		} else {
			(void)sprintf(notification, "exited with status %d.",
					WEXITSTATUS(status) );
			dprintf(D_ALWAYS, "Shadow: Job exited normally with status %d\n",
				WEXITSTATUS(status) );
			ExitReason = JOB_EXITED;
			JobExitStatus = WEXITSTATUS(status);
		}

		proc->status = COMPLETED;
		proc->completion_date = time( (time_t *)0 );

		sprintf(buf, "%s = FALSE", ATTR_ON_EXIT_BY_SIGNAL);
		JobAd->Insert(buf);
		sprintf(buf, "%s = %d", ATTR_ON_EXIT_CODE, WEXITSTATUS(status));
		JobAd->Insert(buf);

		break;

	 case SIGKILL:	/* Kicked off without a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was kicked off without a checkpoint\n" );
		DoCleanup();
		ExitReason = JOB_NOT_CKPTED;
#if 0 /* This is a problem for the new feature where we choose our executable
		 dynamically, so don't do it. */
		if( stat(ICkptName,&st_buf) < 0) {
			dprintf(D_ALWAYS,"No initial ckpt found\n");
			ExitReason = JOB_NO_CKPT_FILE;
		}
#endif
		/* in here, we disregard what the user wanted.
			Otherwise doing a condor_rm will result in the
			wanting to be resubmitted or held by the shadow. */
		sprintf(buf, "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK);
		JobAd->Insert(buf);
		sprintf(buf, "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK);
		JobAd->Insert(buf);
		break;

	 case SIGQUIT:	/* Kicked off, but with a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was checkpointed\n" );
		proc->status = IDLE;
		ExitReason = JOB_CKPTED;

		/* in here, we disregard what the user wanted.
			Otherwise doing a condor_vacate will result in the
			wanting to be resubmitted or held by the shadow. */
		sprintf(buf, "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK);
		JobAd->Insert(buf);
		sprintf(buf, "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK);
		JobAd->Insert(buf);
		break;

	 default:	/* Job exited abnormally */
		if (coredir == NULL) {
			coredir = my_coredir;
#if defined(Solaris)
			getcwd(coredir,_POSIX_PATH_MAX);
#else
			getwd( coredir );
#endif
		}
		if( WCOREDUMP(status) ) {
			if( strcmp(proc->rootdir, "/") == 0 ) {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s/core.%d.%d.",
					 WTERMSIG(status) ,
						coredir, proc->id.cluster, proc->id.proc);
			} else {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s%s/core.%d.%d.",
					 WTERMSIG(status) ,proc->rootdir, coredir, 
					 proc->id.cluster, proc->id.proc);
			}
			ExitReason = JOB_COREDUMPED;
		} else {
			(void)sprintf(notification,
				"was killed by signal %d.", WTERMSIG(status));
			ExitReason = JOB_KILLED;
		}
		dprintf(D_ALWAYS, "Shadow: %s\n", notification);

		proc->status = COMPLETED;
		proc->completion_date = time( (time_t *)0 );

		sprintf(buf, "%s = TRUE", ATTR_ON_EXIT_BY_SIGNAL);
		JobAd->Insert(buf);
		sprintf(buf, "%s = %d", ATTR_ON_EXIT_SIGNAL, WTERMSIG(status));
		JobAd->Insert(buf);

		break;
	}
}


void
checkForDebugging( ClassAd* ad )
{
	// For debugging, see if there's a special attribute in the
    // job ad that sends us into an infinite loop, waiting for
    // someone to attach with a debugger
    int shadow_should_wait = 0;
    ad->LookupInteger( ATTR_SHADOW_WAIT_FOR_DEBUG,
					   shadow_should_wait );
    if( shadow_should_wait ) {
        dprintf( D_ALWAYS, "Job requested shadow should wait for "
            "debugger with %s=%d, going into infinite loop\n",
            ATTR_SHADOW_WAIT_FOR_DEBUG, shadow_should_wait );
        while( shadow_should_wait );
    }
}


int
InitJobAd(int cluster, int proc)
{
  if (!JobAd) {   // just get the job ad from the schedd once

	if ( IpcFile ) {
		// get the jobad file a file
		priv_state priv = set_condor_priv();
		FILE* ipc_fp = fopen(IpcFile,"r");
		int isEOF = 0;
		int isError = 0;
		int isEmpty = 0;
		if ( ipc_fp ) {
			JobAd = new ClassAd(ipc_fp,"***",isEOF,isError,isEmpty);
			fclose(ipc_fp);
			unlink(IpcFile);
		}
		set_priv(priv);
		// if constructor failed, remove the JobAd, and
		// fallback on getting it via sockets.
		if (isError != 0 ) {
			if (JobAd) delete JobAd;
			JobAd = NULL;
		}
		if (JobAd) {
			// we're done
			checkForDebugging( JobAd );
			return 0;
		}

		// if we made it here, we wanted to get the job ad
		// via the file, but failed
		dprintf(D_FULLDEBUG,
			"Failed to read job ad file %s, using socket\n",
			IpcFile);
	}

	if (!ConnectQ(schedd, SHADOW_QMGMT_TIMEOUT, true)) {
		EXCEPT("Failed to connect to schedd!");
	}
	JobAd = GetJobAd( cluster, proc );
	DisconnectQ(NULL);
	checkForDebugging( JobAd );
  }
  if (!JobAd) {
	EXCEPT( "failed to get job ad" );
  }
  return 0;
}

int
part_send_job(
	      int test_starter,
	      char *host,
	      int &reason,
	      char *capability,
	      char *schedd,
	      PROC *proc,
	      int &sd1,
	      int &sd2,
	      char **name)
{
  int reply;
  ReliSock *sock = NULL;
  StartdRec stRec;
  PORTS ports;
  bool done = false;
  int retry_delay = 3;
  int num_retries = 0;

  // make sure we have the job classad
  InitJobAd(proc->id.cluster, proc->id.proc);

  while( !done ) {

	  Daemon startd(DT_STARTD, host, NULL);
	  if (!(sock = (ReliSock*)startd.startCommand ( ACTIVATE_CLAIM, Stream::reli_sock, 90))) {
		  dprintf( D_ALWAYS, "startCommand(ACTIVATE_CLAIM) to startd failed\n");
		  goto returnfailure;
	  }

		  // Send the capability
	  dprintf(D_FULLDEBUG, "send capability %s\n", capability);
	  if( !sock->code(capability) ) {
		  dprintf( D_ALWAYS, "sock->put(\"%s\") failed\n", capability );
		  goto returnfailure;
	  }

	  // Send the starter number
	  if( test_starter ) {
		  dprintf( D_ALWAYS, "Requesting Alternate Starter %d\n", test_starter );
	  } else {
		  dprintf( D_ALWAYS, "Requesting Primary Starter\n" );
	  }
	  if( !sock->code(test_starter) ) {
		  dprintf( D_ALWAYS, "sock->code(%d) failed\n", test_starter );
		  goto returnfailure;
	  }

		  // Send the job info 
	  if( !JobAd->put(*sock) ) {
		  dprintf( D_ALWAYS, "failed to send job ad\n" );
		  goto returnfailure;
	  }	

	  if( !sock->end_of_message() ) {
		  dprintf( D_ALWAYS, "failed to send message to startd\n" );
		  goto returnfailure;
	  }

		  // We're done sending.  Now, get the reply.
	  sock->decode();
	  if( !sock->code(reply) || !sock->end_of_message() ) {
		  dprintf( D_ALWAYS, "failed to receive reply from startd\n" );
		  goto returnfailure;
	  }
	  
	  switch( reply ) {
	  case OK:
		  dprintf( D_ALWAYS, "Shadow: Request to run a job was ACCEPTED\n" );
		  done = true;
		  break;

	  case NOT_OK:
		  dprintf( D_ALWAYS, "Shadow: Request to run a job was REFUSED\n");
		  goto returnfailure;
		  break;

	  case CONDOR_TRY_AGAIN:
		  num_retries++;
		  dprintf( D_ALWAYS,
				   "Shadow: Request to run a job was TEMPORARILY REFUSED\n" );
		  if( num_retries > 20 ) {
			  dprintf( D_ALWAYS, "Shadow: Too many retries, giving up.\n" );
			  goto returnfailure;
		  }			  
		  delete sock;
		  dprintf( D_ALWAYS,
				   "Shadow: will try again in %d seconds\n", retry_delay );
		  sleep( retry_delay );
		  break;

	  default:
		  dprintf(D_ALWAYS,"Unknown reply from startd for command ACTIVATE_CLAIM\n");
		  dprintf(D_ALWAYS,"Shadow: Request to run a job was REFUSED\n");
		  goto returnfailure;
		  break;
	  }
  }

  free(capability);
               
  /* start flock : dhruba */
  sock->decode();
  memset( &stRec, '\0', sizeof(stRec) );
  if( !sock->code(stRec) || !sock->end_of_message() ) {
	  dprintf(D_ALWAYS, "Can't read reply from startd.\n");
	  goto returnfailure;
  }
  ports = stRec.ports;
  if( stRec.ip_addr ) {
	host = stRec.server_name;
	if(name) {
		*name = strdup(stRec.server_name);
	}
    dprintf(D_FULLDEBUG,
	    "host = %s inet_addr = 0x%x port1 = %d port2 = %d\n",
	    host, stRec.ip_addr,ports.port1, ports.port2
	    );
  } else {
    dprintf(D_FULLDEBUG,
	    "host = %s port1 = %d port2 = %d\n",
	    host, ports.port1, ports.port2
	    );
  }    

  if( ports.port1 == 0 ) {
    dprintf( D_ALWAYS, "Shadow: Request to run a job on %s was REFUSED\n",
	     host );
	goto returnfailure;
  }
  /* end  flock ; dhruba */

	  // We don't use the server_name in the StartdRec, because our
	  // DNS query may fail or may give us the wrong IP address
	  // (either because it's stale or because we're talking to a
	  // machine with multiple network interfaces).  Sadly, we can't
	  // use the ip_addr either, because the startd doesn't send it in
	  // the correct byte ordering on little-endian machines.  So, we
	  // grab the IP address from the ReliSock, since we konw the
	  // startd always uses the same IP address for all of its
	  // communication.
  char sinfulstring[40];

  // We can't use snprintf as it isn't availble on all platforms. We'll put
  // it in the util lib post-6.2.0
  // epaulson 7-21-2000
  //snprintf(sinfulstring, 40, "<%s:%d>", sock->endpoint_ip_str(), ports.port1);
  sprintf(sinfulstring, "<%s:%d>", sock->endpoint_ip_str(), ports.port1);
  if( (sd1 = do_connect(sinfulstring, (char *)0, (u_short)ports.port1)) < 0 ) {
    dprintf( D_ALWAYS, "failed to connect to scheduler on %s\n", sinfulstring );
	goto returnfailure;
  }
 
  // No snprintf. See above - epaulson 7-21-2000
  //snprintf(sinfulstring, 40, "<%s:%d>", sock->endpoint_ip_str(), ports.port2);
  sprintf(sinfulstring, "<%s:%d>", sock->endpoint_ip_str(), ports.port2);
  if( (sd2 = do_connect(sinfulstring, (char *)0, (u_short)ports.port2)) < 0 ) {
    dprintf( D_ALWAYS, "failed to connect to scheduler on %s\n", sinfulstring );
	close(sd1);
	goto returnfailure;
  }

  delete sock;
  sock = NULL;

  if ( stRec.server_name ) {
	  free( stRec.server_name );
  }

  return 0;

returnfailure:
  reason = JOB_NOT_STARTED;
  delete sock;
  return -1;
}


/*
  Takes sinful address of startd and sends it the given cmd, along
  with the capability and an end_of_message.
*/
int
send_cmd_to_startd(char *sin_host, char *capability, int cmd)
{
	ReliSock* sock = NULL;

	Daemon startd (DT_STARTD, sin_host, NULL);
	if (!(sock = (ReliSock*)startd.startCommand(cmd, Stream::reli_sock, 20))) {
		dprintf( D_ALWAYS, "Can't connect to startd at %s\n", sin_host );
		return -1;
	}

    
  // send the capability
  dprintf(D_FULLDEBUG, "send capability %s\n", capability);
  if(!sock->code(capability)){
    dprintf( D_ALWAYS, "sock->code(%s) failed.\n", capability );
    delete sock;
    return -3;
  }

  // send end of message
  if( !sock->end_of_message() ) {
    dprintf( D_ALWAYS, "end_of_message failed\n" );
    delete sock;
    return -4;
  }
  dprintf( D_FULLDEBUG, "Sent command %d to startd at %s with cap %s\n",
		   cmd, sin_host, capability );

  delete sock;

  return 0;
}

/* Fill in a PROC structure given a job ClassAd.  This function replaces
   GetProc from qmgr_lib_support.C, so we just get the job classad once
   from the schedd and fill in the PROC structure directly. */
int
MakeProc(ClassAd *ad, PROC *p)
{
	char buf[ATTRLIST_MAX_EXPRESSION];
	float	utime,stime;
	char	*s;
	ExprTree *e;
	
	p->version_num = 3;
	ad->LookupInteger(ATTR_CLUSTER_ID, p->id.cluster);
	ad->LookupInteger(ATTR_PROC_ID, p->id.proc);
	ad->LookupInteger(ATTR_JOB_UNIVERSE, p->universe);
	ad->LookupInteger(ATTR_WANT_CHECKPOINT, p->checkpoint);
	ad->LookupInteger(ATTR_WANT_REMOTE_SYSCALLS, p->remote_syscalls);
	ad->LookupString(ATTR_OWNER, buf);
	p->owner = strdup(buf);
	ad->LookupInteger(ATTR_Q_DATE, p->q_date);
	ad->LookupInteger(ATTR_COMPLETION_DATE, p->completion_date);
	ad->LookupInteger(ATTR_JOB_STATUS, p->status);
	ad->LookupInteger(ATTR_JOB_PRIO, p->prio);
	ad->LookupInteger(ATTR_JOB_NOTIFICATION, p->notification);
	ad->LookupInteger(ATTR_IMAGE_SIZE, p->image_size);
	// This is the old way of getting the string, and it's bad, bad, bad.
   	// It doesn't do well if you have strings > 10K. 
	// ad->LookupString(ATTR_JOB_ENVIRONMENT, buf);
	// p->env = strdup(buf);

	// Note that we are using the new version of LookupString here.
	// It allocates a new string of the correct length with malloc()
	// and returns that string, so we no longer copy it. 
	ad->LookupString(ATTR_JOB_ENVIRONMENT, &(p->env));

	p->n_cmds = 1;
	p->cmd = (char **) malloc(p->n_cmds * sizeof(char *));
	p->args = (char **) malloc(p->n_cmds * sizeof(char *));
	p->in = (char **) malloc(p->n_cmds * sizeof(char *));
	p->out = (char **) malloc(p->n_cmds * sizeof(char *));
	p->err = (char **) malloc(p->n_cmds * sizeof(char *));
	p->exit_status = (int *) malloc(p->n_cmds * sizeof(int));

	ad->LookupString(ATTR_JOB_CMD, buf);
	p->cmd[0] = strdup(buf);
	ad->LookupString(ATTR_JOB_ARGUMENTS, buf);
	p->args[0] = strdup(buf);
	ad->LookupString(ATTR_JOB_INPUT, buf);
	p->in[0] = strdup(buf);
	ad->LookupString(ATTR_JOB_OUTPUT, buf);
	p->out[0] = strdup(buf);
	ad->LookupString(ATTR_JOB_ERROR, buf);
	p->err[0] = strdup(buf);
	ad->LookupInteger(ATTR_JOB_EXIT_STATUS, p->exit_status[0]);

	ad->LookupInteger(ATTR_MIN_HOSTS, p->min_needed);
	ad->LookupInteger(ATTR_MAX_HOSTS, p->max_needed);

	ad->LookupString(ATTR_JOB_ROOT_DIR, buf);
	p->rootdir = strdup(buf);
	ad->LookupString(ATTR_JOB_IWD, buf);
	p->iwd = strdup(buf);

	e = ad->Lookup(ATTR_REQUIREMENTS);
	if (e) {
		buf[0] = '\0';
		e->PrintToStr(buf);
		s = strchr(buf, '=');
		if (s) {
			s++;
			p->requirements = strdup(s);
		} 
		else {
			p->requirements = strdup(buf);
		}
	} else {
	   p->requirements = NULL;
	}
	e = ad->Lookup(ATTR_RANK);
	if (e) {
		buf[0] = '\0';
		e->PrintToStr(buf);
		s = strchr(buf, '=');
		if (s) {
			s++;
			p->preferences = strdup(s);
		} 
		else {
			p->preferences = strdup(buf);
		}
	} else {
		p->preferences = NULL;
	}

	ad->LookupFloat(ATTR_JOB_LOCAL_USER_CPU, utime);
	ad->LookupFloat(ATTR_JOB_LOCAL_SYS_CPU, stime);
	float_to_rusage(utime, stime, &(p->local_usage));

	p->remote_usage = (struct rusage *) malloc(p->n_cmds * 
		sizeof(struct rusage));

	memset(p->remote_usage, 0, sizeof( struct rusage ));

	ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU, utime);
	ad->LookupFloat(ATTR_JOB_REMOTE_SYS_CPU, stime);
	float_to_rusage(utime, stime, &(p->remote_usage[0]));
	
	return 0;
}


