/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#define _POSIX_SOURCE

#if defined(IRIX62)
#include "condor_fdset.h"
#endif 

#include "condor_common.h"
#include "condor_fdset.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "condor_ckpt_name.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_attributes.h"

#include <signal.h>
#include <pwd.h>
#include <netdb.h>

#if defined(AIX31) || defined(AIX32)
#include "syscall.aix.h"
#endif

#ifdef IRIX331
#include <sys.s>
#endif

#if !defined(AIX31) && !defined(AIX32)  && !defined(IRIX331) && !defined(IRIX53) && !defined(Solaris)
#include <syscall.h>
#endif

#if defined(Solaris)
#include <sys/syscall.h>
#include </usr/ucbinclude/sys/wait.h>
#include <fcntl.h>
#endif

#include <sys/uio.h>
#if !defined(LINUX)
#include <sys/buf.h>
#endif
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/resource.h>

#include "condor_fix_string.h"
#include "sched.h"
#include "debug.h"
#include "fileno.h"
#include "files.h"
#include "exit.h"

#if defined(AIX32)
#	include <sys/id.h>
#   include <sys/wait.h>
#   include <sys/m_wait.h>
#	include "condor_fdset.h"
#endif

#if defined(HPUX9)
#	include <sys/wait.h>
#else
#	include "condor_fix_wait.h"
#endif

#undef _POSIX_SOURCE
#include "condor_types.h"

#include "clib.h"
#include "shadow.h"
#include "subproc.h"
#include "afs.h"

#include "condor_constants.h"

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
				union wait *jobstatus, char *coredir );
}

extern int getJobAd(int cluster_id, int proc_id, ClassAd *new_ad);


#if defined(NEW_PROC)
        extern PROC    *Proc;
#else
        V2_PROC *Proc;
#endif

extern int ChildPid;
extern int ExitReason;
extern char    CkptName[];
extern char    TmpCkptName[];
extern int             MyPid;
extern char    *Spool;
extern char    RCkptName[];

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif

#include "condor_qmgr.h"

extern int MainSymbolExists;
extern union wait JobStatus;
extern char    *MailerPgm;
extern char My_UID_Domain[];
extern char *_FileName_;

char *
d_format_time( double dsecs )
{
        int days, hours, minutes, secs;
        static char answer[25];

#define SECONDS 1
#define MINUTES (60 * SECONDS)
#define HOURS   (60 * MINUTES)
#define DAYS    (24 * HOURS)

        secs = (int)dsecs;

        days = secs / DAYS;
        secs %= DAYS;

        hours = secs / HOURS;
        secs %= HOURS;

        minutes = secs / MINUTES;
        secs %= MINUTES;

        (void)sprintf(answer, "%d %02d:%02d:%02d", days, hours, minutes, secs);

        return( answer );
}

void
NotifyUser( char *buf, PROC *proc, char *email_addr )
{
        FILE *mailer;
        char cmd[ BUFSIZ ];
        double rutime, rstime, lutime, lstime;  /* remote/local user/sys times */
        double trtime, tltime;  /* Total remote/local time */
        double  real_time;

        dprintf(D_FULLDEBUG, "NotifyUser() called.\n");

                /* Want the letter to come from "condor" */
        set_condor_priv();

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
                        if( (proc->status == COMPLETED) && (JobStatus.w_termsig != 0) ) {
                                break;
                        } else {
                                return;
                        }
                default:
                        dprintf(D_ALWAYS, "Condor Job %d.%d has a notification of %d\n",
                                        proc->id.cluster, proc->id.proc, proc->notification );
                }
        }

        if ( strchr(email_addr,'@') == NULL )
        {
                /* No host name specified; add uid domain */
                /* Note that if uid domain was set by the admin to "none", then */
                /* earlier we have already set it to be the submit machine's name */
                sprintf(cmd, "%s -s \"Condor Job %d.%d\" %s@%s\n",
                               MailerPgm, proc->id.cluster, proc->id.proc, email_addr,
                               My_UID_Domain
                               );
        }
        else
        {
            /* host name specified; don't add uid domain */
            sprintf(cmd, "%s -s \"Condor Job %d.%d\" %s\n",
                        MailerPgm, proc->id.cluster, proc->id.proc, email_addr);
        }

        dprintf( D_FULLDEBUG, "Notify user using cmd: %s",cmd);

        mailer = popen( cmd, "w" );
        if( mailer == NULL ) {
                EXCEPT(
                        "Shadow: Cannot do execute_program( %s, %s, %s )\n",
                        cmd, proc->owner, "w"
                );
        }

        /* if we got the subject line set correctly earlier, and we
           got the from line generated by setting our uid, then we
           don't need this stuff...
        */

        fprintf(mailer, "Your condor job\n" );
