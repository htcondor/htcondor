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
** Author:   Dhrubajyoti Borthakur
** 	         University of Wisconsin, Computer Sciences Dept.
** Edited further by : Dhaval N Shah
** 	         University of Wisconsin, Computer Sciences Dept.
** Modified by: Cai, Weiru
** 			 University of Wisconsin, Computer Sciences Dept.
** Major clean-up, re-write by Derek Wright (7/10/97)
** 			 University of Wisconsin, Computer Sciences Dept.
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "_condor_fix_resource.h"

#include <std.h>
#include <pwd.h>

extern "C" {
#include <netdb.h>
}

#if defined(Solaris)
#define __EXTENSIONS__
#endif

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <rpc/types.h>

#include "condor_constants.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "master.h"
#include "file_lock.h"
#include "condor_timer_manager.h"
#include "condor_collector.h"
#include "files.h"
#include "cctp_msg.h"
#include "condor_attributes.h"
#include "condor_network.h"
#include "condor_adtypes.h"
#include "dgram_io_handle.h"
#include "condor_io.h"

#if defined(HPUX9)
#include "fake_flock.h"
#endif

#if defined(AIX32)
#   define vfork fork
#endif

#include <sys/wait.h>

#define MAX_LINES 100

typedef void (*SIGNAL_HANDLER)();
void install_sig_handler( int, SIGNAL_HANDLER );

typedef struct {
	long	data[MAX_LINES + 1];
	int		first;
	int		last;
	int		size;
	int		n_elem;
} QUEUE;


extern char	*SigNames[];

// prototypes of library functions
extern "C"
{
#if	(defined(SUNOS41) || defined(Solaris))
	int		sigsetmask(int mask);
	int		sigpause(int mask);
#if !defined(Solaris251)
	pid_t		vfork();
#endif
#elif defined(IRIX53)
	int	vfork();
		/* The following got clobbered when _POSIX_SOURCE was defined
		   before stdio.h was included */
	FILE *popen (const char *command, const char *type);
	int pclose(FILE *stream);
#endif

		/* 	char*	getwd(char* pathname); no longer needed because using getcwdnow */
#if defined(IRIX62)
	int	killpg(long pgrp, int sig);
#else
	int	killpg(int pgrp, int sig);
#endif  /* IRIX62 */
	int 	dprintf_config( char*, int);
	int 	detach();
	int	boolean(char*, char*);
	char*	strdup(const char*);
	int	SetSyscalls( int );

#if defined(LINUX) || defined(HPUX9)
	int	gethostname(char*, unsigned int);
#else
	int	gethostname(char*, int);
#endif

#if defined(HPUX9)
	long sigsetmask(long);
#else
	int	sigsetmask(int);
#endif

	int	udp_connect(char*, int);
	char*	get_arch();
	char*	get_op_sys(); 
	int	udp_unconnect();
	void	fill_dgram_io_handle(DGRAM_IO_HANDLE*, char*, int, int); 
	int	get_inet_address(struct in_addr*); 
}

extern	int		Parse(const char*, ExprTree*&);
char	*param(char*) ;
void	sigchld_handler(), sigquit_handler(),  sighup_handler();
void	RestartMaster();
void	siggeneric_handler(int); 
void 	sigterm_handler(), wait_all_children();

// local function prototypes
void	init_params();
int 	collector_runs_here();
int 	negotiator_runs_here();
void 	obituary( int pid, int status );
void 	tail_log( FILE* output, char* file, int lines );
void 	display_line( long loc, FILE* input, FILE* output );
void 	init_queue( QUEUE* queue, int size );
void 	insert_queue( QUEUE        *queue, long    elem);
long	delete_queue(QUEUE* );
int		empty_queue(QUEUE* );
void	get_lock(char * );
void	do_killpg(int, int);
void	do_kill(int, int);
time_t 	GetTimeStamp(char* file);
int 	NewExecutable(char* file, time_t* tsp);
int		daily_housekeeping(void);
void 	dump_core();
void	usage(const char* );
#if 0
int		hourly_housekeeping(void);
#endif
void	report_to_collector();
int		GetConfig(char*, char*);
int		IsSameHost(const char*);
void	StartConfigServer();

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
int	DoCleanup();

#define MINUTE	60
#define HOUR	60 * MINUTE

char	*MyName;
char	*MailerPgm;
int		MasterLockFD;
TimerManager	tMgr;			// The timer manager	Weiru
int		interval;				// report to collector. Weiru
ClassAd	ad;						// ad to central mgr. Weiru

