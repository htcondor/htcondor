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

#define INCLUDE_STATUS_NAME_ARRAY

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "condor_ckpt_name.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "my_hostname.h"
#include "../condor_ckpt_server/server_interface.h"
#include "sig_install.h"
#include "job_report.h"

#if defined(AIX31) || defined(AIX32)
#include "syscall.aix.h"
#endif

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

#include "shadow.h"

#if !defined( WCOREDUMP )
#define  WCOREDUMP(stat)      ((stat)&WCOREFLG)
#endif

int	UsePipes;

char* mySubSystem = "SHADOW";

extern "C" {
	void reaper();
	void rm();
	void condor_rm();
	void unblock_signal(int sig);
	void block_signal(int sig);
	void display_errors( FILE *fp );
	int count_open_fds( const char *file, int line );
	void HandleSyscalls();
	void Wrapup();
	void send_quit( char *host, char *capability );
	void handle_termination( PROC *proc, char *notification,
				int *jobstatus, char *coredir );
	void get_local_rusage( struct rusage *bsd_rusage );
	void NotifyUser( char *buf, PROC *proc, char *email_addr );
	void MvTmpCkpt();
	FILE	*fdopen();
	int		whoami();
	void update_job_status( struct rusage *localp, struct rusage *remotep );
	void update_job_rusage( struct rusage *localp, struct rusage *remotep );
	int DoCleanup();
	int unlink_local_or_ckpt_server( char* );
	int update_rusage( struct rusage*, struct rusage* );
}

extern int part_send_job( int test_starter, char *host, int &reason,
			  char *capability, char *schedd, PROC *proc, int &sd1,
	      		  int &sd2, char **name);
extern int send_cmd_to_startd(char *sin_host, char *capability, int cmd);
extern int InitJobAd(int cluster, int proc);
extern int MakeProc(ClassAd *ad, PROC *p);

extern int do_REMOTE_syscall();
extern int do_REMOTE_syscall1(int);
extern int do_REMOTE_syscall2(int);
extern int do_REMOTE_syscall3(int);
extern int do_REMOTE_syscall4(int);
extern int do_REMOTE_syscall5(int);

extern void RequestRSCBandwidth();

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif

#include "user_log.c++.h"
UserLog ULog;

#define SMALL_STRING 128
char My_Filesystem_Domain[ SMALL_STRING ];
char My_UID_Domain[ SMALL_STRING ];
int  UseAFS;
int  UseNFS;
int  UseCkptServer;
int  StarterChoosesCkptServer;
int  CompressPeriodicCkpt;
int  CompressVacateCkpt;
int  PeriodicSync;
int  SlowCkptSpeed;
char *CkptServerHost = NULL;
char *LastCkptServer = NULL;
int  LastRestartTime = -1;		// time_t when job last restarted from a ckpt
								// or completed a periodic ckpt
int  LastCkptTime = -1;			// time when job last completed a ckpt
int  NumCkpts = 0;				// count of completed checkpoints
int  NumRestarts = 0;			// count of attempted checkpoint restarts
int  CommittedTime = 0;			// run-time committed in checkpoints
bool ManageBandwidth = false;	// notify negotiator about network usage?
int	 BWInterval = 0;			// how often do we request RSC bandwidth?

extern char *Executing_Arch, *Executing_OpSys;

int		MyPid;
int		LogPipe;
int		ImageSize;

GENERIC_PROC GenProc;
#if defined(NEW_PROC)
	PROC	*Proc = (PROC *)&GenProc;
#else
	V2_PROC	*Proc = (V2_PROC *)&GenProc;
#endif

extern "C"  void initializeUserLog();
extern "C"  void log_termination(struct rusage *, struct rusage *);
extern "C"  void log_execute(char *);
extern "C"  void log_except(char *);

char	*Spool = NULL;
char	*ExecutingHost = NULL;
char	*GlobalCap = NULL;
char	*MailerPgm = NULL;

// count of network bytes send and received for this job so far
extern float	TotalBytesSent, TotalBytesRecvd;

