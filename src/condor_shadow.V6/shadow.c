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
#include "condor_common.h"
#include "condor_fdset.h"

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

#include "sched.h"
#include "debug.h"
#include "except.h"
#include "trace.h"
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
int	UsePipes;

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char 	*getenv(), *strchr(), *strcat(), *param();


typedef void (*SIG_HANDLER)();
void install_sig_handler( int sig, SIG_HANDLER handler );
void reaper();
void rm();
void condor_rm();
void unblock_signal(int sig);
void block_signal(int sig);
void display_errors( FILE *fp );
int count_open_fds( const char *file, int line );

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif

XDR		*xdr_Init();
EXPR	*scan(), *create_expr();
ELEM	*create_elem();
CONTEXT	*create_context(), *build_job_context();
FILE	*fdopen();
void display_uids();

#define SMALL_STRING 128
char My_AFS_Cell[ SMALL_STRING ];
char My_Filesystem_Domain[ SMALL_STRING ];
char My_UID_Domain[ SMALL_STRING ];
int  UseAFS;
int  UseNFS;
int  UseCkptServer;

#if 0
extern int	LockFd;
#endif

extern int	_Condor_SwitchUids;

int		whoami();

int		MyPid;
int		LogPipe;
int		ImageSize;

GENERIC_PROC GenProc;
#if defined(NEW_PROC)
	PROC	*Proc = (PROC *)&GenProc;;
#else
	V2_PROC	*Proc = (V2_PROC *)&GenProc;;
#endif

char	*Spool;
char	*ExecutingHost;
char	*MailerPgm;

#if DBM_QUEUE
char	QueueName[MAXPATHLEN];
DBM		*QueueD = NULL, *OpenJobQueue();
#else
#include "condor_qmgr.h"
#endif

#include "user_log.h"
USER_LOG	*ULog;
USER_LOG *InitUserLog( PROC *p, const char *host );
void LogRusage( USER_LOG *lp, int which,
							struct rusage *loc, struct rusage *rem );

#if DBM_QUEUE
#define OPENJOBQUEUE(q, name, flags, mode) { \
	if( (q) == NULL ) { \
		(q) = OpenJobQueue(name, flags, mode); \
		if( (q) == NULL ) { \
			EXCEPT("OpenJobQueue(%s)", name); \
		} \
	} \
}

#define CLOSEJOBQUEUE(q)	{ CloseJobQueue(q); (q) = NULL; }

#endif /* DBM_QUEUE */

int		CondorGid;
int		CondorUid;
int		ClientUid;
int		ClientGid;

struct rusage LocalUsage;
struct rusage RemoteUsage;

char	CkptName[ MAXPATHLEN ];		/* The name of the ckpt file */
char	ICkptName[ MAXPATHLEN ];	/* The name of the initial ckpt file */
char	RCkptName[ MAXPATHLEN ];	/* The name of the returned ckpt file */
char	TmpCkptName[ MAXPATHLEN ];	/* The name of the temp ckpt file */

char	ErrBuf[ 1024 ];
int		HadErr = 0;


#ifdef NOTDEF
int		InitialJobStatus = -1;
#endif NOTDEF

union wait JobStatus;
struct rusage JobRusage;
struct rusage AccumRusage;
int ChildPid;

int	DoCleanup();

int Termlog;

XDR		*xdr_RSC1, *RSC_ShadowInit();

int MainSymbolExists = 1;

usage()
{
	dprintf( D_ALWAYS, "Usage: shadow host cluster proc\n" );
	dprintf( D_ALWAYS, "or\n" );
	dprintf( D_ALWAYS, "Usage: shadow -pipe cluster proc\n" );
	exit( 1 );
}

#if defined(__STDC__)
	void regular_setup( char *host, char *cluster, char *proc, char* );
	void pipe_setup( char *cluster, char *proc );
	void open_named_pipe( const char *name, int mode, int target_fd );
#else
	void regular_setup();
	void pipe_setup();
	void open_named_pipe();
#endif

#if defined(SYSCALL_DEBUG)
	char	*SyscallLabel;
#endif

