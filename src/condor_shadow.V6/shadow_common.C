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
#include "except.h"
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
}

#if defined(NEW_PROC)
        extern PROC    *Proc;
#else
        V2_PROC *Proc;
#endif

extern int ChildPid;
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
        if( Proc->id.cluster || Proc->id.proc ) {
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