// count of network bytes send and received outside of CEDAR RSC socket
extern float	BytesSent, BytesRecvd;

#include "condor_qmgr.h"

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

int JobStatus;
struct rusage JobRusage;
struct rusage AccumRusage;
int ChildPid;
int ExitReason = JOB_EXITED;		/* Schedd counts on knowing exit reason */
int JobExitStatus = 0;                 /* the job's exit status */
int MaxDiscardedRunTime = 3600;

extern "C" int ExceptCleanup(int,int,char*);
int Termlog;

time_t	RunTime;

ReliSock	*sock_RSC1 = NULL, *RSC_ShadowInit(int rscsock, int errsock);
ReliSock	*RSC_MyShadowInit(int rscsock, int errsock);;
int HandleLog();

int MainSymbolExists = 1;

int
usage()
{
	dprintf( D_ALWAYS, "Usage: shadow schedd_addr host capability cluster proc\n" );
	dprintf( D_ALWAYS, "or\n" );
	dprintf( D_ALWAYS, "Usage: shadow -pipe cluster proc\n" );
	exit( JOB_SHADOW_USAGE );
}

#if defined(__STDC__)
	void regular_setup( char *host, char *cluster, char *proc, char* );
	void pipe_setup( char *cluster, char *proc, char* capability );
	void open_named_pipe( const char *name, int mode, int target_fd );
#else
	void regular_setup();
	void pipe_setup();
	void open_named_pipe();
#endif

#if defined(SYSCALL_DEBUG)
	char	*SyscallLabel;
#endif

extern ClassAd *JobAd;

char*		schedd = NULL;

/*ARGSUSED*/
main(int argc, char *argv[], char *envp[])
{
	char	*tmp;
	int		reserved_swap, free_swap;
	char	*host, *cluster, *proc;
	char	*my_fs_domain, *my_uid_domain, *use_afs, *use_nfs;
	char	*use_ckpt_server;
	char	*capability;
	int		i;

	/* on OSF/1 as installed currently on our machines, attempts to read from
	   the FILE * returned by popen() fail if the underlying file descriptor
	   is 1.  This is ugly, but we have 4K fd's so we won't miss these few */

#if !defined(OSF1) && !defined(Solaris)
	close( 0 );
	close( 1 );
	close( 2 );
#endif

#if defined(SYSCALL_DEBUG)
	SyscallLabel = argv[0] + 7;
#endif

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, (SIG_HANDLER)SIG_IGN );
#endif

	if( argc > 1 ) {
		if( strcmp("-t",argv[1]) == MATCH ) {
			Termlog++;
			argv++;
			argc--;
		}
	}

	LastRestartTime = time(0);

	/* Start up with condor.condor privileges. */
	set_condor_priv();

	_EXCEPT_Cleanup = ExceptCleanup;

	MyPid = getpid();
	
	config( 0 );
	dprintf_config( mySubSystem, SHADOW_LOG );
	DebugId = whoami;

	dprintf( D_ALWAYS, "********** Shadow starting up **********\n" );

	if( (tmp=param("RESERVED_SWAP")) == NULL ) {
		reserved_swap = 5 * 1024;			/* 5 megabytes */
	} else {
		reserved_swap = atoi( tmp ) * 1024;	/* Value specified in megabytes */
		free( tmp );
	}

	free_swap = sysapi_swap_space();

	dprintf( D_FULLDEBUG, "*** Reserved Swap = %d\n", reserved_swap );
	dprintf( D_FULLDEBUG, "*** Free Swap = %d\n", free_swap );
	if( free_swap < reserved_swap ) {
		dprintf( D_ALWAYS, "Not enough reserved swap space\n" );
		exit( JOB_NO_MEM );
	}

	dprintf(D_ALWAYS, "uid=%d, euid=%d, gid=%d, egid=%d\n",
		getuid(), geteuid(), getgid(), getegid());

    dprintf(D_FULLDEBUG, "argc = %d\n", argc);
    for(i = 0; i < argc; i++)
    {
        dprintf(D_FULLDEBUG, "argv[%d] = %s\n", i, argv[i]);
    }
	if( argc != 6 ) {
		usage();
	}

