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
** Modified by : Dhaval N. Shah
**               University of Wisconsin, Computer Sciences Dept.
** Uses <IP:PORT> rather than hostnames
** 
*/ 

#define _POSIX_SOURCE
#include "condor_common.h"

#include <netdb.h>
#include <pwd.h>

#if defined(IRIX331)
#define __EXTENSIONS__
#include <signal.h>
#define BADSIG SIG_ERR
#undef __EXTENSIONS__
#else
#include <signal.h>
#endif

#include <sys/socket.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/utsname.h>

#include "condor_fix_wait.h"

#if defined(HPUX9)
#include "fake_flock.h"
#endif

#if defined(Solaris)
#include "fake_flock.h"
#endif

#include <sys/stat.h>
#include <netinet/in.h>
#include "condor_types.h"
#include "debug.h"
#include "trace.h"
#include "except.h"
#include "sched.h"
#include "clib.h"
#include "exit.h"
#include "dgram_io_handle.h" /* defines DRAM_IO_HANDLE */

#include "condor_fdset.h"

#if defined(BSD43) || defined(DYNIX)
#	define WEXITSTATUS(x) ((x).w_retcode)
#	define WTERMSIG(x) ((x).w_termsig)
#endif

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

CONTEXT	*create_context();

void	reaper();
void	swap_space_exhausted();
void	sigint_handler();
void	sighup_handler();

extern int	errno;
extern int	HasSigchldHandler;
extern char *gen_ckpt_name();
extern EXPR* scan(char*);
extern char*	sin_to_string(struct sockaddr_in*);

char	*param();

CONTEXT	*MachineContext;

char	*Log;
int		SchedDInterval;
int		Foreground = 1;
int		Termlog;
char	*CollectorHost;
char	*NegotiatorHost;
char	*Spool;
char	*Shadow;
int		MaxJobStarts;
int		MaxJobsRunning;
int		JobsRunning;
char	*CondorAdministrator;
char	*Mail;
char *MySockName1=0; /* dhaval */

char	filename[MAXPATHLEN]; /* nameof file to save UpDown object */
int ScheddScalingFactor, ScheddHeavyUserTime ; /* variables for UPDOWN algorithm */
int Step, ScheddHeavyUserPriority;	       /* variables for UPDOWN algorithm */

int		On = 1;

#ifdef vax
struct linger linger = { 0, 0 };	/* Don't linger */
#endif vax

int		ConnectionSock;
int		UdpSock;
DGRAM_IO_HANDLE CollectorHandle;
#if DBM_QUEUE
DBM		*Q, *OpenJobQueue();
#else
#include "condor_qmgr.h"
#endif

extern int	Terse;
extern int	ReservedSwap;		/* Swap space to reserve in kbytes */
extern int	ShadowSizeEstimate;	/* Size of shadow process in kbytes */

char 	*MyName;
time_t	LastTimeout;

#ifdef CONDOR_HISTORY
char *History;

int ClientTimeout;			/* timeout value for get_history */
int ClientSkt = -1;
#endif

usage( name )
char	*name;
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t]\n", name );
	exit( 1 );
}

