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
int	UsePipes;

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

typedef void (*SIG_HANDLER)();
extern "C" {
	char *param(char *);
	void install_sig_handler( int sig, SIG_HANDLER handler );
	void reaper();
	void rm();
	void condor_rm();
	void unblock_signal(int sig);
	void block_signal(int sig);
	void display_errors( FILE *fp );
	int count_open_fds( const char *file, int line );
	void HandleSyscalls();
	void Wrapup();
	void send_quit( char *host );
	void handle_termination( char *notification );
	void get_local_rusage( struct rusage *bsd_rusage );
	int calc_virt_memory();
	int close_kmem();
	void NotifyUser( char *buf, char *email_addr );
	void MvTmpCkpt();
	void display_uids();
	EXPR	*scan(), *create_expr();
	ELEM	*create_elem();
	CONTEXT	*create_context(), *build_job_context(PROC *);
	FILE	*fdopen();
	int		whoami();
	void update_job_status( struct rusage *localp, struct rusage *remotep );
#if defined(HPUX9)
	pid_t waitpid(pid_t pid, int *statusp, int options);
#else
	int waitpid(int pid, int *statusp, int options);
#endif
}

extern int getJobAd(int cluster_id, int proc_id, ClassAd *new_ad);

int do_REMOTE_syscall();

#if !defined(AIX31) && !defined(AIX32)
char *strcpy();
#endif


#define SMALL_STRING 128
char My_AFS_Cell[ SMALL_STRING ];
char My_Filesystem_Domain[ SMALL_STRING ];
char My_UID_Domain[ SMALL_STRING ];
int  UseAFS;
int  UseNFS;
int  UseCkptServer;

extern int	_Condor_SwitchUids;

int		MyPid;
int		LogPipe;
int		ImageSize;

GENERIC_PROC GenProc;
#if defined(NEW_PROC)
	PROC	*Proc = (PROC *)&GenProc;;
#else
	V2_PROC	*Proc = (V2_PROC *)&GenProc;;
#endif

extern "C"  void initializeUserLog(PROC *);
extern "C"  void log_termination(struct rusage *, struct rusage *);
extern "C"  void log_execute(char *);
extern "C"  void log_except();

char	*Spool;
char	*ExecutingHost;
char	*MailerPgm;

#include "condor_qmgr.h"

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
int ExitReason = JOB_EXITED;		/* Schedd counts on knowing exit reason */

int DoCleanup();
int ExceptCleanup();
int Termlog;

ReliSock	*sock_RSC1, *RSC_ShadowInit(int rscsock, int errsock);
ReliSock	*RSC_MyShadowInit(int rscsock, int errsock);;
int HandleLog();

int MainSymbolExists = 1;

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

#ifdef CARMI_OPS
#include <ProcList.h>

extern ProcLIST* ProcList;
extern HostLIST* HostList;
extern ClassLIST* ClassList;
extern ScheddLIST* ScheddList;
extern SpawnLIST* SpawnList;
#endif

char*		schedd;

/*ARGSUSED*/
main(int argc, char *argv[], char *envp[])
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


#if !defined(OSF1) && !defined(Solaris)
#ifndef CARMI_OPS
	close( 0 );					// stdin should remain open for CARMI_OPS
#endif
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

#if defined(USE_ROOT_RUID) && defined(NFSFIX)
	/* Must be condor to write to log files. */
	set_condor_euid(__FILE__,__LINE__);
#endif NFSFIX


	_EXCEPT_Cleanup = ExceptCleanup;
	MyPid = getpid();
	
	config( 0 );
	dprintf_config( "SHADOW", SHADOW_LOG );
	DebugId = whoami;

	dprintf( D_ALWAYS, "********** Shadow starting up **********\n" );

	if( (tmp=param("RESERVED_SWAP")) == NULL ) {
		reserved_swap = 5 * 1024;			/* 5 megabytes */
	} else {
		reserved_swap = atoi( tmp ) * 1024;	/* Value specified in megabytes */
	}

#if defined(USE_ROOT_RUID) && defined(NFSFIX)
	if (getuid()==0) {
		set_root_euid(__FILE__,__LINE__);
	}
#endif

	free_swap = calc_virt_memory();
	close_kmem(); /* only calc virt mem once, so close unneeded fs's */

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


#if 0
		/* Don't want to share log file lock between child and pnarent */
	(void)close( LockFd );
	LockFd = -1;
