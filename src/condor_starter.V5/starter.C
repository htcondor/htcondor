/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 


#define _POSIX_SOURCE

#if defined(Solaris) && !defined(Solaris251)
#include "_condor_fix_types.h"
#include </usr/ucbinclude/sys/rusage.h>
#endif

#include "condor_common.h"
#include "condor_expressions.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "condor_uid.h"
#include "condor_config.h"
#include "condor_io.h"
#include <signal.h>
#include <sys/wait.h>
#include "proto.h"
#include "condor_sys.h"
#include "name_tab.h"

#include "state_machine_driver.h"

#include "starter.h"
#include "fileno.h"
#include "ckpt_file.h"
#include "startup.h"
#include "alarm.h"
#include "afs.h"

#pragma implementation "list.h"
#include "list.h"
#include "user_proc.h"

#if !defined(X86)
typedef List<UserProc> listuserproc; 
#endif 

#include <sys/stat.h>
#include <pwd.h>

#if defined(AIX32)
#	include <sys/id.h>
#endif

extern "C" {
#include <sys/utsname.h>
int free_fs_blocks(const char *);
void display_startup_info( const STARTUP_INFO *s, int flags );
}

#if defined(OSF1) 
#pragma define_template List<UserProc>
#pragma define_template Item<UserProc>
#endif

#if defined(LINK_PVM)
#include "pvm_user_proc.h"
#include "pvm3.h"
#include "sdpro.h"
#endif

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