/*ARGSUSED*/
main(argc,argv,envp)
int argc;
char *argv[];
char *envp[];
{
	char	*tmp;
	int		reserved_swap, free_swap;
	char	*host, *cluster, *proc;
	char	*my_cell, *my_fs_domain, *my_uid_domain, *use_afs, *use_nfs;
	char	*use_ckpt_server, *ckpt_server_host;
	char	*capability;
	int		i;

	/* on OSF/1 as installed currently on our machines, attempts to read from
	   the FILE * returned by popen() fail if the underlying file descriptor
	   is 1.  This is ugly, but we have 4K fd's so we won't miss these few */


#if !defined(OSF1)
	close( 0 );
	close( 1 );
	close( 2 );
#endif

#if defined(SYSCALL_DEBUG)
	SyscallLabel = argv[0] + 7;
#endif

	if( argc > 1 ) {
		if( strcmp("-t",argv[1]) == MATCH ) {
			Termlog++;
			argv++;
			argc--;
		}
	}


	set_fatal_uid_sets( 0 );

#if defined(NFSFIX) && 0
	/* Must be condor to write to log files. */
	set_condor_euid(__FILE__,__LINE__);
#endif NFSFIX

	_EXCEPT_Cleanup = DoCleanup;
	MyPid = getpid();

	config( argv[0], (CONTEXT *)0 );

	dprintf_config( "SHADOW", SHADOW_LOG );
	DebugId = whoami;

	dprintf( D_ALWAYS, "********** Shadow starting up **********\n" );

	if( (tmp=param("RESERVED_SWAP")) == NULL ) {
		reserved_swap = 5 * 1024;			/* 5 megabytes */
	} else {
		reserved_swap = atoi( tmp ) * 1024;	/* Value specified in megabytes */
	}

	free_swap = calc_virt_memory();
	close_kmem(); /* only calc virt mem once, so close unneeded fs's */

	dprintf( D_FULLDEBUG, "*** Reserved Swap = %d\n", reserved_swap );
	dprintf( D_FULLDEBUG, "*** Free Swap = %d\n", free_swap );
	if( free_swap < reserved_swap ) {
		dprintf( D_ALWAYS, "Not enough reserved swap space\n" );
		exit( JOB_NO_MEM );
	}


#ifndef NFSFIX
	if (getuid()==0) {
		set_root_euid(__FILE__,__LINE__);
	}
#endif

	dprintf(D_ALWAYS, "uid=%d, euid=%d, gid=%d, egid=%d\n",
		getuid(), geteuid(), getgid(), getegid());

    dprintf(D_FULLDEBUG, "argc = %d\n", argc);
    for(i = 0; i < argc; i++)
    {
        dprintf(D_FULLDEBUG, "argv[%d] = %s\n", i, argv[i]);
    }
	if( argc != 5 ) {
		usage();
	}

	if( strcmp("-pipe",argv[1]) == 0 ) {
		cluster = argv[2];
		proc = argv[3];
		pipe_setup( cluster, proc );
	} else {
		host = argv[1];
		capability = argv[2];
		cluster = argv[3];
		proc = argv[4];
		regular_setup( host, cluster, proc, capability );
	}


#if 0
		/* Don't want to share log file lock between child and pnarent */
	(void)close( LockFd );
	LockFd = -1;
#endif

	ULog = InitUserLog( Proc, ExecutingHost );

	my_cell = get_host_cell();
	if( my_cell ) {
		strcpy( My_AFS_Cell, my_cell );
	} else {
		strcpy( My_AFS_Cell, "(NO CELL)" );
	}
	dprintf( D_ALWAYS, "My_AFS_Cell = \"%s\"\n", My_AFS_Cell );

	my_fs_domain = param( "FILESYSTEM_DOMAIN" );
	if( my_fs_domain ) {
		strcpy( My_Filesystem_Domain, my_fs_domain );
	} else {
		get_machine_name( My_Filesystem_Domain );
	}
	dprintf( D_ALWAYS, "My_Filesystem_Domain = \"%s\"\n", My_Filesystem_Domain );

	my_uid_domain = param( "UID_DOMAIN" );
	if( my_uid_domain ) {
		strcpy( My_UID_Domain, my_uid_domain );
	} else {
		get_machine_name( My_UID_Domain );
	}
	dprintf( D_ALWAYS, "My_UID_Domain = \"%s\"\n", My_UID_Domain );

	use_afs = param( "USE_AFS" );
	if( use_afs && (use_afs[0] == 'T' || use_afs[0] == 't') ) {
		UseAFS = TRUE;
	} else {
		UseAFS = FALSE;
	}

	use_nfs = param( "USE_NFS" );
	if( use_nfs && (use_nfs[0] == 'T' || use_nfs[0] == 't') ) {
		UseNFS = TRUE;
	} else {
		UseNFS = FALSE;
	}

	use_ckpt_server = param( "USE_CKPT_SERVER" );
	ckpt_server_host = param( "CKPT_SERVER_HOST" );
	if( ckpt_server_host && use_ckpt_server &&
	   (use_ckpt_server[0] == 'T' || use_ckpt_server[0] == 't') ) {
		UseCkptServer = TRUE;
	} else {
		UseCkptServer = FALSE;
	}

	MailerPgm = param( "MAIL" );
	if( MailerPgm == NULL ) {
		MailerPgm = "/bin/mail";
	}

	install_sig_handler( SIGCHLD, reaper );
	install_sig_handler( SIGUSR1, condor_rm ); /* sent by schedd when a */
												/*job is removed */

	/* Here we block the async signals.  We do this mainly because on HPUX,
	 * XDR wigs out if it is interrupted by a signal.  We do it on all
	 * platforms because it seems like a safe idea.  We unblock the signals
	 * during before select(), which is where we spend most of our time. */
	block_signal(SIGCHLD);
	block_signal(SIGUSR1);      
#if 0
	count_open_fds( __FILE__, __LINE__ );
#endif
	HandleSyscalls();
	Wrapup();

#ifndef DBM_QUEUE
	ConnectQ(NULL);
	SetAttributeInt(Proc->id.cluster, Proc->id.proc, "CurrentHosts", 0);
	DisconnectQ(NULL);
#endif

	dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
	exit( 0 );
}


HandleSyscalls()
{
	int		fake_arg;

	if (getuid()==0) {
		/* Must be root for chroot() call. */
		set_root_euid(__FILE__,__LINE__);
	}

	dprintf(D_FULLDEBUG, 
			"HandleSyscalls: about to chdir(%s)\n", Proc->rootdir);
	if( chdir(Proc->rootdir) < 0 ) {
		sprintf( ErrBuf,  "Can't access \"%s\"", Proc->rootdir );
		HadErr = TRUE;
		return;
	}


	/*
	**	Set up group array for job owner, but include Condor's gid
	*/
	/*
	**	dprintf(D_ALWAYS,"Shadow: about to initgroups(%s, %d)\n",
	**					Proc->owner,CondorGid);
	*/
	if(getuid()==0)
		if( initgroups(Proc->owner, CondorGid) < 0 ) {
			EXCEPT("Can't initgroups(%s, %d)", Proc->owner, CondorGid);
		}

	/*
	** dprintf(D_ALWAYS,"Shadow: about to setrgid(%d)\n", ClientGid);
	*/
		/* Set the rgid to job's owner - keep him out of trouble */
#if defined(AIX31) || defined(AIX32)
	if( setregid(ClientGid,ClientGid) < 0 ) {
		EXCEPT( "setregid(%d,%d)", ClientGid, ClientGid);
	}
#endif

		/* Set euid to job's owner - keep him out of trouble */
#if defined(AIX32)
	display_uids();
	errno = 0;
	if( setuidx(ID_REAL|ID_EFFECTIVE,ClientUid) < 0 ) {
		dprintf( D_ALWAYS, "errno = %d\n", errno );
		EXCEPT( "setuidx(ID_REAL|ID_EFFECTIVE,%d)", ClientUid );
	}
#else 
	if(getuid()==0) {
		_Condor_SwitchUids = 1;
		if( setgid(ClientGid) < 0 ) {
			EXCEPT( "setgid(%d), errno = %d", ClientGid, errno);
		}
		if( seteuid(ClientUid) < 0 ) {
			EXCEPT( "seteuid(%d), errno = %d", ClientUid, errno );
		}
	}
#endif

	dprintf(D_FULLDEBUG, "HandleSyscalls: about to chdir(%s)\n", Proc->iwd);
	if( chdir(Proc->iwd) < 0 ) {
		sprintf( ErrBuf,  "Can't access \"%s\"", Proc->iwd );
		HadErr = TRUE;
		return;
	}

	dprintf(D_SYSCALLS, "Shadow: Starting to field syscall requests\n");
	errno = 0;

	for(;;) {	/* get a request and fulfill it */
		if( do_REMOTE_syscall() < 0 ) {
			dprintf(D_SYSCALLS, "Shadow: do_REMOTE_syscall returned < 0\n");
			break;
		}
#if defined(SYSCALL_DEBUG)
		strcpy( SyscallLabel, "shadow" );
#endif
	}

		/* Take back normal condor privileges */
#if defined(AIX32)
	if( setuidx(ID_REAL|ID_EFFECTIVE,0) < 0 ) {
		EXCEPT( "setuidx(ID_REAL|ID_EFFECTIVE,0)" );
	}
	if( setuidx(ID_REAL|ID_EFFECTIVE,CondorUid) < 0 ) {
		EXCEPT( "setuidx(ID_REAL|ID_EFFECTIVE,%d)", CondorUid );
	}
#else 
	if(getuid()==0) {
		if( seteuid(0) < 0 ) {
			EXCEPT( "seteuid(%d)", 0 );
		}
		if( seteuid(CondorUid) < 0 ) {
			EXCEPT( "seteuid(%d)", CondorUid );
		}
	}
#endif
	_Condor_SwitchUids = 0;

		/* If we are debugging with named pipes as our communications medium,
		   won't have a condor_startd running - don't try to send to it.
		*/
	if( !UsePipes ) {
		send_quit( ExecutingHost );
	}

	dprintf(D_ALWAYS,
		"Shadow: Job %d.%d exited, termsig = %d, coredump = %d, retcode = %d\n",
			Proc->id.cluster, Proc->id.proc, JobStatus.w_termsig,
			JobStatus.w_coredump, JobStatus.w_retcode );

}