#endif

	// initialize the user log
	initializeUserLog(Proc);

	// by this time, the job has been sent and accepted to run, so
	// log a ULOG_EXECUTE event
	log_execute(host);

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
	if( (my_uid_domain) && (strcasecmp(my_uid_domain,"none") != 0) ) {
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

	ConnectQ(schedd);
	SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_CURRENT_HOSTS, 0);
	DisconnectQ(NULL);

	dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
	exit( ExitReason );
}

void
HandleSyscalls()
{
	int		fake_arg;
	register int cnt;
	fd_set readfds;
	int nfds = -1;

	nfds = (RSC_SOCK > CLIENT_LOG ) ? (RSC_SOCK + 1) : (CLIENT_LOG + 1);

#ifdef CARMI_OPS
	struct timeval select_to;
	fd_set exceptfds;
	int sockfd, errsock;
	struct sockaddr_in from;
	int len = sizeof(from);
	ProcLIST *plist;
	ProcLIST *tempproc;
	NotifyLIST *temp, *next;
	int list;

	Proc = &(ProcList->proc);
#endif

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
#ifdef CARMI_OPS
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		nfds = (nfds > fileno(stdin))?nfds:fileno(stdin);
		
		plist = ProcList;
		while (plist != NULL)
		{
		  if (plist->proc.status != COMPLETED)
		  {
		     FD_SET(plist->rsc_sock, &readfds);
		     if (plist->rsc_sock > nfds) nfds = plist->rsc_sock;
		     FD_SET(plist->client_log, &readfds);
		     if (plist->client_log > nfds) nfds = plist->client_log;
		  }
		  plist = plist->next;
		}
		
		select_to.tv_sec = 1;
		select_to.tv_usec = 0;
		unblock_signal(SIGCHLD);
		unblock_signal(SIGUSR1);
		/* TODO: include LogSock in this select */
		cnt = select(nfds + 1, (fd_set *) &readfds, (fd_set *) 0,
			 (fd_set *) 0, &select_to);
		block_signal(SIGCHLD);
		block_signal(SIGUSR1);

		if (cnt <= 0) /* means timer expiration or interrupt got */
		  continue;
		    
		if (FD_ISSET(fileno(stdin), &readfds))
		    {
		      /* means new host from the schedd */
		      NewHostFound();
		    }
		
		plist = ProcList;
		while (plist != NULL)
		{
		  if (FD_ISSET(plist->client_log, &readfds))
		    {
		      char buf[275];
		      int len;

		      dprintf(D_ALWAYS, "ErrSock read to be done \n");
                      len = read(plist->client_log, buf, sizeof(buf)-1);
		      if (len != 0)
			{
			  int old_cnt, cnter;

			  buf[len] = '\0';
			  old_cnt = 0;
			  for(cnter = 0; cnter <= len; cnter++)
			    {
			      if ((buf[cnter] == '\n' || buf[cnter] == '\0')&&
				  (cnter - old_cnt > 1))
				  {
				    buf[cnter] = '\0';
				    dprintf(D_ALWAYS | D_NOHEADER, "->%s\n",
					    &(buf[old_cnt]));
                                    old_cnt = cnter+1;
				  }
	                     }
                        }
       		   }
			plist = plist->next;
	        }

		plist = ProcList;
		while (plist != NULL)
		{
		  if (FD_ISSET(plist->rsc_sock, &readfds))
                    {
		      if (do_REMOTE_syscall(plist->xdr_RSC1, plist) < 0)
			{
			  if( !UsePipes ) 
			    {
			      send_quit( plist->hostname );
			    }
			  
			  dprintf(D_ALWAYS,
				  "Shadow: Job %d.%d exited, 
				  termsig = %d, coredump = %d, retcode = %d\n",
				  Proc->id.cluster, Proc->id.proc, 
				  JobStatus.w_termsig, JobStatus.w_coredump, 
				  JobStatus.w_retcode );
			  
			  Proc = &(plist->proc);
			  Wrapup();
			  xdr_destroy(plist->xdr_RSC1);
			  free(plist->xdr_RSC1);
			  close(plist->client_log);
			  close(plist->rsc_sock);
			  for(tempproc = ProcList; 
v			      ((tempproc->next != plist)&&(ProcList != plist));
			      tempproc = tempproc->next);
			  if (ProcList == plist)
			    {
			      /* have to delete the head */
			      ProcList = ProcList->next;

			      /* Empty out the DelNotifyList, etc. from plist */
			      /*
			      for (list=1; list<=3; list++)
			      {
				   if (list == 1) temp=plist->DelNotifyList;
				   if (list == 2) temp=plist->ResNotifyList;
				   if (list == 3) temp=plist->SusNotifyList;

				   while(temp)
				   {
				       next = temp->next;
				       free(temp);
				       temp = next;
				   }
				}
				*/

			      free(plist);
			      plist = ProcList;
			    }
			  else
			    {
			      tempproc->next = plist->next;

			      /* Empty out the DelNotifyList, etc. from plist */
			      /*
			      for (list=1; list<=3; list++)
			      {
				   if (list == 1) temp=plist->DelNotifyList;
				   if (list == 2) temp=plist->ResNotifyList;
				   if (list == 3) temp=plist->SusNotifyList;
				   while(temp)
				   {
				       next = temp->next;
				       free(temp);
				       temp = next;
				   }
				}
				*/

			      free(plist);
			      plist = tempproc->next;
			    }
			}
		      else
			plist = plist->next;
		    }
		  else
		    plist = plist->next;
		}
		nfds = -1;
		if ((ProcList == NULL) && (SpawnList == NULL))  break;
	
#else
		FD_ZERO(&readfds);
		FD_SET(RSC_SOCK, &readfds);
		FD_SET(CLIENT_LOG, &readfds);

		unblock_signal(SIGCHLD);
		unblock_signal(SIGUSR1);
#if defined(AIX31) || defined(AIX32)
		errno = EINTR; /* Shouldn't need to do this... */ 
#endif
#if defined(LINUX) || defined(IRIX62) || defined(Solaris)
		cnt = select(nfds, &readfds, (fd_set *)0, (fd_set *)0,
					 (struct timeval *)0);
#else
		cnt = select(nfds, &readfds, 0, 0,
					 (struct timeval *)0);
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

#if defined(SYSCALL_DEBUG)
		strcpy( SyscallLabel, "shadow" );
#endif
	}
