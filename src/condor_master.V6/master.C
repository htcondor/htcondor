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
*/ 

#include <std.h>

#define _POSIX_SOURCE

#include <signal.h>

#include <errno.h>
#include <pwd.h>

extern "C" {
#include <netdb.h>
}
#if defined(Solaris)
#define __EXTENSIONS__
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <rpc/types.h>
#include "condor_fix_stdio.h"    
#include <sys/resource.h>
#include "condor_constants.h"
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_expressions.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "condor_mach_status.h"
#include "master.h"
#include "file_lock.h"
#include "condor_timer_manager.h"
#include "condor_classad.h"
#include "condor_collector.h"


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
	int 	gethostname(char* name, int len);
	#if !defined(Solaris251)
	int		vfork();
	#endif
#elif defined(IRIX53)
	int	vfork();
	int	gethostname(char *name, int len);
	/* The following got clobbered when _POSIX_SOURCE was defined
	   before stdio.h was included */
	FILE *popen (const char *command, const char *type);
	int pclose(FILE *stream);
#endif

/* 	char*	getwd(char* pathname); no longer needed because using getcwdnow */
	int		killpg(int pgrp, int sig);
	int 	dprintf_config( char*, int);
	int 	detach();
	int		boolean(char*, char*);
	char*	strdup(const char*);
	void	set_machine_status(int);
	int		SetSyscalls( int );
	int		gethostname(char*, int);
	int		sigsetmask(int);
	int		udp_connect(char*, int);
	int		snd_int(XDR*, int, int);
}

extern	int		Parse(const char*, ExprTree*&);
extern	int		xdr_classad(XDR*, ClassAd*);
char	*param(char*) ;
void		sigchld_handler(),  sigint_handler(),
		sigquit_handler(),  sighup_handler();
void	SigalrmHandler(), RestartMaster();
void 	vacate_machine(), vacate_machine_part_two();
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
int		hourly_housekeeping(void);
void	report_to_collector();

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
#if 0
char	*NegotiatorHost;
#endif
int		ceiling;
float	e_factor;								// exponential factor
int		r_factor;								// recover factor
int		Foreground;
int		Termlog;
char	*CondorAdministrator;
char	*FS_Preen;
char	*Log;
char	*FS_CheckJobQueue;

int		NotFlag;
int		PublishObituaries;
int		Lines;

char	*default_daemon_list[] = {
	"MASTER",
	"COLLECTOR",
	"NEGOTIATOR",
	"STARTD",
	"SCHEDD",
	"KBDD",
	"CKPT_SERVER",
	0};


// create an object of class daemons.
class Daemons daemons;
extern "C" 
{
	void	add_to_path(char* name);
}

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
	daemons.SignalAll(SIGKILL);
	return 1;
}