char	*CollectorHost;
int					UdpSock;

int		ceiling = 3600;
float	e_factor = 2.0;								// exponential factor
int		r_factor = 300;								// recover factor
int		Foreground = 0;
int		Termlog;
char*	config_location;						// config file from server
int		doConfigFromServer = FALSE; 
char	*CondorAdministrator;
char	*FS_Preen;
char	*Log;
char	*FS_CheckJobQueue;

int		NotFlag;
int		PublishObituaries;
int		Lines;

extern int ConfigLineNo;

char	*default_daemon_list[] = {
	"MASTER",
	"STARTD",
	"SCHEDD",
	"KBDD",
	0};

// create an object of class daemons.
class Daemons daemons;
char*			configServer;
extern BUCKET	*ConfigTab[];


void
usage( const char* name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n]\n", name );
	exit( 1 );
}

int
DoCleanup()
{
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
	daemons.SignalAll(SIGTERM);
	return 1;
}

int
main( int argc, char* argv[] )
{
	char			**ptr, *startem;
	
	MyName = argv[0];

	// run as condor.condor at all times unless root privilege is needed
	set_condor_priv();

	for( ptr=argv+1; *ptr; ptr++ ) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
		  case 'f':
			Foreground++;
			break;
		  case 't':
			Termlog++;
			break;
		  case 'n':
			NotFlag++;
			break;
		  default:
			usage( argv[0] );
		}
	}

		// Put this before we fork so that if something goes wrong, we
		// see it.
	config_master(&ad);

	if( !Foreground ) {
		if( fork() ) {
			exit( 0 );
		}
	}
	
	// I moved these here because I might start config server earlier than any
	// other daemon and if the config server dies it will kill the master if
	// a sigchld handler is not installed.
	install_sig_handler( SIGILL, dump_core );
	install_sig_handler( SIGCHLD, sigchld_handler );
	install_sig_handler( SIGQUIT, sigquit_handler );
	install_sig_handler( SIGHUP, sighup_handler );
	install_sig_handler( SIGUSR1, RestartMaster );
	install_sig_handler( SIGTERM, sigterm_handler );
	install_sig_handler( SIGSEGV, (void(*)())siggeneric_handler );
	install_sig_handler( SIGBUS, (void(*)())siggeneric_handler );

	init_params();

	if( argc > 4 ) {
		usage( argv[0] );
	}
	_EXCEPT_Cleanup = DoCleanup;

	/* This is so if we dump core it'll go in the log directory */
	if( chdir(Log) < 0 ) {
		EXCEPT( "chdir to log directory <%s>", Log );
	}

	dprintf_config( "MASTER", 2 );

	startem = param("START_DAEMONS");
	if( !startem || *startem == 'f' || *startem == 'F' ) {
		dprintf( D_ALWAYS, "START_DAEMONS flag was set to %s.  Exiting.\n",
				startem?startem:"(NULL)");
		exit( 0 );
	}

	MailerPgm = param( "MAIL" );
	if ( MailerPgm == NULL ) {
		MailerPgm = BIN_MAIL;
	}

	if( !Termlog ) {
#if defined(HPUX9)   || defined(Solaris)
		setsid();
#else
		detach();
#endif
	}

	/* Make sure we are the only copy of condor_master running */
	char*	log_file = daemons.DaemonLog(getpid());
	get_lock( log_file);  

	dprintf( D_ALWAYS,"*************************************************\n" );
	dprintf( D_ALWAYS,"***          CONDOR_MASTER STARTING UP        ***\n" );
	dprintf( D_ALWAYS,"***               PID = %-6d                ***\n",
			getpid() );
	dprintf( D_ALWAYS,"*************************************************\n" );

	// once a day at 3:30 a.m.
	tMgr.NewTimer(NULL, 3600, (void*)daily_housekeeping, 3600 * 24);

	/* hourly_housekeeping unnecessary with new qmgmt.  -Jim B. */
#if 0
	// 6:00 am and 3:30 pm so somebody is around to respond
	tMgr.NewTimer( NULL, 7200, (void*)hourly_housekeeping, 3600 * 24 );
	tMgr.NewTimer( NULL, 32400, (void*)hourly_housekeeping, 3600 * 24 );
#endif

	tMgr.NewTimer( &daemons, 100, (void*)(daemons.CheckForNewExecutable), 300);
	tMgr.NewTimer(NULL, 100, (void*)report_to_collector, interval);

	daemons.StartAllDaemons();

	tMgr.Start();
	
	/* Can never get here */
	return 0;
}


