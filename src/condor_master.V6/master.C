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

#include "condor_common.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "my_hostname.h"
#include "master.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_collector.h"
#include "cctp_msg.h"
#include "condor_attributes.h"
#include "condor_network.h"
#include "condor_adtypes.h"
// #include "dgram_io_handle.h"
#include "condor_io.h"

#ifndef WANT_DC_PM
int sigchld_handler(Service *,int);
#endif

#define MAX_LINES 100

typedef void (*SIGNAL_HANDLER)();
extern "C" void install_sig_handler( int, SIGNAL_HANDLER );

typedef struct {
	long	data[MAX_LINES + 1];
	int		first;
	int		last;
	int		size;
	int		n_elem;
} QUEUE;

#ifdef WIN32
extern void register_service();
extern void terminate(DWORD);
#endif

extern "C" char	*SigNames[];

// prototypes of library functions
extern "C"
{
	int 	detach();
	char*	get_arch();
	char*	get_op_sys(); 
	int	get_inet_address(struct in_addr*); 
}

// void	sigchld_handler(), sigquit_handler(),  sighup_handler();

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
int		daily_housekeeping(Service*);
void	usage(const char* );
void	report_to_collector();
int		GetConfig(char*, char*);
void	StartConfigServer();
int		main_shutdown_graceful();
int		main_shutdown_fast();
int		main_config();
int		master_reaper(Service *, int, int);
int		admin_command_handler(Service *, int, Stream *);

extern "C" int	DoCleanup();

char	*MailerPgm;
int		MasterLockFD;
int		interval;				// report to collector. Weiru
ClassAd	ad;						// ad to central mgr. Weiru

char	*CollectorHost;

int		ceiling = 3600;
float	e_factor = 2.0;								// exponential factor
int		r_factor = 300;								// recover factor
char*	config_location;						// config file from server
int		doConfigFromServer = FALSE; 
char	*CondorAdministrator;
char	*FS_Preen;
char	*Log;
char	*FS_CheckJobQueue;
int		NT_ServiceFlag = FALSE;		// TRUE if running on NT as an NT Service

int		NotFlag;
int		PublishObituaries;
int		Lines;
int		AllowAdminCommands = FALSE;

char	*default_daemon_list[] = {
	"MASTER",
	"STARTD",
	"SCHEDD",
#if defined(OSF1) || defined(IRIX53)    // Only need KBDD on alpha and sgi
	"KBDD",
#endif
	0};

// create an object of class daemons.
class Daemons daemons;
char*			configServer;

// for daemonCore
char *mySubSystem = "MASTER";


int
master_exit(int retval)
{
#ifdef WIN32
	if ( NT_ServiceFlag == TRUE )
		terminate(retval);
#endif

	exit(retval);
	return 1;	// just to satisfy vc++
}

void
usage( const char* name )
{
	dprintf( D_ALWAYS, "Usage: %s [-f] [-t] [-n]\n", name );
	master_exit( 1 );
}

int
DoCleanup()
{
	static int already_excepted = FALSE;

	// This function gets called as the last thing by EXCEPT().
	// At this point, the MASTER presumably has had an unrecoverable error
	// and is about to die.  We'll attempt to gracefully shutdown our
	// kiddies before we fade away.  Use a static flag to prevent
	// infinite recursion in the case that there is another EXCEPT.

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
		main_shutdown_graceful();  // this will exit if successful
	}

	if ( NT_ServiceFlag == FALSE ) {
		return 1;	// this will return to the EXCEPT code, which will just exit
	} else {
		master_exit(1); // this does not return
		return 1;		// just to satisfy VC++
	}
}

