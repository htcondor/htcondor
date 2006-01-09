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

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_io.h"
#include "condor_ckpt_name.h"
#include "condor_debug.h"
#include "internet.h"
#include "condor_uid.h"
#include "condor_adtypes.h"
#include "condor_version.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "condor_config.h"
#include "my_hostname.h"
#include "../condor_ckpt_server/server_interface.h"
#include "sig_install.h"
#include "job_report.h"
#include "../condor_c++_util/directory.h"
#include "condor_distribution.h"
#include "condor_environ.h"

#include "user_job_policy.h"

#if defined(AIX31) || defined(AIX32)
#include "syscall.aix.h"
#endif

#include "condor_debug.h"
#include "fileno.h"
#include "exit.h"

/* XXX This should not be here */
#if !defined( WCOREDUMP )
#define  WCOREDUMP(stat)      ((stat)&WCOREFLG)
#endif

int	UsePipes;

char* mySubSystem = "SHADOW";

extern "C" {
#if (defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))) || defined(HPUX11) || defined(AIX)
	/* XXX These should really be selected in a better fashion */
	void reaper(int);
	void handle_sigusr1(int);
	void handle_sigquit(int);
	void handle_sigterm(int);
#else
	void reaper();
	void handle_sigusr1();
	void handle_sigquit();
	void handle_sigterm();
#endif


	void unblock_signal(int sig);
	void block_signal(int sig);
	void display_errors( FILE *fp );
	int count_open_fds( const char *file, int line );
	void HandleSyscalls();
	void Wrapup();
	void send_quit( char *host, char *capability );
	void send_vacate( char *host, char *capability );
	void handle_termination( PROC *proc, char *notification,
				int *jobstatus, char *coredir );
	void get_local_rusage( struct rusage *bsd_rusage );
	void NotifyUser( char *buf, PROC *proc );
	FILE	*fdopen(int, const char *);
	int		whoami(FILE*);
	void update_job_status( struct rusage *localp, struct rusage *remotep );
	void update_job_rusage( struct rusage *localp, struct rusage *remotep );
	int DoCleanup();
	int unlink_local_or_ckpt_server( char* );
	int update_rusage( struct rusage*, struct rusage* );
	void HoldJob( const char * );
}

extern int part_send_job( int test_starter, char *host, int &reason,
			  char *capability, char *schedd, PROC *proc, int &sd1,
	      		  int &sd2, char **name);
extern int send_cmd_to_startd(char *sin_host, char *capability, int cmd);
extern int InitJobAd(int cluster, int proc);
extern int MakeProc(ClassAd *ad, PROC *p);
static bool periodic_policy(void);
static void static_policy(void);

extern int do_REMOTE_syscall();
extern int do_REMOTE_syscall1(int);
extern int do_REMOTE_syscall2(int);
extern int do_REMOTE_syscall3(int);
extern int do_REMOTE_syscall4(int);
extern int do_REMOTE_syscall5(int);

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif

#include "user_log.c++.h"
UserLog ULog;

char * My_Filesystem_Domain;
char * My_UID_Domain;
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
int  ShadowBDate = -1;
int  LastRestartTime = -1;		// time_t when job last restarted from a ckpt
								// or completed a periodic ckpt
int  LastCkptTime = -1;			// time when job last completed a ckpt
int  NumCkpts = 0;				// count of completed checkpoints
int  NumRestarts = 0;			// count of attempted checkpoint restarts
int  CommittedTime = 0;			// run-time committed in checkpoints
// last known checkpointing signature of a successful checkpoint
extern char *LastCkptPlatform;

#ifdef WANT_NETMAN
extern bool ManageBandwidth;	// notify negotiator about network usage?
extern int NetworkHorizon;		// how often do we request network bandwidth?
extern "C" void file_stream_child_exit(pid_t pid);
extern "C" void abort_file_stream_transfer();
extern "C" void RequestRSCBandwidth();
#endif

extern char *Executing_Arch, *Executing_OpSys;

extern char *IpcFile;

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
extern "C"  void log_except(char *);

char	*Spool = NULL;
char	*ExecutingHost = NULL;
char	*GlobalCap = NULL;

// count of total network bytes previously send and received for this
// job from previous runs (i.e., includes ckpt transfers)
extern float	TotalBytesSent, TotalBytesRecvd;

// count of network bytes previously sent and received for RSC only
// from previous runs
float RSCBytesSent = 0.0, RSCBytesRecvd = 0.0;

// count of network bytes send and received outside of CEDAR RSC
// socket during this run
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
#endif /* NOTDEF */

