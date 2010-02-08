/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "starter_common.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_distribution.h"
#include "subsystem_info.h"

#include "proto.h"
#include "name_tab.h"

#include "state_machine_driver.h"

#include "starter.h"
#include "fileno.h"
#include "startup.h"
#include "alarm.h"
#include "list.h"
#include "user_proc.h"
#include "sig_install.h"
#include "../condor_sysapi/sysapi.h"


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

/* For daemonCore, etc. */
DECL_SUBSYSTEM( "STARTER", SUBSYSTEM_TYPE_STARTER );

#undef ASSERT
#define ASSERT(cond) \
	if( !(cond) ) { \
		EXCEPT( "Assertion ERROR on (%s)\n", #cond ); \
	} else {\
		dprintf( D_FULLDEBUG, "Assertion Ok on (%s)\n", #cond ); \
	}

	// Constants
const pid_t	ANY_PID = -1;		// arg to waitpid() for any process

ReliSock	*SyscallStream = NULL;	// stream to shadow for remote system calls
List<UserProc>	UProcList;		// List of user processes
char	*Execute;				// Name of directory where user procs execute
int		ExecTransferAttempts;	// How many attempts at getting the initial ckpt
char	*UidDomain=NULL;		// Machines we share UID space with
bool	TrustUidDomain=false;	// Should we trust what the submit side claims?

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

	// Prototypes of local interest only
int update_one( UserProc *proc );
void send_final_status( UserProc *proc );
void resume_all();
void req_exit_all();
void req_ckpt_exit_all();
int needed_fd( int fd );
void determine_user_ids( uid_t &requested_uid, gid_t &requested_gid );
void init_environment_info();
void determine_user_ids( uid_t &requested_uid, gid_t &requested_gid );
StateMachine	*condor_starter_ptr;

void
printClassAd( void )
{
	printf( "%s = False\n", ATTR_IS_DAEMON_CORE );
	printf( "%s = True\n", ATTR_HAS_REMOTE_SYSCALLS );
	printf( "%s = True\n", ATTR_HAS_CHECKPOINTING );
	printf( "%s = \"%s\"\n", ATTR_VERSION, CondorVersion() );
}


/*
  Main routine for condor_starter.  DEBUGGING can be turned on if we
  need to attach a debugger at run time.  Everything is driven by the
  finite state machine, so we just intiialize it and run it.
*/
int
main( int argc, char *argv[] )
{
	myDistro->Init( argc, argv );
	if( argc == 2 && strincmp(argv[1], "-cl", 3) == MATCH ) {
		printClassAd();
		exit( 0 );
	}

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
    free(UidDomain);
	dprintf( D_ALWAYS, "********* STARTER terminating normally **********\n" );
	return 0;
}


/*
  Initialization stuff to be done before getting any use processes.
*/
int
init()
{
	move_to_execute_directory();
	init_environment_info();
	sysapi_set_resource_limits();
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
	(void) dup2( 1, RSC_SOCK );
	(void) dup2( 2, CLIENT_LOG );
	SyscallStream = init_syscall_connection( FALSE);
		/* Set a timeout on remote system calls.  This is needed in case
		   the user job exits in the middle of a remote system call, leaving
		   the shadow blocked.  -Jim B. */
	SyscallStream->timeout(300);
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

	TrustUidDomain = false;
	tmp = param( "TRUST_UID_DOMAIN" );
	if( tmp ) {
		if( tmp[0] == 't' || tmp[0] == 'T' ) { 
			TrustUidDomain = true;
		}			
		free( tmp );
	}


	// We can configure how many times the starter wishes to attempt to
	// pull over the initial checkpoint
	ExecTransferAttempts = param_integer( "EXEC_TRANSFER_ATTEMPTS", 3, 1 );
}

/*
  Get one user process from the shadow and append it to our list of
  user processes.  Note - this routine does not transfer any checkpoint
  or executable files.  That is done in a separate routine.
*/
int
get_proc()
{
	UserProc *new_process;

	dprintf( D_ALWAYS, "Entering get_proc()\n" );

	if( (new_process=get_job_info()) ) {
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
	while( (proc = UProcList.Next()) ) {
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
	while( (proc = UProcList.Next()) ) {
		if ( proc->get_class() != CONDOR_UNIVERSE_PVMD ) {
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
	while( (proc = UProcList.Next()) ) {
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
	while( (proc = UProcList.Next()) ) {
		send_final_status( proc );
		proc->delete_files();
		UProcList.DeleteCurrent();
		delete proc; // DeleteCurrent() doesn't delete it, so we do
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
	//int			image_size;
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
		(void)REMOTE_CONDOR_image_size( image_size );
	}
#endif

		// update shadow with it's resource usage and exit status
	status = proc->bsd_exit_status();
	if( proc->get_class() == CONDOR_UNIVERSE_PVM ) {
		rusage = proc->accumulated_rusage();	// All resource usage
		id = proc->get_id();
		(void)REMOTE_CONDOR_subproc_status( (int)id, (int*)status, 
			(struct rusage*)rusage);
	} else {
		rusage = proc->guaranteed_rusage();		// Only usage guaranteed by ckpt
		(void)REMOTE_CONDOR_reallyexit( (int*)status, (struct rusage*)rusage);
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
	while( (proc = UProcList.Next()) ) {
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
	char *susp_msg = "TISABH Starter: Suspended user job: ";
	char *unsusp_msg = "TISABH Starter: Unsuspended user job.";
	char msg[4096];
	UserProc	*proc;
	int sum;

	stop_all();

	/* determine how many pids the starter suspended */
	sum = 0;
	UProcList.Rewind();
	while( (proc = UProcList.Next()) ) {
		if( proc->is_suspended() ) {
			sum += proc->get_num_pids_suspended();
		}
	}

	/* Now that we've suspended the jobs, write to our client log
		fd some information about what we did so the shadow
		can figure out we suspended the job. TISABH stands for
		"This Is Such A Bad Hack".  Note: The starter isn't
		supposed to be messing with this fd like this, but
		the high poobah wanted this feature hacked into
		the old starter/shadow combination. So here it
		is. Sorry. If you change this string, please go to
		ops.C in the shadow.V6 directory and change the function
		log_old_starter_shadow_suspend_event_hack() to reflect
		the new choice.  -psilord 2/1/2001 */

	sprintf(msg, "%s%d\n", susp_msg, sum);
	/* Hmm... maybe I should write a loop that checks to see if this is ok */
	write(CLIENT_LOG, msg, strlen(msg));

	susp_self();

	/* Before we unsuspend the jobs, write to the client log that we are
		about to so the shadow knows. Make sure to do this BEFORE we unsuspend
		the jobs.
		-psilord 2/1/2001 */
	sprintf(msg, "%s\n", unsusp_msg);
	/* Hmm... maybe I should write a loop that checks to see if this is ok */
	write(CLIENT_LOG, msg, strlen(msg));

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
	while( (proc = UProcList.Next()) ) {
		if( proc->is_running() && proc->get_class() != CONDOR_UNIVERSE_PVMD ) {
			proc->suspend();
			dprintf( D_ALWAYS, "\tRequested user job to suspend\n" );
		}
	}

	UProcList.Rewind();
	while( (proc = UProcList.Next()) ) {
		if( proc->is_running() && proc->get_class() == CONDOR_UNIVERSE_PVMD ) {
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
	while( (proc = UProcList.Next()) ) {
		if( proc->is_suspended() && proc->get_class() == CONDOR_UNIVERSE_PVMD ) {
			proc->resume();
			dprintf( D_ALWAYS, "\tRequested user job to resume\n" );
		}
	}

	UProcList.Rewind();
	while( (proc = UProcList.Next()) ) {
		if( proc->is_suspended() && proc->get_class() != CONDOR_UNIVERSE_PVMD ) {
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
	while( (proc = UProcList.Next()) ) {
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
	while( (proc = UProcList.Next()) ) {
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
		}
		free( pval );

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
	static Transition	*tr = 0;


	// dprintf( D_ALWAYS, "Entering function supervise_all()\n" );

	UProcList.Rewind();
	while( (proc = UProcList.Next()) ) {
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
	delete proc; // DeleteCurrent() doesn't delete it, so we do
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
	//UserProc	*proc;
	//void		*rusage;

/* looks to me like this is not getting called anymore, and CONDOR_send_rusage
   is now used by the new condor 5.6 periodic checkpointing mechanism */
#if 0
 
		// Grab a pointer to proc which just exited
	proc = UProcList.Current();

	rusage = proc->accumulated_rusage();
	(void)REMOTE_CONDOR_send_rusage( rusage );
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
	STARTUP_INFO	s;
/*
	int wait_for_debugger = 1;

	while ( wait_for_debugger ) ; */

	// make sure startup info struct is initialized to zero pointers
	memset((char *)&s, 0, sizeof(STARTUP_INFO));

	dprintf( D_ALWAYS, "Entering get_job_info()\n" );

	memset( &s, 0, sizeof(s) );
	REMOTE_CONDOR_startup_info_request( &s );
	display_startup_info( &s, D_ALWAYS );

	determine_user_ids( s.uid, s.gid );

	dprintf( D_ALWAYS, "User uid set to %d\n", s.uid );
	dprintf( D_ALWAYS, "User uid set to %d\n", s.gid );

	set_user_ids( s.uid, s.gid );

	switch( s.job_class ) {
#if 0
		case CONDOR_UNIVERSE_PVMD:
			u_proc = new PVMdProc( s );
			break;
		case CONDOR_UNIVERSE_PVM:
			u_proc = new PVMUserProc( s );
			break;
#endif
		default:
			u_proc = new UserProc( s );
			break;
	}

	u_proc->display();

	// We need to clean up the memory allocated in the STARTUP_INFO I
	// think that STARTUP_INFO should probably be a class with a
	// destructor, but I am pretty unfamiliar with the code, so I'm
	// going to make a minimal change here. By the way, we know it's
    // safe to free this memory, because the UserProc class makes
	// copies of it. --Alain 25-Sep-2001
	if (s.cmd)  free(s.cmd);
	if (s.args_v1or2) free(s.args_v1or2);
	if (s.env_v1or2)  free (s.env_v1or2);
	if (s.iwd)  free (s.iwd);

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
	while( (proc = UProcList.Next()) ) {
		if ( proc->get_class() != CONDOR_UNIVERSE_PVMD ) {
			proc->kill_forcibly();
			proc->delete_files();
			UProcList.DeleteCurrent();
			delete proc; // DeleteCurrent() doesn't delete it, so we do
		}
	}
	UProcList.Rewind();
	while ( (proc = UProcList.Next()) ) {
		if ( proc->get_class() == CONDOR_UNIVERSE_PVMD ) {
			proc->kill_forcibly();
			proc->delete_files();
			UProcList.DeleteCurrent();
			delete proc; // DeleteCurrent() doesn't delete it, so we do
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
  Find out information about our UID domain and file sharing
  domain.  Register this information with the shadow.
*/
void
init_environment_info()
{
	char 		*my_fs_domain;
	char		*my_uid_domain;
	char		*ckpt_server_host;
	char		*arch, *opsys;
	const char  *ckptpltfrm;

	my_fs_domain = param( "FILESYSTEM_DOMAIN" );
	if( my_fs_domain ) {
		REMOTE_CONDOR_register_fs_domain( my_fs_domain );
		free(my_fs_domain);
	}

	my_uid_domain = param( "UID_DOMAIN" );
	if( my_uid_domain ) {
		REMOTE_CONDOR_register_uid_domain( my_uid_domain );
		free(my_uid_domain);
	}

	ckptpltfrm = sysapi_ckptpltfrm();
	/* don't forget one more for the NULL which needs to go over as well */
	REMOTE_CONDOR_register_ckpt_platform( ckptpltfrm, strlen(ckptpltfrm) + 1);

#if !defined(CONTRIB)
	ckpt_server_host = param( "CKPT_SERVER_HOST" );
	if( ckpt_server_host ) {
		REMOTE_CONDOR_register_ckpt_server( ckpt_server_host );
		free(ckpt_server_host);
	}

	arch = param( "ARCH" );
	if (arch) {
		REMOTE_CONDOR_register_arch( arch );
		free(arch);
	}

	opsys = param( "OPSYS" );
	if (opsys) {
		REMOTE_CONDOR_register_opsys( opsys );
		free(opsys);
	}
#endif
}