#ifdef WAIT_FOR_DEBUGGER
	int x = 1;
	while( x ) {

	}
#endif WAIT_FOR_DEBUGGER

	if( strcmp("-pipe",argv[1]) == 0 ) {
		capability = argv[2];
		cluster = argv[3];
		proc = argv[4];
		pipe_setup( cluster, proc, capability );
	} else {
		schedd = argv[1];
		host = argv[2];
		capability = argv[3];
		cluster = argv[4];
		proc = argv[5];
		regular_setup( host, cluster, proc, capability );
	}

	GlobalCap = strdup(capability);

#if 0
		/* Don't want to share log file lock between child and pnarent */
	(void)close( LockFd );
	LockFd = -1;
#endif

	// initialize the user log
	initializeUserLog();

	// by this time, the job has been sent and accepted to run, so
	// log a ULOG_EXECUTE event
	log_execute(host);

	my_fs_domain = param( "FILESYSTEM_DOMAIN" );
	if( my_fs_domain ) {
		strcpy( My_Filesystem_Domain, my_fs_domain );
	} else {
		strcpy( My_Filesystem_Domain, my_full_hostname() );
	}
	dprintf( D_ALWAYS, "My_Filesystem_Domain = \"%s\"\n", My_Filesystem_Domain );

	my_uid_domain = param( "UID_DOMAIN" );
	if( (my_uid_domain) && (strcasecmp(my_uid_domain,"none") != 0) ) {
		strcpy( My_UID_Domain, my_uid_domain );
	} else {
		strcpy( My_UID_Domain, my_full_hostname() );
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

	// if job specifies a checkpoint server host, this overrides
	// the config file parameters
	tmp = (char *)malloc(_POSIX_PATH_MAX);
	if (JobAd->LookupString(ATTR_USE_CKPT_SERVER, tmp) == 1) {
		if (CkptServerHost) free(CkptServerHost);
		UseCkptServer = TRUE;
		CkptServerHost = strdup(tmp);
		StarterChoosesCkptServer = FALSE;
		free(tmp);
	} else {
		free(tmp);
		use_ckpt_server = param( "USE_CKPT_SERVER" );
		if (CkptServerHost) free(CkptServerHost);
		CkptServerHost = param( "CKPT_SERVER_HOST" );
		if( CkptServerHost && use_ckpt_server &&
			(use_ckpt_server[0] == 'T' || use_ckpt_server[0] == 't') ) {
			UseCkptServer = TRUE;
		} else {
			UseCkptServer = FALSE;
		}
		if (use_ckpt_server) free(use_ckpt_server);

		StarterChoosesCkptServer = TRUE;
		if( (tmp = param("STARTER_CHOOSES_CKPT_SERVER")) ) {
			if( tmp[0] == 'F' || tmp[0] == 'f' ) {
				StarterChoosesCkptServer = FALSE;
			}
			free(tmp);
		}
	}

	tmp = param( "MAX_DISCARDED_RUN_TIME" );
	if (tmp) {
		MaxDiscardedRunTime = atoi(tmp);
		free(tmp);
	} else {
		MaxDiscardedRunTime = 3600;
	}

	tmp = param( "COMPRESS_PERIODIC_CKPT" );
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		CompressPeriodicCkpt = TRUE;
	} else {
		CompressPeriodicCkpt = FALSE;
	}
	if (tmp) free(tmp);

	tmp = param( "PERIODIC_MEMORY_SYNC" );
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		PeriodicSync = TRUE;
	} else {
		PeriodicSync = FALSE;
	}
	if (tmp) free(tmp);

	tmp = param( "COMPRESS_VACATE_CKPT" );
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		CompressVacateCkpt = TRUE;
	} else {
		CompressVacateCkpt = FALSE;
	}
	if (tmp) free(tmp);

	tmp = param( "SLOW_CKPT_SPEED" );
	if (tmp) {
		SlowCkptSpeed = atoi(tmp);
		free(tmp);
	} else {
		SlowCkptSpeed = 0;
	}

	tmp = param("MANAGE_BANDWIDTH");
	if (!tmp) {
		ManageBandwidth = false;
	} else {
		if (tmp[0] == 'T' || tmp[0] == 't') {
			ManageBandwidth = true;
		} else {
			ManageBandwidth = false;
		}
		free(tmp);
		tmp = param("RSC_BANDWIDTH_INTERVAL");
		if (!tmp) {
			BWInterval = 300;	// should be equal to NETWORK_HORIZON
		} else {
			BWInterval = atoi(tmp);
			free(tmp);
		}
	}

	MailerPgm = param( "MAIL" );
	if( MailerPgm == NULL ) {
		MailerPgm = "/bin/mail";
	}

	// Install signal handlers such that all signals are blocked when inside
	// the handler.
	sigset_t fullset;
	sigfillset(&fullset);
	install_sig_handler_with_mask( SIGCHLD,&fullset, reaper );
	install_sig_handler_with_mask( SIGTERM,&fullset, rm );
	// SIGUSR1 is sent by the schedd when a job is removed with condor_rm
	install_sig_handler_with_mask( SIGUSR1,&fullset, condor_rm );

	/* Here we block the async signals.  We do this mainly because on HPUX,
	 * XDR wigs out if it is interrupted by a signal.  We do it on all
	 * platforms because it seems like a safe idea.  We unblock the signals
	 * during before select(), which is where we spend most of our time. */
	block_signal(SIGCHLD);
	block_signal(SIGUSR1);      

	HandleSyscalls();

	Wrapup();

	dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
	exit( ExitReason );
}