int
main( int argc, char* argv[] )
{
	struct passwd		*pwd;
	char				**ptr, *startem;

	MyName = argv[0];

#if 0
	if( getuid() != 0 ) {
		dprintf( D_ALWAYS, "%s must be run as ROOT\n", MyName );
		exit( 1 );
	}
#endif

#if defined(X86) && defined(Solaris)
/* Set the environment for the X11 dynamic library for condor_kbdd */
putenv("LD_LIBRARY_PATH=/s/X11R6-2/sunx86_54/lib");
#else if defined(sun4m) && defined(Solaris)
/* Set the environment for the X11 dynamic library for condor_kbdd */
putenv("LD_LIBRARY_PATH=/s/X11R6-2/sun4m_54/lib");
#endif

		/* Run as group condor so we can access log files even if they
		   are remotely mounted with NFS - needed because
		   root = nobody on the remote file system */
	if( (pwd=getpwnam("condor")) == NULL ) {
		EXCEPT( "condor not in passwd file" );
	}
	if( setgid(pwd->pw_gid) < 0 ) {
		EXCEPT( "setgid(%d)", pwd->pw_gid );
	}

	/*
	** A NFS bug fix prohibits root from writing to an NFS partition
	** even if he has group write permission.  We need to be Condor
	** most of the time.
	*/
	set_condor_euid();


	config( MyName, (CONTEXT *)0 );

	init_params();
	add_to_path( "/etc" );
	add_to_path( "/usr/etc" );

	if( argc > 4 ) {
		usage( argv[0] );
	}
	_EXCEPT_Cleanup = DoCleanup;

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

	if( !Foreground ) {
		if( fork() ) {
			exit( 0 );
		}
	}


	dprintf( D_ALWAYS,"*************************************************\n" );
	dprintf( D_ALWAYS,"***          CONDOR_MASTER STARTING UP        ***\n" );
	dprintf( D_ALWAYS,"***               PID = %-6d                ***\n",
																	getpid() );
	dprintf( D_ALWAYS,"*************************************************\n" );

	install_sig_handler( SIGILL, dump_core );
	install_sig_handler( SIGCHLD, sigchld_handler );
	install_sig_handler( SIGINT, sigint_handler );
	install_sig_handler( SIGQUIT, sigquit_handler );
//	install_sig_handler( SIGTERM, sigquit_handler );
	install_sig_handler( SIGHUP, sighup_handler );
	install_sig_handler( SIGUSR1, RestartMaster );
	install_sig_handler( SIGTERM, vacate_machine );


		// once a day at 3:30 a.m.
	tMgr.NewTimer(NULL, 3600, (Event)daily_housekeeping, 3600 * 24);

		// 6:00 am and 3:30 pm so somebody is around to respond
	tMgr.NewTimer( NULL, 7200, (Event)hourly_housekeeping, 3600 * 24 );
	tMgr.NewTimer( NULL, 32400, (Event)hourly_housekeeping, 3600 * 24 );
	tMgr.NewTimer( &daemons, 100, (Event)(daemons.CheckForNewExecutable), 300);
	tMgr.NewTimer(NULL, 100, (Event)report_to_collector, interval);

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
		if( WTERMSIG(status) != SIGKILL && PublishObituaries ) {
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
		EXCEPT( "No log directory specified in config file\n" );
	}


	if( (CollectorHost = param("COLLECTOR_HOST")) == NULL ) {
		EXCEPT( "COLLECTOR_HOST not specified in config file" );
	}

#if 0
	if( (NegotiatorHost = param("NEGOTIATOR_HOST")) == NULL ) {
		EXCEPT( "NEGOTIATOR_HOST not specified in config file" );
	}
#endif

	if( (CondorAdministrator = param("CONDOR_ADMIN")) == NULL ) {
		EXCEPT( "CONDOR_ADMIN not specified in config file" );
	}

	tmp = param("PUBLISH_OBITUARIES");
	if( tmp && (*tmp == 't' || *tmp == 'T') ) {
		PublishObituaries = TRUE;
	} else {
		PublishObituaries = FALSE;
	}

	tmp = param("OBITUARY_LOG_LENGTH");
	if( tmp == NULL ) {
		Lines = 20;
	} else {
		Lines = atoi( tmp );
	}

	tmp = param( "MASTER_BACKOFF_CEILING" );
	if( tmp == NULL ) {
		ceiling = 3600;
	} else {
		ceiling = atoi( tmp );
	}

	tmp = param( "MASTER_BACKOFF_FACTOR" );
    if( tmp == NULL ) {
    	e_factor = 2.0;
    } else {
        e_factor = atof( tmp );
    }
	
	tmp = param( "MASTER_RECOVER_FACTOR" );
    if( tmp == NULL ) {
    	r_factor = 300;
    } else {
        r_factor = atoi( tmp );
    }
	
	tmp = param( "MASTER_UPDATE_CTMR_INTERVAL" );
    if( tmp == NULL ) {
        interval = 300;
    } else {
        interval = atoi( tmp );
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
	//ad.SetMyTypeName("ScheddAd");
	//ad.SetTargetTypeName("");

	if( param("MASTER_DEBUG") ) {
		if( boolean("MASTER_DEBUG","Foreground") ) {
			Foreground++;
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
					new_daemon = new daemon(daemon_name);
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


#if 0
collector_runs_here()
{
	char	hostname[512];
	char	*my_host_name;
	char	*mgr_host_name;
	struct hostent	*hp;
	
		/* Get the "official" name of our own host */
	if( gethostname(hostname,sizeof(hostname)) < 0 ) {
		EXCEPT( "gethostname(0x%x,%d)", hostname, sizeof(hostname) );
	}
	if( (hp=gethostbyname(hostname)) == NULL ) {
		EXCEPT( "gethostbyname(%s)", hostname );
	}
	my_host_name = strdup( hp->h_name );

		/* Get the "official" name of the collector host */
	if( (hp=gethostbyname(CollectorHost)) == NULL ) {
		EXCEPT( "gethostbyname(%s)", CollectorHost );
	}
	mgr_host_name = strdup( hp->h_name );

	return strcmp(my_host_name,mgr_host_name) == MATCH;
}

negotiator_runs_here()
{
	char	hostname[512];
	char	*my_host_name;
	char	*mgr_host_name;
	struct hostent	*hp;
	
		/* Get the "official" name of our own host */
	if( gethostname(hostname,sizeof(hostname)) < 0 ) {
		EXCEPT( "gethostname(0x%x,%d)", hostname, sizeof(hostname) );
	}
	if( (hp=gethostbyname(hostname)) == NULL ) {
		EXCEPT( "gethostbyname(%s)", hostname );
	}
	my_host_name = strdup( hp->h_name );

		/* Get the "official" name of the negotiator host */
	if( (hp=gethostbyname(NegotiatorHost)) == NULL ) {
		EXCEPT( "gethostbyname(%s)", NegotiatorHost );
	}
	mgr_host_name = strdup( hp->h_name );

	return strcmp(my_host_name,mgr_host_name) == MATCH;
}
#endif

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
    if ( strcmp( daemons.SymbolicName(pid), "KBDD") == 0 )
        return;

    name = daemons.DaemonName( pid );
    log = daemons.DaemonLog( pid );

    dprintf( D_ALWAYS, "Sending obituary for \"%s\" to \"%s\"\n",
                                                name, CondorAdministrator );

    if( gethostname(hostname,sizeof(hostname)) < 0 ) {
        EXCEPT( "gethostname(0x%x,%d)", hostname, sizeof(hostname) );
    }

    mail_prog = param("MAIL");
    if (mail_prog) {
        (void)sprintf( cmd, "%s %s", mail_prog, CondorAdministrator );
    } else {
        EXCEPT("\"MAIL\" not specified in the config file! ");
    }

    if( (mailer=popen(cmd,"w")) == NULL ) {
        EXCEPT( "popen(\"%s\",\"w\")", cmd );
    }

    fprintf( mailer, "To: %s\n", CondorAdministrator );
    fprintf( mailer, "Subject: CONDOR Problem\n" );
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
	int		status;

	if( !pgrp ) {
		return;
	}
if (getuid()==0)
	set_root_euid();

	(void)kill(-pgrp, sig );

	set_condor_euid();

}

void
do_kill( int pid, int sig )
{
	int		status;

	if( !pid ) {
		return;
	}

if (getuid()==0)
	set_root_euid();

	status = kill( pid, sig );

	set_condor_euid();

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
	dprintf( D_ALWAYS, "Re reading config file\n" );
	config( MyName, (CONTEXT *)0 );
	init_params();
	daemons.SignalAll(SIGHUP);
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
}

/*
** Kill and restart all daemons.
*/
void
sigint_handler()
{
	dprintf( D_ALWAYS, "Killing all daemons\n" );
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
	daemons.SignalAll(SIGKILL);
	dprintf( D_ALWAYS, "Restarting all daemons\n" );
	sleep( 5 );	/* NOT a good way to do this... */
	install_sig_handler( SIGCHLD ,sigchld_handler );
	daemons.StartAllDaemons();
}
/*
** Kill all daemons and go away.
*/
void
sigquit_handler()
{
	dprintf( D_ALWAYS, "Killed by SIGQUIT\n" );
	DoCleanup();
	set_machine_status( CONDOR_DOWN );
	exit( 0 );
}

/*
** Cause job(s) to vacate and kill all daemons.
*/
void
vacate_machine()
{
	dprintf( D_ALWAYS, "Vacating machine\n" );
	install_sig_handler( SIGCHLD, vacate_machine_part_two );
	daemons.Vacate();
}

void
vacate_machine_part_two()
{
	int pid = 0;
	int status;

	while( (pid=waitpid(-1,&status,WNOHANG)) != 0 ) {
		if( pid == -1 ) {
			EXCEPT( "waitpid()" );
		}
		if( WIFSTOPPED(status) ) {
			continue;
		}
		if( daemons.IsDaemon(pid) ) {
			dprintf( D_ALWAYS,
					"Job(s) should now be gone.  Shutting down daemons.\n" );
			install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
			daemons.SignalAll(SIGKILL);
			dprintf( D_ALWAYS, "Goodbye.\n" );
			set_machine_status( CONDOR_DOWN );
			exit( 0 );
		}
	}
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
		dprintf( D_ALWAYS, "Shell pid is %d\n", child_pid );
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
		dprintf( D_ALWAYS, "Shell pid is %d\n", child_pid );
		return 0;
	}
	dprintf(D_FULLDEBUG, "exit hourly_housekeeping\n");
}

void
dump_core()
{
	int		my_pid;
	char		cwd[ _POSIX_PATH_MAX ];

	dprintf( D_ALWAYS, "Entered dump_core()\n" );

	install_sig_handler( SIGILL, (SIGNAL_HANDLER)SIG_DFL );

if (getuid()==0)
	set_root_euid();

	my_pid = getpid();
	sigsetmask( 0 );

	dprintf( D_ALWAYS, "ruid = %d, euid = %d\n", getuid(), geteuid() );
	dprintf( D_ALWAYS, "Directory = \"%s\"\n", getcwd(cwd,sizeof(cwd)) );
	dprintf( D_ALWAYS, "My PID = %d\n", my_pid );


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

	if( sigaction(sig,&act,0) < 0 ) {
		EXCEPT( "Can't install handler for signal %d\n", sig );
	}
}

void report_to_collector()
{
	int		cmd = UPDATE_MASTER_AD;
	XDR		xdr, *xdrs = NULL;
	int		sd;

	dprintf(D_FULLDEBUG, "enter report_to_collector\n");
	if((sd = udp_connect(CollectorHost, COLLECTOR_UDP_PORT)) < 0)
	{
		dprintf(D_ALWAYS, "Failed to connect to the collector\n");
		return;
	}

	xdrs = xdr_Init(&sd, &xdr);
	if(!snd_int(xdrs, cmd, FALSE))
	{
		xdr_destroy(xdrs);
		close(sd);
		dprintf(D_ALWAYS, "Can't send UPDATE_MASTER_AD to the collector\n");
		return;
	}
	xdrs->x_op = XDR_ENCODE;
	if(!xdr_classad(xdrs, &ad))
	{
		xdr_destroy(xdrs);
		close(sd);
		dprintf(D_ALWAYS, "Can't send ClassAd to the collector\n");
		return;
	}
	if(!xdrrec_endofrecord(xdrs, TRUE));
	{
		dprintf(D_ALWAYS, "Can't send endofrecord to the collector\n");
	}
	xdr_destroy(xdrs);
	close(sd);
	dprintf(D_FULLDEBUG, "exit report_to_collector\n");
}