main_c( argc, argv)
int		argc;
char	*argv[];
{
	int		count;
	fd_set	readfds;
	struct timeval	timer;
	char	**ptr;
#ifdef CONDOR_HISTORY
	void	alarm_handler();
#endif
	sigset_t nset;
	char	job_queue_name[_POSIX_PATH_MAX];
    char    Name[MAXHOSTNAMELEN];
    ELEM    temp;
    int     ScheddName=0;
    struct  utsname name;

	if (getuid()==0) {
#ifdef NFSFIX
		/* Must be condor to write to log files. */
		set_condor_euid(__FILE__,__LINE__);
#endif NFSFIX
	}

	MachineContext = create_context();

	MyName = *argv;
	config( MyName, MachineContext );

	init_params();
	Terse = 1;


	if( argc > 3 ) {
		usage( argv[0] );
	}
	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
			case 'f':
				Foreground = 1;
				break;
			case 't':
				Termlog++;
				break;
			case 'b':
				Foreground = 0;
				break;
			case 'n':
                strcpy(Name,*(++ptr));
                ++ptr;
                ScheddName++;
                break;
			default:
				usage( argv[0] );
		}
	}

    /* If a name was not specified, assume the name of the machine */
    if (!ScheddName)
    {
        uname (&name);
        strcpy (Name, name.nodename);
    }

    /* Throw the name into the machine context */
    {
        EXPR *name_expr;
        char NameExpr [MAXHOSTNAMELEN + 10];
        sprintf (NameExpr, "Name = \"%s\"", Name);
        name_expr = scan (NameExpr);
        if (name_expr == NULL)
        {
                EXCEPT ("Could not scan expression: [%s]", NameExpr);
        }
        store_stmt (name_expr, MachineContext);
    }


		/* This is so if we dump core it'll go in the log directory */
	if( chdir(Log) < 0 ) {
		EXCEPT( "chdir to log directory <%s>", Log );
	}

		/* Arrange to run in background */
	if( !Foreground ) {
		if( fork() )
			exit( 0 );
	}

		/* Set up logging */
	dprintf_config( "SCHEDD", 2 );

	dprintf( D_ALWAYS, "**************************************************\n" );
	dprintf( D_ALWAYS, "***          CONDOR_SCHEDD STARTING UP         ***\n" );
	dprintf( D_ALWAYS, "**************************************************\n" );
	dprintf( D_ALWAYS, "\n" );

	install_sig_handler(SIGINT,sigint_handler);
	install_sig_handler(SIGHUP,sighup_handler);
	install_sig_handler(SIGPIPE,SIG_IGN);
#ifdef CONDOR_HISTORY
	install_sig_handler(SIGALRM,alarm_handler);
	ClientTimeout = 120;
#endif

	ConnectionSock = init_connection_sock( "condor_schedd", SCHED_PORT );
	
	 UdpSock = udp_unconnect();
	fill_dgram_io_handle(&CollectorHandle, CollectorHost,
			     UdpSock, COLLECTOR_UDP_PORT);

	install_sig_handler(SIGCHLD,reaper);
	HasSigchldHandler = TRUE;

	install_sig_handler(SIGUSR1,swap_space_exhausted);

#if DBM_QUEUE
	create_job_queue();
#else
	sprintf(job_queue_name, "%s/job_queue.log", Spool);
	ReadLog(job_queue_name);