int JobStatus;
struct rusage JobRusage;
int ExitReason = JOB_EXCEPTION;		/* Schedd counts on knowing exit reason */
volatile int check_static_policy = 1;	/* don't check if condor_rm'ed */
int JobExitStatus = 0;                 /* the job's exit status */
int MaxDiscardedRunTime = 3600;

extern "C" int ExceptCleanup(int,int,char*);
extern int Termlog;

ReliSock	*sock_RSC1 = NULL, *RSC_ShadowInit(int rscsock, int errsock);
ReliSock	*RSC_MyShadowInit(int rscsock, int errsock);
int HandleLog();
void RemoveNewShadowDroppings(char *cluster, char *proc);

int MainSymbolExists = 1;

void
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

char		*schedd = NULL, *scheddName = NULL;


void
printClassAd( void )
{
	printf( "%s = False\n", ATTR_IS_DAEMON_CORE );
	printf( "%s = True\n", ATTR_HAS_REMOTE_SYSCALLS );
	printf( "%s = True\n", ATTR_HAS_CHECKPOINTING );
	printf( "%s = True\n", ATTR_HAS_OLD_VANILLA );
	printf( "%s = True\n", ATTR_HAS_JOB_AD_FROM_FILE );
	printf( "%s = \"%s\"\n", ATTR_VERSION, CondorVersion() );
}