Wrapup( )
{
	union wait status;
	struct rusage local_rusage;
	char notification[ BUFSIZ ];
	int pid;
	fd_set	readfds, template;
	int	cnt;
	int	nfds;
	FILE	*log_fp;
	int		s;
	char coredir[ MAXPATHLEN ];
	int		coredir_len;
	int		msg_type;

	install_sig_handler( SIGTERM, rm );
	dprintf(D_FULLDEBUG, "Entering Wrapup()\n" );

	notification[0] = '\0';

	if( HadErr ) {
		Proc->status = COMPLETED;
		Proc->completion_date = time( (time_t *)0 );
#if 0
		dprintf( D_ALWAYS, "MsgBuf = \"%s\"\n", MsgBuf );
#endif
		(void)strcpy( notification, ErrBuf );
	} else {
		handle_termination( notification );
	}

	/* fill in the Proc structure's exit_status with JobStatus, so that when
	 * we call update_job_status the exit status gets written into the job queue,
	 * so that the history file will have exit status information for the job.
	 * note that JobStatus is in BSD format, and Proc->exit_status is in POSIX int
	 * format, so we copy very carefully.  on most platforms the 2 formats are
	 * interchangeable, so most of the time we'll get intelligent results. -Todd, 11/95
	*/
	memcpy(&(Proc->exit_status[0]),&JobStatus,sizeof( (Proc->exit_status[0]) ));
	
	get_local_rusage( &local_rusage );
	update_job_status( &local_rusage, &JobRusage );
	LogRusage( ULog,  THIS_RUN, &local_rusage, &JobRusage );

	if( Proc->status == COMPLETED ) {
		LogRusage( ULog, TOTAL, &Proc->local_usage, &Proc->remote_usage[0] );
	}
	if( notification[0] ) {
		NotifyUser( notification );
	}
}

handle_termination( notification )
char	*notification;
{
	char	coredir[ 4096 ];
	dprintf(D_FULLDEBUG, "handle_termination() called.\n");

	switch( JobStatus.w_termsig ) {
	 case 0: /* If core, bad executable -- otherwise a normal exit */
		if( JobStatus.w_coredump && JobStatus.w_retcode == ENOEXEC ) {
			(void)sprintf( notification, "is not executable." );
			dprintf( D_ALWAYS, "Shadow: Job file not executable" );
			PutUserLog( ULog, NOT_EXECUTABLE );

		} else if( JobStatus.w_coredump && JobStatus.w_retcode == 0 ) {
				(void)sprintf(notification,
"was killed because it was not properly linked for execution \nwith Version 5 Condor.\n" );
				PutUserLog( ULog, BAD_LINK );
				MainSymbolExists = FALSE;
		} else {
			(void)sprintf(notification, "exited with status %d.",
					JobStatus.w_retcode );
			dprintf(D_ALWAYS, "Shadow: Job exited normally with status %d\n",
				JobStatus.w_retcode );
			PutUserLog( ULog, NORMAL_EXIT, JobStatus.w_retcode );
		}

		Proc->status = COMPLETED;
		Proc->completion_date = time( (time_t *)0 );
		break;
	 case SIGKILL:	/* Kicked off without a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was kicked off without a checkpoint\n" );
		PutUserLog(ULog, NOT_CHECKPOINTED );
		DoCleanup();
		break;
	 case SIGQUIT:	/* Kicked off, but with a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was checkpointed\n" );
		PutUserLog(ULog, CHECKPOINTED );
		if( strcmp(Proc->rootdir, "/") != 0 ) {
			MvTmpCkpt();
		}
		Proc->status = IDLE;
		break;
	 default:	/* Job exited abnormally */
#if defined(Solaris)
		getcwd(coredir,_POSIX_PATH_MAX);
#else
		getwd( coredir );
#endif
		if( JobStatus.w_coredump ) {
			if( strcmp(Proc->rootdir, "/") == 0 ) {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s/core.%d.%d.",
					 JobStatus.w_termsig,
						coredir, Proc->id.cluster, Proc->id.proc);
				PutUserLog( ULog, ABNORMAL_EXIT, JobStatus.w_termsig);
				PutUserLog( ULog, CORE_NAME,
					coredir, Proc->id.cluster, Proc->id.proc
				);
			} else {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s%s/core.%d.%d.",
						 JobStatus.w_termsig,
						Proc->rootdir, coredir, Proc->id.cluster, Proc->id.proc);
					PutUserLog( ULog, ABNORMAL_EXIT, JobStatus.w_termsig );
					PutUserLog( ULog, CORE_NAME,
						Proc->rootdir, coredir, Proc->id.cluster, Proc->id.proc
				   );
			}
		} else {
			(void)sprintf(notification,
				"was killed by signal %d.", JobStatus.w_termsig);
			PutUserLog( ULog, ABNORMAL_EXIT, JobStatus.w_termsig );
		}
		dprintf(D_ALWAYS, "Shadow: %s\n", notification);

		Proc->status = COMPLETED;
		Proc->completion_date = time( (time_t *)0 );
		break;
	}
}