#undef ASSERT
#define ASSERT(cond) \
	if( !(cond) ) { \
		EXCEPT( "Assertion ERROR on (%s)\n", #cond ); \
	} else {\
		dprintf( D_FULLDEBUG, "Assertion Ok on (%s)\n", #cond ); \
	}

	// Global flags
int	QuitNow = FALSE;		// Quit after completing current checkpoint
int	XferCkpts = TRUE;		// Transfer ckpt files before exiting
int	Running_PVMD = FALSE;	// True if we are running a pvm job

	// Constants
int	CkptLimit = 300;	// How long to wait for proc to ckpt and exit
const int	VacateLimit = 60;	// How long to wait for proc to exit
const pid_t	ANY_PID = -1;		// arg to waitpid() for any process

ReliSock	*SyscallStream;		// stream to shadow for remote system calls
List<UserProc>	UProcList;		// List of user processes
//listuserproc	UProcList;		// List of user processes
char	*Execute;				// Name of directory where user procs execute
int		MinCkptInterval;		// Use this ckpt interval to start
int		MaxCkptInterval;		// Don't increase ckpt interval beyond this
int		DoDelays;				// Insert artificial delays for testing
char	*UidDomain;				// Machines we share UID space with

Alarm		MyAlarm;			// Don't block forever on user process exits
CkptTimer	*MyTimer;			// Timer for periodic checkpointing
char	*InitiatingHost;		// Machine where shadow is running
char	*ThisHost;				// Machine where we are running

int		UsePipes = FALSE;		// Connect to shadow via pipes for debugging

extern State StateTab[];
extern Transition TransTab[];
extern int EventSigs[];

extern NameTable ProcStates;
extern NameTable PvmMsgNames;

extern int shadow_tid;

	// Prototypes of local interest only
int update_one( UserProc *proc );
void send_final_status( UserProc *proc );
void resume_all();
void req_exit_all();
void req_ckpt_exit_all();
int needed_fd( int fd );
void determine_user_ids( uid_t &requested_uid, gid_t &requested_gid );
int host_in_domain( const char *domain, const char *hostname );
void init_environment_info();

StateMachine	*condor_starter_ptr;

/*
  Main routine for condor_starter.  DEBUGGING can be turned on if we
  need to attach a debugger at run time.  Everything is driven by the
  finite state machine, so we just intiialize it and run it.
*/
int
main( int argc, char *argv[] )
{
	StateMachine condor_starter( StateTab, TransTab, EventSigs, START, END );

#define DEBUGGING FALSE
#if DEBUGGING
	wait_for_debugger( TRUE );
#else
	wait_for_debugger( FALSE );
#endif

	if (argc == 2 && strcmp(argv[1], "-dot") == MATCH) {
		condor_starter.dot_print(stdout);
		exit(0);
	}

	set_posix_environment();
	initial_bookeeping( argc, argv );

	condor_starter_ptr = &(condor_starter);
//	condor_starter.display();
	condor_starter.execute();
	dprintf( D_ALWAYS, "********* STARTER terminating normally **********\n" );
	return 0;
}

void
initial_bookeeping( int argc, char *argv[] )
{
	struct utsname uts;
	char	my_host[ _POSIX_PATH_MAX ];

		/* These items must be completed before we can print anything */
	if( argc > 1 ) {
		if( argv[1][0] == '-' && argv[1][1] == 'p' ) {
			UsePipes = TRUE;
		} else {
			UsePipes = FALSE;
		}
	}

	(void) SetSyscalls( SYS_LOCAL | SYS_MAPPED );

	set_condor_priv();
	(void)umask( 0 );

	init_shadow_connections();

	read_config_files();
	init_logging();

		/* Now if we have an error, we can print a message */
	if( argc != 2 ) {
// printf("argc is %d",argc);
		usage( argv[0] );	/* usage() exits */
	}

	init_sig_mask();

	if( uname(&uts) < 0 ) {
		EXCEPT( "Can't find own hostname" );
	}
	ThisHost = uts.nodename;

	if( UsePipes ) {
		InitiatingHost = ThisHost;
	} else {
		InitiatingHost = argv[1];
	}


	dprintf( D_ALWAYS, "********** STARTER starting up ***********\n" );
	dprintf( D_ALWAYS, "Submitting machine is \"%s\"\n", InitiatingHost );

	_EXCEPT_Cleanup = exception_cleanup;
}

/*
  Unblock all signals except those which will encode asynchronous
  events.  Those will be unblocked at the proper moment by the finite
  state machine driver.
*/
void
init_sig_mask()
{
	sigset_t	sigmask;
	int			i;

	sigemptyset( &sigmask );
	for( i=0; EventSigs[i]; i++ ) {
		sigaddset( &sigmask, EventSigs[i] );
	}
	(void)sigprocmask( SIG_SETMASK, &sigmask, 0 );
}

/*
  If TESTING is set to TRUE, we delay approximately sec seconds for
  purposes of debugging.  If TESTING is not TRUE, we just return.
*/
void
delay( int sec )
{
	int		i;
	int		j, k;

#if defined(SPARC)
	int		lim = 250000;
#elif defined(Solaris)
	int		lim = 250000;
#elif defined(R6000)
	int		lim = 250000;
#elif defined(MIPS)
	int		lim = 200000;
#elif defined(ALPHA)
	int		lim = 300000;
#elif defined(HPPAR)
	int		lim = 650000;
#elif defined(LINUX)
	int		lim = 300000;
#elif defined(SGI)
	int		lim = 300000;
#endif


	if( !DoDelays || sec == 0 ) {
		return;
	}

	for( i=sec; i > 0; i-- ) {
		dprintf( D_ALWAYS | D_NOHEADER,  "%d\n", i );
		for( j=0; j<lim; j++ ) {
			for( k=0; k<5; k++ );
		}
	}
	dprintf( D_ALWAYS | D_NOHEADER,  "0\n" );
}

/*
  Initialization stuff to be done before getting any use processes.
*/
int
init()
{
	move_to_execute_directory();
	init_environment_info();
	set_resource_limits();
	close_unused_file_descriptors();
	MyTimer = new CkptTimer( MinCkptInterval, MaxCkptInterval );

	return DEFAULT;
}


/*
  Suspend ourself and wait for a SIGCONT signal.
*/
int
susp_self()
{
	dprintf( D_ALWAYS, "Suspending self\n" );
	kill( getpid(), SIGSTOP );
	dprintf( D_ALWAYS, "Resuming self\n" );
	return(0);
}

/*
  We were called with incorrect set of command line arguments.  Print a
  message and exit.
*/
void
usage( char *my_name )
{
	dprintf( D_ALWAYS, "Usage: %s ( -pipe | initiating_host)\n", my_name );
	exit( 0 );
}

/*
  Wait up for one of those nice debuggers which attaches to a running
  process.  These days, most every debugger can do this with a notable
  exception being the ULTRIX version of "dbx".
*/
void wait_for_debugger( int do_wait )
{
	sigset_t	sigset;

	// This is not strictly POSIX conforming becuase it uses SIGTRAP, but
	// since it is only used in those environments where is is defined, it
	// will probably pass...
#if defined(SIGTRAP)
		/* Make sure we don't block the signal used by the
		** debugger to control the debugged process (us).
		*/
	sigemptyset( &sigset );
	sigaddset( &sigset, SIGTRAP );
	sigprocmask( SIG_UNBLOCK, &sigset, 0 );
#endif

	while( do_wait )
		;
}

/*
  We're born with two TCP connections to the shadow.  The connection at
  file descriptor #1 is for remote system calls, and the one at file
  descriptor #2 is for logging.  Initialize these and set them up to be
  accessed by the well known numbers used by all the remote system call
  and logging code.
*/
void
init_shadow_connections()
{
	int		scm;


	if( UsePipes ) {
		SyscallStream = init_syscall_connection( TRUE );
	} else {
		(void) dup2( 1, RSC_SOCK );
		(void) dup2( 2, CLIENT_LOG );
		SyscallStream = init_syscall_connection( FALSE);
	}

}

/*
  Set up file descriptor for log file.  If LOCAL_LOGGING is TRUE, we
  put the log in "/tmp/starter_log".  In this case we just seek to the
  end and continue writing.  This is convenient if we are "tail'ing"
  the log on several runs in a row.

  If LOCAL_LOGGING is FALSE, we will send all our logging information
  to the shadow, where it will show up in the shadow's log file.
*/
void
init_logging()
{
	int	log_fd;
	char	*pval;

	pval = param( "STARTER_LOCAL_LOGGING" );
	if( pval && (pval[0] == 't' || pval[0] == 'T') ) {
		dprintf_config( "STARTER", -1 );	// Log file on local machine
		free( pval );
		return;
	}

		// Set up to do logging through the shadow

	close( fileno(stderr) );
	dup2( CLIENT_LOG, fileno(stderr) );
	setvbuf( stderr, NULL, _IOLBF, 0 ); // line buffering

	pval = param( "STARTER_DEBUG" );
	if( pval ) {
		set_debug_flags( pval );
		free( pval );
	}
	// DebugFlags |= D_NOHEADER;

}

/*
  Get relevant information from "condor_config" and "condor_config.local"
  files.
*/
void
read_config_files( void )
{
	config( 0 );

		/* bring important parameters into global data items */
	init_params();

	dprintf( D_ALWAYS, "Done reading config files\n" );
}

/*
  Change directory to where we will run our user processes.
*/
void
move_to_execute_directory()
{
	if( chdir(Execute) ) {
		EXCEPT( "chdir(%s)", Execute );
	}
	dprintf( D_FULLDEBUG, "Done moving to directory \"%s\"\n", Execute );
}

/*
  Close any unnecessary open file descriptors which may be lying
  around.  This is important becuase our fd's will get duplicated in
  our user processes, and we don't want to fill up the kernel's fd
  table needlessly.
*/
void
close_unused_file_descriptors()
{
	long		open_max;
	long		i;


		/* first find out how many fd's are available on this system */
	errno = 0;
	if( (open_max=sysconf(_SC_OPEN_MAX)) == -1 ) {
		if( errno == 0 ) {
			open_max = RSC_SOCK;
			dprintf( D_ALWAYS, "OPEN_MAX is indeterminite, using %d\n",
																RSC_SOCK );
		} else {
			EXCEPT( "_SC_OPEN_MAX not recognized" );
		}
	}

		/* now close everything except the ones we use */
	for( i=0; i<open_max; i++ ) {
		if( !needed_fd(i) ) {
			(void) close( i );
		}
	}
	dprintf( D_FULLDEBUG, "Done closing file descriptors\n" );
}

/*
  Grab initialization parameters and put them into global variables for
  quick and easy access.
*/
void
init_params()
{
	char	*tmp;

	if( (Execute=param("EXECUTE")) == NULL ) {
		EXCEPT( "Execute directory not specified in config file" );
	}

	if( (tmp=param("MIN_CKPT_INTERVAL")) == NULL ) {
		MinCkptInterval = 30 * MINUTE;
	} else {
		MinCkptInterval = atoi( tmp );
	}

	if( (tmp=param("MAX_CKPT_INTERVAL")) == NULL ) {
		MaxCkptInterval = 2 * HOUR;
	} else {
		MaxCkptInterval = atoi( tmp );
	}

	if( (tmp=param("CKPT_TIMEOUT")) != NULL ) {
		CkptLimit = atoi( tmp );
	}

	if( (tmp=param("STARTER_DELAYS")) == NULL ) {
		DoDelays = FALSE;
	} else {
		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			DoDelays = TRUE;
		} else {
			DoDelays = FALSE;
		}
	}

		// find out domain of machines whose UIDs we honor
	UidDomain = param( "UID_DOMAIN" );

		// if the domain is null, don't honor any UIDs
	if( UidDomain == NULL || UidDomain[0] == '\0' ) {
		UidDomain = "Unknown";
	}

		// if the domain is "*", honor all UIDs - a dangerous idea
	if( UidDomain[0] == '*' ) {
		UidDomain[0] = '\0';
	}
}

/*
  Get one user process from the shadow and append it to our list of
  user processes.  Note - this routine does not transfer any checkpoint
  or executable files.  That is done in a separate routine.
*/
int
get_proc()
{
	int	answer;
	UserProc *new_process;

	dprintf( D_ALWAYS, "Entering get_proc()\n" );

	if( new_process=get_job_info() ) {
		UProcList.Append( new_process );
		return SUCCESS;
	} else {
		return TRY_LATER;
	}
}

/*
  Transfer a checkpoint or executable file from the submitting machine to
  our own file system.
*/
int
get_exec()
{
	UserProc	*new_process;

	dprintf( D_ALWAYS, "Entering get_exec()\n" );


	new_process = UProcList.Current();
	delay( 5 );
	if( new_process->fetch_ckpt() != TRUE ) {
		return FAILURE;
	}
	return SUCCESS;
}


/*
  We've been asked to vacate the machine, but we're allowed to checkpoint
  any running jobs or complete any checkpoints we have in progress first.
*/
int
handle_vacate_req()
{
	// dprintf( D_ALWAYS, "Entering function handle_vacate_req()\n" );

	MyTimer->clear();		// clear ckpt timer
	stop_all();				// stop any running user procs
	QuitNow = TRUE;			// set flag so other routines know we have to leave
	return(0);
}


/*
  We've been asked to leave the machine, and we may not create or
  update any more checkpoint files.  We may however transfer any
  existing checkpoint files back to the submitting machine.
*/
int
req_vacate()
{

	MyAlarm.cancel();	// Cancel supervise test_connection() alarm

		// In V5 ckpt so fast, we can do it here
	req_ckpt_exit_all();

	MyAlarm.set( CkptLimit );

	return(0);
}

/*
  We've been asked to leave the machine, and we may not create or
  update any more checkpoint files.  Also, we may not transfer any
  existing checkpoint files back to the submitting machine.
*/
int
req_die()
{
	MyAlarm.cancel();	// Cancel supervise test_connection() alarm
	req_exit_all();
	MyAlarm.set( VacateLimit );
	XferCkpts = FALSE;

	return(0);
}



/*
  Request every user process to checkpoint, then exit now.
*/
void
req_ckpt_exit_all()
{
	UserProc	*proc;

		// Request all the processes to ckpt and then exit
	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if ( proc->get_class() != PVMD ) {
			dprintf( D_ALWAYS, "req_exit_all: Proc %d in state %s\n", 
					proc->get_id(),	ProcStates.get_name(proc->get_state())
					);
			if( proc->is_running() || proc->is_suspended() ) {
				dprintf( D_ALWAYS, "Requesting Exit on proc #%d\n",
						proc->get_id());
				if( proc->get_class() == VANILLA ) {
					proc->kill_forcibly();
				} else {
					proc->request_ckpt();
				}

			}
		}
	}

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if ( proc->get_class() == PVMD ) {
			if( proc->is_running() || proc->is_suspended() ) {
				proc->request_exit();
			}
		}
	}
}