/*ARGSUSED*/
int
main(int argc, char *argv[] )
{
	char	*tmp = NULL;
	int		reserved_swap, free_swap;
	char	*host = NULL, *cluster = NULL, *proc = NULL;
	char	*use_afs = NULL, *use_nfs = NULL;
	char	*use_ckpt_server = NULL;
	char	*capability;
	int		i;

	myDistro->Init( argc, argv );
	if( argc == 2 && strincmp(argv[1], "-cl", 3) == MATCH ) {
		printClassAd();
		exit( 0 );
	}

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

	ShadowBDate = LastRestartTime = time(0);

	_EXCEPT_Cleanup = ExceptCleanup;

	MyPid = getpid();
	
	config();

	/* Start up with condor.condor privileges. */
	/*
	  we need to do this AFTER we call config() so that if CONDOR_IDS
	  is being defined in the config file, we'll get the right value
	*/ 
	set_condor_priv();

	dprintf_config( mySubSystem, SHADOW_LOG );
	DebugId = whoami;

	dprintf( D_ALWAYS, "******* Standard Shadow starting up *******\n" );
	dprintf( D_ALWAYS, "** %s\n", CondorVersion() );
	dprintf( D_ALWAYS, "** %s\n", CondorPlatform() );
	dprintf( D_ALWAYS, "*******************************************\n" );

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
	if( argc < 6 ) {
		usage();
	}

#ifdef WAIT_FOR_DEBUGGER
	int x = 1;
	while( x ) {

	}
#endif /* WAIT_FOR_DEBUGGER */

	if( strcmp("-pipe",argv[1]) == 0 ) {
		capability = argv[2];
		cluster = argv[3];
		proc = argv[4];
		// read the big comment in the function for why this is here.
		RemoveNewShadowDroppings(cluster, proc);
		pipe_setup( cluster, proc, capability );
	} else {
		schedd = argv[1];
		host = argv[2];
		capability = argv[3];
		cluster = argv[4];
		proc = argv[5];
		if ( argc > 6 ) {
			IpcFile = argv[6];
			dprintf(D_FULLDEBUG,"Setting IpcFile to %s\n",IpcFile);
		} else {
			IpcFile = NULL;
		}
		// read the big comment in the function for why this is here.
		RemoveNewShadowDroppings(cluster, proc);
		regular_setup( host, cluster, proc, capability );
	}
	scheddName = getenv( EnvGetName( ENV_SCHEDD_NAME ) );

	GlobalCap = strdup(capability);

#if 0
		/* Don't want to share log file lock between child and pnarent */
	(void)close( LockFd );
	LockFd = -1;
#endif

	// initialize the user log
	initializeUserLog();

	My_Filesystem_Domain = param( "FILESYSTEM_DOMAIN" ); 
	dprintf( D_ALWAYS, "My_Filesystem_Domain = \"%s\"\n", 
			 My_Filesystem_Domain );

	My_UID_Domain = param( "UID_DOMAIN" ); 
	dprintf( D_ALWAYS, "My_UID_Domain = \"%s\"\n", My_UID_Domain );

	use_afs = param( "USE_AFS" );
	if( use_afs && (use_afs[0] == 'T' || use_afs[0] == 't') ) {
		UseAFS = TRUE;
	} else {
		UseAFS = FALSE;
	}
    if (use_afs)    free( use_afs );

	use_nfs = param( "USE_NFS" );
	if( use_nfs && (use_nfs[0] == 'T' || use_nfs[0] == 't') ) {
		UseNFS = TRUE;
	} else {
		UseNFS = FALSE;
	}
    if (use_nfs)    free( use_nfs );

	// if job specifies a checkpoint server host, this overrides
	// the config file parameters
	tmp = (char *)malloc(_POSIX_PATH_MAX);
	if (JobAd->LookupString(ATTR_CKPT_SERVER, tmp) == 1) {
		if (CkptServerHost) free(CkptServerHost);
		UseCkptServer = TRUE;
		CkptServerHost = strdup(tmp);
		StarterChoosesCkptServer = FALSE;
		free(tmp);
	} else {
		free(tmp);
		use_ckpt_server = param( "USE_CKPT_SERVER" );
		if (CkptServerHost) {
            free(CkptServerHost);
        }
		CkptServerHost = param( "CKPT_SERVER_HOST" );
		if( !CkptServerHost ||
			(use_ckpt_server && (use_ckpt_server[0] == 'F' ||
								 use_ckpt_server[0] == 'f')) ) {
				// We don't have a ckpt server defined, or the user
				// explicitly configures USE_CKPT_SERVER = False.
			UseCkptServer = FALSE;
		} else {
				// We've got a checkpoint server, so let's use it.
			UseCkptServer = TRUE;
		}
		if (use_ckpt_server) {
            free(use_ckpt_server);
        }

		StarterChoosesCkptServer = TRUE; // True by default
		if( (tmp = param("STARTER_CHOOSES_CKPT_SERVER")) ) {
			if( tmp[0] == 'F' || tmp[0] == 'f' ) {
				StarterChoosesCkptServer = FALSE;
			}
			free(tmp);
		}
	}

		// Initialize location of our checkpoint file.  If we stored it
		// on a checkpoint server then set LastCkptServer.  Otherwise,
		// LastCkptServer should be NULL to indicate that we should
		// look on the local disk for our checkpoint file.
	LastCkptServer = (char *)malloc(_POSIX_PATH_MAX);
	if (JobAd->LookupString(ATTR_LAST_CKPT_SERVER,
							LastCkptServer) == 0) {
		free(LastCkptServer);
		LastCkptServer = NULL;
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
	if (tmp) {
        free(tmp);
    }

	tmp = param( "PERIODIC_MEMORY_SYNC" );
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		PeriodicSync = TRUE;
	} else {
		PeriodicSync = FALSE;
	}
	if (tmp) {
        free(tmp);
    }

	tmp = param( "COMPRESS_VACATE_CKPT" );
	if (tmp && (tmp[0] == 'T' || tmp[0] == 't')) {
		CompressVacateCkpt = TRUE;
	} else {
		CompressVacateCkpt = FALSE;
	}
	if (tmp) {
        free(tmp);
    }

	tmp = param( "SLOW_CKPT_SPEED" );
	if (tmp) {
		SlowCkptSpeed = atoi(tmp);
		free(tmp);
	} else {
		SlowCkptSpeed = 0;
	}

#ifdef WANT_NETMAN
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
		tmp = param("NETWORK_HORIZON");
		if (!tmp) {
			NetworkHorizon = 300;
		} else {
			NetworkHorizon = atoi(tmp);
			free(tmp);
		}
	}
#endif

	// Install signal handlers such that all signals are blocked when inside
	// the handler.
	sigset_t fullset;
	sigfillset(&fullset);
	install_sig_handler_with_mask( SIGCHLD,&fullset, reaper );

		// SIGUSR1 is sent by the schedd when a job is removed with
		// condor_rm.
	install_sig_handler_with_mask( SIGUSR1, &fullset, handle_sigusr1 );

		// SIGQUIT is sent for a fast shutdow.
	install_sig_handler_with_mask( SIGQUIT, &fullset, handle_sigquit );

		// SIGTERM is sent for a graceful shutdow.
	install_sig_handler_with_mask( SIGTERM, &fullset, handle_sigterm );


	/* Here we block the async signals.  We do this mainly because on HPUX,
	 * XDR wigs out if it is interrupted by a signal.  We do it on all
	 * platforms because it seems like a safe idea.  We unblock the signals
	 * during before select(), which is where we spend most of our time. */
	block_signal(SIGCHLD);
	block_signal(SIGUSR1);      

	HandleSyscalls();

	Wrapup();

	/* HACK! WHOOO!!!!! Throw the old shadow away already! */
	/* This will figure out whether or not the job should go on hold AFTER the
		job has exited for whatever reason, or if the job should be allowed
		to exit. It modifies ExitReason approriately for job holding, or, get
		this, just EXCEPTs if the jobs is supposed to go into idle state and
		not leave. :) */
	/* Small change by Todd : only check the static policy if the job really
	   exited of its own accord -- we don't want to even evaluate the static
	   policy if the job exited because it was preempted, for instance */
	if (check_static_policy && 
		(ExitReason == JOB_EXITED || ExitReason == JOB_KILLED 
		     	|| ExitReason == JOB_COREDUMPED)) 
	{
		static_policy();
	}
    if( My_UID_Domain ) {
        free( My_UID_Domain );
    }
    if( My_Filesystem_Domain ) {
        free( My_Filesystem_Domain );
    }
	dprintf( D_ALWAYS, "********** Shadow Exiting(%d) **********\n",
		ExitReason );
	exit( ExitReason );
}