#endif
	mark_jobs_idle();

	LastTimeout = time( (time_t *)0 );     /* UPDOWN */
	timeout();
	LastTimeout = time( (time_t *)0 );

	for(;;) {

		FD_ZERO( &readfds );
		FD_SET( ConnectionSock, &readfds );

		/* Under silly HPUX, the xdr library routines do not deal with being
		 * interrupted by a signal very well at all.  So, we leave most signals
		 * blocked when on HPUX: we unblock them before the main outer loop 
		 * select call to let them run, and then block again after the select
		 * returns.  Todd Tannenbaum, July 1995 */
		/* This just seems like a good idea in general. JCP June '96 */
		sigemptyset(&nset);
		sigaddset(&nset,SIGCHLD);
		sigaddset(&nset,SIGUSR1);
		sigaddset(&nset,SIGHUP);
		sigprocmask(SIG_UNBLOCK,&nset,NULL);

		timer.tv_usec = 0;
		timer.tv_sec = SchedDInterval - ( time((time_t *)0) - LastTimeout );
		if( timer.tv_sec < 0 ) {
			timer.tv_sec = 0;
		}
		if( timer.tv_sec > SchedDInterval ) {
			timer.tv_sec = 0;
		}
#if defined(AIX31) || defined(AIX32)
		errno = EINTR;	/* Shouldn't have to do this... */
#endif
		count = select(FD_SETSIZE, (fd_set *)&readfds, (fd_set *)0, 
					   (fd_set *)0, (struct timeval *)&timer );
		if( count < 0 ) {
			if( errno == EINTR ) {
				timeout();
				continue;
			} else {
				EXCEPT( "select(FD_SETSIZE,0%o,0,0,%d sec)",
												readfds, timer.tv_sec );
			}
		}

		/* Under silly HPUX, the xdr library routines do not deal with being
		 * interrupted by a signal very well at all.  So, we leave most signals
		 * blocked when on HPUX: we unblock them before the main outer loop 
		 * select call to let them run, and then block again after the select
		 * returns.  Todd Tannenbaum, July 1995 */
		/* This just seems like a good idea in general. JCP June '96 */
		sigemptyset(&nset);
		sigaddset(&nset,SIGCHLD);
		sigaddset(&nset,SIGUSR1);
		sigaddset(&nset,SIGHUP);
		sigprocmask(SIG_BLOCK,&nset,NULL);

		if( NFDS(count) == 0 ) {
			timeout();
			LastTimeout = time( (time_t *)0 );
		} else {
			if( !FD_ISSET(ConnectionSock,&readfds) ) {
				EXCEPT( "select returns %d, ConnectionSock (%d) not set",
												NFDS(count), ConnectionSock );
			}
			accept_connection();
		}
	}
}

XDR * xdr_Init( int *sock, XDR *xdrs );

accept_connection()
{
	struct sockaddr_in	from;
	int		len;
	int		fd;
	XDR		xdr, *xdrs;

	len = sizeof from;
	memset( (char *)&from,0, sizeof from );
#ifndef CONDOR_HISTORY
	fd = accept( ConnectionSock, (struct sockaddr *)&from, &len );

	if( fd < 0 && errno != EINTR ) {
		EXCEPT( "accept" );
	}

	if( fd >= 0 ) {
		xdrs = xdr_Init( &fd, &xdr );
		do_command( xdrs );
		xdr_destroy( xdrs );
		(void)close( fd );
	}
#else
	ClientSkt = accept( ConnectionSock, (struct sockaddr *)&from, &len);

	if( ClientSkt < 0 && errno != EINTR ) {
		EXCEPT( "accept" );
	}

	if( ClientSkt >= 0 ) {
		xdrs = xdr_Init( &ClientSkt, &xdr );
#if 0	/* may cause a job queue deadlock, leave it out for now */
		(void) alarm( (unsigned) ClientTimeout );	/* Don't hang here forever*/
#endif
		do_command( xdrs );
#if 0
		(void) alarm( (unsigned) 0 );				/* Cancel alarm */
#endif
		xdr_destroy( xdrs );
		(void)close( ClientSkt );
	}
#endif
}

init_connection_sock( service, port )
char	*service;
int		port;
{
	struct sockaddr_in	sin;
	struct servent *servp;
	int		sock;

	memset( (char *)&sin, 0,sizeof sin );
	servp = getservbyname(service, "tcp");
	if( servp ) {
		sin.sin_port = htons( (u_short)servp->s_port );
	} else {
		sin.sin_port = htons( (u_short)port );
	}

	if( (sock=socket(AF_INET,SOCK_STREAM,0)) < 0 ) {
		EXCEPT( "socket" );
	}

#if defined(Solaris)
	if( setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char*)&On,sizeof(On)) <0) {
#else
	if( setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(caddr_t *)&On,sizeof(On)) <0) {
#endif
		EXCEPT( "setsockopt" );
	}

#ifdef vax
	if( setsockopt(sock,SOL_SOCKET,SO_LINGER,&linger,sizeof(linger)) < 0 ) {
		EXCEPT( "setsockopt" );
	}