void
sigchld_handler()
{
	int		pid = 0;
	int	status;

	dprintf(D_FULLDEBUG, "SIG_CLD caught\n");

	errno = 0;
	while( (pid=waitpid(-1,&status,WNOHANG)) != 0 ) {
		if( pid == -1 ) {
			// several UNIX's occasionally return -1 with ECHILD
			// instead of returning 0....
			if(errno == ECHILD)
			{
				break;
			}
			EXCEPT( "waitpid(), error # = %d", errno );
		}
		if( WIFSTOPPED(status) ) {
			continue;
		}
		if( !daemons.IsDaemon(pid) ) {
			dprintf( D_ALWAYS, "Pid %d died with ", pid );
			if( WIFEXITED(status) ) {
				dprintf( D_ALWAYS | D_NOHEADER,
						"status %d\n", WEXITSTATUS(status) );
				continue;
			}
			if( WIFSIGNALED(status) ) {
				dprintf( D_ALWAYS | D_NOHEADER,
						"signal %d\n", WTERMSIG(status) );
			}
			continue;
		}
		if( PublishObituaries ) {
			obituary( pid, status );
		}
		daemons.Restart( pid );
		dprintf( D_ALWAYS | D_NOHEADER, "\n" );
		errno = 0;
	}
}

int
SetSyscalls( int val )
{
	return val;
}


void
init_params()
{
	char	*tmp;
	char	*daemon_list;
	char	*daemon_name;
	int		i;
	daemon	*new_daemon;
	ExprTree*	tree;

	Log = param( "LOG" );
	if( Log == NULL )  {
		EXCEPT( "No log directory specified in config file" );
	}

	if( (CollectorHost = param("COLLECTOR_HOST")) == NULL ) {
		EXCEPT( "COLLECTOR_HOST not specified in config file" );
	}
	
	UdpSock = udp_unconnect();
	
	if( (CondorAdministrator = param("CONDOR_ADMIN")) == NULL ) {
		EXCEPT( "CONDOR_ADMIN not specified in config file" );
	}

	tmp = param("PUBLISH_OBITUARIES");
	if( tmp && (*tmp == 't' || *tmp == 'T') ) {
		PublishObituaries = TRUE;
	} else {
		PublishObituaries = FALSE;
	}
	if( tmp ) {
		free( tmp );
	}

	tmp = param("OBITUARY_LOG_LENGTH");
	if( tmp ) {
		Lines = atoi( tmp );
		free( tmp );
	} else {
		Lines = 20;
	}

	tmp = param( "MASTER_BACKOFF_CEILING" );
	if( tmp ) {
		ceiling = atoi( tmp );
		free( tmp );
	} else {
		ceiling = 3600;
	}

	tmp = param( "MASTER_BACKOFF_FACTOR" );
    if( tmp ) {
        e_factor = atof( tmp );
		free( tmp );
    } else {
    	e_factor = 2.0;
    }
	
	tmp = param( "MASTER_RECOVER_FACTOR" );
    if( tmp ) {
        r_factor = atoi( tmp );
		free( tmp );
    } else {
    	r_factor = 300;
    }
	
	tmp = param( "MASTER_UPDATE_CTMR_INTERVAL" );
    if( tmp ) {
        interval = atoi( tmp );
		free( tmp );
    } else {
        interval = 300;
    }

	tmp = new char[100];
	sprintf(tmp, "Machine = \"");
	if(gethostname(tmp+11, 80) < 0)
	{
		EXCEPT("gethostname(), errno = %d", errno);
	}
	strcat(tmp, "\"");
	Parse(tmp, tree);
	ad.Insert(tree);
	delete tmp;
	ad.SetMyTypeName(MASTER_ADTYPE);
	ad.SetTargetTypeName("");

	if( param("MASTER_DEBUG") ) {
		if( boolean("MASTER_DEBUG","Foreground") ) {
			Foreground = 1;
		}
	}

	FS_Preen = param( "PREEN" );
	FS_CheckJobQueue = param( "CHECKJOBQUEUE" );

	daemon_list = param("DAEMON_LIST");
	if (daemon_list) {
		do {
			while(*daemon_list && (*daemon_list == ' ' || *daemon_list== '\t'))
				daemon_list++;
			if (*daemon_list) {
				daemon_name = daemon_list;
				while (*daemon_list != ',' && *daemon_list) 
					daemon_list++;
				if (*daemon_list) {
					*daemon_list = '\0';
					if(daemons.GetIndex(daemon_name) < 0)
					{
						new_daemon = new daemon(daemon_name);
					}
					daemon_list++;
				} else {
					new_daemon = new daemon(daemon_name);
				}
			}
		} while (*daemon_list);
	} else {
		for(i = 0; default_daemon_list[i]; i++) {
			new_daemon = new daemon(default_daemon_list[i]);
		}
	}
	daemons.InitParams();
}