void
HandleSyscalls()
{
	int				fake_arg;
	register int	cnt;
	fd_set 			readfds;
	int 			nfds = -1;
	priv_state		priv;
	time_t			last_bw_update = time(0);

	nfds = (RSC_SOCK > CLIENT_LOG ) ? (RSC_SOCK + 1) : (CLIENT_LOG + 1);

	init_user_ids(Proc->owner);
	set_user_priv();

	dprintf(D_FULLDEBUG, "HandleSyscalls: about to chdir(%s)\n", Proc->iwd);
	if( chdir(Proc->iwd) < 0 ) {
		sprintf( ErrBuf,  "Can't access \"%s\"", Proc->iwd );
		HadErr = TRUE;
		return;
	}

	dprintf(D_SYSCALLS, "Shadow: Starting to field syscall requests\n");
	errno = 0;

	for(;;) {	/* get a request and fulfill it */

		FD_ZERO(&readfds);
		FD_SET(RSC_SOCK, &readfds);
		FD_SET(CLIENT_LOG, &readfds);

		struct timeval *ptimer = NULL, timer;
		if (ManageBandwidth) {
			timer.tv_sec = BWInterval;
			timer.tv_usec = 0;
			ptimer = &timer;
		}

		unblock_signal(SIGCHLD);
		unblock_signal(SIGUSR1);
#if defined(LINUX) || defined(IRIX) || defined(Solaris)
		cnt = select(nfds, &readfds, (fd_set *)0, (fd_set *)0, ptimer);
#else
		cnt = select(nfds, &readfds, 0, 0, ptimer);
#endif
		block_signal(SIGCHLD);
		block_signal(SIGUSR1);

		if( cnt < 0 && errno != EINTR ) {
			EXCEPT("HandleSyscalls: select: errno=%d, rsc_sock=%d, client_log=%d",errno,RSC_SOCK,CLIENT_LOG);
		}

		if( FD_ISSET(CLIENT_LOG, &readfds) ) {
			if( HandleLog() < 0 ) {
				EXCEPT( "Peer went away" );
			}
		}

		if( FD_ISSET(RSC_SOCK, &readfds) ) {
			if( do_REMOTE_syscall() < 0 ) {
				dprintf(D_SYSCALLS,
						"Shadow: do_REMOTE_syscall returned < 0\n");
				break;
			}
		}

		if( FD_ISSET(UMBILICAL, &readfds) ) {
			dprintf(D_ALWAYS,
				"Shadow: Local scheduler apparently died, so I die too\n");
			exit(1);
		}

		if (ManageBandwidth) {
			time_t current_time = time(0);
			if (current_time > last_bw_update + BWInterval) {
				RequestRSCBandwidth();
				last_bw_update = current_time;
			}
		}

#if defined(SYSCALL_DEBUG)
		strcpy( SyscallLabel, "shadow" );
#endif
	}

	/*
	The user job might exit while there is still unread data in the log.
	So, select with a timeout of zero, and flush everything from the log.
	*/
		/* 
		   NOTE: Since HandleLog does it's own loop to make sure it's
		   read everything, we don't need a loop here, and should only
		   call HandleLog once.  In fact, if there's a problem w/
		   select(), a loop here can cause an infinite loop.  
		   -Derek Wright and Jim Basney, 2/17/99.
		*/
	HandleLog();
	
		/* Take back normal condor privileges */
	set_condor_priv();

		/* If we are debugging with named pipes as our communications medium,
		   won't have a condor_startd running - don't try to send to it.
		*/
	if( !UsePipes ) {
		send_quit( ExecutingHost, GlobalCap );
	}

	dprintf(D_ALWAYS,
		"Shadow: Job %d.%d exited, termsig = %d, coredump = %d, retcode = %d\n",
			Proc->id.cluster, Proc->id.proc, WTERMSIG(JobStatus),
			WCOREDUMP(JobStatus), WEXITSTATUS(JobStatus));
}

