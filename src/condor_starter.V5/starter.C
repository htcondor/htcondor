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

#include "condor_common.h"
#include "starter_common.h"

#include "proto.h"
#include "name_tab.h"

#include "state_machine_driver.h"

#include "starter.h"
#include "fileno.h"
#include "ckpt_file.h"
#include "startup.h"
#include "alarm.h"
#include "list.h"
#include "user_proc.h"
#include "sig_install.h"

#if !defined(X86)
typedef List<UserProc> listuserproc; 
#endif 

extern "C" {
void display_startup_info( const STARTUP_INFO *s, int flags );
}

#if defined(LINK_PVM)
#include "pvm_user_proc.h"
#include "pvm3.h"
#include "sdpro.h"
#endif


char* mySubSystem = "STARTER";

#undef ASSERT
#define ASSERT(cond) \
	if( !(cond) ) { \
		EXCEPT( "Assertion ERROR on (%s)\n", #cond ); \
	} else {\
		dprintf( D_FULLDEBUG, "Assertion Ok on (%s)\n", #cond ); \
	}

	// Constants
const pid_t	ANY_PID = -1;		// arg to waitpid() for any process

ReliSock	*SyscallStream;		// stream to shadow for remote system calls
List<UserProc>	UProcList;		// List of user processes
char	*Execute;				// Name of directory where user procs execute
int		DoDelays;				// Insert artificial delays for testing
char	*UidDomain;				// Machines we share UID space with

Alarm		MyAlarm;			// Don't block forever on user process exits
char	*InitiatingHost;		// Machine where shadow is running
char	*ThisHost;				// Machine where we are running

extern State StateTab[];
extern Transition TransTab[];
extern int EventSigs[];

extern NameTable ProcStates;
extern NameTable PvmMsgNames;

extern int shadow_tid;

	// instantiate template
template class List<UserProc>;
template class Item<UserProc>;

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

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
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
	dprintf( D_ALWAYS, "Usage: %s ( -pipe | initiating_host) [<STARTD_IP:STARTD_PORT>]\n", my_name );
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


	(void) dup2( 1, RSC_SOCK );
	(void) dup2( 2, CLIENT_LOG );
	SyscallStream = init_syscall_connection( FALSE);
		/* Set a timeout on remote system calls.  This is needed in case
		   the user job exits in the middle of a remote system call, leaving
		   the shadow blocked.  -Jim B. */
	SyscallStream->timeout(300);
}

/*
  Get relevant information from "condor_config" and "condor_config.local"
  files.
*/
void
read_config_files( void )
{
	config();

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
		dprintf( D_ALWAYS, "req_ckpt_exit_all: Proc %d in state %s\n", 
				 proc->get_id(),	ProcStates.get_name(proc->get_state())
				 );
		if( proc->is_running() || proc->is_suspended() ) {
			dprintf( D_ALWAYS, "Requesting Exit on proc #%d\n",
					 proc->get_id());
			proc->request_exit();
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
				proc->kill_forcibly();
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

	return DEFAULT;
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
		// Note: for now, updating the image size is commented out.
		// condor_syscall_lib updates the image size, and someday
		// we should add code for the starter to estimate the image size
		// for other universe jobs and then update here, but until we
		// add the image size estimate code, we may as well comment this
		// out to be safe. -Todd 3/98
#if 0
	image_size = proc->get_image_size();
	if ( image_size > 0 ) {
		(void)REMOTE_syscall( CONDOR_image_size, image_size );
	}
#endif

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
	return(0);
}


/*
  Suspend all user processes and ourself - wait for a SIGCONT.
*/
int
susp_all()
{
	stop_all();

	susp_self();

	resume_all();

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
		if( proc->ckpt_enabled() ) {
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

	if (tr == 0) {
		tr = condor_starter_ptr->find_transition( ALARM );
		if ( tr ) {
			condor_starter_ptr->dont_print_transition( tr );
		}
	}

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
#if 0
 
		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

	rusage = proc->accumulated_rusage();
	(void)REMOTE_syscall( CONDOR_send_rusage, rusage );
#endif
	return(0);
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

	// make sure startup info struct is initialized to zero pointers
	memset((char *)&s, 0, sizeof(STARTUP_INFO));

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
extern "C" {
int
exception_cleanup(int,int,char*)
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
} /* extern "C" */


int	AvoidNFS = 0;

char Condor_CWD[ _POSIX_PATH_MAX ];

#if defined(OSF1) || defined(HPUX)
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
		  return FALSE;

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

		/* check to see if there is an entry in the passwd file for this uid */
		if( (pwd_entry=getpwuid(requested_uid)) == NULL ) {
			char *want_soft = NULL;
	
			if ( (want_soft=param("SOFT_UID_DOMAIN")) == NULL || 
				 (*want_soft != 'T' && *want_soft != 't') ) {
			  EXCEPT("Uid not found in passwd file & SOFT_UID_DOMAIN is False");
			}
			if ( want_soft ) {
				free(want_soft);
			}
		}
		(void)endpwent();

		return;
	}

		// otherwise, we run the process an "nobody"
	if( (pwd_entry = getpwnam("nobody")) == NULL ) {
#ifdef HPUX
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

#ifdef HPUX
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

#ifdef IRIX
		// Same weirdness on IRIX.  60001 is the default uid for
		// nobody, lets hope that works.
	if ( (requested_uid >= UID_MAX ) || (requested_uid < 0) )
		requested_uid = 60001;
	if ( (requested_gid >= UID_MAX) || (requested_gid < 0) )
		requested_gid = 60001;
#endif

}


/*
  Find out information about our UID domain and file sharing
  domain.  Register this information with the shadow.
*/
void
init_environment_info()
{
	char		*my_cell;
	char 		*my_fs_domain;
	char		*my_uid_domain;
	char		*ckpt_server_host;
	char		*arch, *opsys;

	my_fs_domain = param( "FILESYSTEM_DOMAIN" );
	if( my_fs_domain ) {
		REMOTE_syscall( CONDOR_register_fs_domain, my_fs_domain );
		free(my_fs_domain);
	}

	my_uid_domain = param( "UID_DOMAIN" );
	if( my_uid_domain ) {
		REMOTE_syscall( CONDOR_register_uid_domain, my_uid_domain );
		free(my_uid_domain);
	}

#if !defined(CONTRIB)
	ckpt_server_host = param( "CKPT_SERVER_HOST" );
	if( ckpt_server_host ) {
		REMOTE_syscall( CONDOR_register_ckpt_server, ckpt_server_host );
		free(ckpt_server_host);
	}

	arch = param( "ARCH" );
	if (arch) {
		REMOTE_syscall( CONDOR_register_arch, arch );
		free(arch);
	}

	opsys = param( "OPSYS" );
	if (opsys) {
		REMOTE_syscall( CONDOR_register_opsys, opsys);
		free(opsys);
	}
#endif
}