void
obituary( int pid, int status )
{
    char    cmd[512];
    char    hostname[512];
    FILE    *mailer;
    char    *name, *log;
    char    *mail_prog;


	/* If daemon with a serious bug gets installed, we may end up
	 ** doing many restarts in rapid succession.  In that case, we
	 ** don't want to send repeated mail to the CONDOR administrator.
	 ** This could overwhelm the administrator's machine.
	 */
    int restart = daemons.NumberRestarts(pid);
    if ( restart == -1 ) {
        EXCEPT( "Pid %d returned by wait3(), but not a child\n", pid );
    }
    if ( restart > 3 ) return;

    // always return for KBDD
    if ( strcmp( daemons.SymbolicName(pid), "KBDD") == 0 ) {
        return;
	}

	// Just return if process was killed with SIGKILL.  This means the
	// admin did it, and thus no need to send email informing the
	// admin about something they did...
	if ( (WIFSIGNALED(status)) && (WTERMSIG(status) == SIGKILL) ) {
		return;
	}

    name = daemons.DaemonName( pid );
    log = daemons.DaemonLog( pid );

    dprintf( D_ALWAYS, "Sending obituary for \"%s\" to \"%s\"\n",
			name, CondorAdministrator );

    if( gethostname(hostname,sizeof(hostname)) < 0 ) {
        EXCEPT( "gethostname(0x%x,%d)", hostname, sizeof(hostname) );
    }

    mail_prog = param("MAIL");
    if (mail_prog) {
        (void)sprintf( cmd, "%s %s -s \"%s\"", mail_prog,
					   CondorAdministrator, 
					   "CONDOR Problem" );
    } else {
        EXCEPT("\"MAIL\" not specified in the config file! ");
    }

    if( (mailer=popen(cmd,"w")) == NULL ) {
        EXCEPT( "popen(\"%s\",\"w\")", cmd );
    }

    fprintf( mailer, "\n" );

    if( WIFSIGNALED(status) ) {
        fprintf( mailer, "\"%s\" on \"%s\" died due to signal %d\n",
				name, hostname, WTERMSIG(status) );
        /*
		   fprintf( mailer, "(%s core was produced)\n",
		   status->w_coredump ? "a" : "no" );
		   */
    } else {
        fprintf( mailer,
				"\"%s\" on \"%s\" exited with status %d\n",
				name, hostname, WEXITSTATUS(status) );
    }
    tail_log( mailer, log, Lines );

	/* Don't do a pclose here, it wait()'s, and may steal an
	 ** exit notification of one of our daemons.  Instead we'll clean
	 ** up popen's child in our SIGCHLD handler.
	 */
#if defined(HPUX9)
    /* on HPUX, however, do a pclose().  This is because pclose() on HPUX
	 ** will _not_ steal an exit notification, and just doing an fclose
	 ** can cause problems the next time we try HPUX's popen(). -Todd */
    pclose(mailer);
#else
    (void)fclose( mailer );
#endif
}

void
tail_log( FILE* output, char* file, int lines )
{
	FILE	*input;
	int		ch, last_ch;
	long	loc;
	QUEUE	queue, *q = &queue;

	if( (input=fopen(file,"r")) == NULL ) {
		fprintf( stderr, "Can't open %s\n", file );
		return;
	}

	init_queue( q, lines );
	last_ch = '\n';

	while( (ch=getc(input)) != EOF ) {
		if( last_ch == '\n' && ch != '\n' ) {
			insert_queue( q, ftell(input) - 1 );
		}
		last_ch = ch;
	}


	while( !empty_queue( q ) ) {
		loc = delete_queue( q );
		display_line( loc, input, output );
	}
	(void)fclose( input );
}

void
display_line( long loc, FILE* input, FILE* output )
{
	int		ch;

	(void)fseek( input, loc, 0 );

	for(;;) {
		ch = getc(input);
		(void)putc( ch, output );
		if( ch == EOF || ch == '\n' ) {
			return;
		}
	}
}