void
HandleSyscalls()
{
	register int	cnt;
	fd_set 			readfds;
	int 			nfds = -1;

	time_t			periodic_interval_len = 20; /* secs, empirically found :) */

	nfds = (RSC_SOCK > CLIENT_LOG ) ? (RSC_SOCK + 1) : (CLIENT_LOG + 1);

	init_user_ids(Proc->owner, NULL);
	set_user_priv();

	dprintf(D_FULLDEBUG, "HandleSyscalls: about to chdir(%s)\n", Proc->iwd);
	if( chdir(Proc->iwd) < 0 ) {
		sprintf( ErrBuf,  "Can't chdir() to \"%s\"! [%s(%d)]", Proc->iwd, 
			strerror(errno), errno );
		HadErr = TRUE;
		return;
	}

	dprintf(D_SYSCALLS, "Shadow: Starting to field syscall requests\n");
	errno = 0;

	time_t current_time = time(0);
	time_t next_periodic_update = current_time + periodic_interval_len;
#if WANT_NETMAN
	time_t next_bw_update = current_time + NetworkHorizon;
#endif
	
	for(;;) {	/* get a request and fulfill it */

		FD_ZERO(&readfds);
		FD_SET(RSC_SOCK, &readfds);
		FD_SET(CLIENT_LOG, &readfds);

		struct timeval *ptimer = NULL, timer;
		timer.tv_sec = next_periodic_update - current_time;
		timer.tv_usec = 0;
		ptimer = &timer;
#if WANT_NETMAN
		if (ManageBandwidth && next_bw_update > current_time) {
			timer.tv_sec = next_bw_update - current_time;
			timer.tv_usec = 0;
			ptimer = &timer;
		}
#endif
		/* if the current timer is set for a time longer than this, than
			truncate the timer required to the periodic limit. After 
			inspection of the bandwidth timer, it seems that it will recorrect
			itself if select comes out of the loop before the timer goes off
			anyway to handle syscalls */
		if ( timer.tv_sec > periodic_interval_len) {
			timer.tv_sec = next_periodic_update - current_time;
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

		if( cnt < 0 && errno == EINTR ) {
			continue;
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

		current_time = time(0);

		/* if this is true, then do the periodic_interval_len events */
		if (current_time >= next_periodic_update) {
			next_periodic_update = current_time + periodic_interval_len;

			/* evaluate some attributes for policies like determining what to
			do if a job suspends wierdly or some such thing. This function
			has the possibility of making the shadow exit with JOB_SHOULD_HOLD
			or futzing up some global variables about how the job could've
			exited and letting Wraup take care of it. */
			if (periodic_policy() == true)
			{
				break;
			}
		}

#if WANT_NETMAN
		if (ManageBandwidth && current_time >= next_bw_update) {
			RequestRSCBandwidth();
			next_bw_update = current_time + NetworkHorizon;
		}
#endif

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

	dprintf(D_FULLDEBUG, "Entering Wrapup()\n" );

	notification[0] = '\0';

	/* This HadErr business isn't well defined. There doesn't seem
		to be a clean way of representing the event of a
		pre-job startup initialization failure.  So, instead of trying
		to propogate the nonexistant JobStatus around to future
		code paths trying to clean up a mess specified by HadErr,
		we are simply going to EXCEPT(). HadErr only gets specified
		when either the std file can't be opened or the shadow
		cannot chdir() to the initialdir(often because of a
		permission/existance problem). */
	if( HadErr ) {
		/* dump the specific error if it has been determined */
		if (ErrBuf[0] != '\0') {
			EXCEPT("%s", ErrBuf);
		} else {
			EXCEPT("Couldn't set up std files or chdir to initialdir--"
				"probably a permissions error.");
		}
	} else {
		/* all event logging has been moved from handle_termination() to
		   log_termination()	--RR */
		handle_termination( Proc, notification, &JobStatus, NULL );
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
		NotifyUser( notification, Proc );
	}
}

/* evaluate various periodic checks during the running of the shadow and
	perform actions based upon what special attributes evaluate to. */
bool periodic_policy(void)
{
	ClassAd *result;
	int val;
	int action;
	char buf[4096];

	/* See what the user job policy has in store for me. */
	result = user_job_policy(JobAd);
	
	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if (val == 1)
	{
		dprintf(D_ALWAYS, "There was an error in the periodic policy\n");
		delete result;
		return false;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == 1)
	{
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);

		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		switch(action)
		{
			case REMOVE_JOB:
				dprintf(D_ALWAYS, "Periodic Policy: removing job because %s "
					"has become true\n", buf);
				/* set some yucky global variables */
				JobStatus = 0;
				JobExitStatus = 0;
				delete result;
				return true;
				break;

			case HOLD_JOB:
				sprintf(buf, "Periodic Policy: holding job because %s has "
					"become true\n", buf);

				sprintf(buf, "Your job has been held because %s has become "
					"true\n", ATTR_PERIODIC_HOLD_CHECK);

				delete result;

				/* This exits */
				HoldJob(buf);
				break;

			default:
				dprintf(D_ALWAYS, "WARNING! Ignoring unknown action type in "
						"periodic_policy()\n");
				delete result;
				return false;
				break;
		}
	}

	delete result;
	return false;
}

/* After the job exits, look into the classad to see if certain things are
	true or not */
void static_policy(void)
{
	ClassAd *result;
	int val;
	int action;
	char buf[4096];

	/* See what the user job policy has in store for me. */
	result = user_job_policy(JobAd);
	
	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if (val == 1)
	{
		dprintf(D_ALWAYS, "There was an error in the static policy\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == 1)
	{
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);

		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		switch(action)
		{
			case REMOVE_JOB:
				dprintf(D_ALWAYS, "Static Policy: removing job because %s has "
					"become true\n", buf);

				/* do nothing, the nasty old shadow logic takes it from here. */

				delete result;
				return;
				break;

			case HOLD_JOB:
				dprintf(D_ALWAYS, "Static Policy: holding job because %s has "
					"become true\n", buf);

				delete result;

				sprintf(buf, "Your job has been held because %s has become "
					"true\n", ATTR_PERIODIC_HOLD_CHECK);

				/* This exits */
				HoldJob(buf);
				return;
				break;

			default:
				dprintf(D_ALWAYS, "WARNING! Ignoring unknown action type in "
						"periodic_policy()\n");
				delete result;
				return;
				break;
		}
	}

	delete result;

	dprintf( D_ALWAYS, "Static policy: don't remove on exit\n" );
	EXCEPT( "Job didn't exit under conditions specifed in %s",
		ATTR_ON_EXIT_REMOVE_CHECK );
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
	int		status = -1;
	float utime = 0.0;
	float stime = 0.0;
	int tot_sus=0, cum_sus=0, last_sus=0;

	// If the job completed, and there is no HISTORY file specified,
	// the don't bother to update the job ClassAd since it is about to be
	// flushed into the bit bucket by the schedd anyway.
	char *myHistoryFile = param("HISTORY");
	if ((Proc->status == COMPLETED) && (myHistoryFile==NULL)) {
		return;
	}

	if (myHistoryFile) {
		free(myHistoryFile);
	}

	if (!JobAd)
	{
		EXCEPT( "update_job_status(): No job ad\n");
	}
	JobAd->LookupInteger(ATTR_TOTAL_SUSPENSIONS, tot_sus);
	JobAd->LookupInteger(ATTR_CUMULATIVE_SUSPENSION_TIME, cum_sus);
	JobAd->LookupInteger(ATTR_LAST_SUSPENSION_TIME, last_sus);

	//new syntax, can use filesystem to authenticate
	if (!ConnectQ(schedd, SHADOW_QMGMT_TIMEOUT) ||
		GetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_JOB_STATUS,
						&status) < 0) {
		EXCEPT("Failed to connect to schedd!");
	}
	job_report_update_queue( Proc );

	if( status == REMOVED ) {
		dprintf( D_ALWAYS, "update_job_status(): Job %d.%d has been removed "
				 "by condor_rm\n", Proc->id.cluster, Proc->id.proc );
	} else {

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, 
			ATTR_TOTAL_SUSPENSIONS, tot_sus);

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, 
			ATTR_CUMULATIVE_SUSPENSION_TIME, cum_sus);

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, 
			ATTR_LAST_SUSPENSION_TIME, last_sus);

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
			float TotalBytesSentUpdate =
				TotalBytesSent + sock_RSC1->get_bytes_sent() + BytesSent;
			float TotalBytesRecvdUpdate =
				TotalBytesRecvd + sock_RSC1->get_bytes_recvd() + BytesRecvd;
			SetAttributeFloat( Proc->id.cluster, Proc->id.proc,
							   ATTR_BYTES_SENT, TotalBytesSentUpdate );
			SetAttributeFloat( Proc->id.cluster, Proc->id.proc,
							   ATTR_BYTES_RECVD, TotalBytesRecvdUpdate );
			float RSCBytesSentUpdate =
				sock_RSC1->get_bytes_sent() + RSCBytesSent;
			float RSCBytesRecvdUpdate = 
				sock_RSC1->get_bytes_recvd() + RSCBytesRecvd;
			SetAttributeFloat( Proc->id.cluster, Proc->id.proc,
							   ATTR_RSC_BYTES_SENT, RSCBytesSentUpdate );
			SetAttributeFloat( Proc->id.cluster, Proc->id.proc,
							   ATTR_RSC_BYTES_RECVD, RSCBytesRecvdUpdate );
		}

		if( ExitReason == JOB_CKPTED || ExitReason == JOB_NOT_CKPTED ) {
			SetAttributeInt( Proc->id.cluster, Proc->id.proc,
							 ATTR_LAST_VACATE_TIME, time(0) );
		}

		// if we had checkpointed, then save all of these attributes as well.
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
				// If we wrote a checkpoint, store the location in the
				// LastCkptServer attribute.  If we didn't use a checkpoint
				// server (i.e., we stored it locally), then make sure
				// no LastCkptServer attribute is set.
			if (LastCkptServer) {
				SetAttributeString(Proc->id.cluster, Proc->id.proc,
								   ATTR_LAST_CKPT_SERVER, LastCkptServer);
			} else {
				DeleteAttribute(Proc->id.cluster, Proc->id.proc,
								   ATTR_LAST_CKPT_SERVER);
			}

			if (LastCkptPlatform) {
				SetAttributeString(Proc->id.cluster, Proc->id.proc,
								   ATTR_LAST_CHECKPOINT_PLATFORM, 
								   LastCkptPlatform);
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

		// if the job completed, then update the exit code/signal attributes
		if (Proc->status == COMPLETED)
		{
			int exit_code, exit_signal, exit_by_signal;

			// only new style ads have ATTR_ON_EXIT_BY_SIGNAL, so only
			// SetAttribute for those types of ads
			if (JobAd->LookupInteger(ATTR_ON_EXIT_BY_SIGNAL, exit_by_signal)==1)
			{
				SetAttributeInt(Proc->id.cluster, Proc->id.proc,
			   		ATTR_ON_EXIT_BY_SIGNAL, exit_by_signal);

				if (exit_by_signal == 1) /* exited via signal */
				{
					JobAd->LookupInteger(ATTR_ON_EXIT_SIGNAL, exit_signal);
					SetAttributeInt(Proc->id.cluster, Proc->id.proc,
						   			ATTR_ON_EXIT_SIGNAL, exit_signal);
				}
				else
				{
					JobAd->LookupInteger(ATTR_ON_EXIT_CODE, exit_code);
					SetAttributeInt(Proc->id.cluster, Proc->id.proc,
						   			ATTR_ON_EXIT_CODE, exit_code);
				}
			}
		}
	}

	if (!DisconnectQ(0)) {
		EXCEPT("Failed to commit updated job queue status!");
	}
}