#endif vax

	if( bind(sock,(struct sockaddr *)&sin,sizeof(sin)) < 0 ) {
		if( errno == EADDRINUSE ) {
			EXCEPT( "CONDOR_SCHEDD ALREADY RUNNING" );
		} else {
			EXCEPT( "bind" );
		}
	}

	get_inet_address( &(sin.sin_addr) );
	MySockName1 = strdup( sin_to_string( &sin ) ); /* dhaval */
	dprintf(D_ALWAYS, "Listening at address %s\n", MySockName1);

	if( listen(sock,5) < 0 ) {
		EXCEPT( "listen" );
	}

	return sock;
}


/*
** Somebody has connected to our socket with a request.  Read the request
** and handle it.
*/
do_command( xdrs )
XDR		*xdrs;
{
	int		cmd;

		/* Read the request */
	xdrs->x_op = XDR_DECODE;
	if( !xdr_int(xdrs,&cmd) ) {
		dprintf( D_ALWAYS, "Can't read command\n" );
		return;
	}

	switch( cmd ) {
		case NEGOTIATE:		/* Negotiate with cent negotiator to run a job */
			negotiate( xdrs );
			timeout();		/* update collector now*/
			break;
		case RESCHEDULE:	/* Reorder job queue and update collector now */
			timeout();
			LastTimeout = time( (time_t *)0 );
			reschedule_negotiator();
			break;
		case KILL_FRGN_JOB:
			abort_job( xdrs );
			break;
		case RECONFIG:
			sighup_handler();
			break;
#ifdef CONDOR_HISTORY
		case GET_HISTORY:
			getHistory(xdrs);
			break;
#endif
		case SEND_ALL_JOBS:
			send_all_jobs( xdrs );
			break;
		case SEND_ALL_JOBS_PRIO:
			send_all_jobs_prioritized( xdrs );
			break;
		case QMGMT_CMD:
			handle_q( xdrs );
			break;
		default:
			EXCEPT( "Got unknown command (%d)\n", cmd );
	}
}