#endif // CARMI_OPS

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
		/* we need to set the Uid _and_ the Gid so that we can 
		 * log properly -Todd, 2/97 */
		if( setegid(CondorGid) < 0 ) {
			EXCEPT( "setegid(%d)", CondorGid );
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
		/* all event logging has been moved from handle_termination() to
		   log_termination()	--RR */
		handle_termination( notification );
	}

	
	/*
	 * the job may have an email address to whom the notification message
     * should go.  this info is in the classad, and must be gotten from the
     * Qmgr *before* the job status is updated (i.e., classad is dequeued).
     */
#ifndef DBM_QUEUE
    ConnectQ (NULL);
    if (-1 == GetAttributeString (Proc->id.cluster,Proc->id.proc,"NotifyUser",
                email_addr))
    {
        dprintf (D_ALWAYS, "Job %d.%d did not have 'NotifyUser'\n",
                                Proc->id.cluster, Proc->id.proc);
        strcpy (email_addr, Proc->owner);
    }
    DisconnectQ (NULL);
#else
    strcpy (email_addr, Proc->owner);
#endif

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
		NotifyUser( notification, email_addr );
	}
}

void
handle_termination( char *notification )
{
	char	coredir[ 4096 ];
	dprintf(D_FULLDEBUG, "handle_termination() called.\n");

	switch( JobStatus.w_termsig ) {

	 case 0: /* If core, bad executable -- otherwise a normal exit */
		if( JobStatus.w_coredump && JobStatus.w_retcode == ENOEXEC ) {
			(void)sprintf( notification, "is not executable." );
			dprintf( D_ALWAYS, "Shadow: Job file not executable" );
			ExitReason = JOB_EXCEPTION;
		} else if( JobStatus.w_coredump && JobStatus.w_retcode == 0 ) {
				(void)sprintf(notification,
"was killed because it was not properly linked for execution \nwith Version 5 Condor.\n" );
				MainSymbolExists = FALSE;
				ExitReason = JOB_EXCEPTION;
		} else {
			(void)sprintf(notification, "exited with status %d.",
					JobStatus.w_retcode );
			dprintf(D_ALWAYS, "Shadow: Job exited normally with status %d\n",
				JobStatus.w_retcode );
			ExitReason = JOB_EXITED;
		}

		Proc->status = COMPLETED;
		Proc->completion_date = time( (time_t *)0 );
		break;

	 case SIGKILL:	/* Kicked off without a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was kicked off without a checkpoint\n" );
		DoCleanup();
		ExitReason = JOB_NOT_CKPTED;
		break;

	 case SIGQUIT:	/* Kicked off, but with a checkpoint */
		dprintf(D_ALWAYS, "Shadow: Job was checkpointed\n" );
		if( strcmp(Proc->rootdir, "/") != 0 ) {
			MvTmpCkpt();
		}
		Proc->status = IDLE;
		ExitReason = JOB_CKPTED;
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
			} else {
				(void)sprintf(notification,
					"was killed by signal %d.\nCore file is %s%s/core.%d.%d.",
					 JobStatus.w_termsig,Proc->rootdir, coredir, 
					 Proc->id.cluster, Proc->id.proc);
			}
			ExitReason = JOB_COREDUMPED;
		} else {
			(void)sprintf(notification,
				"was killed by signal %d.", JobStatus.w_termsig);
			ExitReason = JOB_KILLED;
		}
		dprintf(D_ALWAYS, "Shadow: %s\n", notification);

		Proc->status = COMPLETED;
		Proc->completion_date = time( (time_t *)0 );
		break;
	}
}