/*
** Connect to the startd on the remote host and ask to run a job.  The
** connection to the starter will become our main connection to the
** remote starter, and the scheduler will send back an aux port number to
** be used for out of band (error) traffic.
*/
void
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
		dprintf( D_ALWAYS, "********** Shadow Exiting(%d) **********\n",
			reason);
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


/*
** Opens job queue (Q), and reads in process structure (Proc) as side
** affects.
*/
void
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
	JobAd->LookupFloat(ATTR_RSC_BYTES_SENT, RSCBytesSent);
	JobAd->LookupFloat(ATTR_RSC_BYTES_RECVD, RSCBytesRecvd);
	JobAd->LookupInteger(ATTR_NUM_CKPTS, NumCkpts);
	JobAd->LookupInteger(ATTR_NUM_RESTARTS, NumRestarts);

#define TESTING
#if !defined(HPUX) && !defined(TESTING)
	if( Proc->status != RUNNING ) {
		dprintf( D_ALWAYS, "Shadow: Asked to run proc %d.%d, but status = %d\n",
							Proc->id.cluster, Proc->id.proc, Proc->status );
		dprintf(D_ALWAYS, "********** Shadow Exiting(%d) **********\n",
			JOB_BAD_STATUS);
		exit( JOB_BAD_STATUS );	/* don't cleanup here */
	}