reschedule_negotiator()
{
	int		sock = -1;
	int		cmd;
	XDR		xdr, *xdrs = NULL;

	dprintf( D_ALWAYS, "Called reschedule_negotiator()\n" );

		/* Connect to the negotiator */
	if( (sock=do_connect(NegotiatorHost,"condor_negotiator",NEGOTIATOR_PORT))
																	< 0 ) {
		dprintf( D_ALWAYS, "Can't connect to CONDOR negotiator\n" );
		return;
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	cmd = RESCHEDULE;
	(void)xdr_int( xdrs, &cmd );
	(void)xdrrec_endofrecord( xdrs, TRUE );

	xdr_destroy( xdrs );
	(void)close( sock );
	return;
}

SetSyscalls(){}

init_params()
{
	char	*tmp;

	Log = param( "LOG" );
	if( Log == NULL )  {
		EXCEPT( "No log directory specified in config file\n" );
	}

	CollectorHost = param( "COLLECTOR_HOST" );
	if( CollectorHost == NULL ) {
		EXCEPT( "No Collector host specified in config file\n" );
	}

	NegotiatorHost = param( "NEGOTIATOR_HOST" );
	if( NegotiatorHost == NULL ) {
		EXCEPT( "No NegotiatorHost host specified in config file\n" );
	}

	tmp = param( "SCHEDD_INTERVAL" );
	if( tmp == NULL ) {
		SchedDInterval = 120;
	} else {
		SchedDInterval = atoi( tmp );
		free( tmp );
	}

	if( (tmp=param("SCHEDD_DEBUG")) == NULL ) {
		EXCEPT( "\"SCHEDD_DEBUG\" not specified" );
	}
	free( tmp );
	if( param_in_pattern("SCHEDD_DEBUG","Foreground") ) {
		Foreground = 1;
	}

	Spool = param( "SPOOL" );
	if( Spool == NULL ) {
		EXCEPT( "No Spool directory specified" );
	}

	if( (Shadow=param("SHADOW")) == NULL ) {
		EXCEPT( "SHADOW not specified in config file\n" );
	}

	if( (tmp=param("MAX_JOB_STARTS")) == NULL ) {
		MaxJobStarts = 5;
	} else {
		MaxJobStarts = atoi( tmp );
		free( tmp );
	}

	if( (tmp=param("MAX_JOBS_RUNNING")) == NULL ) {
		MaxJobsRunning = 15;
	} else {
		MaxJobsRunning = atoi( tmp );
		free( tmp );
	}

#ifdef CONDOR_HISTORY
	History = param( "HISTORY" );
	if( History == NULL ) {
		EXCEPT( "History file not specified" );
	}

	tmp = param( "CLIENT_TIMEOUT" );
	if( tmp == NULL ) {
		ClientTimeout = 120;
	} else {
		ClientTimeout = atoi( tmp );
		free( tmp );
	}
#endif

	if( (tmp=param("RESERVED_SWAP")) == NULL ) {
		ReservedSwap = 5 * 1024;			/* 5 megabytes */
	} else {
		ReservedSwap = atoi( tmp ) * 1024;	/* Value specified in megabytes */
		free( tmp );
	}

	if( (tmp=param("SHADOW_SIZE_ESTIMATE")) == NULL ) {
		ShadowSizeEstimate =  128;			/* 128 K bytes */
	} else {
		ShadowSizeEstimate = atoi( tmp );	/* Value specified in kilobytes */
		free( tmp );
	}
		
	if( (CondorAdministrator = param("CONDOR_ADMIN")) == NULL ) {
		EXCEPT( "CONDOR_ADMIN not specified in config file" );
	}

	/* parameters for UPDOWN alogorithm */
	tmp = param( "SCHEDD_SCALING_FACTOR" );
	if( tmp == NULL ) ScheddScalingFactor = 60;
	 else ScheddScalingFactor = atoi( tmp );
	if ( ScheddScalingFactor == 0 )
	      ScheddScalingFactor = 60; 
	
	tmp = param( "SCHEDD_HEAVY_USER_TIME" );
	if( tmp == NULL )  ScheddHeavyUserTime = 120; 
	 else ScheddHeavyUserTime = atoi( tmp );
	
	Step                    =  SchedDInterval/ScheddScalingFactor;
	ScheddHeavyUserPriority = Step * ScheddHeavyUserTime;

	upDown_SetParameters(Step,ScheddHeavyUserPriority); /* UPDOWN */

	/* update upDown objct from disk file */
	/* ignore errors		      */
	sprintf(filename,"%s/%s",Spool, "UserPrio");
	upDown_ReadFromFile(filename);			      /* UPDOWN */

	if( (Mail=param("MAIL")) == NULL ) {
		EXCEPT( "MAIL not specified in config file\n" );
	}
}


/*
** Allow child processes to die a decent death, don't keep them
** hanging around as <defunct>.
**
** NOTE: This signal handler calls routines which will attempt to lock
** the job queue.  Be very careful it is not called when the lock is
** already held, or deadlock will occur!
*/
void
reaper( sig, code, scp )
int		sig, code;
struct sigcontext	*scp;
{
	pid_t	pid;
	int		status;

	if( sig == 0 ) {
		dprintf( D_ALWAYS, "***********  Begin Extra Checking ********\n" );
	} else {
		dprintf( D_ALWAYS, "Entered reaper( %d, %d, 0x%x )\n", sig, code, scp );
	}

	for(;;) {
		if( (pid = waitpid(-1,&status,WNOHANG)) <= 0 ) {
			dprintf( D_FULLDEBUG, "wait3() returned %d, errno = %d\n",
															pid, errno );
			break;
		}
		if( WIFEXITED(status) ) {
			dprintf( D_FULLDEBUG, "Shadow pid %d exited with status %d\n",
											pid, WEXITSTATUS(status) );
			if( WEXITSTATUS(status) == JOB_NO_MEM ) {
				swap_space_exhausted();
			}
		} else if( WIFSIGNALED(status) ) {
			dprintf( D_FULLDEBUG, "Shadow pid %d died with signal %d\n",
											pid, WTERMSIG(status) );
		}
		delete_shadow_rec( pid );
	}
	if( sig == 0 ) {
		dprintf( D_ALWAYS, "***********  End Extra Checking ********\n" );
	}
}

/*
** The shadow running this job has died.  If things went right, the job
** has been marked as idle, unexpanded, or completed as appropriate.
** However, if the shadow terminated abnormally, the job might still
** be marked as running (a zombie).  Here we check for that conditon,
** and mark the job with the appropriate status.
*/
check_zombie( pid, job_id )
int			pid;
PROC_ID		*job_id;
{
	char	queue[MAXPATHLEN];

#if DBM_QUEUE
	PROC	proc;
#endif
	int		status;

	dprintf( D_ALWAYS, "Entered check_zombie( %d, 0x%x )\n", pid, job_id );

#if DBM_QUEUE
	(void)sprintf( queue, "%s/job_queue", Spool );
	if( (Q=OpenJobQueue(queue,O_RDWR,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue );
	}

	LOCK_JOB_QUEUE( Q, WRITER );

	proc.id = *job_id;
	if( FetchProc(Q,&proc) < 0 ) { 	
		proc.status = REMOVED;
	}
	status = proc.status;
#else
	if (GetAttributeInt(job_id->cluster, job_id->proc, "Status", &status) < 0){
		status = REMOVED;
	}
#endif

	switch( status ) {
		case RUNNING:
			kill_zombie( pid, job_id );
			break;
		case REMOVED:
			cleanup_ckpt_files( pid, job_id );
			break;
		default:
			break;
	}

#if DBM_QUEUE
	CLOSE_JOB_QUEUE( Q );
#endif
	dprintf( D_ALWAYS, "Exited check_zombie( %d, 0x%x )\n", pid, job_id );
}

kill_zombie( pid, job_id )
int		pid;
PROC_ID	*job_id;
{
	char	ckpt_name[MAXPATHLEN];

	dprintf( D_ALWAYS,
		"Shadow %d died, and left job %d.%d marked RUNNING\n",
		pid, job_id->cluster, job_id->proc );

#if DBM_QUEUE
	mark_job_stopped( job_id, Q );
#else
	mark_job_stopped( job_id, 0 );
#endif
}

cleanup_ckpt_files( pid, job_id )
int		pid;
PROC_ID	*job_id;
{
	char	ckpt_name[MAXPATHLEN];

		/* Remove any checkpoint file */
#if defined(V2)
	(void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
								Spool, job_id->cluster, job_id->proc  );
#else
	strcpy(ckpt_name, gen_ckpt_name(Spool,job_id->cluster,job_id->proc,0) );
#endif
	(void)unlink( ckpt_name );

		/* Remove any temporary checkpoint file */
	(void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d.tmp",
								Spool, job_id->cluster, job_id->proc  );
	(void)unlink( ckpt_name );
}

void
sigint_handler()
{
	dprintf( D_ALWAYS, "Killed by SIGINT\n" );
	exit( 0 );
}

void
sighup_handler()
{
	dprintf( D_ALWAYS, "Re reading config file\n" );

	free_context( MachineContext );
	MachineContext = create_context();
	config( MyName, MachineContext );

	init_params();
	timeout();
}

#ifdef CONDOR_HISTORY
/*
** The machine we are talking to has stopped responding, close our
** end.  Then the read()/write() will fail, and whichever client routine
** is involved will clean up.
*/
void
alarm_handler()
{
	dprintf( D_ALWAYS,
		"No response in %d seconds, breaking connection\n", ClientTimeout);
	
	if( ClientSkt >= 0 ) {
		(void)close( ClientSkt );
		dprintf( D_ALWAYS, "Closed %d at %d\n", ClientSkt, __LINE__ );
	}
	ClientSkt = -1;
}
#endif

/*
** On a machine where condor is newly installed, there may be no job queue.
** Here we make sure one is initialized and has condor as owner and group.
*/
create_job_queue()
{
	int		oumask;
	char	name[MAXPATHLEN];
	int		fd;

	/* set_condor_euid(); */
	oumask = umask( 0 );

	(void)sprintf( name, "%s/job_queue.dir", Spool );
	if( (fd=open(name,O_RDWR|O_CREAT,0660)) < 0 ) {
		EXCEPT( "open(%s,O_RDWR,0660)", name );
	}
	(void)close( fd );

	(void)sprintf( name, "%s/job_queue.pag", Spool );
	if( (fd=open(name,O_RDWR|O_CREAT,0660)) < 0 ) {
		EXCEPT( "open(%s,O_RDWR,0660)", name );
	}
	(void)close( fd );

	(void)sprintf( name, "%s/history", Spool );
	if( (fd=open(name,O_RDWR|O_CREAT,0660)) < 0 ) {
		EXCEPT( "open(%s,O_RDWR,0660)", name );
	}
	(void)close( fd );

	if (getuid()==0) {
		 set_root_euid(); 
	 }

	(void)umask( oumask );
}


/*
** There should be no jobs running when we start up.  If any were killed
** when the last schedd died, they will still be listed as "running" in
** the job queue.  Here we go in and mark them as idle.
*/
mark_jobs_idle()
{
	char	queue[MAXPATHLEN];
	int		mark_idle();

	/* set_condor_euid(); */

#if DBM_QUEUE
	(void)sprintf( queue, "%s/job_queue", Spool );
	if( (Q=OpenJobQueue(queue,O_RDWR,0)) == NULL ) {
		EXCEPT( "OpenJobQueue(%s)", queue );
	}

	LOCK_JOB_QUEUE( Q, WRITER );


	ScanJobQueue( Q, mark_idle );
 

	CLOSE_JOB_QUEUE( Q );
#else
	WalkJobQueue( mark_idle );
#endif

	if (getuid()==0) { 
		set_root_euid(); 
	}
}

#if DBM_QUEUE
mark_idle( proc )
PROC	*proc;
{
	char	ckpt_name[MAXPATHLEN];

	if( proc->status != RUNNING ) {
		return;
	}

#if defined(V2)
	(void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
								Spool, proc->id.cluster, proc->id.proc  );
#else
	strcpy(ckpt_name, gen_ckpt_name(Spool,proc->id.cluster,proc->id.proc,0) );
#endif

	if( access(ckpt_name,F_OK) != 0 ) {
		proc->status = UNEXPANDED;
	} else {
		proc->status = IDLE;
	}

	if( StoreProc(Q,proc) < 0 ) {
		EXCEPT( "StoreProc(0x%x,0x%x)", Q, proc );
	}
}

#else
mark_idle( cluster, proc )
int cluster;
int proc;
{
	char	ckpt_name[MAXPATHLEN];
	int		status;
	char	owner[_POSIX_PATH_MAX];

	SetAttributeInt(cluster, proc, "CurrentHosts", 0);
	GetAttributeInt(cluster, proc, "Status", &status);

	if( status != RUNNING ) {
		return;
	}

#if defined(V2)
	(void)sprintf( ckpt_name, "%s/job%06d.ckpt.%d",
								Spool, proc->id.cluster, proc->id.proc  );
#else
	strcpy(ckpt_name, gen_ckpt_name(Spool, cluster, proc,0) );
#endif

	GetAttributeString(cluster, proc, "Owner", owner);

	if (FileExists(ckpt_name, owner)) {
		status = IDLE;
	} else {
		status = UNEXPANDED;
	}

	SetAttributeInt(cluster, proc, "Status", status);
}

#endif