void
Wrapup( )
{
	struct rusage local_rusage;
	char notification[ BUFSIZ ];
	char email_addr[ 256 ]; 
	int pid;
	int	cnt;
	int	nfds;
	FILE	*log_fp;
	int		s;
	char coredir[ MAXPATHLEN ];
	int		coredir_len;
	int		msg_type;

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
		/* all event logging has been moved from handle_termination() to
		   log_termination()	--RR */
		handle_termination( Proc, notification, &JobStatus, NULL );
	}

	if( sock_RSC1 ) {
	} 

	/*
	 * the job may have an email address to whom the notification message
     * should go.  this info is in the job classad
     */
	email_addr[0] = '\0';
	if (!JobAd->LookupString(ATTR_NOTIFY_USER, email_addr)) {
        dprintf (D_ALWAYS, "Job %d.%d did not have 'NotifyUser'\n",
				 Proc->id.cluster, Proc->id.proc);
        strcpy (email_addr, Proc->owner);
    }
    else if (!email_addr[0])
    {
        dprintf (D_ALWAYS,
		    "Job %d.%d has NULL email address - using owner instead\n",
                                Proc->id.cluster, Proc->id.proc);
        strcpy (email_addr, Proc->owner);
    }

	/* fill in the Proc structure's exit_status with JobStatus, so that when
	 * we call update_job_status the exit status is written into the job queue,
	 * so that the history file will have exit status information for the job.
	 * note that JobStatus is in BSD format, and Proc->exit_status is in POSIX
	 * int format, so we copy very carefully.  on most platforms the 2 formats
	 * are interchangeable, so most of the time we'll get intelligent results.
	 *    -Todd, 11/95
	*/
	memcpy(&(Proc->exit_status[0]),&JobStatus,sizeof((Proc->exit_status[0])));
	get_local_rusage( &local_rusage );
	update_job_status( &local_rusage, &JobRusage );

	/* log the event associated with the termination; pass local and remote
	   rusage for the run.  The total local and remote rusages for the job are
	   stored in the Proc --- these values were updated by update_job_status */
	log_termination (&local_rusage, &JobRusage);

	if( notification[0] ) {
		NotifyUser( notification, Proc, email_addr );
	}
}