char *
d_format_time( dsecs )
double dsecs;
{
	int days, hours, minutes, secs;
	static char answer[25];

#define SECONDS	1
#define MINUTES	(60 * SECONDS)
#define HOURS	(60 * MINUTES)
#define DAYS	(24 * HOURS)

	secs = dsecs;

	days = secs / DAYS;
	secs %= DAYS;

	hours = secs / HOURS;
	secs %= HOURS;

	minutes = secs / MINUTES;
	secs %= MINUTES;

	(void)sprintf(answer, "%d %02d:%02d:%02d", days, hours, minutes, secs);

	return( answer );
}


NotifyUser( buf )
char *buf;
{
	FILE *mailer;
	char cmd[ BUFSIZ ];
	char notifyUser[256];  /* email address of user to be notified */
	double rutime, rstime, lutime, lstime;	/* remote/local user/sys times */
	double trtime, tltime;	/* Total remote/local time */
	double	real_time;


	dprintf(D_FULLDEBUG, "NotifyUser() called.\n");

		/* Want the letter to come from "condor" */
#if defined(AIX32)
	if( setuidx(ID_REAL|ID_EFFECTIVE,0) < 0 ) {
		EXCEPT( "setuidx(ID_REAL|ID_EFFECTIVE,0)" );
	}
#else 
	if(getuid()==0){
	if( seteuid(0) < 0 ) {
		EXCEPT( "seteuid" );
	}
	
	set_condor_euid(__FILE__,__LINE__);
}
#endif
	/* If user loaded program incorrectly, always send a message. */
	if( MainSymbolExists == TRUE ) {
		switch( Proc->notification ) {
		case NOTIFY_NEVER:
			return;
		case NOTIFY_ALWAYS:
			break;
		case NOTIFY_COMPLETE:
			if( Proc->status == COMPLETED ) {
				break;
			} else {
				return;
			}
		case NOTIFY_ERROR:
			if( (Proc->status == COMPLETED) && (JobStatus.w_termsig != 0) ) {
				break;
			} else {
				return;
			}
		default:
			dprintf(D_ALWAYS, "Condor Job %d.%d has a notification of %d\n",
					Proc->id.cluster, Proc->id.proc, Proc->notification );
		}
	}

#if 0
	(void)sprintf(cmd, "%s %s", BIN_MAIL, Proc->owner );
#else
		/* the "-s" tries to set the subject line appropriately */
	/*
	sprintf(cmd, "%s -s \"Condor Job %d.%d\" %s\n",
					MailerPgm, Proc->id.cluster, Proc->id.proc, Proc->owner );
	*/

	/*
		The ClassAd of the Job also contains a variable called 'NotifyUser'
		which is the email address of the user to be mailed.  This information
        is not in the PROC structure.  Thus, use this information only if not
        using DBM queues    --RR
	*/

#ifndef DBM_QUEUE
	ConnectQ (NULL);
	if (-1 == GetAttributeString (Proc->id.cluster,Proc->id.proc,"NotifyUser",
                              notifyUser))
	{
		strcpy (notifyUser, Proc->owner);
	}
	DisconnectQ (NULL);
#else
	strcpy (notifyUser, Proc->owner);
#endif

	sprintf(cmd, "%s -s \"Condor Job %d.%d\" %s@%s\n",
					MailerPgm, Proc->id.cluster, Proc->id.proc, notifyUser,
					My_UID_Domain
					);
#endif

#if 0
	mailer = execute_program( cmd, Proc->owner, "w" );
#else
	mailer = popen( cmd, "w" );
#endif
	if( mailer == NULL ) {
		EXCEPT(
			"Shadow: Cannot do execute_program( %s, %s, %s )\n",
			cmd, Proc->owner, "w"
		);
	}

	/* if we got the subject line set correctly earlier, and we
	   got the from line generated by setting our uid, then we
	   don't need this stuff...
	*/
#if 0
	fprintf(mailer, "From: Condor\n" );
	fprintf(mailer, "To: %s\n", Proc->owner );
	fprintf(mailer, "Subject: Condor Job %d.%d\n\n",
								Proc->id.cluster, Proc->id.proc );
#endif

	fprintf(mailer, "Your condor job\n" );
#if defined(NEW_PROC)
	fprintf(mailer, "\t%s %s\n", Proc->cmd[0], Proc->args[0] );
#else
	fprintf(mailer, "\t%s %s\n", Proc->cmd, Proc->args );
#endif
	fprintf(mailer, "%s\n\n", buf );

	display_errors( mailer );

	fprintf(mailer, "Submitted at:        %s", ctime( (time_t *)&Proc->q_date) );
	if( Proc->completion_date ) {
		real_time = Proc->completion_date - Proc->q_date;
		fprintf(mailer, "Completed at:        %s",
									ctime((time_t *)&Proc->completion_date) );
		fprintf(mailer, "Real Time:           %s\n", d_format_time(real_time));
	}
	fprintf( mailer, "\n" );

#if defined(NEW_PROC)
	rutime = Proc->remote_usage[0].ru_utime.tv_sec;
	rstime = Proc->remote_usage[0].ru_stime.tv_sec;
	trtime = rutime + rstime;
#else
	rutime = Proc->remote_usage.ru_utime.tv_sec;
	rstime = Proc->remote_usage.ru_stime.tv_sec;
	trtime = rutime + rstime;
#endif

	lutime = Proc->local_usage.ru_utime.tv_sec;
	lstime = Proc->local_usage.ru_stime.tv_sec;
	tltime = lutime + lstime;


	fprintf(mailer, "Remote User Time:    %s\n", d_format_time(rutime) );
	fprintf(mailer, "Remote System Time:  %s\n", d_format_time(rstime) );
	fprintf(mailer, "Total Remote Time:   %s\n\n", d_format_time(trtime));
	fprintf(mailer, "Local User Time:     %s\n", d_format_time(lutime) );
	fprintf(mailer, "Local System Time:   %s\n", d_format_time(lstime) );
	fprintf(mailer, "Total Local Time:    %s\n\n", d_format_time(tltime));

	if( tltime >= 1.0 ) {
		fprintf(mailer, "Leveraging Factor:   %2.1f\n", trtime / tltime );
	}
	fprintf(mailer, "Virtual Image Size:  %d Kilobytes\n", Proc->image_size );

#if 0
	terminate_program();
#else
	(void)pclose( mailer );
#endif
}


/* A file we want to remove (partiucarly a checkpoint file) may actually be
   stored on the checkpoint server.  We'll make sure it gets removed from
   both places. JCP
*/