#endif

	LocalUsage = Proc->local_usage;
	RemoteUsage = Proc->remote_usage[0];
	ImageSize = Proc->image_size;

	if (Proc->universe != CONDOR_UNIVERSE_STANDARD) {
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
	dprintf( D_FULLDEBUG, "Shadow: Entered DoCleanup()\n" );

	if( TmpCkptName[0] != '\0' ) {
		dprintf(D_ALWAYS, "Shadow: DoCleanup: unlinking TmpCkpt '%s'\n",
															TmpCkptName);
		(void) unlink_local_or_ckpt_server( TmpCkptName );
	}

#if WANT_NETMAN
	abort_file_stream_transfer(); // if we've got one
#endif

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
		sprintf( ErrBuf, "Can't open std input file \"%s\"! [%s(%d)]", 
			proc->in, strerror(errno), errno );
		HadErr = TRUE;
		return;
	}
	if( fd != 0 ) {
		EXCEPT( "open returns %d, expected 0", fd );
	}

	if( (fd=open(proc->out,O_WRONLY,0)) < 0 ) {
		sprintf( ErrBuf, "Can't open std output file \"%s\"! [%s(%d)]",
			proc->out, strerror(errno), errno );
		HadErr = TRUE;
		return;
	}
	if( fd != 1 ) {
		dprintf( D_ALWAYS, "Can't open \"%s\"\n", proc->err );
		EXCEPT( "open returns %d, expected 1", fd );
	}


	if( (fd=open(proc->err,O_WRONLY,0)) < 0 ) {
		sprintf( ErrBuf, "Can't open std error file \"%s\"! [%s(%d)]", 
			proc->err, strerror(errno), errno );
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
  Connect to the startd on the remote host and forcibly vacate our
  claim. 
*/
void
send_quit( char *host, char *capability )
{
	if( send_cmd_to_startd( host, capability, 
							DEACTIVATE_CLAIM_FORCIBLY ) < 0 ) {
		dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
				 host );
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Parent Exiting(%d) **********\n",
			JOB_NOT_STARTED);
		exit( JOB_NOT_STARTED );
	}
}