void
init_queue( QUEUE* queue, int size )
{
	queue->first = 0;
	queue->last = 0;
	queue->size = size;
	queue->n_elem = 0;
}

void
insert_queue( QUEUE	*queue, long	elem)
{
	if( queue->n_elem == queue->size ) {
		queue->first = (queue->first + 1) % (queue->size + 1);
	} else {
		queue->n_elem += 1;
	}
	queue->data[queue->last] = elem;
	queue->last = (queue->last + 1) % (queue->size + 1);
}

long
delete_queue( QUEUE	*queue)
{
	long	answer;

	queue->n_elem -= 1;
	answer = queue->data[ queue->first ];
	queue->first = (queue->first + 1) % (queue->size + 1);
	return answer;
}

int
empty_queue( QUEUE	*queue)
{
	return queue->first == queue->last;
}


FileLock *MasterLock;

void
get_lock( char* file_name )
{

	if( (MasterLockFD=open(file_name,O_RDWR,0)) < 0 ) {
		EXCEPT( "open(%s,0,0)", file_name );
	}

	// This must be a global so that it doesn't go out of scope
	// cause the destructor releases the lock.
	MasterLock = new FileLock( MasterLockFD );
	MasterLock->set_blocking( FALSE );
	if( !MasterLock->obtain(WRITE_LOCK) ) {
		EXCEPT( "Can't get lock on \"%s\"", file_name );
	}
}


void
do_killpg( int pgrp, int sig )
{
	priv_state	priv;

	if( !pgrp ) {
		return;
	}

	priv = set_root_priv();

	(void)kill(-pgrp, sig );

	set_priv(priv);
}

void
do_kill( int pid, int sig )
{
	int 		status;
	priv_state	priv;

	if( !pid ) {
		return;
	}

	priv = set_root_priv();

	status = kill( pid, sig );

	set_priv(priv);

	if( status < 0 ) {
		EXCEPT( "kill(%d,%d)", pid, sig );
	}
	dprintf( D_ALWAYS, "Sent %s to process %d\n", SigNames[sig], pid );
}

/*
 ** Re read the config file, and send all the daemons a signal telling
 ** them to do so also.
 */
void
sighup_handler()
{
	dprintf( D_ALWAYS, "Re-reading master config file\n" );
	config_master( &ad );
	init_params();
	dprintf_config( "MASTER", 2 );
	dprintf( D_ALWAYS, "Sending all daemons a SIGHUP\n" );
	daemons.SignalAll(SIGHUP);
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
}

/*
 ** Kill all daemons and go away.
 */
void
sigquit_handler()
{
	dprintf( D_ALWAYS, "Killed by SIGQUIT.  Performing quick shut down.\n" );
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
	dprintf( D_ALWAYS, "Sending all daemons a SIGQUIT\n" );
	daemons.SignalAll(SIGQUIT);
	wait_all_children();
	dprintf( D_ALWAYS, "All daemons have exited.\n" );
	exit( 0 );
}


/*
 ** Cause job(s) to vacate, kill all daemons and go away.
 */
void
sigterm_handler()
{
	int pid;
	dprintf( D_ALWAYS, "Killed by SIGTERM.  Performing graceful shut down.\n" );
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
	dprintf( D_ALWAYS, "Sending all daemons a SIGTERM\n" );
	daemons.SignalAll(SIGTERM);
	wait_all_children();
	dprintf( D_ALWAYS, "All daemons have exited.\n" );
	exit( 0 );
}

void
wait_all_children()
{
	int pid;
	dprintf( D_FULLDEBUG, "Begining to wait for all children\n" );
	for(;;) {
		pid = wait( 0 );
		dprintf( D_FULLDEBUG, "Wait() returns pid %d\n", pid );
		if( pid < 0 ) {
			if( errno == ECHILD ) {
				break;
			} else {
				EXCEPT( "wait( 0 ), errno = %d", errno );
			}
		}
	}
	dprintf( D_FULLDEBUG, "Done waiting for all children\n" );
}

time_t
GetTimeStamp(char* file)
{
	struct stat sbuf;

	if( stat(file, &sbuf) < 0 ) {
		return( (time_t) -1 );
	}

	return( sbuf.st_mtime );
}

int
NewExecutable(char* file, time_t* tsp)
{
	time_t cts = GetTimeStamp(file);
	dprintf(D_FULLDEBUG, "Time stamp of running %s is %d\n",
			file, (int)*tsp);
	dprintf(D_FULLDEBUG, "GetTime stamp returned %d\n",(int)cts);

	if( cts == (time_t) -1 ) {
		/*
		 **	We could have been in the process of installing a new
		 **	version, and that's why the 'stat' failed.  Catch it
		 **  next time around.
		 */
		return( FALSE );
	}

	if( cts != *tsp ) {
		*tsp = cts;
		return( TRUE );
	}

	return FALSE;
}