unlink_local_or_ckpt_server( file )
char	*file;
{
	int		rval;

	dprintf( D_ALWAYS, "Trying to unlink %s\n", file);
	rval = unlink( file );
	if (rval) {
		/* Local failed, so lets try to do it on the server, maybe we
		   should do that anyway??? */
		dprintf( D_ALWAYS, "Remove from ckpt server returns %d\n",
				RemoveRemoteFile(Proc->owner, file));
	}
}


update_job_status( localp, remotep )
struct rusage *localp, *remotep;
{
	PROC	my_proc;
	PROC	*proc = &my_proc;
	char	*gen_ckpt_name();
	static struct rusage tstartup_local, tstartup_remote;
	static int tstartup_flag = 0;
	int		status;

#if DBM_QUEUE
	OPENJOBQUEUE(QueueD, QueueName, O_RDWR, 0);
	LockJobQueue( QueueD, WRITER );

	proc->id.cluster = Proc->id.cluster;
	proc->id.proc = Proc->id.proc;
	if( FetchProc(QueueD,proc) < 0 ) {
		EXCEPT( "Shadow: FetchProc(%d.%d)", proc->id.cluster, proc->id.proc );
	}
	status = proc->status;
#else
	ConnectQ(NULL);
	GetAttributeInt(Proc->id.cluster, Proc->id.proc, "Status", &status);
#endif
	if( status == REMOVED ) {
		dprintf( D_ALWAYS, "Job %d.%d has been removed by condor_rm\n",
											proc->id.cluster, proc->id.proc );
		if( CkptName[0] != '\0' ) {
			dprintf(D_ALWAYS, "Shadow: unlinking Ckpt '%s'\n", CkptName);
			(void) unlink_local_or_ckpt_server( CkptName );
		}
		if( TmpCkptName[0] != '\0' ) {
			dprintf(D_ALWAYS, "Shadow: unlinking TmpCkpt '%s'\n", TmpCkptName);
			(void) unlink_local_or_ckpt_server( TmpCkptName );
		}
	} else {

		/* Here we keep usage info on the job when the Shadow first starts in
		 * tstartup_local/remote.  This way we can "reset" it back each subsequent
		 * time we are called so that update_rusage always adds up correctly.  We
		 * are called by pseudo calls really_exit and update_rusage, and both of
		 * these are passing in times _since_ the remote job started, NOT since
		 * the last time this function was called.  Thus this code here.... -Todd */
		if (tstartup_flag == 0 ) {
			tstartup_flag = 1;
			memcpy(&tstartup_local, &Proc->local_usage, sizeof(struct rusage) );
			memcpy(&tstartup_remote, &Proc->remote_usage[0], sizeof(struct rusage) );
		} else {
			memcpy(&Proc->local_usage,&tstartup_local,sizeof(struct rusage));
			memcpy(&Proc->remote_usage[0],&tstartup_remote, sizeof(struct rusage) );
		}

		update_rusage( &Proc->local_usage, localp );
		update_rusage( &Proc->remote_usage[0], remotep );
		Proc->image_size = ImageSize;

#if DBM_QUEUE
		if( StoreProc(QueueD,Proc) < 0 ) {
			CLOSEJOBQUEUE( QueueD );
			EXCEPT( "StoreProc(%d.%d)", Proc->id.cluster, Proc->id.proc );
		}
#else
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, "Image_size", 
						ImageSize);
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, "Status", 
						Proc->status);
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc, "Local_CPU", 
						  rusage_to_float(Proc->local_usage));
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc, "Remote_CPU", 
						  rusage_to_float(Proc->remote_usage[0]));
#endif
		dprintf( D_ALWAYS, "Shadow: marked job status %d\n", Proc->status );

		if( Proc->status == COMPLETED ) {
#if DBM_QUEUE
			if( TerminateProc(QueueD,&Proc->id,COMPLETED) < 0 ) {
				EXCEPT( "TerminateProc(0x%x,%d.%d,COMPLETED)",
									QueueD, Proc->id.cluster, Proc->id.proc );
			}
#else
			if( DestroyProc(Proc->id.cluster, Proc->id.proc) < 0 ) {
				EXCEPT("DestroyProc(%d.%d)", Proc->id.cluster, Proc->id.proc );
			}
#endif
		}
	}

#if DBM_QUEUE
	CLOSEJOBQUEUE( QueueD );
#else
		DisconnectQ(0);
#endif
}

/*
** Connect to the startd on the remote host and ask to run a job.  The
** connection to the starter will become our main connection to the
** remote starter, and the scheduler will send back an aux port number to
** be used for out of band (error) traffic.
*/
send_job( proc, host, cap )
#if defined(NEW_PROC)
PROC	*proc;
#else
V2_PROC	*proc;
#endif