/*
  Request every user process to exit now.
*/
void
req_exit_all()
{
	UserProc	*proc;

		// Request all the processes to exit
	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if ( proc->get_class() != PVMD ) {
			dprintf( D_ALWAYS, "req_exit_all: Proc %d in state %s\n", 
					proc->get_id(),	ProcStates.get_name(proc->get_state())
					);
			if( proc->is_running() || proc->is_suspended() ) {
				dprintf( D_ALWAYS, "Requesting Exit on proc #%d\n",
						proc->get_id());
				proc->request_exit();
			}
		}
	}

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if ( proc->get_class() == PVMD ) {
			if( proc->is_running() || proc->is_suspended() ) {
				proc->request_exit();
			}
		}
	}
}

/*
  Wait for every user process to exit.
*/
int
terminate_all()
{
	UserProc	*proc;

	// dprintf( D_ALWAYS, "Entering function terminate_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->is_running() || proc->is_suspended() ) {
			return DO_WAIT;
		}

		// if the child has exited in the mean time, we want to send the
		// core back to the shadow -- Raghu
		if( proc->exited_abnormally() ) {
			proc->store_core();
			return DEFAULT;
		}
	}

		// Cancel alarm
	MyAlarm.cancel();

#if 0
	if( XferCkpts ) {
		return DO_XFER;
	} else {
		return DONT_XFER;
	}