char	*Shell = "/bin/sh";

int
daily_housekeeping(void)
{
	int		child_pid;

	dprintf(D_FULLDEBUG, "enter daily_housekeeping\n");
	if( FS_Preen == NULL ) {
		return 0;
	}

	/* Check log, spool, and execute for any junk files left lying around */
	dprintf( D_ALWAYS,
			"Calling execl( \"%s\", \"sh\", \"-c\", \"%s\", 0 )\n", Shell, FS_Preen );

	if( (child_pid=vfork()) == 0 ) {	/* The child */
		execl( Shell, "sh", "-c", FS_Preen, 0 );
		_exit( 127 );
	} else {				/* The parent */
		dprintf( D_ALWAYS, "Shell (preen) pid is %d\n", child_pid );
		return 0;
	}

	/*
	   Note: can't use system() here.  That calls wait(), but the child's
	   status will be captured our own sigchld_handler().  This would
	   cause the wait() called by system() to hang forever...
	   -- mike

	   (void)system( FS_Preen );
	   */
	dprintf(D_FULLDEBUG, "exit daily_housekeeping\n");
}

#if 0
int
hourly_housekeeping(void)
{
	int		child_pid;

	dprintf(D_FULLDEBUG, "enter hourly_housekeeping\n");

	if( FS_CheckJobQueue == NULL ) {
		dprintf(D_ALWAYS, "CheckJobQueue == NULL \n");
		return 0;
	}

	dprintf( D_ALWAYS,
			"Calling execl( \"%s\", \"sh\", \"-c\", \"%s\", 0 )\n",
			Shell,
			FS_CheckJobQueue
			);

	if( (child_pid=vfork()) == 0 ) {	/* The child */
		execl( Shell, "sh", "-c", FS_CheckJobQueue, 0 );
		_exit( 127 );
	} else {				/* The parent */
		dprintf( D_ALWAYS, "Shell (CheckJobQueue) pid is %d\n", child_pid );
		return 0;
	}
	dprintf(D_FULLDEBUG, "exit hourly_housekeeping\n");
}
#endif

void
dump_core()
{
	int		my_pid;
	char		cwd[ _POSIX_PATH_MAX ];

	dprintf( D_ALWAYS, "Entered dump_core()\n" );

	install_sig_handler( SIGILL, (SIGNAL_HANDLER)SIG_DFL );

	my_pid = getpid();
	sigsetmask( 0 );

	dprintf( D_ALWAYS, "ruid = %d, euid = %d\n", getuid(), geteuid() );
	dprintf( D_ALWAYS, "Directory = \"%s\"\n", getcwd(cwd,sizeof(cwd)) );
	dprintf( D_ALWAYS, "My PID = %d\n", my_pid );

	set_root_priv();

	if( kill(my_pid,SIGILL) < 0 ) {
		EXCEPT( "kill(getpid(),SIGILL)" );
	}

	/* should never get here */
	EXCEPT( "Attempt to dump core failed" );
}

void
RestartMaster()
{
	daemons.RestartMaster();
}


typedef void (*GNU_SIGNAL_HANDLER)(...);
typedef void (*POSIX_SIGNAL_HANDLER)();
typedef void (*ANSI_SIGNAL_HANDLER)(int);

void
install_sig_handler( int sig, SIGNAL_HANDLER handler )
{
	struct sigaction act;

#if defined(__GNUG__)
	act.sa_handler = (GNU_SIGNAL_HANDLER)handler;
#elif defined(OSF1)
	act.sa_handler = (ANSI_SIGNAL_HANDLER)handler;
#else
	act.sa_handler = (POSIX_SIGNAL_HANDLER)handler;
#endif

	sigemptyset( &act.sa_mask );
	act.sa_flags = 0;

	if( sigaction(sig,&act,NULL) < 0 ) {
		EXCEPT( "Can't install handler for signal %d\n", sig );
	}
}