char *
d_format_time( double dsecs )
{
	int days, hours, minutes, secs;
	static char answer[25];

#define SECONDS	1
#define MINUTES	(60 * SECONDS)
#define HOURS	(60 * MINUTES)
#define DAYS	(24 * HOURS)

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
NotifyUser( char *buf, char *email_addr )
{
	FILE *mailer;
	char cmd[ BUFSIZ ];
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

	/* the "-s" tries to set the subject line appropriately */

	if ( strchr(email_addr,'@') == NULL ) 
	{
		/* No host name specified; add uid domain */
		/* Note that if uid domain was set by the admin to "none", then */
		/* earlier we have already set it to be the submit machine's name */
		sprintf(cmd, "%s -s \"Condor Job %d.%d\" %s@%s\n",
		               MailerPgm, Proc->id.cluster, Proc->id.proc, email_addr,
		               My_UID_Domain
		               );
	}
	else
	{
	    /* host name specified; don't add uid domain */
	    sprintf(cmd, "%s -s \"Condor Job %d.%d\" %s\n",
	                MailerPgm, Proc->id.cluster, Proc->id.proc, email_addr);
	}
	
	dprintf( D_FULLDEBUG, "Notify user using cmd: %s",cmd);


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
#if defined(Solaris)
	fprintf(mailer, "\t%s\n", Proc->cmd[0] );
#else
	fprintf(mailer, "\t%s %s\n", Proc->cmd[0], Proc->args[0] );
#endif
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

unlink_local_or_ckpt_server( char *file )
{
	int		rval;

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
update_job_status( struct rusage *localp, struct rusage *remotep )
{
	PROC	my_proc;
	PROC	*proc = &my_proc;
	static struct rusage tstartup_local, tstartup_remote;
	static int tstartup_flag = 0;
	int		status;

	ConnectQ(schedd);
	GetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_JOB_STATUS, &status);

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
		 * tstartup_local/remote, so we can "reset" it back each subsequent
		 * time we are called so that update_rusage always adds up correctly.
		 * We are called by pseudo calls really_exit and update_rusage, and 
		 * both of these are passing in times _since_ the remote job started, 
		 * NOT since the last time this function was called.  Thus this code 
		 * here.... -Todd */
		if (tstartup_flag == 0 ) {
			tstartup_flag = 1;
			memcpy(&tstartup_local, &Proc->local_usage, sizeof(struct rusage) );
			memcpy(&tstartup_remote, &Proc->remote_usage[0], sizeof(struct rusage) );
		} else {
			memcpy(&Proc->local_usage,&tstartup_local,sizeof(struct rusage));
			/* Now only copy if time in remotep > 0.  Otherwise, we'll erase the
			 * time updates from any periodic checkpoints if the job ends up 
			 * exiting without a checkpoint on this run.  -Todd, 6/97. 
			*/
			if ( remotep->ru_utime.tv_sec + remotep->ru_stime.tv_sec > 0 )
				memcpy(&Proc->remote_usage[0],&tstartup_remote, sizeof(struct rusage) );
		}

		update_rusage( &Proc->local_usage, localp );
		update_rusage( &Proc->remote_usage[0], remotep );
		Proc->image_size = ImageSize;

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_IMAGE_SIZE, 
						ImageSize);
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_JOB_STATUS, 
						Proc->status);
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc, ATTR_JOB_LOCAL_CPU, 
						  rusage_to_float(Proc->local_usage));
		SetAttributeFloat(Proc->id.cluster, Proc->id.proc,ATTR_JOB_REMOTE_CPU, 
						  rusage_to_float(Proc->remote_usage[0]));
		dprintf( D_ALWAYS, "Shadow: marked job status %d\n", Proc->status );

		if( Proc->status == COMPLETED ) {
			if( DestroyProc(Proc->id.cluster, Proc->id.proc) < 0 ) {
				EXCEPT("DestroyProc(%d.%d)", Proc->id.cluster, Proc->id.proc );
			}
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
#if defined(NEW_PROC)
send_job( PROC *proc, char *host, char *cap)
#else
send_job( V2_PROC *proc, char *host, char *cap)
#endif
{
	int		cmd;
	int		reply;
	PORTS	ports;
	int		sd;
	ReliSock	*sock;
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
	if( (sd = do_connect(host, "condor_startd", START_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Shadow: Can't connect to condor_startd on %s\n",
						host);
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( JOB_NOT_STARTED );
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
    if(!sock->put(capability, SIZE_OF_CAPABILITY_STRING))
    {
        EXCEPT( "sock->put()" );
    }
    free(capability);

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
	ASSERT( sock->eom() );

	if( reply != OK ) {
		dprintf( D_ALWAYS, "Shadow: Request to run a job was REFUSED\n" );
		DoCleanup();
		dprintf( D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( JOB_NOT_STARTED );
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
        exit( JOB_NOT_STARTED );
    }
    /* end  flock ; dhruba */

	delete sock;

#ifdef CARMI_OPS

	if( (ProcList->rsc_sock = do_connect(host, (char *)0, (u_short)ports.port1)) < 0 ) {
		EXCEPT( "connect to scheduler on \"%s\", port1 = %d",
													host, ports.port1 );
	}

	dprintf( D_ALWAYS, "Shadow: RSC_SOCK connected, fd = %d\n", ProcList->rsc_sock);

	if( (ProcList->client_log = do_connect(host, (char *)0, (u_short)ports.port2)) < 0 ) {
		EXCEPT( "connect to scheduler on \"%s\", port2 = %d",
													host, ports.port2 );
	}

	dprintf( D_ALWAYS, "Shadow: CLIENT_LOG connected, fd = %d\n", ProcList->client_log );

	ProcList->xdr_RSC1 = RSC_MyShadowInit( &(ProcList->rsc_sock), &(ProcList->client_log));
        free(stRec.server_name); 

#else

	if( (sd = do_connect(host, (char *)0, (u_short)ports.port1)) < 0 ) {
		EXCEPT( "connect to scheduler on \"%s\", port1 = %d",
													host, ports.port1 );
	}

	if( sd != RSC_SOCK ) {
		ASSERT(dup2(sd, RSC_SOCK) >= 0);
		(void)close(sd);
	}
	dprintf( D_ALWAYS, "Shadow: RSC_SOCK connected, fd = %d\n", RSC_SOCK );

	if( (sd = do_connect(host, (char *)0, (u_short)ports.port2)) < 0 ) {
		EXCEPT( "connect to scheduler on \"%s\", port2 = %d",
													host, ports.port2 );
	}

	if( sd != CLIENT_LOG ) {
		ASSERT(dup2(sd, CLIENT_LOG) >= 0);
		(void)close(sd);
	}
	dprintf( D_ALWAYS, "Shadow: CLIENT_LOG connected, fd = %d\n", CLIENT_LOG );

	sock_RSC1 = RSC_ShadowInit( RSC_SOCK, CLIENT_LOG );

#endif // CARMI_OPS

}


get_client_ids( PROC *proc )
{
	struct passwd *pwd;
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

#if 0 /* dead code??? */
send_job_info( ReliSock *sock, V2_PROC *proc )
{
	struct stat st_buf;
	char	*return_name;
	char	*orig_name;
	CONTEXT *context;
	int		soft_kill = SIGUSR1;

	dprintf( D_ALWAYS, "Shadow: send_job_info( 0x%x, %d.%d, %s, %s, %s, %s )\n",
		sock, proc->id.cluster, proc->id.proc,
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

	sock->encode();
	dprintf( D_FULLDEBUG, "Shadow: Sending proc structure\n" );
	V2_ProcObj procobj(proc);
	ClassAd ad(&procobj);
	ASSERT(ad.put(*sock));

	return_name = RCkptName;
	dprintf( D_FULLDEBUG, "Shadow: Sending name = '%s'\n", return_name );
	ASSERT( sock->code(return_name) );

	dprintf( D_FULLDEBUG, "Shadow: Sending orig_name = %s\n", orig_name );
	ASSERT( sock->code(orig_name) );

	dprintf( D_FULLDEBUG, "Shadow: Sending soft_kill = %d\n", soft_kill );
	ASSERT( sock->signal(soft_kill) );

	ASSERT( sock->end_of_message() );
}
#endif


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

/*
** Opens job queue (Q), and reads in process structure (Proc) as side
** affects.
*/
start_job( char *cluster_id, char *proc_id )
{
	int		cluster_num;
	int		proc_num;

#ifdef CARMI_OPS
	ProcLIST*	Proc;

	Proc = (ProcLIST *) malloc(sizeof(ProcLIST)); /* is this needed ?? */
	Proc->proc.id.cluster = atoi( cluster_id );
	Proc->proc.id.proc = atoi( proc_id );
#else
	Proc->id.cluster = atoi( cluster_id );
	Proc->id.proc = atoi( proc_id );
#endif

	cluster_num = atoi( cluster_id );
	proc_num = atoi( proc_id );

	ConnectQ(schedd);
#ifdef CARMI_OPS
	if (GetProc(cluster_num, proc_num, &(Proc->proc)) < 0) {
		EXCEPT("GetProc(%d.%d)", cluster_num, proc_num);
	}
#else
	if (GetProc(cluster_num, proc_num, Proc) < 0) {
		EXCEPT("GetProc(%d.%d)", cluster_num, proc_num);
	}
#endif

	DisconnectQ(0);

#define TESTING
#if !defined(HPUX9) && !defined(TESTING)
	if( Proc->status != RUNNING ) {
		dprintf( D_ALWAYS, "Shadow: Asked to run proc %d.%d, but status = %d\n",
							Proc->id.cluster, Proc->id.proc, Proc->status );
		dprintf(D_ALWAYS, "********** Shadow Exiting **********\n" );
		exit( JOB_BAD_STATUS );	/* don't cleanup here */
	}
#endif

#ifdef CARMI_OPS
	LocalUsage = Proc->proc.local_usage;
#if defined(NEW_PROC)
	RemoteUsage = Proc->proc.remote_usage[0];
#else
	RemoteUsage = Proc->proc.remote_usage;
#endif
	ImageSize = Proc->proc.image_size;

	get_client_ids( &(Proc->proc) );

	strcpy( Proc->CkptName, CkptName );
	strcpy( Proc->ICkptName, ICkptName );
	strcpy( Proc->RCkptName, RCkptName );
	strcpy( Proc->TmpCkptName, TmpCkptName );
	Proc->proc.n_pipes = 0; // temporary 
	Proc->procId = (ProcList == NULL) ? 1 : ProcList->procId + 1;
	Proc->next = ProcList;
	ProcList = Proc;
	/* don't forget to add the host here */
	/* and also to update the class information */
#else
	LocalUsage = Proc->local_usage;
#if defined(NEW_PROC)
	RemoteUsage = Proc->remote_usage[0];
#else
	RemoteUsage = Proc->remote_usage;
#endif
	ImageSize = Proc->image_size;

	get_client_ids( Proc );
#endif
}

int
ExceptCleanup()
{
  log_except();
  return DoCleanup();
}

int
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
		ConnectQ(schedd);
		fetch_rval = GetAttributeInt(Proc->id.cluster, Proc->id.proc, 
									 ATTR_JOB_STATUS, &status);

		if( fetch_rval < 0 || status == REMOVED ) {
			dprintf(D_ALWAYS, "Job %d.%d has been removed by condor_rm\n",
											Proc->id.cluster, Proc->id.proc );
			dprintf(D_ALWAYS, "Shadow: unlinking Ckpt '%s'\n", CkptName);
			(void) unlink_local_or_ckpt_server( CkptName );
			DisconnectQ(0);
			return 0;
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
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_CURRENT_HOSTS,0);
		Proc->image_size = ImageSize;
		Proc->status = status;

		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_JOB_STATUS,
						status);
		SetAttributeInt(Proc->id.cluster, Proc->id.proc, ATTR_IMAGE_SIZE, 
						ImageSize);

		DisconnectQ(0);
		dprintf( D_ALWAYS, "Shadow: marked job status %d\n", Proc->status );
	}

	return 0;
}

void
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

extern "C" SetSyscalls(){}

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
send_quit( char *host )
{
	int		cmd = KILL_FRGN_JOB;
	int		sd;
	ReliSock	sock(host, 0);

	dprintf( D_FULLDEBUG, "Shadow: Entering send_quit(%s)\n", host );

	sock.encode();

		/* Send the command */
	if( !sock.code(cmd) || !sock.end_of_message() ) {
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

	ExecutingHost = host;
	start_job( cluster, proc );
#ifdef CARMI_OPS
	send_job( &(ProcList->proc), host, capability );
	strcpy(ProcList->hostname, host);
#else
	send_job( Proc, host, capability );
#endif
}

void
pipe_setup( char *cluster, char *proc, char *capability )
{
	static char	buf[1024];

	UsePipes = TRUE;
	dprintf( D_ALWAYS, "Job = %s.%s\n", cluster, proc );

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "Spool directory not specified in config file" );
	}

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

#include <sys/resource.h>
#define _POSIX_SOURCE
#	if defined(OSF1)
#		include <time.h>			/* need POSIX CLK_TCK */
#	elif !defined(HPUX9)
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
void
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
#ifdef CARMI_OPS
	SpawnLIST* sp_list;
	SpawnLIST* sp_prev;
	int found;
#endif
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

#ifdef CARMI_OPS
                found = 0;
		sp_list = SpawnList;
		sp_prev = NULL;
		if( WIFEXITED(status) ) {
			dprintf( D_ALWAYS,
				"Reaped child status - pid %d exited with status %d\n",
				pid, WEXITSTATUS(status)
			);
			if ( ((status >> 8) & 0x0ff) == 2)
			{
			  /* the process reaped was a spawn process */
			  while(!found && (sp_list != NULL))
			  {
			    if (sp_list->pid == pid)
			       found = 1;
                            else
			     {
			       sp_prev = sp_list;
			       sp_list = sp_list->next;
			     }
			  }
			  if (found)
			    {
			      regular_setup(sp_list->hostname, sp_list->Cl_str,
					    sp_list->Pr_str);
			      if (!sp_prev)
				SpawnList = sp_list->next;
			      else
				sp_prev->next = sp_list->next;
			      
			      free(sp_list);
			    }
			}
						  
		} else {
			dprintf( D_ALWAYS,
				"Reaped child status - pid %d killed by signal %d\n",
				pid, WTERMSIG(status)
			);
		}
			
	}

#else

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

#endif

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
#ifdef CARMI_OPS
	ProcLIST*       plist;

	for (plist = ProcList; plist != NULL; plist = ProcList)	{
		ReliSock	sock(plist->hostname, START_PORT);

#else

	ReliSock	sock(ExecutingHost, START_PORT);

#endif

	int			cmd;

	dprintf(D_ALWAYS, "Shadow received rm command from Schedd \n");

    sock.encode();

    cmd = KILL_FRGN_JOB;
    if( !sock.code(cmd) ) {
		dprintf( D_ALWAYS, "Can't send KILL_FRGN_JOB cmd to schedd on %s\n",
			ExecutingHost );
		return;
	}
		
	if( !sock.end_of_message() ) {
		dprintf( D_ALWAYS, "Can't send end_of_message to schedd on %s\n",
			ExecutingHost );
		return;
	}

    dprintf( D_ALWAYS,"Sent KILL_FRGN_JOB command to startd on %s\n",
		ExecutingHost);

#ifdef CARMI_OPS
	 xdr_destroy(ProcList->xdr_RSC1);
	 free(ProcList->xdr_RSC1);
	 close(ProcList->rsc_sock);
	 close(ProcList->client_log);
	 ProcList = plist->next;
	 free(plist);
       }
#endif

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