#else
	return DEFAULT;
#endif
}

/*
  Send final exit status and cleanup files for every user proc in the
  list.
*/
int
dispose_all()
{
	UserProc	*proc;

	// dprintf( D_ALWAYS, "Entering dispose_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		send_final_status( proc );
		proc->delete_files();
		UProcList.DeleteCurrent();
	}

	return DEFAULT;
}


/*
  Send final image size, exit status and resource usage for a process
  to the shadow.
*/
void
send_final_status( UserProc *proc )
{
	int			image_size;
	void		*status;
	void		*rusage;
	int			id;
	int			cluster_no = proc->get_cluster();
	int			proc_no = proc->get_proc();


	dprintf( D_ALWAYS,
		"Sending final status for process %d.%d\n", cluster_no, proc_no
	);

		// update shadow with it's image size
	if( proc->ckpt_enabled() ) {
		image_size = proc->get_image_size();
		(void)REMOTE_syscall( CONDOR_image_size, image_size );
	}

		// update shadow with it's resource usage and exit status
	status = proc->bsd_exit_status();
	if( proc->get_class() == PVM ) {
		rusage = proc->accumulated_rusage();	// All resource usage
		id = proc->get_id();
		(void)REMOTE_syscall( CONDOR_subproc_status, id, status, rusage);
	} else {
		rusage = proc->guaranteed_rusage();		// Only usage guaranteed by ckpt
		(void)REMOTE_syscall( CONDOR_reallyexit, status, rusage);
	}

	dprintf( D_FULLDEBUG,
		"Done sending final status for process %d.%d\n", cluster_no, proc_no
	);
		
}