void report_to_collector()
{
	int		cmd = UPDATE_MASTER_AD;
	SafeSock	sock(CollectorHost, COLLECTOR_UDP_COMM_PORT);

	dprintf(D_FULLDEBUG, "enter report_to_collector\n");
	sock.attach_to_file_desc(UdpSock);

	sock.encode();
	if(!sock.code(cmd))
	{
		dprintf(D_ALWAYS, "Can't send UPDATE_MASTER_AD to the collector\n");
		return;
	}
	if(!ad.put(sock))
	{
		dprintf(D_ALWAYS, "Can't send ClassAd to the collector\n");
		return;
	}
	if(!sock.end_of_message())
	{
		dprintf(D_ALWAYS, "Can't send endofrecord to the collector\n");
	}
	dprintf(D_FULLDEBUG, "exit report_to_collector\n");
}

#if defined(USE_CONFIG_SERVER)
////////////////////////////////////////////////////////////////////////////////
// Get the configuration file from the config server. If for any reason the
// operation wasn't successful, set the value of the attribute LastUpdate in
// the master ad to negative infinity (0). Otherwise take it from the classad
// from configuration server.
////////////////////////////////////////////////////////////////////////////////
const	char	DELIMITOR	=	'#';
const	char*	DELIMITOR_STRING = "#";

int GetConfig(char* config_server_name, char* condor_dir)
{
	char	hostname[MAXPATHLEN];
	char	cond[100]; 
	char*	arch;
	char*	opSys;
	char	*listPtr, *ptr, *listPtr1, *ptr1;
	char	reqAd[1000];
	char	tmp[1000];
	int		reqNum, reqFor;
	CCTP_Status	status;
	char	id[30];
	char	reason[100];
	FILE*	conf_fp;
	char	file_name[1024];
	char	configList[4096];
	time_t	timestamp;
	int		len, len1;
	char	macronames[200];
	char	macroname[100];
	char*	port;
	int		portNum;
	int		doFreeArch = TRUE, doFreeOpSys = TRUE;
	char	outputName[MAXPATHLEN];
	int		timer;
	
	// prepare request
	if(gethostname(hostname, MAXHOSTNAMELEN) < 0)
	{
		EXCEPT("Can't find host name\n");
	}
	arch = param("ARCH");
	if(!arch)
	{
		arch = get_arch();
		doFreeArch = FALSE; 
	} 
	opSys = param("OPSYS");
	if(!opSys)
	{
		opSys = get_op_sys();
		doFreeOpSys = FALSE; 
	}
	
	sprintf(reqAd, "MyType = \"machine\"%sTargetType = \"config\"%sHost = \"%s\"%sRequirement = True", DELIMITOR_STRING, DELIMITOR_STRING, hostname, DELIMITOR_STRING);
	if(arch)
	{
		strcat(reqAd, DELIMITOR_STRING);
		strcat(reqAd, "Arch = \"");
		strcat(reqAd, arch);
		strcat(reqAd, "\"");
		if(doFreeArch)
		{
			free(arch);
		} 
	}
	if(opSys)
	{
		strcat(reqAd, DELIMITOR_STRING);
		strcat(reqAd, "OpSys = \"");
		strcat(reqAd, opSys);
		strcat(reqAd, "\"");
		if(doFreeOpSys)
		{
			free(opSys);
		}
	} 

	sprintf(tmp, "%s = 0", ATTR_LAST_UPDATE);

	// connect to config server
	port = param("CONFIG_SERVER_PORT");
	if(!port)
	{
		portNum = DEFAULT_CONFIG_SERVER_PORT;
	}
	else
	{
		portNum = atoi(port);
		free(port);
	}
	
	if(!config_server_connect(config_server_name, portNum))
	{
		ad.Insert(tmp);
		return -1;
	}
	timer = tMgr.NewTimer(NULL, 600, (void*)config_server_disconnect, 0);
	reqNum = send_request(CONDOR_MASTER, ACTION_GET, reqAd, DELIMITOR, "", 0);
	if(!reqNum)
	{
		ad.Insert(tmp);
		return -1;
	}
	reqFor = get_response(id, &status, configList, DELIMITOR, reason, &timestamp);
	tMgr.CancelTimer(timer);
	
	if(!reqFor)
	{
		ad.Insert(tmp);
		return -1;
	}
	if(reqNum != reqFor)
	{
		config_server_disconnect();
		ad.Insert(tmp);
		return -1;
	}
	if(status != STATUS_SUCCESS)
	{
		config_server_disconnect();
		ad.Insert(tmp);
		return -1;
	}
	config_server_disconnect();

	sprintf(tmp, "%s = %d", ATTR_LAST_UPDATE, timestamp);
	ad.Insert(tmp);
	// formatting the response
    listPtr = configList;
    do {
        ptr = listPtr;
        len = 0;
        while(*ptr != ' ') {
            ptr++;
			len++;
        }
        strncpy(cond, listPtr, len);
        cond[len] = '\0';
        if(strcmp(cond, "COPY_TO_CONTEXT") == 0) {
            len = 0;
            while((*ptr == ' ') || (*ptr == '=')) { ptr++; }
			listPtr = ptr;
			while((*ptr != DELIMITOR) && (*ptr != '\0')) {
				ptr++; len++;
			}
			strncpy(macronames, listPtr, len);
			macronames[len] = '\0';
			break;
        }
        while((*ptr != DELIMITOR) && (*ptr != '\0')) { ptr++; }
        listPtr = ptr;
        if(*listPtr == DELIMITOR) { listPtr++; }
    }while(*listPtr != '\0');

    listPtr1 = macronames;
    ptr1 = listPtr1;
    while(*listPtr1 != '\0') {
        len1 = 0;
        while(*ptr1 != ' ') { ptr1++; len1++; }
        strncpy(macroname, listPtr1, len1);
        macroname[len1] = '\0';
        listPtr = configList;
        do {
			ptr = listPtr;
			len = 0;
			while(*ptr != ' '){
				ptr++;
				len++;
			}
			strncpy(cond, listPtr, len);
			cond[len] = '\0';
			if(strcmp(cond, macroname) == 0) {
				while(*ptr == ' ') { ptr++; }
				if(*ptr == '=') {
					*ptr = ':';
					break;
				}
			}
			while((*ptr != DELIMITOR) && (*ptr != '\0')) { ptr++; }
			if(*ptr == DELIMITOR) { ptr++; }
			listPtr = ptr;
        }while(*listPtr != '\0');

        while(*ptr1 == ' ') { ptr1++; }
        listPtr1 = ptr1;
    }


    /* write configs to local file */
	sprintf(outputName, "%s/%s", condor_dir, SERVER_CONFIG); 
    conf_fp = fopen(outputName, "w");
    if( conf_fp == NULL) {
		fprintf(stderr, "Cannot open %s, errno = %d\n", SERVER_CONFIG, errno);
		ad.Insert(tmp);
		return -1;
    }

    listPtr = configList;
    while((*listPtr != '\0') || (*listPtr != DELIMITOR)) {
        if(*listPtr == DELIMITOR) {
            *listPtr = '\n';
        }
        else if(*listPtr == '\0') {
            break;
        }
        else listPtr++;
    }

    fprintf(conf_fp, "%s", configList);
    fclose(conf_fp);
}
#endif