#if defined(NEW_PROC)
#if defined(Solaris)
        fprintf(mailer, "\t%s\n", proc->cmd[0] );
#else
        fprintf(mailer, "\t%s %s\n", proc->cmd[0], proc->args[0] );
#endif
#else
        fprintf(mailer, "\t%s %s\n", proc->cmd, proc->args );
#endif

        fprintf(mailer, "%s\n\n", buf );

        display_errors( mailer );

        fprintf(mailer, "Submitted at:        %s", ctime( (time_t *)&proc->q_date) );
        if( proc->completion_date ) {
                real_time = proc->completion_date - proc->q_date;
                fprintf(mailer, "Completed at:        %s",
                                                                        ctime((time_t *)&proc->completion_date) );
                fprintf(mailer, "Real Time:           %s\n", d_format_time(real_time));
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

        (void)pclose( mailer );
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

        (void) unlink_local_or_ckpt_server( TmpCkptName );
        (void) unlink_local_or_ckpt_server( CkptName );

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

#include <sys/resource.h>
#define _POSIX_SOURCE
#       if defined(OSF1)
#               include <time.h>                        /* need POSIX CLK_TCK */
#       elif !defined(HPUX9)
#               define _SC_CLK_TCK      3               /* shouldn't do this */
#       endif
#undef _POSIX_SOURCE
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


#define _POSIX_SOURCE
#include <sys/times.h>          /* need POSIX struct tms */
#undef _POSIX_SOURCE
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
handle_termination( PROC *proc, char *notification, union wait *jobstatus,
			char *coredir )
{
	char my_coredir[4096];
	dprintf(D_FULLDEBUG, "handle_termination() called.\n");

	switch( jobstatus->w_termsig ) {

	 case 0: /* If core, bad executable -- otherwise a normal exit */
		if( jobstatus->w_coredump && jobstatus->w_retcode == ENOEXEC ) {
			(void)sprintf( notification, "is not executable." );
			dprintf( D_ALWAYS, "Shadow: Job file not executable" );
			ExitReason = JOB_EXCEPTION;
		} else if( jobstatus->w_coredump && jobstatus->w_retcode == 0 ) {
				(void)sprintf(notification,
"was killed because it was not properly linked for execution \nwith Version 6 Condor.\n" );
				MainSymbolExists = FALSE;
				ExitReason = JOB_EXCEPTION;
		} else {
			(void)sprintf(notification, "exited with status %d.",
					jobstatus->w_retcode );
			dprintf(D_ALWAYS, "Shadow: Job exited normally with status %d\n",
				jobstatus->w_retcode );
			ExitReason = JOB_EXITED;
		}

		proc->status = COMPLETED;
		proc->completion_date = time( (time_t *)0 );
		break;

	 case SIGKILL:	/* Kicked off without a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was kicked off without a checkpoint\n" );
		DoCleanup();
		ExitReason = JOB_NOT_CKPTED;
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
		if( jobstatus->w_coredump ) {
			if( strcmp(proc->rootdir, "/") == 0 ) {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s/core.%d.%d.",
					 jobstatus->w_termsig,
						coredir, proc->id.cluster, proc->id.proc);
			} else {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s%s/core.%d.%d.",
					 jobstatus->w_termsig,proc->rootdir, coredir, 
					 proc->id.cluster, proc->id.proc);
			}
			ExitReason = JOB_COREDUMPED;
		} else {
			(void)sprintf(notification,
				"was killed by signal %d.", jobstatus->w_termsig);
			ExitReason = JOB_KILLED;
		}
		dprintf(D_ALWAYS, "Shadow: %s\n", notification);

		proc->status = COMPLETED;
		proc->completion_date = time( (time_t *)0 );
		break;
	}
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
  
#ifdef 0
  if (test_starter) {
    cmd = SCHED_VERS + ALT_STARTER_BASE + test_starter;
    dprintf( D_ALWAYS, "Requesting Test Starter %d\n", test_starter );
  } else {
    cmd = START_FRGN_JOB;
    dprintf( D_ALWAYS, "Requesting Standard Starter\n" );
  }
#else
  cmd = ACTIVATE_CLAIM; // new protocol in V6 startd
#endif 

  /* Connect to the startd */
  sd = do_connect_with_timeout(host, "condor_startd", START_PORT, 90);
  if( sd < 0 ) {
    reason = JOB_NOT_STARTED;
    return -1;
  }
  
  sock = new ReliSock();
  sock->attach_to_file_desc(sd);
  sock->encode();

  /* Send the command */
  if( !sock->code(cmd) ) {
    EXCEPT( "sock->code(%d)", cmd );
  }

  /* send the capability */
  dprintf(D_FULLDEBUG, "send capability %s\n", capability);
  if(!sock->code(capability)) {
    EXCEPT( "sock->put()" );
  }
	  // Do we really want to free this here?
  free(capability);
               
  /* send the starter number */
  dprintf( D_ALWAYS, "Requesting Test Starter %d\n", test_starter );
  if( !sock->code(test_starter) ) {
    EXCEPT( "sock->code(%d)", test_starter );
  }

  /* Send the job info */
  ClassAd ad;
  ConnectQ(schedd);
  getJobAd( proc->id.cluster, proc->id.proc, &ad );
  DisconnectQ(NULL);
  if( !ad.put(*sock) ) {
    EXCEPT( "failed to send job ad" );
  }
  if( !sock->end_of_message() ) {
    EXCEPT( "end_of_message failed" );
  }

  sock->decode();
  ASSERT( sock->code(reply) );
  ASSERT( sock->end_of_message() );

  if( reply != OK ) {
    dprintf( D_ALWAYS, "Shadow: Request to run a job was REFUSED\n");
    reason = JOB_NOT_STARTED;
    return -1;
  }
  dprintf( D_ALWAYS, "Shadow: Request to run a job was ACCEPTED\n" );

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

  return 0;
}

// takes sinful address of startd and sends it a cmd
int
send_cmd_to_startd(char *sin_host, char *capability, int cmd)
{
  int sd;
  
  // connect to the startd - when sin_host is given, START_PORT is ignored ?
  sd = do_connect(sin_host, "condor_startd", START_PORT);
  if( sd < 0 ) {
    dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
	     sin_host);
    return -1;
  }

  // create a relisock to communicate with startd
  ReliSock        *sock;
  sock = new ReliSock();
  sock->attach_to_file_desc(sd);
  sock->encode();

  // Send the command
  if( !sock->code(cmd) ) {
    dprintf( D_ALWAYS, "sock->code(%d)", cmd );
    return -2;
  }
  
  // send the capability
  dprintf(D_FULLDEBUG, "send capability %s\n", capability);
  if(!sock->put(capability, SIZE_OF_CAPABILITY_STRING)){
    dprintf( D_ALWAYS, "sock->put()" );
    return -3;
  }

  // send end of message
  if( !sock->end_of_message() ) {
    dprintf( D_ALWAYS, "end_of_message failed" );
    return -4;
  }

  return 0;
}
