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

#include "condor_common.h"
#include "condor_expressions.h"
#include "condor_debug.h"
#include "condor_constants.h"
#include "condor_jobqueue.h"
#include "condor_uid.h"
#include "condor_config.h"
#include <signal.h>
#include <sys/wait.h>
#include "proto.h"
#include "condor_sys.h"
#include "name_tab.h"

#include "../condor_c++_util/state_machine_driver.h"

#include "starter.h"
#include "fileno.h"
#include "ckpt_file.h"
#include "user_proc.h"
#include "alarm.h"

#include "../condor_c++_util/list.h"

#include <sys/stat.h>

extern "C" {
#include <sys/utsname.h>
int free_fs_blocks(const char *);
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
const int	VacateTimer = 60;	// How long to wait for proc requested to exit
const pid_t	ANY_PID = -1;		// arg to waitpid() for any process

XDR		*SyscallStream;			// XDR stream to shadow for remote system calls
List<UserProc>	UProcList;		// List of user processes
char	*Execute;				// Name of directory where user procs execute
int		MinCkptInterval;		// Use this ckpt interval to start
int		MaxCkptInterval;		// Don't increase ckpt interval beyond this
int		DoDelays;				// Insert artificial delays for testing

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

extern "C" {
	void dprintf_config( char *subsys, int logfd );
}

/*
  Main routine for condor_starter.  DEBUGGING can be turned on if we
  need to attach a debugger at run time.  Everything is driven by the
  finite state machine, so we just intiialize it and run it.
*/
int
main( int argc, char *argv[] )
{
	StateMachine	condor_starter( StateTab, TransTab, EventSigs, START, END );

#define DEBUGGING FALSE
#if DEBUGGING
	wait_for_debugger( TRUE );
#else
	wait_for_debugger( FALSE );
#endif

	set_posix_environment();
	initial_bookeeping( argc, argv );

	// condor_starter.display();
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

	set_condor_euid();
	(void)umask( 0 );

	init_shadow_connections();

	read_config_files( argv[0] );
	init_logging();

		/* Now if we have an error, we can print a message */
	if( argc != 2 ) {
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
#elif defined(R6000)
	int		lim = 250000;
#elif defined(MIPS)
	int		lim = 200000;
#elif defined(ALPHA)
	int		lim = 300000;
#elif defined(HPPAR)
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
	set_resource_limits();
	close_unused_file_descriptors();
	MyTimer = new CkptTimer( MinCkptInterval, MaxCkptInterval );
	return DEFAULT;
}


/*
  Suspend ourself and wait for a SIGCONT signal.
*/
void
susp_self()
{
	dprintf( D_ALWAYS, "Suspending self\n" );
	kill( getpid(), SIGSTOP );
	dprintf( D_ALWAYS, "Resuming self\n" );
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
		return;
	}

		// Set up to do logging through the shadow

	close( fileno(stderr) );
	dup2( CLIENT_LOG, fileno(stderr) );
	setvbuf( stderr, NULL, _IOLBF, 0 ); // line buffering

	pval = param( "STARTER_DEBUG" );
	if( pval ) {
		set_debug_flags( pval );
	}
	DebugFlags |= D_NOHEADER;

}

/*
  Get relevant information from "condor_config" and "condor_config.local"
  files.
*/
void
read_config_files( char *my_name )
{
	config( my_name, (CONTEXT *)0 );

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

	if( (tmp=param("STARTER_DELAYS")) == NULL ) {
		DoDelays = FALSE;
	} else {
		if( tmp[0] == 't' || tmp[0] == 'T' ) {
			DoDelays = TRUE;
		} else {
			DoDelays = FALSE;
		}
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

	// dprintf( D_ALWAYS, "Entering get_proc()\n" );
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

	// dprintf( D_ALWAYS, "Entering get_exec()\n" );


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
void
handle_vacate_req()
{
	// dprintf( D_ALWAYS, "Entering function handle_vacate_req()\n" );

	MyTimer->clear();		// clear ckpt timer
	stop_all();				// stop any running user procs
	QuitNow = TRUE;			// set flag so other routines know we have to leave
}


/*
  We've been asked to leave the machine, and we may not create or
  update any more checkpoint files.  We may however transfer any
  existing checkpoint files back to the submitting machine.
*/
void
req_vacate()
{
		// In V5 ckpt so fast, we can do it here
	req_ckpt_exit_all();

	MyAlarm.set( VacateTimer );
}

/*
  We've been asked to leave the machine, and we may not create or
  update any more checkpoint files.  Also, we may not transfer any
  existing checkpoint files back to the submitting machine.
*/
void
req_die()
{
	req_exit_all();
	MyAlarm.set( VacateTimer );
	XferCkpts = FALSE;
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
void
reaper()
{
	int			st;
	pid_t		pid;
	UserProc	*proc;

		// Wait for a child process to exit
	while( (pid=waitpid(ANY_PID,&st,0)) == -1 && errno == EINTR )
		;

		// Should never happen...
	if( pid == -1 ) {
		EXCEPT( "wait()" );
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

		// If we didn't find the process's object, bail out now
	if( proc == NULL ) {
		EXCEPT(
			"wait() returned pid %d, but no such process marked as EXECUTING",
			pid
		);
	}

		// Ok, update the object
	dprintf( D_FULLDEBUG, "Found object for process %d\n", pid );
	proc->handle_termination( st );
}


/*
  Temporarily suspend the timer which controls periodic checkpointing.
*/
void
susp_ckpt_timer()
{
	MyTimer->suspend();
}

/*
  Set a global flags which says we may not transfer any checkpoints back
  to the initiating machine.
*/
void
unset_xfer()
{
	// dprintf( D_ALWAYS, "Setting XferCkpts to FALSE\n" );
	XferCkpts = FALSE;
}

/*
  Suspend all user processes and ourself - wait for a SIGCONT.
*/
void
susp_all()
{
	MyTimer->suspend();
	stop_all();

	susp_self();

	resume_all();
	MyTimer->resume();
}

/*
  Set a global flag which says we must leave after completing and
  transferring the current batch of checkpoints.
*/
void
set_quit()
{
	// dprintf( D_ALWAYS, "Entering function set_quit()\n" );

	QuitNow = TRUE;
}

/*
  Suspend every user process.
*/
void
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
  Start up every runnable user process which isn't already running.
*/
void
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


	// dprintf( D_ALWAYS, "Entering function supervise_all()\n" );

	UProcList.Rewind();
	while( proc = UProcList.Next() ) {
		if( proc->ckpt_enabled() ) {
			periodic_checkpointing = TRUE;
			break;
		}
	}

	if( periodic_checkpointing ) {
		dprintf( D_FULLDEBUG, "Periodic checkpointing NOT implemented yet\n" );
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

	for(;;) {
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
void
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
}

/*
  Dispose of one user process from our list.
*/
void
make_runnable()
{
	UserProc	*proc;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

		// Send proc's status to shadow
	proc->make_runnable();
	proc->execute();

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
  accumulated CPU usage so that it the user process calls getrusage(),
  then usage from previous checkpointed runs will be included.
*/
void
update_cpu()
{
	UserProc	*proc;
	void		*rusage;

		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

	rusage = proc->accumulated_rusage();
	(void)REMOTE_syscall( CONDOR_send_rusage, rusage );
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
	char	buf[1024];

		// Set up to receive either a version 2 or a version 3 proc
	GENERIC_PROC	proc_struct;
	V2_PROC			&v2_proc = (V2_PROC &)proc_struct;
	V3_PROC			&v3_proc = (V3_PROC &)proc_struct;

	char	a_out [ _POSIX_PATH_MAX ];
	char	orig_file[ _POSIX_PATH_MAX ];
	char	target_file[ _POSIX_PATH_MAX ];
	uid_t	uid, gid;
	int		scm;
	UserProc	*u_proc;
	int		cmd = CONDOR_work_request;
	int		id = -1;
	int		soft_kill;

	// dprintf( D_ALWAYS, "Entering get_job_info()\n" );

	memset( &proc_struct, 0, sizeof(proc_struct) );
	id = REMOTE_syscall( CONDOR_work_request,
					&proc_struct, a_out, target_file, orig_file, &soft_kill
	);

#define NOBODY -2
#define RUN_AS_NOBODY FALSE
#if RUN_AS_NOBODY
	uid = NOBODY;
	gid = NOBODY;
#else
	uid = REMOTE_syscall( CONDOR_geteuid );
	gid = REMOTE_syscall( CONDOR_getegid );
#endif

	if( uid == 0 ) {
		EXCEPT( "Attempt to start user process with root privileges" );
	}

	switch( v2_proc.version_num ) {
	  case 2:
		u_proc = new UserProc( v2_proc, a_out, orig_file, target_file, uid, gid, soft_kill );
		break;
	  case 3:
		switch(v3_proc.universe) {
#if defined(LINK_PVM)
		    case PVMD:
			    u_proc = new PVMdProc( v3_proc, a_out, orig_file, target_file, uid, 
									  gid, id, soft_kill);
				break;
		    case PVM:
			    u_proc = new PVMUserProc( v3_proc, a_out, orig_file, target_file, 
										 uid, gid, id, soft_kill);
				break;
#endif
		    default:
			    u_proc = new UserProc( v3_proc, a_out, orig_file, target_file, uid, 
									  gid, id, soft_kill);
				break;
		}
		break;
	  default:
		EXCEPT( "UNKNOWN Proc Version (%d)", v2_proc.version_num );
	}

	u_proc->display();

	/* N.B. should free up these XDR allocated structures here... */

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
void
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

#if defined(ULTRIX42) || defined(ULTRIX43) || defined(SUNOS41) || defined(OSF1)
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