/*
  Wait for some user process to exit, and update it's object in the
  list with the exit status information.
*/
int
reaper()
{
	int			st;
	pid_t		pid;
	UserProc	*proc;
	int continue_fsa = -2;

	MyAlarm.cancel();	// Cancel supervise test_connection() alarm

	for (;;) {

	pid = waitpid(ANY_PID,&st,WNOHANG);

	if( pid == -1 ) {
		if ( errno == EINTR ) {
			continue;
		} else {
			break;
		}
	}

	if( pid == 0 ) {
		break;
	}
	
		// Find the corresponding UserProc object
	dprintf( D_FULLDEBUG,
		"Process %d exited, searching process list...\n", pid 
	);

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->get_pid() == pid ) {
			break;
		}
	}

		// If we found the process's object, update it now
	if( proc != NULL ) {
		dprintf( D_FULLDEBUG, "Found object for process %d\n", pid );	
		continue_fsa = 0;
		proc->handle_termination( st );
	}

	}	/* end of infinite for loop */

	return(continue_fsa);
}


/*
  Temporarily suspend the timer which controls periodic checkpointing.
*/
int
susp_ckpt_timer()
{
	MyAlarm.cancel();	// Cancel supervise test_connection() alarm
	MyTimer->suspend();
	return(0);
}

/*
  Set a global flags which says we may not transfer any checkpoints back
  to the initiating machine.
*/
int
unset_xfer()
{
	// dprintf( D_ALWAYS, "Setting XferCkpts to FALSE\n" );
	XferCkpts = FALSE;
	return(0);
}

/*
  Suspend all user processes and ourself - wait for a SIGCONT.
*/
int
susp_all()
{
	MyTimer->suspend();
	stop_all();

	susp_self();

	resume_all();
	MyTimer->resume();

	return(0);
}

/*
  Set a global flag which says we must leave after completing and
  transferring the current batch of checkpoints.
*/
int
set_quit()
{
	// dprintf( D_ALWAYS, "Entering function set_quit()\n" );

	QuitNow = TRUE;
	return(0);
}

/*
  Suspend every user process.
*/
int
stop_all()
{
	UserProc	*proc;

	// dprintf( D_ALWAYS, "Entering function stop_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->is_running() && proc->get_class() != PVMD ) {
			proc->suspend();
			dprintf( D_ALWAYS, "\tRequested user job to suspend\n" );
		}
	}

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->is_running() && proc->get_class() == PVMD ) {
			proc->suspend();
			dprintf( D_ALWAYS, "\tRequested user job to suspend\n" );
		}
	}
	return(0);
}

/*
  Resume every suspended user process.
*/
void
resume_all()
{
	UserProc	*proc;

	// dprintf( D_ALWAYS, "Entering function resume_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->is_suspended() && proc->get_class() == PVMD ) {
			proc->resume();
			dprintf( D_ALWAYS, "\tRequested user job to resume\n" );
		}
	}

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->is_suspended() && proc->get_class() != PVMD ) {
			proc->resume();
			dprintf( D_ALWAYS, "\tRequested user job to resume\n" );
		}
	}
}

/*
  Request all standard jobs perform a periodic checkpoint 
*/
int
periodic_ckpt_all()
{
	UserProc	*proc;

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->ckpt_enabled() && proc->get_class() != PVMD ) {
			proc->request_periodic_ckpt();
			dprintf( D_ALWAYS, "\tRequested user job to do a periodic checkpoint\n" );
		}
	}
	return(0);
}

/*
  Start up every runnable user process which isn't already running.
*/
int
spawn_all()
{
	UserProc	*proc;

	// dprintf( D_ALWAYS, "Entering function spawn_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->is_runnable() ) {
			proc->execute();
		} else {
			dprintf( D_ALWAYS, "Proc %d not runnable in state %d\n", 
					proc->get_id(), proc->get_state() );
		}
		if( proc->is_suspended() ) {
			proc->resume();
		}
	}
	return(0);
}