/*
  Connect to the startd on the remote host and gracefully vacate our
  claim. 
*/
void
send_vacate( char *host, char *capability )
{
	if( send_cmd_to_startd( host, capability, DEACTIVATE_CLAIM ) < 0 ) {
		dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
				 host );
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Parent Exiting(%d) **********\n",
				 JOB_NOT_STARTED );
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
	if( Spool ) {
		free( Spool );
	}
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

	if( Spool ) {
		free( Spool );
	}
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

#if (defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))) || defined(HPUX11) || defined(AIX)
void
reaper(int unused)
#else
void
reaper()
#endif
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
#if WANT_NETMAN
			// tell the file_stream code whenever a child exits, so it
			// can do some cleanup with its children
		file_stream_child_exit(pid);
#endif
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
  When we get a SIGUSR1 (which the schedd sends us on condor_rm), all
  we have to do is send a fast vacate to the job (by sending a
  DEACTIVATE_CLAIM_FORCIBLY command to the startd).  Then, we just
  have to go back to service RSCs while the job cleans itself
  up.  We don't need or want to exit with any special status, since
  the schedd already knows this job should be removed.
  Cleaned up, clarified and simplified on 5/12/00 by Derek Wright
*/
#if (defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))) || defined(HPUX11) || defined(AIX)
void
handle_sigusr1( int unused )
#else
void
handle_sigusr1( void )
#endif
{
	dprintf( D_ALWAYS, 
			 "Shadow received SIGUSR1 (rm command from schedd)\n" );
	check_static_policy = 0;
	send_quit( ExecutingHost, GlobalCap );
}