void
update_job_rusage( struct rusage *localp, struct rusage *remotep )
{
	static struct rusage tstartup_local, tstartup_remote;
	static int tstartup_flag = 0;

	/* Here we keep usage info on the job when the Shadow first starts in
		 * tstartup_local/remote, so we can "reset" it back each subsequent
		 * time we are called so that update_rusage always adds up correctly.
		 * We are called by pseudo calls really_exit and update_rusage, and 
		 * both of these are passing in times _since_ the remote job started, 
		 * NOT since the last time this function was called.  Thus this code 
		 * here.... -Todd */
	if (tstartup_flag == 0 ) {
		tstartup_flag = 1;
		memcpy(&tstartup_local, &Proc->local_usage, sizeof(struct rusage));
		memcpy(&tstartup_remote, &Proc->remote_usage[0],
			   sizeof(struct rusage));
	} else {
		memcpy(&Proc->local_usage,&tstartup_local,sizeof(struct rusage));
		/* Now only copy if time in remotep > 0.  Otherwise, we'll
		 * erase the time updates from any periodic checkpoints if
		 * the job ends up exiting without a checkpoint on this
		 * run.  -Todd, 6/97.  */
		if ( remotep->ru_utime.tv_sec + remotep->ru_stime.tv_sec > 0 )
			memcpy(&Proc->remote_usage[0],&tstartup_remote,
				   sizeof(struct rusage) );
	}

	update_rusage( &Proc->local_usage, localp );
	update_rusage( &Proc->remote_usage[0], remotep );
}

void
update_job_status( struct rusage *localp, struct rusage *remotep )
{
	PROC	my_proc;
	PROC	*proc = &my_proc;
	int		status = -1;
	float utime = 0.0;
	float stime = 0.0;

	//new syntax, can use filesystem to authenticate
	if (!ConnectQ(schedd, SHADOW_QMGMT_TIMEOUT)) {
		EXCEPT("Failed to connect to schedd!");
	}
	job_report_update_queue( Proc );
	GetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_JOB_STATUS, &status);

	if( status == REMOVED ) {
		dprintf( D_ALWAYS, "update_job_status(): Job %d.%d has been removed "
				 "by condor_rm\n", Proc->id.cluster, Proc->id.proc );
	} else {

		update_job_rusage( localp, remotep );

		Proc->image_size = ImageSize;

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_IMAGE_SIZE, 
						ImageSize);

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_JOB_EXIT_STATUS,
						JobExitStatus);

		rusage_to_float( Proc->local_usage, &utime, &stime );
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc,
						  ATTR_JOB_LOCAL_USER_CPU, utime);
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc,
						  ATTR_JOB_LOCAL_SYS_CPU, stime);

		rusage_to_float( Proc->remote_usage[0], &utime, &stime );
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc,
						  ATTR_JOB_REMOTE_USER_CPU, utime);
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc,
						  ATTR_JOB_REMOTE_SYS_CPU, stime);
		dprintf(D_FULLDEBUG,"TIME DEBUG 3 USR remotep=%lu Proc=%lu utime=%f\n",
				remotep->ru_utime.tv_sec,
				Proc->remote_usage[0].ru_utime.tv_sec, utime);
		dprintf(D_FULLDEBUG,"TIME DEBUG 4 SYS remotep=%lu Proc=%lu utime=%f\n",
				remotep->ru_stime.tv_sec,
				Proc->remote_usage[0].ru_stime.tv_sec, stime);

		if( sock_RSC1 ) {
			TotalBytesSent += sock_RSC1->get_bytes_sent() + BytesSent;
			TotalBytesRecvd += sock_RSC1->get_bytes_recvd() + BytesRecvd;
			SetAttributeFloat( Proc->id.cluster, Proc->id.proc,
							   ATTR_BYTES_SENT, TotalBytesSent );
			SetAttributeFloat( Proc->id.cluster, Proc->id.proc,
							   ATTR_BYTES_RECVD, TotalBytesRecvd );
		}

		if( ExitReason == JOB_CKPTED || ExitReason == JOB_NOT_CKPTED ) {
			SetAttributeInt( Proc->id.cluster, Proc->id.proc,
							 ATTR_LAST_VACATE_TIME, time(0) );
		}

		if (LastCkptServer) {
			SetAttributeString(Proc->id.cluster, Proc->id.proc,
							   ATTR_LAST_CKPT_SERVER, LastCkptServer);
		}
		if (LastCkptTime > LastRestartTime) {
			SetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_LAST_CKPT_TIME, LastCkptTime);
			CommittedTime=0;
			GetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_JOB_COMMITTED_TIME, &CommittedTime);
			CommittedTime += LastCkptTime - LastRestartTime;
			SetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_JOB_COMMITTED_TIME, CommittedTime);
			LastRestartTime = LastCkptTime;
			SetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_NUM_CKPTS, NumCkpts);
			SetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_NUM_RESTARTS, NumRestarts);
			if (Executing_Arch) {
				SetAttributeString(Proc->id.cluster, Proc->id.proc,
								   ATTR_CKPT_ARCH, Executing_Arch);
			}
			if (Executing_OpSys) {
				SetAttributeString(Proc->id.cluster, Proc->id.proc,
								   ATTR_CKPT_OPSYS, Executing_OpSys);
			}
		}
		// if the job completed, we should include the run-time in
		// committed time, since it contributed to the completion of
		// the job
		if (Proc->status == COMPLETED) {
			CommittedTime = 0;
			GetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_JOB_COMMITTED_TIME, &CommittedTime);
			CommittedTime += Proc->completion_date - LastRestartTime;
			SetAttributeInt(Proc->id.cluster, Proc->id.proc,
							ATTR_JOB_COMMITTED_TIME, CommittedTime);
		}
	}

	DisconnectQ(0);
}