/*
  Test our connection back to the shadow.  If we cannot communicate with the
  shadow for whatever reason, close up shop.  We perform this test by simply sending
  a "null" message to the shadow's log socket (which the shadow will discard), i.e.
  the equivelent of a shadow "ping"
*/
int
test_connection()
{
	char    *pval;

	if ( write(CLIENT_LOG,"\0\n",2) == -1 ) {
		
        pval = param( "STARTER_LOCAL_LOGGING" );
        if( pval && (pval[0] == 't' || pval[0] == 'T') ) {
			dprintf( D_ALWAYS, "Lost our connection to the shadow! Exiting.\n" );
			free( pval );
		}

			// Send a SIGKILL to our whole process group
		set_root_priv();
		kill( -(getpid()), SIGKILL );

	}
	return 0;
}
	

/*
  Start up the periodic checkpoint timer, then wait for some asynchronous
  event.  The event could be the timer, but may also be caused by a
  user process exiting, a vacate request, etc.  If we run out of user
  processes, we will just sit here until we either get a GET_NEW_PROC
  or a VACATE or DIE request.
*/
int
supervise_all()
{
	UserProc	*proc;
	int			periodic_checkpointing = FALSE;
	int			bufid;
	int			src, tag;
	static Transition	*tr = 0;


	// dprintf( D_ALWAYS, "Entering function supervise_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->ckpt_enabled() ) {
			periodic_checkpointing = TRUE;
			break;
		}
	}

#if defined(LINK_PVM)
	if( Running_PVMD ) {
		//	Check for a queued PVM message, and get ready to handle it if
		//	ready.
		for(;;) {
			bufid = pvm_recv(-1, -1);
			pvm_bufinfo(bufid, NULL, &tag, &src);
			dprintf( D_ALWAYS, "supervise: received msg %s from t%x\n",
					PvmMsgNames.get_name(tag), src
				   );
			return PVM_MSG;
		}
	}
#endif

#if 1
	if (tr == 0) {
		tr = condor_starter_ptr->find_transition( ALARM );
		if ( tr ) {
			condor_starter_ptr->dont_print_transition( tr );
		}
	}
#endif

	for(;;) {
		// Set an ALARM so we regularly test_connection every 5 minutes
		MyAlarm.set(300);

		// Wait for an async event
		pause();
	}

		// Can never get here
	return NO_EVENT;
}

/*
  Some user process exited while we were in SUPERVISE state.  Determine
  the cause of the exit, and jump to the appropriate new state.
*/
int
proc_exit()
{
	UserProc	*proc;
	PROC_STATE	state;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

	switch( state = proc->get_state() ) {

	  case CHECKPOINTING:
		return CKPT_EXIT; // Started own checkpoint, go to UPDATE_CKPT state

	  case ABNORMAL_EXIT:
		return HAS_CORE;

	  case NON_RUNNABLE:
	  case NORMAL_EXIT:
		return NO_CORE;

	  default:
		EXCEPT( "Unexpected proc state (%d)\n", state );
	}

		// Can never get here.
	return DEFAULT;
}

/*
  Dispose of one user process from our list.
*/
int
dispose_one()
{
	UserProc	*proc;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

		// Send proc's status to shadow
	send_final_status( proc );

		// Delete proc from our list
	proc->delete_files();
	UProcList.DeleteCurrent();
	return(0);
}

/*
  Dispose of one user process from our list.
*/
int
make_runnable()
{
	UserProc	*proc;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

		// Send proc's status to shadow
	proc->make_runnable();
	proc->execute();

	return(0);
}

/*
  Some user process exited abnormally with a core.  Attempt to send that
  core back to the submitting machine so the user can debug it.
*/
int send_core()
{
	UserProc	*proc;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();
	proc->store_core();

	return DEFAULT;
}

/*
  Update checkpoint files for every process which wants checkpointing.
  At this point, every user process should be stopped or already exited
  with a core file for checkpointing.
*/
int
update_all()
{
	UserProc	*proc;

	MyTimer->clear();
	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		switch( proc->get_state() ) {
		  
		  case CHECKPOINTING:			// already exited with a core
			return UPDATE_ONE;

		  case SUSPENDED:				// request checkpoint now
			if( proc->ckpt_enabled() ) {
				proc->request_ckpt();
				return DO_WAIT;
				break;
			}
		}
	}

	if( QuitNow ) {
		return DO_QUIT;
	} else {
		MyTimer->update_interval();
		return SUCCESS;
	}
}

/*
  Wait for some asynchronous event.  We do the pause in a loop becuase it
  may return due to a SUSPEND/CONTINUE pair which is basically a no-op
  from the state machine's point of view.
*/
int
asynch_wait()
{
	for(;;) {
		pause();
	}
		// Can never get here
	return NO_EVENT;
}