/*
  If we get a SIGQUIT (from shutdown, a master that's killing our
  process group, etc), we want to behave much like we do when we get
  an rm.  We'll even exit with the same status... the difference is
  that the schedd doesn't have us marked as "pending removal", so when
  we eventually exit with JOB_NOT_CKPTED, we'll stay in the queue.
  We still want to send the DEACTIVATE_CLAIM_FORCIBLY command to the
  startd, to force the job to quickly vacate.
  Cleaned up, clarified and simplified on 5/12/00 by Derek Wright
*/
#if (defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))) || defined(HPUX11) || defined(AIX)
void
handle_sigquit( int unused )
#else
void
handle_sigquit( void )
#endif
{
	dprintf( D_ALWAYS, "Shadow recieved SIGQUIT (fast shutdown)\n" ); 
	check_static_policy = 0;
	send_quit( ExecutingHost, GlobalCap );
}


/*

  If we get a SIGTERM (from the schedd, a shutdown, etc), we want to
  try to do a graceful shutdown and allow our job to checkpoint. 
*/
#if (defined(LINUX) && (defined(GLIBC22) || defined(GLIBC23))) || defined(HPUX11)
void
handle_sigterm( int unused )
#else
void
handle_sigterm( void )
#endif
{
	dprintf( D_ALWAYS, "Shadow recieved SIGTERM (graceful shutdown)\n" ); 
	check_static_policy = 0;
	send_vacate( ExecutingHost, GlobalCap );
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

void RemoveNewShadowDroppings(char *cluster, char *proc)
{
	char names[2][1024];
	int j;
	char *ckpt_name;
	char *myspool;
	struct stat buf;
	int clusternum, procnum;

	memset(&names[0], 0, 1024);
	memset(&names[1], 0, 1024);

	/* XXX I'm sorry.
		There are some incompatibilities between the new
		shadow and the old shadow. The new shadow now makes a
		_directory_ with the usual ckeckpoint name because there
		might eventually be more than one file that has to get
		checkpointed with a job. The old shadow is dumb, and it
		only makes a _file_ named the usual checkpoint name. So a
		contention happens when we are using opsys/arch to choose
		an executable name for both NT and UNIX between vanilla
		only jobs and standard universe jobs. What happens is
		that the old shadow gets back a correct stat() on the new
		shadow created directory but misinterprets it as a file
		and hilarity ensues. So, my nasty hack is to make the
		old shadow determine if the file it found is actually
		a directory and if so, then remove it and everything
		underneath it.	I somehow feel that this might bite us
		in the ass in the future, so each time the shadow does
		this, it logs it so a human can figure out what happened.
		I don't have to worry about the converse issue of a new
		shadow starting up with an old file-based checkpoint
		because whomever adds standard universe support to
		the new shadow will have to do something intelligent,
		and our submit program places expressions into the
		requirements attribute in the job forcing a checkpointed
		job to always run on the architecture it checkpointed
		on. 

		-psilord 7/30/01
	*/

	myspool = param("SPOOL");
	if (myspool == NULL)
	{
		EXCEPT ("RemoveNewShadowDroppings(): No Spool directory!?!\n");
	}
	clusternum = atoi(cluster);
	procnum = atoi(proc);
	if (clusternum < 0 || procnum < 0) /* sanity checks */
	{
		dprintf(D_ALWAYS, "RemoveNewShadowDroppings(): Asked to deal with "
			"negative cluster or proc numbers. Ignoring.\n");
		free(myspool);
		return;
	}
	ckpt_name = gen_ckpt_name( myspool, clusternum, procnum, 0 );

	strcpy(names[0], ckpt_name);
	strcpy(names[1], ckpt_name);
	strcat(names[1], ".tmp");
	
	for (j = 0; j < 2; j++)
	{
		if (stat(names[j], &buf) == 0) {
			/* ok, we have a hit, let's see if it is a directory... */
			if (IsDirectory(names[j]) == true) {
				/* it is, so blow away everything inside it */
				{
					Directory todd_droppings(names[j]);
					if (todd_droppings.Remove_Entire_Directory() == false) {
						dprintf(D_ALWAYS, "RemoveNewShadowDroppings(): Old "
							"shadow failed to remove new shadow ckpt directory "
							"contents: %s\n", names[j]);
						}
				}
				/* now delete the directory itself */
				if (rmdir(names[j]) < 0) {
					dprintf(D_ALWAYS, "RemoveNewShadowDroppings(): Old shadow "
						"failed to remove new shadow ckpt directory: %s (%s)\n",
						names[j], strerror(errno));
				} else {
					dprintf(D_ALWAYS, "RemoveNewShadowDroppings(): Old shadow "
						"removed new shadow ckpt directory: %s\n", names[j]);
				}
			}
		}
	}

	free(myspool);
}