char	*host;
char	*cap;
{
	int		cmd;
	int		reply;
	bool_t	xdr_ports();
	PORTS	ports;
	int		sock;
	XDR		xdr, *xdrs;
	CONTEXT	*context;
	StartdRec   stRec;
	char*	capability = strdup(cap);

	dprintf( D_FULLDEBUG, "Shadow: Entering send_job()\n" );

		/* Test starter 0 - Mike's combined pvm/vanilla starter. */
		/* Test starter 1 - Jim's pvm starter */
		/* Test starter 2 - Mike's V5 starter */
#define TEST_STARTER 2
#if defined(TEST_STARTER)
	cmd = SCHED_VERS + ALT_STARTER_BASE + TEST_STARTER;
	dprintf( D_ALWAYS, "Requesting Test Starter %d\n", TEST_STARTER );
#else
	cmd = START_FRGN_JOB;
	dprintf( D_ALWAYS, "Requesting Standard Starter\n" );
#endif 

		/* Connect to the startd */
	/* weiru */
	/* for ip:port pair and capability */
	if( (sock = do_connect(host, "condor_startd", START_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
						host);
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( 0 );
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

		/* Send the command */
	if( !xdr_int(xdrs, &cmd) ) {
		EXCEPT( "xdr_int()" );
	}

	/* send the capability */
    dprintf(D_FULLDEBUG, "send capability %s\n", capability);
    if(!xdr_string(xdrs, &capability, SIZE_OF_CAPABILITY_STRING))
    {
        EXCEPT( "xdr_string()" );
    }
    free(capability);

		/* Send the job info */
	context = build_job_context( proc );
	display_context( context );
	if( !xdr_context(xdrs,context) ) {
		EXCEPT( "xdr_context(0x%x,0x%x)", xdrs, context );
	}
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		EXCEPT( "xdrrec_endofrecord(TRUE)" );
	}
	free_context( context );

	xdrs->x_op = XDR_DECODE;
	ASSERT( xdr_int(xdrs,&reply) );
	ASSERT( xdrrec_skiprecord(xdrs) );

	if( reply != OK ) {
		dprintf( D_ALWAYS, "Shadow: Request to run a job was REFUSED\n" );
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( 0 );
	}
	dprintf( D_ALWAYS, "Shadow: Request to run a job was ACCEPTED\n" );

    /* start flock : dhruba */
	memset( &stRec, '\0', sizeof(stRec) );
    ASSERT( xdr_startd_info(xdrs, &stRec));
    ASSERT( xdrrec_skiprecord(xdrs) );
    ports = stRec.ports;
	if( stRec.ip_addr ) {
		host = ExecutingHost = stRec.server_name;
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
        dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
        exit( 0 );
    }
    /* end  flock ; dhruba */

	close(sock);

	if( (sock = do_connect(host, (char *)0, (u_short)ports.port1)) < 0 ) {
		EXCEPT( "connect to scheduler on \"%s\", port1 = %d",
													host, ports.port1 );
	}

	if( sock != RSC_SOCK ) {
		ASSERT(dup2(sock, RSC_SOCK) >= 0);
		(void)close(sock);
	}
	dprintf( D_ALWAYS, "Shadow: RSC_SOCK connected, fd = %d\n", RSC_SOCK );

	if( (sock = do_connect(host, (char *)0, (u_short)ports.port2)) < 0 ) {
		EXCEPT( "connect to scheduler on \"%s\", port2 = %d",
													host, ports.port2 );
	}

	if( sock != CLIENT_LOG ) {
		ASSERT(dup2(sock, CLIENT_LOG) >= 0);
		(void)close(sock);
	}
	dprintf( D_ALWAYS, "Shadow: CLIENT_LOG connected, fd = %d\n", CLIENT_LOG );

	xdr_RSC1 = RSC_ShadowInit( RSC_SOCK, CLIENT_LOG );

#if 0	/* wait to be asked - we're supposed to be an rpc server */
	send_job_info( xdr_RSC1, proc );
#endif

}


get_client_ids( proc )
PROC *proc;
{
	struct passwd *pwd;
	char *gen_ckpt_name();
	char *tmp;

	tmp = gen_ckpt_name( Spool, proc->id.cluster, proc->id.proc, 0 );
	strcpy( CkptName, tmp );

	tmp = gen_ckpt_name( Spool, proc->id.cluster, ICKPT, 0 );
	strcpy( ICkptName, tmp );

	sprintf( TmpCkptName, "%s.tmp", CkptName );

	strcpy( RCkptName, CkptName );

	pwd = getpwnam("condor");
	if( pwd == NULL ) {
		EXCEPT("Can't find password entry for user 'condor'");
	}

	CondorGid = pwd->pw_gid;
	CondorUid = pwd->pw_uid;

	pwd = getpwnam(proc->owner);
	if( pwd == NULL ) {
		EXCEPT("Can't find password entry for '%s'", proc->owner);
	}

	ClientUid = pwd->pw_uid;
	ClientGid = pwd->pw_gid;
	dprintf( D_ALWAYS, "ClientUid = %d, ClientGid = %d\n",
										ClientUid, ClientGid );
}

send_job_info( xdrs, proc )
XDR		*xdrs;
V2_PROC	*proc;
{
	struct stat st_buf;
	char	*return_name;
	char	*orig_name;
	int		soft_kill = SIGUSR1;

	dprintf( D_ALWAYS, "Shadow: send_job_info( 0x%x, %d.%d, %s, %s, %s, %s )\n",
		xdrs, proc->id.cluster, proc->id.proc,
		proc->in, proc->out, proc->err, proc->args );
	
	dprintf(D_FULLDEBUG, "Shadow: send_job_info: RootDir = <%s>\n",
															proc->rootdir);

	if( stat(CkptName,&st_buf) == 0 ) {
		orig_name = CkptName;
	} else if( stat(ICkptName,&st_buf) == 0 ) {
		orig_name = ICkptName;
	} else {
		EXCEPT( "Can't find checkpoint file" );
	}

	xdrs->x_op = XDR_ENCODE;
	dprintf( D_FULLDEBUG, "Shadow: Sending proc structure\n" );
	ASSERT(xdr_proc(xdrs, proc));

	return_name = RCkptName;
	dprintf( D_FULLDEBUG, "Shadow: Sending name = '%s'\n", return_name );
	ASSERT( xdr_string(xdrs, &return_name, (u_int) MAXPATHLEN) );

	dprintf( D_FULLDEBUG, "Shadow: Sending orig_name = %s\n", orig_name );
	ASSERT( xdr_string(xdrs, &orig_name, (u_int) MAXPATHLEN) );

	dprintf( D_FULLDEBUG, "Shadow: Sending soft_kill = %d\n", soft_kill );
	ASSERT( xdr_sig_num(xdrs, &soft_kill) );

	/*
	ASSERT( xdrrec_endofrecord(xdrs, TRUE) );
	*/
}


extern char	*SigNames[];

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

	exit( 1 );
}


/*
** Print an identifier saying who we are.  This function gets handed to
** dprintf().
*/
whoami( fp )
FILE *fp;
{
	if( Proc->id.cluster || Proc->id.proc ) {
		fprintf( fp, "(%d.%d) (%d):", Proc->id.cluster, Proc->id.proc, MyPid );
	} else {
		fprintf( fp, "(?.?) (%d):", MyPid );
	}
}