/*
  Update the checkpoint file for the process which just exited.  It's
  possible the process has just exited for some other reason, so we
  must handle the other cases here too.
*/
int
update_one()
{
	UserProc	*proc;
	PROC_STATE	state;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

	switch( state = proc->get_state() ) {

	  case CHECKPOINTING:
		if( proc->update_ckpt() == FALSE ) {
			return FAILURE;
		}
		proc->commit_ckpt();
		return SUCCESS;

	  case ABNORMAL_EXIT:
		proc->store_core();
		return EXITED;

	  case NON_RUNNABLE:
	  case NORMAL_EXIT:
		return EXITED;

	  default:
		EXCEPT( "Unexpected proc state (%d)\n", state );
	}

		// Can never get here
	return SUCCESS;
}

/*
  User process has just been checkpointed.  Update the shadow with its
  accumulated CPU usage so that if the user process calls getrusage(),
  then usage from previous checkpointed runs will be included.
*/
int
update_cpu()
{
	UserProc	*proc;
	void		*rusage;

/* looks to me like this is not getting called anymore, and CONDOR_send_rusage
   is now used by the new condor 5.6 periodic checkpointing mechanism */
#ifdef 0
 
		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

	rusage = proc->accumulated_rusage();
	(void)REMOTE_syscall( CONDOR_send_rusage, rusage );
#endif
	return(0);
}



/*
  Send all checkpoint files back to the submitting machine.
*/
int
send_ckpt_all()
{
	UserProc	*proc;

	// dprintf( D_ALWAYS, "Entering function send_ckpt_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		proc->store_ckpt();
	}
	return DEFAULT;
}

/*
  Get information regarding one user process from the shadow.  Our
  protocal with the shadow calls for RPC's with us as the client and
  the shadow as the server, so we do the asking.
*/
UserProc *
get_job_info()
{
	UserProc		*u_proc;
	int				cmd = CONDOR_startup_info_request;
	STARTUP_INFO	s;
/*
	int wait_for_debugger = 1;

	while ( wait_for_debugger ) ; */

	dprintf( D_ALWAYS, "Entering get_job_info()\n" );

	memset( &s, 0, sizeof(s) );
	REMOTE_syscall( CONDOR_startup_info_request, &s );
	display_startup_info( &s, D_ALWAYS );

	determine_user_ids( s.uid, s.gid );
	dprintf( D_ALWAYS, "User uid set to %d\n", s.uid );
	dprintf( D_ALWAYS, "User uid set to %d\n", s.gid );

	set_user_ids( s.uid, s.gid );

	switch( s.job_class ) {
#if 0
		case PVMD:
			u_proc = new PVMdProc( s );
			break;
		case PVM:
			u_proc = new PVMUserProc( s );
			break;
#endif
		default:
			u_proc = new UserProc( s );
			break;
	}

	u_proc->display();
	return u_proc;
}

/*
  We have been asked to leave the machine, and have requested all of our
  user processes to exit.  Unfortunately however, one or more user
  processes hasn't exited after being sent a reqest and waiting a
  reasonable time.  Now we kill them all with SIGKILL (-9), delete
  their files, and remove them from our list.  We don't try to send
  any status's to the shadow becuase some process may have left our
  RPC stream to the shadow in an inconsitent state and we could hang
  if we tried using that.
*/
int
cleanup()
{
	UserProc	*proc;

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if ( proc->get_class() != PVMD ) {
			proc->kill_forcibly();
			proc->delete_files();
			UProcList.DeleteCurrent();
		}
	}
	UProcList.Rewind();
	while ( proc = UProcList.Next() ) {
		if ( proc->get_class() == PVMD ) {
			proc->kill_forcibly();
			proc->delete_files();
			UProcList.DeleteCurrent();
		}
	}
	return(0);
}

/*
  Clean up as well as we can in the face of an exception.  Make as few
  assumptions as possible about the state of things here - the
  exception could be called from anywhere.

  This function returns no useful value, and should be of type "void",
  but it has to get registered with the EXCEPT mechanism which is older
  and not typed correctly...
*/
int
exception_cleanup()
{
	sigset_t	mask;

		// Don't want to be called recursively by any further exceptions
	_EXCEPT_Cleanup = 0;

	alarm( 0 );			// cancel any pending alarm
	(void)cleanup();	// get rid of all user processes

		// make sure we don't have alarm's blocked
	sigemptyset( &mask );
	sigaddset( &mask, SIGALRM );
	sigprocmask( SIG_UNBLOCK, &mask, 0 );

		// Hang around awhile to make sure our last log messages get out...
		// I don't think this should be needed, but it is - mike
	sleep( 10 );
	return 0;
}