/*
** Connect to the startd on the remote host and ask to run a job.  The
** connection to the starter will become our main connection to the
** remote starter, and the scheduler will send back an aux port number to
** be used for out of band (error) traffic.
*/
int
#if defined(NEW_PROC)
send_job( PROC *proc, char *host, char *cap)
#else
send_job( V2_PROC *proc, char *host, char *cap)
#endif
{
	int reason, retval, sd1, sd2;
	char*	capability = strdup(cap);

	dprintf( D_FULLDEBUG, "Shadow: Entering send_job()\n" );

		/* starter 0 - Regular starter */
		/* starter 1 - PVM starter */
	retval = part_send_job(0, host, reason, capability, schedd, proc, sd1, sd2, NULL);
	if (retval == -1) {
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( reason );
	}
	if( sd1 != RSC_SOCK ) {
		ASSERT(dup2(sd1, RSC_SOCK) >= 0);
		(void)close(sd1);
	}
	dprintf( D_ALWAYS, "Shadow: RSC_SOCK connected, fd = %d\n", RSC_SOCK );

	if( sd2 != CLIENT_LOG ) {
		ASSERT(dup2(sd2, CLIENT_LOG) >= 0);
		(void)close(sd2);
	}
	dprintf( D_ALWAYS, "Shadow: CLIENT_LOG connected, fd = %d\n", CLIENT_LOG );

	sock_RSC1 = RSC_ShadowInit( RSC_SOCK, CLIENT_LOG );
}


extern char	*SigNames[];

