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
#include "condor_io.h"
#include "condor_ckpt_name.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_attributes.h"
#include "condor_email.h"
#include "../condor_ckpt_server/server_interface.h"

#include "sched.h"
#include "debug.h"
#include "fileno.h"
#include "files.h"
#include "exit.h"
#include "shadow.h"

#if !defined( WCOREDUMP )
#define  WCOREDUMP(stat)      ((stat)&WCOREFLG)
#endif

extern "C" {
	void NotifyUser( char *buf, PROC *proc, char *email_addr );
	void display_errors( FILE *fp );
	char *d_format_time( double dsecs );
	int unlink_local_or_ckpt_server( char *file );
	void rm();
	int whoami();
	void MvTmpCkpt();
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

extern int ChildPid;
extern int ExitReason;
extern int JobExitStatus;
extern char    ICkptName[];
extern char    CkptName[];
extern char    TmpCkptName[];
extern int             MyPid;
extern char    *Spool;
extern char    RCkptName[];
extern int LastRestartTime;
extern int CommittedTime;
extern int NumCkpts;
extern int NumRestarts;
extern float TotalBytesSent, TotalBytesRecvd;

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif

#include "condor_qmgr.h"

extern int MainSymbolExists;
extern int JobStatus;
extern char    *MailerPgm;
extern char My_UID_Domain[];

ClassAd *JobAd = NULL;			// ClassAd which describes this job
extern char *schedd;

void
NotifyUser( char *buf, PROC *proc, char *email_addr )
{
        FILE *mailer;
        char subject[ BUFSIZ ];	
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

        dprintf(D_FULLDEBUG, "NotifyUser() called.\n");


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

		sprintf(subject, "Condor Job %d.%d\n", 
				proc->id.cluster, proc->id.proc);

        dprintf( D_FULLDEBUG, "Notify user using address: %s, subject: %s",
			email_addr, subject);

		mailer = email_open(email_addr, subject);
        if( mailer == NULL ) {
                EXCEPT(
                        "Shadow: Cannot notify user( %s, %s, %s )\n",
                        subject, proc->owner, "w"
                );
        }

        /* if we got the subject line set correctly earlier, and we
           got the from line generated by setting our uid, then we
           don't need this stuff...
        */

        fprintf(mailer, "Your condor job\n" );
#if defined(NEW_PROC)
		if ( proc->args[0] )
        	fprintf(mailer, "\t%s %s\n", proc->cmd[0], proc->args[0] );
		else
        	fprintf(mailer, "\t%s\n", proc->cmd[0] );
#else
        fprintf(mailer, "\t%s %s\n", proc->cmd, proc->args );
#endif

        fprintf(mailer, "%s\n\n", buf );

        display_errors( mailer );

		arch_time = proc->q_date;
        fprintf(mailer, "Submitted at:        %s", ctime(&arch_time));

        if( proc->completion_date ) {
                real_time = proc->completion_date - proc->q_date;

				arch_time = proc->completion_date;
                fprintf(mailer, "Completed at:        %s\n",
						ctime(&arch_time));

                fprintf(mailer, "Real Time:           %s\n", 
					d_format_time(real_time));

				if (JobAd) {
					JobAd->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK, run_time);
				}
				run_time += proc->completion_date - LastRestartTime;
				
				fprintf(mailer, "Run Time:            %s\n",
						d_format_time(run_time));

				if (CommittedTime > 0) {
					fprintf(mailer, "Committed Time:      %s\n",
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


        fprintf(mailer, "Remote User Time:    %s\n", d_format_time(rutime) );
        fprintf(mailer, "Remote System Time:  %s\n", d_format_time(rstime) );
        fprintf(mailer, "Total Remote Time:   %s\n\n", d_format_time(trtime));
        fprintf(mailer, "Local User Time:     %s\n", d_format_time(lutime) );
        fprintf(mailer, "Local System Time:   %s\n", d_format_time(lstime) );
        fprintf(mailer, "Total Local Time:    %s\n\n", d_format_time(tltime));

        if( tltime >= 1.0 ) {
                fprintf(mailer, "Leveraging Factor:   %2.1f\n", trtime / tltime);
        }
        fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n", proc->image_size);

		if (NumCkpts > 0) {
			fprintf(mailer, "Checkpoints written: %d\n", NumCkpts);
			fprintf(mailer, "Checkpoint restarts: %d\n", NumRestarts);
		}

		if (TotalBytesSent > 0.0) {
			// TotalBytesSent and TotalBytesRecvd are from the shadow's
			// perspective, and we want to display the stats from the job's
			// perspective.
			fprintf(mailer,
					"Network Usage:       %.0f Kilobytes read\n"
					"                     %.0f Kilobytes written\n",
					TotalBytesSent/1024.0, TotalBytesRecvd/1024.0);
		}

		email_close(mailer);
}


/* A file we want to remove (partiucarly a checkpoint file) may actually be
   stored on the checkpoint server.  We'll make sure it gets removed from
   both places. JCP
*/

unlink_local_or_ckpt_server( char *file )
{
        int             rval;

        dprintf( D_ALWAYS, "Trying to unlink %s\n", file);
        rval = unlink( file );
        if (rval) {
                /* Local failed, so lets try to do it on the server, maybe we
                   should do that anyway??? */
                dprintf( D_FULLDEBUG, "Remove from ckpt server returns %d\n",
                                RemoveRemoteFile(Proc->owner, file));
        }
}


void
rm()
{
        dprintf( D_ALWAYS, "Shadow: Got RM command *****\n" );

        if( ChildPid ) {
                (void) kill( ChildPid, SIGKILL );
        }

        if( strcmp(Proc->rootdir, "/") != 0 ) {
                (void)sprintf( TmpCkptName, "%s/job%06d.ckpt.%d",
                                        Proc->rootdir, Proc->id.cluster, Proc->id.proc );
        }

		if( TmpCkptName[0] != '\0' ) {
			dprintf(D_ALWAYS, "Shadow: DoCleanup: unlinking TmpCkpt '%s'\n",
					TmpCkptName);
			(void) unlink_local_or_ckpt_server( TmpCkptName );
		}

        exit( JOB_KILLED );
}

/*
** Print an identifier saying who we are.  This function gets handed to
** dprintf().
*/
whoami( FILE *fp )
{
        if ((Proc) && (Proc->id.cluster || Proc->id.proc)) {
                fprintf( fp, "(%d.%d) (%d):", Proc->id.cluster, Proc->id.proc, MyPid );
        } else {
                fprintf( fp, "(?.?) (%d):", MyPid );
        }
}

void
MvTmpCkpt()
{
        char buf[ BUFSIZ * 8 ];
        register int tfd, rfd, rcnt, wcnt;
        priv_state      priv;

        /* checkpoint file is owned by user (???) */
        priv = set_user_priv();

        (void)sprintf( TmpCkptName, "%s/job%06d.ckpt.%d.tmp",
                        Spool, Proc->id.cluster, Proc->id.proc );

        (void)sprintf( RCkptName, "%s/job%06d.ckpt.%d",
                                Proc->rootdir, Proc->id.cluster, Proc->id.proc )
;

        rfd = open( RCkptName, O_RDONLY, 0 );
        if( rfd < 0 ) {
                EXCEPT(RCkptName);
        }

        tfd = open(TmpCkptName, O_WRONLY|O_CREAT|O_TRUNC, 0775);
        if( tfd < 0 ) {
                EXCEPT(TmpCkptName);
        }

        for(;;) {
                rcnt = read( rfd, buf, sizeof(buf) );
                if( rcnt < 0 ) {
                        EXCEPT("read <%s>", RCkptName);
                }

                wcnt = write( tfd, buf, rcnt );
                if( wcnt != rcnt ) {
                        EXCEPT("wcnt %d != rcnt %d", wcnt, rcnt);
                }

                if( rcnt != sizeof(buf) ) {
                        break;
                }
        }

        (void)unlink_local_or_ckpt_server( RCkptName );

        if( rename(TmpCkptName, CkptName) < 0 ) {
                EXCEPT("rename(%s, %s)", TmpCkptName, CkptName);
        }

        /* (void)fchown( tfd, CondorUid, CondorGid ); */

        (void)close( rfd );
        (void)close( tfd );

        set_priv(priv);
}

extern "C" SetSyscalls(){}

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
	int status = *jobstatus;
	struct stat st_buf;
	char my_coredir[_POSIX_PATH_MAX];
	dprintf(D_FULLDEBUG, "handle_termination() called.\n");

	switch( WTERMSIG(status) ) {
	 case -1: /* On Digital Unix, WTERMSIG returns -1 if we weren't
				 killed by a sig.  This is the same case as sig 0 */  
	 case 0: /* If core, bad executable -- otherwise a normal exit */
		if( WCOREDUMP(status) && WEXITSTATUS(status) == ENOEXEC ) {
			(void)sprintf( notification, "is not executable." );
			dprintf( D_ALWAYS, "Shadow: Job file not executable" );
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
		break;

	 case SIGQUIT:	/* Kicked off, but with a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was checkpointed\n" );
		if( strcmp(proc->rootdir, "/") != 0 ) {
			MvTmpCkpt();
		}
		proc->status = IDLE;
		ExitReason = JOB_CKPTED;
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
		break;
	}
}

int
InitJobAd(int cluster, int proc)
{
  if (!JobAd) {   // just get the job ad from the schedd once
	  if (!ConnectQ(schedd, SHADOW_QMGMT_TIMEOUT, true)) {
		EXCEPT("Failed to connect to schedd!");
	  }
  	JobAd = GetJobAd( cluster, proc );
  	DisconnectQ(NULL);
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
  int cmd, sd, reply;
  ReliSock *sock;
  StartdRec stRec;
  PORTS ports;
  bool done = false;
  int retry_delay = 3;
  int num_retries = 0;

  cmd = ACTIVATE_CLAIM; // new protocol in V6 startd

  // make sure we have the job classad
  InitJobAd(proc->id.cluster, proc->id.proc);

  while( !done ) {

		  // Connect to the startd
	  sock = new ReliSock();
	  sock->timeout(90);
	  if ( sock->connect(host,START_PORT) == FALSE ) {
		  reason = JOB_NOT_STARTED;
		  delete sock;
		  return -1;
	  }

	  sock->encode();

		  // Send the command
	  if( !sock->code(cmd) ) {
		  EXCEPT( "sock->code(%d)", cmd );
	  }

		  // Send the capability
	  dprintf(D_FULLDEBUG, "send capability %s\n", capability);
	  if( !sock->code(capability) ) {
		  EXCEPT( "sock->put()" );
	  }

	  // Send the starter number
	  if( test_starter ) {
		  dprintf( D_ALWAYS, "Requesting Alternate Starter %d\n", test_starter );
	  } else {
		  dprintf( D_ALWAYS, "Requesting Primary Starter\n" );
	  }
	  if( !sock->code(test_starter) ) {
		  EXCEPT( "sock->code(%d)", test_starter );
	  }

		  // Send the job info 
	  if( !JobAd->put(*sock) ) {
		  EXCEPT( "failed to send job ad" );
	  }	

	  if( !sock->end_of_message() ) {
		  EXCEPT( "end_of_message failed" );
	  }

		  // We're done sending.  Now, get the reply.
	  sock->decode();
	  ASSERT( sock->code(reply) );
	  ASSERT( sock->end_of_message() );
	  
	  switch( reply ) {
	  case OK:
		  dprintf( D_ALWAYS, "Shadow: Request to run a job was ACCEPTED\n" );
		  done = true;
		  break;

	  case NOT_OK:
		  dprintf( D_ALWAYS, "Shadow: Request to run a job was REFUSED\n");
		  reason = JOB_NOT_STARTED;
		  delete sock;
		  return -1;
		  break;

	  case CONDOR_TRY_AGAIN:
		  num_retries++;
		  delete sock;
		  dprintf( D_ALWAYS,
				   "Shadow: Request to run a job was TEMPORARILY REFUSED\n" );
		  if( num_retries > 20 ) {
			  dprintf( D_ALWAYS, "Shadow: Too many retries, giving up.\n" );
			  reason = JOB_NOT_STARTED;
			  return -1;
		  }			  
		  dprintf( D_ALWAYS,
				   "Shadow: will try again in %d seconds\n", retry_delay );
		  sleep( retry_delay );
		  break;

	  default:
		  dprintf(D_ALWAYS,"Unknown reply from startd for command ACTIVATE_CLAIM\n");
		  dprintf(D_ALWAYS,"Shadow: Request to run a job was REFUSED\n");
		  reason = JOB_NOT_STARTED;
		  delete sock;
		  return -1;
		  break;
	  }
  }

  free(capability);
               
  /* start flock : dhruba */
  sock->decode();
  memset( &stRec, '\0', sizeof(stRec) );
  ASSERT( sock->code(stRec));
  ASSERT( sock->end_of_message() );
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
    reason = JOB_NOT_STARTED;
	delete sock;
    return -1;
  }
  /* end  flock ; dhruba */

  delete sock;
  if( (sd1 = do_connect(host, (char *)0, (u_short)ports.port1)) < 0 ) {
    EXCEPT( "connect to scheduler on \"%s\", port1 = %d", host, ports.port1 );
  }
  
  if( (sd2 = do_connect(host, (char *)0, (u_short)ports.port2)) < 0 ) {
    EXCEPT( "connect to scheduler on \"%s\", port2 = %d", host, ports.port2 );
  }

  if ( stRec.server_name ) {
	  free( stRec.server_name );
  }

  return 0;
}

// takes sinful address of startd and sends it a cmd
int
send_cmd_to_startd(char *sin_host, char *capability, int cmd)
{
  // create a relisock to communicate with startd
  ReliSock sock;
  if( sock.timeout( 20 ) < 0 ) {
	  dprintf( D_ALWAYS, "Can't set timeout on sock.\n" );
	  return -5;
  }

  if( !sock.connect( sin_host, 0 ) ) {
	  dprintf( D_ALWAYS, "Can't connect to startd at %s\n", sin_host );
	  return -1;
  }

  sock.encode();

  // Send the command
  if( !sock.code(cmd) ) {
    dprintf( D_ALWAYS, "sock.code(%d)", cmd );
    return -2;
  }
  
  // send the capability
  dprintf(D_FULLDEBUG, "send capability %s\n", capability);
  if(!sock.code(capability)){
    dprintf( D_ALWAYS, "sock.code(%s)", capability );
    return -3;
  }

  // send end of message
  if( !sock.end_of_message() ) {
    dprintf( D_ALWAYS, "end_of_message failed" );
    return -4;
  }
  dprintf( D_FULLDEBUG, "Sent command %d to startd at %s with cap %s\n",
		   cmd, sin_host, capability );

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
	ad->LookupString(ATTR_JOB_ENVIRONMENT, buf);
	p->env = strdup(buf);

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