/*
  Find out how many 1024 byte "blocks" are free in the current file system.
*/
int
calc_free_disk_blocks()
{
	return free_fs_blocks( "." );
}

int	AvoidNFS = 0;

char Condor_CWD[ _POSIX_PATH_MAX ];

#if defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41) || defined(OSF1) || defined(HPUX9)
/*
  None of this stuff is ever used, but it must be here so that we
  can link with the remote system call library without getting
  "undefined" errors.
*/
int SyscallInProgress;
int CkptWanted;
int KillWanted;
extern "C" {
	void do_kill();
	void ckpt();
}
#endif

#if defined( NO_CKPT )
void do_kill(){}
void ckpt(){}
extern "C"	void _updateckpt( char *foo, char *bar, char *glarch ) {}
#endif

#if defined(AIX32)

void sigdanger_handler( int signo );

void
set_sigdanger_handler()
{
	struct sigaction action;

	action.sa_handler = sigdanger_handler;
	sigemptyset( &action.sa_mask );
	action.sa_flags = 0;

	if( sigaction(SIGDANGER,&action,0) < 0 ) {
		EXCEPT( "Can't set handler for SIGDANGER" );
	}
	
}

void
sigdanger_handler( int signo )
{
	EXCEPT( "GOT SIGDANGER!!!" );
}
#endif

/*
  Return true if we need to keep this fd open.  Any other random file
  descriptors which may have been left open by our parent process
  will get closed.
*/
int
needed_fd( int fd )
{
	switch( fd ) {

	  case 2:
	  case CLIENT_LOG:
		return TRUE;

	  case REQ_SOCK:		// used for remote system calls via named pipes
		if( UsePipes) {
			return TRUE;
		} else {
			return FALSE;
		}

	  case RSC_SOCK:		// for remote system calls TCP sock OR pipes
		return TRUE;

	  default:
		return FALSE;
	}
}

/*
  Return true if the given domain contains the given hostname.  For
  example the domain "cs.wisc.edu" constain the host "padauk.cs.wisc.edu".
*/
int
host_in_domain( const char *domain, const char *hostname )
{
	const char	*ptr;

	for( ptr=hostname; *ptr; ptr++ ) {
		if( strcmp(ptr,domain) == MATCH ) {
			return TRUE;
		}
	}

	return FALSE;
}

/*
  We've been requested to start the user job with the given UID, but
  we apply our own rules to determine whether we'll honor that request.
*/
void
determine_user_ids( uid_t &requested_uid, gid_t &requested_gid )
{

	struct passwd	*pwd_entry;

		// don't allow any root processes
	if( requested_uid == 0 || requested_gid == 0 ) {

		EXCEPT( "Attempt to start user process with root privileges" );
	}
		
		// if the submitting machine is in our shared UID domain, honor
		// the request
	if( host_in_domain(UidDomain,InitiatingHost) ) {
		return;
	}

		// otherwise, we run the process an "nobody"
	if( (pwd_entry = getpwnam("nobody")) == NULL ) {
#ifdef HPUX9
		// the HPUX9 release does not have a default entry for nobody,
		// so we'll help condor admins out a bit here...
		requested_uid = 59999;
		requested_gid = 59999;
		return;
#else
		EXCEPT( "Can't find UID for \"nobody\" in passwd file" );
#endif
	}

	requested_uid = pwd_entry->pw_uid;
	requested_gid = pwd_entry->pw_gid;

#ifdef HPUX9
	// HPUX9 has a bug in that getpwnam("nobody") always returns
	// a gid of 60001, no matter what the group file (or NIS) says!
	// on top of that, legal UID/GIDs must be -1<x<60000, so unless we
	// patch here, we will generate an EXCEPT later when we try a
	// setgid().  -Todd Tannenbaum, 3/95
	if ( (requested_uid > 59999) || (requested_uid < 0) )
		requested_uid = 59999;
	if ( (requested_gid > 59999) || (requested_gid < 0) )
		requested_gid = 59999;
#endif

}


/*
  Find out information about our AFS cell, UID domain, and file sharing
  domain.  Register this information with the shadow.
*/
void
init_environment_info()
{
	AFS_Info	info;
	char		*my_cell;
	char 		*my_fs_domain;
	char		*my_uid_domain;

	my_cell = info.my_cell();
	if( my_cell ) {
		REMOTE_syscall( CONDOR_register_afs_cell, my_cell );
	}

	my_fs_domain = param( "FILESYSTEM_DOMAIN" );
	if( my_fs_domain ) {
		REMOTE_syscall( CONDOR_register_fs_domain, my_fs_domain );
	}

	my_uid_domain = param( "UID_DOMAIN" );
	if( my_uid_domain ) {
		REMOTE_syscall( CONDOR_register_uid_domain, my_uid_domain );
	}
}