int
main_init( int argc, char* argv[] )
{
	char			**ptr, *startem;
	int i;

#ifdef WIN32
	// Daemon Core will call main_init with argc -1 if need to register
	// as a WinNT Service.
	if ( argc == -1 ) {
		register_service();
		return TRUE;
	}
#endif

	if ( argc > 2 ) {
		usage( argv[0] );
	}
	
	i = 0;
	for( ptr=argv+1; *ptr && ( i < argc - 2 ); ptr++, i++ ) {
		if( ptr[0][0] != '-' ) {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
		  case 'n':
			NotFlag++;
			break;
		  default:
			usage( argv[0] );
		}
	}

		// Put this before we fork so that if something goes wrong, we
		// see it....  Unfortunately, now the daemon core will have already
		// forked.  TODO: fix this... Todd, 11/97.
	//config_master(&ad);
	config_master(NULL);

	
	// I moved these here because I might start config server earlier than any
	// other daemon and if the config server dies it will kill the master if
	// a sigchld handler is not installed.
#ifndef WIN32
	// install_sig_handler( SIGILL, dump_core );
	install_sig_handler( SIGSEGV, (void(*)())siggeneric_handler );
	install_sig_handler( SIGBUS, (void(*)())siggeneric_handler );
#endif

#ifdef WANT_DC_PM
	daemonCore->Register_Reaper("daemon reaper",(ReaperHandler)master_reaper,
		"master_reaper()");
#else
	daemonCore->Cancel_Signal(DC_SIGCHLD);
	daemonCore->Register_Signal(DC_SIGCHLD,"DC_SIGCHLD",
		(SignalHandler)sigchld_handler,"sigchld_handler");
#endif
	
	init_params();

		// Register admin commands
	daemonCore->Register_Command( RECONFIG, "RECONFIG",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler" );
	daemonCore->Register_Command( RESTART, "RESTART",
								  (CommandHandler)admin_command_handler, 
								  "admin_command_handler" );

	_EXCEPT_Cleanup = DoCleanup;

	startem = param("START_DAEMONS");
	if( !startem || *startem == 'f' || *startem == 'F' ) {
		dprintf( D_ALWAYS, "START_DAEMONS flag was set to %s.  Exiting.\n",
				startem?startem:"(NULL)");
		master_exit( 0 );
	}

	MailerPgm = param( "MAIL" );
	if ( MailerPgm == NULL ) {
		MailerPgm = BIN_MAIL;
	}

	if( !Termlog ) {
#if defined(HPUX9)   || defined(Solaris)
		setsid();
#elif !defined(WIN32)
		detach();
#endif
	}

	/* Make sure we are the only copy of condor_master running */
#ifndef WIN32
	char*	log_file = daemons.DaemonLog( daemonCore->getpid() );
	get_lock( log_file);  
#endif

	// once a day  (starts up PREEN)
	daemonCore->Register_Timer(3600,24 * 3600,(TimerHandler)daily_housekeeping,
		"daily_housekeeping()");
	
	daemonCore->Register_Timer(5,300,(TimerHandlercpp)Daemons::CheckForNewExecutable,
		"Daemons::CheckForNewExecutable()",&daemons);
	
	daemonCore->Register_Timer(0,interval,(TimerHandler)report_to_collector,
		"report_to_collector()");
	
	daemons.StartAllDaemons();

	return TRUE;
}


int
admin_command_handler(Service *, int cmd, Stream *)
{
	if(! AllowAdminCommands ) {
		dprintf( D_FULLDEBUG, 
				 "Got admin command (%d) while not allowed. Ignoring.\n",
				 cmd );
		return FALSE;
	}
	dprintf( D_FULLDEBUG, 
			 "Got admin command (%d) and allowing it.\n", cmd );
	switch( cmd ) {
	case RECONFIG:
		return main_config();
	case RESTART:
		RestartMaster();
		return TRUE;
	default: 
		EXCEPT( "Unknown admin command in handle_admin_commands" );
	}
	return FALSE;
}


int
master_reaper(Service *, int pid, int status)
{
	if( !daemons.IsDaemon(pid) ) {
		dprintf( D_ALWAYS, "Pid %d died with ", pid );
		if( WIFEXITED(status) ) {
			dprintf( D_ALWAYS | D_NOHEADER,
					"status %d\n", WEXITSTATUS(status) );
			return FALSE;
		}
		if( WIFSIGNALED(status) ) {
			dprintf( D_ALWAYS | D_NOHEADER,
					"signal %d\n", WTERMSIG(status) );
		}
		return FALSE;
	}
	if( PublishObituaries ) {
		obituary( pid, status );
	}
	daemons.Restart( pid );
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
	return TRUE;
}

#if !defined(WANT_DC_PM)
int
sigchld_handler(Service *,int)
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
		master_reaper(NULL,pid,status);
		errno = 0;
	}
	return TRUE;
}
#endif  // of !defined(WANT_DC_PM)