/*
** Opens job queue (Q), and reads in process structure (Proc) as side
** affects.
*/
start_job( cluster_id, proc_id )
char	*cluster_id;
char	*proc_id;
{
	int		cluster_num;
	int		proc_num;

	Proc->id.cluster = atoi( cluster_id );
	Proc->id.proc = atoi( proc_id );

#if DBM_QUEUE
	OPENJOBQUEUE(QueueD, QueueName, O_RDWR, 0);
	LockJobQueue( QueueD, WRITER );

	if( FetchProc(QueueD,Proc) < 0 ) {
		EXCEPT( "FetchProc(%d.%d)", Proc->id.cluster, Proc->id.proc );
	}
#else
	cluster_num = atoi( cluster_id );
	proc_num = atoi( proc_id );

	ConnectQ(NULL);
	if (GetProc(cluster_num, proc_num, Proc) < 0) {
		EXCEPT("GetProc(%d.%d)", cluster_num, proc_num);
	}
	DisconnectQ(0);

#endif

#define TESTING
#if !defined(HPUX9) && !defined(TESTING)
	if( Proc->status != RUNNING ) {
		dprintf( D_ALWAYS, "Shadow: Asked to run proc %d.%d, but status = %d\n",
							Proc->id.cluster, Proc->id.proc, Proc->status );
		dprintf(D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( 0 );	/* don't cleanup here */
	}
#endif

	LocalUsage = Proc->local_usage;
#if defined(NEW_PROC)
	RemoteUsage = Proc->remote_usage[0];
#else
	RemoteUsage = Proc->remote_usage;
#endif
	ImageSize = Proc->image_size;

#if DBM_QUEUE
	CLOSEJOBQUEUE( QueueD );
#endif
	get_client_ids( Proc );
}

DoCleanup()
{
	int		status;
	int		fetch_rval;

	dprintf( D_FULLDEBUG, "Shadow: Entered DoCleanup()\n" );

	if( TmpCkptName[0] != '\0' ) {
		dprintf(D_ALWAYS, "Shadow: DoCleanup: unlinking TmpCkpt '%s'\n",
															TmpCkptName);
		(void) unlink_local_or_ckpt_server( TmpCkptName );
	}

	if( Proc->id.cluster ) {
#if DBM_QUEUE
		if( QueueD == NULL ) {
			if( (QueueD=OpenJobQueue(QueueName, O_RDWR, 0)) == NULL ) {
				dprintf( D_ALWAYS,
						"Shadow: OpenJobQUeue(%s) failed, errno %d\n",
					QueueName, errno );
				dprintf( D_ALWAYS,
				"********** Shadow Exiting **********\n" );
				exit( 1 );
			}
		}
		LockJobQueue( QueueD, WRITER );
		fetch_rval = FetchProc(QueueD,Proc);
		status = Proc->status;
#else
		ConnectQ(NULL);
		fetch_rval = GetAttributeInt(Proc->id.cluster, Proc->id.proc, 
									 "Status", &status);
#endif

		if( fetch_rval < 0 || status == REMOVED ) {
			dprintf(D_ALWAYS, "Job %d.%d has been removed by condor_rm\n",
											Proc->id.cluster, Proc->id.proc );
			dprintf(D_ALWAYS, "Shadow: unlinking Ckpt '%s'\n", CkptName);
			(void) unlink_local_or_ckpt_server( CkptName );
#if DBM_QUEUE
			CLOSEJOBQUEUE( QueueD );
#else
			DisconnectQ(0);
#endif
			return;
		}
			
		if( ! FileExists(CkptName, Proc->owner) ) {
			status = UNEXPANDED;
			dprintf(D_ALWAYS,
					"Shadow: no ckpt %s found, resetting to UNEXPANDED\n",
					CkptName);
		} else {
			dprintf( D_FULLDEBUG,
					"Shadow: ckpt found ok, setting status to IDLE\n");
			status = IDLE;
		}
		dprintf(D_FULLDEBUG, "Shadow: setting CurrentHosts back to 0\n");
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, "CurrentHosts", 0);
		Proc->image_size = ImageSize;
		Proc->status = status;

#if DBM_QUEUE
		if( StoreProc(QueueD,Proc) < 0 ) {
			dprintf( D_ALWAYS, "Shadow: StoreProc(%d.%d) failed, errno = %d",
									Proc->id.cluster, Proc->id.proc, errno );
			dprintf(D_ALWAYS, "********** Shadow Exiting **********\n" );
			exit( 1 );
		}
#else
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, "Status", status);
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, "Image_size", 
						ImageSize);
#endif

#if DBM_QUEUE
		CLOSEJOBQUEUE( QueueD );
#else
		DisconnectQ(0);
#endif
		dprintf( D_ALWAYS, "Shadow: marked job status %d\n", Proc->status );
	}
}

MvTmpCkpt()
{
	char buf[ BUFSIZ * 8 ];
	register int tfd, rfd, rcnt, wcnt;

#ifdef NFSFIX
	/* Need to be root to seteuid() to ClientUid. */
	if(getuid()==0)
		set_root_euid(__FILE__,__LINE__);
#endif NFSFIX

if(getuid()==0){
	if( setegid(CondorGid) < 0 ) {
		EXCEPT("Cannot setegid(%d)", CondorGid);
	}

	if( seteuid(ClientUid) < 0 ) {
		EXCEPT("Cannot seteuid(%d)", ClientUid);
	}
}
	(void)sprintf( TmpCkptName, "%s/job%06d.ckpt.%d.tmp",
			Spool, Proc->id.cluster, Proc->id.proc );

	(void)sprintf( RCkptName, "%s/job%06d.ckpt.%d",
				Proc->rootdir, Proc->id.cluster, Proc->id.proc );

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

	(void)fchown( tfd, CondorUid, CondorGid );

	(void)close( rfd );
	(void)close( tfd );

#ifdef NFSFIX
	/* Go back to being condor. */
	set_condor_euid(__FILE__,__LINE__);
#endif NFSFIX
}

SetSyscalls(){}

open_std_files( proc )
V2_PROC	*proc;
{
	int		fd;

	dprintf( D_FULLDEBUG, "Entered open_std_files()\n" );

	(void)close( 0 );
	(void)close( 1 );
	(void)close( 2 );

	if( (fd=open(proc->in,O_RDONLY,0)) < 0 ) {
		sprintf( ErrBuf, "Can't open \"%s\"", proc->in );
		HadErr = TRUE;
		return;
	}
	if( fd != 0 ) {
		EXCEPT( "open returns %d, expected 0", fd );
	}

	if( (fd=open(proc->out,O_WRONLY,0)) < 0 ) {
		sprintf( ErrBuf, "Can't open \"%s\"", proc->out );
		HadErr = TRUE;
		return;
	}
	if( fd != 1 ) {
		dprintf( D_ALWAYS, "Can't open \"%s\"\n", proc->err );
		EXCEPT( "open returns %d, expected 1", fd );
	}


	if( (fd=open(proc->err,O_WRONLY,0)) < 0 ) {
		sprintf( ErrBuf, "Can't open \"%s\"", proc->err );
		HadErr = TRUE;
		return;
	}
	if( fd != 2 ) {
		EXCEPT( "open returns %d, expected 2", fd );
	}

	dprintf( D_ALWAYS, "Opened \"%s\", \"%s\", and \"%s\"\n",
					proc->in, proc->out, proc->err );
}