int IsSameHost(const char* host)
{
	struct in_addr		local;
	struct hostent*		hostp;
	
	if(get_inet_address(&local) < 0)
	{
		EXCEPT("Can't find address");
	}
	
	hostp = gethostbyname(host);
	if(!hostp)
	{
		EXCEPT("Can't find host %s", host);
	}
	if(hostp->h_addrtype != AF_INET)
	{
		EXCEPT("host (%s) address isn't AF_INET type", host);
	}
	
	if(memcmp((char*)&local, hostp->h_addr_list[0], sizeof(local)) != 0)
	{
		return FALSE;
	}
	return TRUE; 
}

void StartConfigServer()
{
	daemon*			newDaemon;
	
	newDaemon = new daemon("CONFIG_SERVER");
	newDaemon->process_name = param(newDaemon->name_in_config_file);
	if(newDaemon->process_name == NULL && newDaemon->flag)
	{
		dprintf(D_ALWAYS, "Process not found in config file: %s\n",
				newDaemon->name_in_config_file);
		EXCEPT("Can't continue...");
	}
	newDaemon->config_info_file = param("CONFIG_SERVER_FILE");
	newDaemon->port = param("CONFIG_SERVER_PORT");

	// check that log file is necessary
	if(newDaemon->log_filename_in_config_file != NULL)
	{
		newDaemon->log_name = param(newDaemon->log_filename_in_config_file);
		if(newDaemon->log_name == NULL && newDaemon->flag)
		{
			dprintf(D_ALWAYS, "Log file not found in config file: %s\n",
					newDaemon->log_filename_in_config_file);
		}
	}

	newDaemon->StartDaemon();
	newDaemon->timeStamp = GetTimeStamp(newDaemon->process_name);
	
	// sleep for a while because we want the config server to stable down
	// before doing anything else
	sleep(5); 
}

void siggeneric_handler(int sig)
{
	EXCEPT("signal (%d) received", sig);
}