void
init_params()
{
	char	*tmp;
	char	*daemon_list;
	char	*daemon_name;
	int		i;
	daemon	*new_daemon;

	Log = param( "LOG" );
	if( Log == NULL )  {
		EXCEPT( "No log directory specified in config file" );
	}

	if( (CollectorHost = param("COLLECTOR_HOST")) == NULL ) {
		EXCEPT( "COLLECTOR_HOST not specified in config file" );
	}
	
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

	AllowAdminCommands = FALSE;
	tmp = param( "ALLOW_ADMIN_COMMANDS" );
	if( tmp ) {
		if( *tmp == 'T' || *tmp == 't' ) {
			AllowAdminCommands = TRUE;
		}
		free( tmp );	
	}

	char line[100];
	sprintf(line, "%s = \"%s\"", ATTR_MACHINE, my_hostname() );
	ad.Insert(line);

	sprintf(line, "%s = \"%s\"", ATTR_NAME, my_hostname() );
	ad.Insert(line);

	sprintf(line, "%s = \"%s\"", ATTR_MASTER_IP_ADDR,
			daemonCore->InfoCommandSinfulString() );
	ad.Insert(line);

	ad.SetMyTypeName(MASTER_ADTYPE);
	ad.SetTargetTypeName("");

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
#if !defined(WIN32)		// until we add email support to WIN32 port...
    char    cmd[512];
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

	// Just return if process exited with status 0.  If everthing's
	// ok, why bother sending email?
	if ( (WIFEXITED(status)) && (WEXITSTATUS(status) == 0 ) ) {
		return;
	}

    name = daemons.DaemonName( pid );
    log = daemons.DaemonLog( pid );

    dprintf( D_ALWAYS, "Sending obituary for \"%s\" to \"%s\"\n",
			name, CondorAdministrator );

    mail_prog = param("MAIL");
    if (mail_prog) {
        (void)sprintf( cmd, "%s -s \"%s\" %s", mail_prog,
					   "CONDOR Problem",
					   CondorAdministrator );
    } else {
        EXCEPT("\"MAIL\" not specified in the config file! ");
    }

    if( (mailer=popen(cmd,"w")) == NULL ) {
        EXCEPT( "popen(\"%s\",\"w\")", cmd );
    }

    fprintf( mailer, "\n" );

    if( WIFSIGNALED(status) ) {
        fprintf( mailer, "\"%s\" on \"%s\" died due to signal %d\n",
				name, my_hostname(), WTERMSIG(status) );
        /*
		   fprintf( mailer, "(%s core was produced)\n",
		   status->w_coredump ? "a" : "no" );
		   */
    } else {
        fprintf( mailer,
				"\"%s\" on \"%s\" exited with status %d\n",
				name, my_hostname(), WEXITSTATUS(status) );
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

#endif	// of !defined(WIN32)
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
#ifndef WIN32
	priv_state	priv;

	if( !pgrp ) {
		return;
	}

	priv = set_root_priv();

	(void)kill(-pgrp, sig );

	set_priv(priv);
#endif	// ifndef WIN32
}

void
do_kill( int pid, int sig )
{
	int 		status;

	if( !pid ) {
		return;
	}

#ifdef WANT_DC_PM
	status = daemonCore->Send_Signal(pid,sig);
	if ( status == FALSE )
		status = -1;
	else
		status = 1;
#else
	priv_state priv = set_root_priv();
	status = kill( pid, sig );
	set_priv(priv);
#endif

	if( status < 0 ) {
		// EXCEPT( "kill(%d,%d)", pid, sig );
		// An EXCEPT seemed to harsh here... we want our Master to _stick around_ !!!
		// Instead, log an error message.
		dprintf(D_ALWAYS,"ERROR: failed to send %s to pid %d\n",SigNames[sig],pid);
	} else {
		dprintf( D_ALWAYS, "Sent %s to process %d\n", SigNames[sig], pid );
	}
}

/*
 ** Re read the config file, and send all the daemons a signal telling
 ** them to do so also.
 */
int
main_config()
{
	config_master( &ad );
	init_params();
	dprintf( D_ALWAYS, "Sending all daemons a SIGHUP\n" );
#ifdef WANT_DC_PM
	daemons.SignalAll(DC_SIGHUP);
#else
	daemons.SignalAll(SIGHUP);
#endif
	dprintf( D_ALWAYS | D_NOHEADER, "\n" );
	return TRUE;
}

/*
 ** Kill all daemons and go away.
 */
int
main_shutdown_fast()
{
	dprintf( D_ALWAYS, "Killed by SIGQUIT.  Performing quick shut down.\n" );
	dprintf( D_ALWAYS, "Sending all daemons a SIGQUIT\n" );

#ifdef WANT_DC_PM
	daemons.SignalAll(DC_SIGQUIT);
	daemons.Wait_And_Exit( 0 );		// this will return now, but will exit after last child
	return TRUE;
#else
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
	daemons.SignalAll(SIGQUIT);
	wait_all_children();
	dprintf( D_ALWAYS, "All daemons have exited.\n" );
	master_exit( 0 );
	return TRUE;
#endif
}


/*
 ** Cause job(s) to vacate, kill all daemons and go away.
 */
int
main_shutdown_graceful()
{
	dprintf( D_ALWAYS, "Killed by SIGTERM.  Performing graceful shut down.\n" );
	dprintf( D_ALWAYS, "Sending all daemons a SIGTERM\n" );

#ifdef WANT_DC_PM
	daemons.SignalAll(DC_SIGTERM);
	daemons.Wait_And_Exit( 0 );
	return TRUE;
#else
	install_sig_handler( SIGCHLD, (SIGNAL_HANDLER)SIG_IGN );
	daemons.SignalAll(SIGTERM);
	wait_all_children();
	dprintf( D_ALWAYS, "All daemons have exited.\n" );
	master_exit( 0 );
	return TRUE;
#endif
}

void
wait_all_children()
{
#ifndef WANT_DC_PM
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
#endif  // of ifndef WANT_DC_PM
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
daily_housekeeping(Service*)
{
	int		child_pid;

	dprintf(D_FULLDEBUG, "enter daily_housekeeping\n");
	if( FS_Preen == NULL ) {
		return 0;
	}

	/* Exec Preen to check log, spool, and execute for any junk files left lying around */
#ifdef WANT_DC_PM
	child_pid = daemonCore->Create_Process(
					FS_Preen,		// program to exec
					FS_Preen,			// args
					PRIV_CONDOR,	// privledge level
					1,				// which reaper ID to use; use default reaper
					FALSE );		// we do _not_ want this process to have a command port; PREEN is not a daemon core process
	dprintf( D_ALWAYS, "Preen pid is %d\n", child_pid );
#else
	dprintf( D_ALWAYS,
			"Calling execl( \"%s\", \"sh\", \"-c\", \"%s\", 0 )\n", Shell, FS_Preen );

	if( (child_pid=vfork()) == 0 ) {	/* The child */
		execl( Shell, "sh", "-c", FS_Preen, 0 );
		_exit( 127 );
	} else {				/* The parent */
		dprintf( D_ALWAYS, "Shell (preen) pid is %d\n", child_pid );
		return 0;
	}
#endif
	/*
	   Note: can't use system() here.  That calls wait(), but the child's
	   status will be captured our own sigchld_handler().  This would
	   cause the wait() called by system() to hang forever...
	   -- mike

	   (void)system( FS_Preen );
	   */
	dprintf(D_FULLDEBUG, "exit daily_housekeeping\n");
	return TRUE;
}

void
RestartMaster()
{
	daemons.RestartMaster();
}


void report_to_collector()
{
	int		cmd = UPDATE_MASTER_AD;
	SafeSock sock(CollectorHost, COLLECTOR_UDP_COMM_PORT,2);

	dprintf(D_FULLDEBUG, "enter report_to_collector\n");

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
	
	// sleep for a while because we want the config server to stable down
	// before doing anything else
	sleep(5); 
}


void siggeneric_handler(int sig)
{
	EXCEPT("signal (%d) received", sig);
}