/*
  Connect to the startd on the remote host and tell it we are done
  running jobs.
*/
send_quit( host )
char	*host;
{
	int		cmd = KILL_FRGN_JOB;
	int		sock;
	XDR		xdr, *xdrs;

	dprintf( D_FULLDEBUG, "Shadow: Entering send_quit()\n" );
	

		/* Connect to the startd */
	if( (sock = do_connect(host, "condor_startd", START_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
						host);
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Parent Exiting **********\n" );
		exit( 0 );
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

		/* Send the command */
	if( !xdr_int(xdrs, &cmd) ) {
		EXCEPT( "xdr_int()" );
	}
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		EXCEPT( "xdrrec_endofrecord(TRUE)" );
	}

	(void) close( sock );

}

void
regular_setup( host, cluster, proc, capability )
char *host;
char *cluster;
char *proc;
char *capability;
{
	dprintf( D_ALWAYS,
		"Hostname = \"%s\", Job = %s.%s\n",
		host, cluster, proc
	);
	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "Spool directory not specified in config file" );
	}
#if DBM_QUEUE
	(void)sprintf( QueueName, "%s/job_queue", Spool );
#endif

	ExecutingHost = host;
	start_job( cluster, proc );
	send_job( Proc, host, capability );
}

void
pipe_setup( cluster, proc )
char *cluster;
char *proc;
{
	static char	buf[1024];

	UsePipes = TRUE;
	dprintf( D_ALWAYS, "Job = %s.%s\n", cluster, proc );

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "Spool directory not specified in config file" );
	}

#if DBM_QUEUE
	(void)sprintf( QueueName, "%s/job_queue", Spool );
#endif

	if( gethostname(buf,sizeof(buf)) < 0 ) {
		EXCEPT( "gethostname()" );
	}
	ExecutingHost = buf;

	open_named_pipe( "/tmp/syscall_req", O_RDONLY, REQ_SOCK );
	dprintf( D_ALWAYS, "Shadow: REQ_SOCK connected, fd = %d\n", REQ_SOCK );

	open_named_pipe( "/tmp/syscall_rpl", O_WRONLY, RPL_SOCK );
	dprintf( D_ALWAYS, "Shadow: RPL_SOCK connected, fd = %d\n", RPL_SOCK );

	open_named_pipe( "/tmp/log", O_RDONLY, CLIENT_LOG );
	dprintf( D_ALWAYS, "Shadow: CLIENT_LOG connected, fd = %d\n", CLIENT_LOG );

	xdr_RSC1 = RSC_ShadowInit( RSC_SOCK, CLIENT_LOG );
	start_job( cluster, proc );
}

/*
  Open the named pipe in the given mode, and get the file descriptor to
  be "target_fd".
*/
void
open_named_pipe( name, mode, target_fd )
const char *name;
int mode;
int target_fd;
{
	int		fd;

	if( (fd=open(name,mode)) < 0 ) {
		EXCEPT( "Can't open named pipe %s", name );
	}

	if( fd != target_fd ) {
		ASSERT(dup2(fd, target_fd) >= 0);
		(void)close(fd);
	}
}

#include <sys/resource.h>
#define _POSIX_SOURCE
#	if defined(OSF1)
#		include <time.h>			/* need POSIX CLK_TCK */
#	else
#		define _SC_CLK_TCK	3		/* shouldn't do this */
#	endif
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
#include <sys/times.h>		/* need POSIX struct tms */
#undef _POSIX_SOURCE
get_local_rusage( struct rusage *bsd_rusage )
{
	clock_t		user;
	clock_t		sys;
	struct tms	posix_usage;

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
reaper()
{
	pid_t		pid;
#if defined(AIX32)
	union wait 		status;
#else
	int			status;
#endif

#ifdef HPUX9
#undef _BSD
#endif

	for(;;) {
#if defined(AIX32)
		pid = waitpid( -1, &status.w_status, WNOHANG );
#else
		pid = waitpid( -1, &status, WNOHANG );
#endif

		if( pid == 0 || pid == -1 ) {
			break;
		}
		if( WIFEXITED(status) ) {
			dprintf( D_ALWAYS,
				"Reaped child status - pid %d exited with status %d\n",
				pid, WEXITSTATUS(status)
			);
		} else {
			dprintf( D_ALWAYS,
				"Reaped child status - pid %d killed by signal %d\n",
				pid, WTERMSIG(status)
			);
		}
			
	}

#ifdef HPUX9
#define _BSD
#endif
}

void
display_uids()
{
	dprintf( D_ALWAYS, "ruid = %d, euid = %d\n", getuid(), geteuid() );
	dprintf( D_ALWAYS, "rgid = %d, egid = %d\n", getgid(), getegid() );
}


/* 
**                     changes for condor flocking 
**						Dhrubajyoti Borthakur
**							29 July, 1994
**	When condor_rm is executed by the user, it send a command KILL_FRGN_JOB
**	to the schedd. The schedd informs the corresponding shadow. The shadow
**	has to send KILL_FRGN_JOB to the startd on the executing machine.
*/

void
condor_rm()
{
	XDR		xdr, *xdrs = NULL;
	int		sock = -1;
	int		cmd;

	dprintf(D_ALWAYS, "Shadow received rm command from Schedd \n");

      /* Connect to the startd on the serving host */
    if( (sock = do_connect(ExecutingHost, "condor_startd", START_PORT)) < 0 ) {
        dprintf( D_ALWAYS, "Can't connect to startd on %s\n", ExecutingHost );
        return;
    }
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;

    cmd = KILL_FRGN_JOB;
    if( !xdr_int(xdrs, &cmd) ) {
		dprintf( D_ALWAYS, "Can't send KILL_FRGN_JOB cmd to schedd on %s\n",
			ExecutingHost );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}
		
	if( !xdrrec_endofrecord(xdrs,TRUE) ) {
		dprintf( D_ALWAYS, "Can't send xdr end_of_record to schedd on %s\n",
			ExecutingHost );
		xdr_destroy( xdrs );
		close( sock );
		return;
	}

    dprintf( D_ALWAYS,"Sent KILL_FRGN_JOB command to startd on %s\n",
		ExecutingHost);
	xdr_destroy( xdrs );
	close( sock );
}

int
count_open_fds( const char *file, int line )
{
	struct stat buf;
	int		i;
	int		open_files = 0;

	for( i=0; i<256; i++ ) {
		if( fstat( i, &buf ) == 0 ) {
			dprintf( D_ALWAYS, "%d open\n", i );
			open_files++;
		}
	}
	dprintf( D_ALWAYS,
		"At %d in %s %d files are open\n", line, file, open_files );
	return open_files;
}