/*
** Opens job queue (Q), and reads in process structure (Proc) as side
** affects.
*/
int
start_job( char *cluster_id, char *proc_id )
{
	int		cluster_num;
	int		proc_num;
	char	*tmp;

	Proc->id.cluster = atoi( cluster_id );
	Proc->id.proc = atoi( proc_id );

	cluster_num = atoi( cluster_id );
	proc_num = atoi( proc_id );

	InitJobAd(cluster_num, proc_num); // make sure we have the job classad

	if (MakeProc(JobAd, Proc) < 0) {
		EXCEPT("MakeProc()");
	}

	JobAd->LookupFloat(ATTR_BYTES_SENT, TotalBytesSent);
	JobAd->LookupFloat(ATTR_BYTES_RECVD, TotalBytesRecvd);
	JobAd->LookupInteger(ATTR_NUM_CKPTS, NumCkpts);
	JobAd->LookupInteger(ATTR_NUM_RESTARTS, NumRestarts);

#define TESTING
#if !defined(HPUX) && !defined(TESTING)
	if( Proc->status != RUNNING ) {
		dprintf( D_ALWAYS, "Shadow: Asked to run proc %d.%d, but status = %d\n",
							Proc->id.cluster, Proc->id.proc, Proc->status );
		dprintf(D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( JOB_BAD_STATUS );	/* don't cleanup here */
	}
#endif

	LocalUsage = Proc->local_usage;
	RemoteUsage = Proc->remote_usage[0];
	ImageSize = Proc->image_size;

	if (Proc->universe != STANDARD) {
		strcpy( CkptName, "" );
		strcpy( TmpCkptName, "" );
	} else {
		tmp = gen_ckpt_name( Spool, Proc->id.cluster, Proc->id.proc, 0 );
		strcpy( CkptName, tmp );
		sprintf( TmpCkptName, "%s.tmp", CkptName );
	}

	tmp = gen_ckpt_name( Spool, Proc->id.cluster, ICKPT, 0 );
	strcpy( ICkptName, tmp );

	strcpy( RCkptName, CkptName );
}

extern "C" {
int
ExceptCleanup(int, int, char *buf)
{
  log_except(buf);
  return DoCleanup();
}
} /* extern "C" */

int
DoCleanup()
{
	int		status = -1;
	int		fetch_rval;

	dprintf( D_FULLDEBUG, "Shadow: Entered DoCleanup()\n" );

	if( TmpCkptName[0] != '\0' ) {
		dprintf(D_ALWAYS, "Shadow: DoCleanup: unlinking TmpCkpt '%s'\n",
															TmpCkptName);
		(void) unlink_local_or_ckpt_server( TmpCkptName );
	}

	return 0;
}


void
open_std_files( V2_PROC *proc )
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
void
send_quit( char *host, char *capability )
{
	if (send_cmd_to_startd(host, capability, KILL_FRGN_JOB) < 0) {
		dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
						host);
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Parent Exiting **********\n" );
		exit( JOB_NOT_STARTED );
	}
}

void
regular_setup( char *host, char *cluster, char *proc, char *capability )
{
	dprintf( D_ALWAYS,
		"Hostname = \"%s\", Job = %s.%s\n",
		host, cluster, proc
	);
	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "Spool directory not specified in config file" );
	}

	if (host[0] != '<') {
		EXCEPT( "Want sinful string !!" );
	}
	ExecutingHost = host;
	start_job( cluster, proc );
	send_job( Proc, host, capability );
}

void
pipe_setup( char *cluster, char *proc, char *capability )
{
	static char	host[1024];

	UsePipes = TRUE;
	dprintf( D_ALWAYS, "Job = %s.%s\n", cluster, proc );

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "Spool directory not specified in config file" );
	}

	strcpy( host, my_hostname() );
	ExecutingHost = host;

	open_named_pipe( "/tmp/syscall_req", O_RDONLY, REQ_SOCK );
	dprintf( D_ALWAYS, "Shadow: REQ_SOCK connected, fd = %d\n", REQ_SOCK );

	open_named_pipe( "/tmp/syscall_rpl", O_WRONLY, RPL_SOCK );
	dprintf( D_ALWAYS, "Shadow: RPL_SOCK connected, fd = %d\n", RPL_SOCK );

	open_named_pipe( "/tmp/log", O_RDONLY, CLIENT_LOG );
	dprintf( D_ALWAYS, "Shadow: CLIENT_LOG connected, fd = %d\n", CLIENT_LOG );

	sock_RSC1 = RSC_ShadowInit( RSC_SOCK, CLIENT_LOG );
	start_job( cluster, proc );
}

/*
  Open the named pipe in the given mode, and get the file descriptor to
  be "target_fd".
*/
void
open_named_pipe( const char *name, int mode, int target_fd )
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

void
reaper()
{
	pid_t		pid;
	int			status;

#ifdef HPUX
#undef _BSD
#endif

	for(;;) {
		pid = waitpid( -1, &status, WNOHANG );

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

#ifdef HPUX
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
	int retval;

	dprintf(D_ALWAYS, "Shadow received rm command from Schedd \n");

	retval = send_cmd_to_startd(ExecutingHost, GlobalCap, KILL_FRGN_JOB);

	if (retval < 0) 
		return;
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
