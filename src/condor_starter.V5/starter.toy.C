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
#include "condor_debug.h"
#include "condor_uid.h"
#include "condor_config.h"
#include "proto.h"
#include "name_tab.h"
#include "state_machine_driver.h"
#include "starter.h"
#include "fileno.h"
#include "ckpt_file.h"
#include "alarm.h"


int	Quit;
int	Xfer = TRUE;
int NewProcs = 0;
int	VacateTimer = 60;

#undef ASSERT
#define ASSERT(cond) \
	if( !(cond) ) { \
		dprintf( D_ALWAYS, "Assertion ERROR on (cond)\n"); \
		exit( 1 ); \
	} else {\
		dprintf( D_FULLDEBUG, "Assertion Ok on (cond)\n"); \
	}


XDR		*SyscallStream;
char	*Execute;
int		MinCkptInterval;
int		MaxCkptInterval;

Alarm		MyAlarm;
CkptTimer	*MyTimer;


int
main( int argc, char *argv[] )
{
	StateMachine	condor_starter( StateTab, TransTab, EventSigs, START, END );

#define DEBUGGING 0
#if DEBUGGING
	wait_for_debugger( TRUE );
#else
	wait_for_debugger( FALSE );
#endif

	initial_bookeeping( argc, argv );

	condor_starter.display();
	// condor_starter.execute();
	return 0;
}

void
initial_bookeeping( int argc, char *argv[] )
{
	SetSyscalls( SYS_LOCAL | SYS_MAPPED );
	printf( "Hello World\n" );
	fflush( stdout );
	dprintf( D_ALWAYS, "Entered function initial_bookeeping()\n" );
}

int
init()
{
	dprintf( D_ALWAYS, "Entering function init()\n" );
	return DEFAULT;
}


void
kill_all()
{
	dprintf( D_ALWAYS, "Entering function kill_all()\n" );
}

int
set_quit()
{
	dprintf( D_ALWAYS, "Entering function set_quit()\n" );
	return(0);
}

int
stop_all()
{
	dprintf( D_ALWAYS, "Entering function stop_all()\n" );
	return(0);
}

void
suspend_alarm()
{
	dprintf( D_ALWAYS, "Entering function suspend_alarm()\n" );
}

void
reap_supervise()
{
	dprintf( D_ALWAYS, "Entering function reap_supervise()\n" );
}

void
reap_terminate()
{
	dprintf( D_ALWAYS, "Entering function reap_terminate()\n" );
}

void
reap_update()
{
	dprintf( D_ALWAYS, "Entering function reap_update()\n" );
}

int
unset_xfer()
{
	dprintf( D_ALWAYS, "Entering function unset_xfer()\n" );
	return(0);
}

void
incr_new_procs()
{
	dprintf( D_ALWAYS, "Entering function incr_new_procs()\n" );
}

int
handle_vacate_req()
{
	dprintf( D_ALWAYS, "Entering function handle_vacate_req()\n" );
	return(0);
}

void
handle_sync_ckpt()
{
	dprintf( D_ALWAYS, "Entering function handle_sync_ckpt()\n" );
}

int
spawn_all()
{
	dprintf( D_ALWAYS, "Entering function spawn_all()\n" );
	return(0);
}

int
susp_self()
{
	dprintf( D_ALWAYS, "Suspending self\n" );
	kill( getpid(), SIGSTOP );
	dprintf( D_ALWAYS, "Resuming self\n" );
	return(0);
}

int
get_exec()
{
	dprintf( D_ALWAYS, "Entering function get_exec()\n" );
	delay( 20 );
	return SUCCESS;
}

int
terminate_all()
{
	dprintf( D_ALWAYS, "Entering function terminate_all()\n" );
	delay( 20 );
	return DO_XFER;
}

int
supervise_all()
{
	dprintf( D_ALWAYS, "Entering function supervise()\n" );
	delay( 20 );
	return ALARM;
}


int
send_ckpt()
{
	dprintf( D_ALWAYS, "Entering function send_ckpt()\n" );
	delay( 20 );
	return DEFAULT;
}



int
update_ckpt()
{
	dprintf( D_ALWAYS, "Entering function update_ckpt()\n" );
	delay( 20 );
	return SUCCESS;
}

int
susp_all()
{
	dprintf( D_ALWAYS, "Entering function susp_user_proc()\n" );

	MyTimer->suspend();
	dprintf( D_ALWAYS, "\tRequested user job to suspend\n" );

	susp_self();

	MyTimer->resume();
	dprintf( D_ALWAYS, "\tRequested user job to restart\n" );
	return(0);
}

void
usage( char *my_name )
{
	dprintf( D_ALWAYS, "Usage: %s cluster_id proc_id job_file\n", my_name );
	exit( 0 );
}

/*
** Wait up for one of those nice debuggers which attaches to a
** running process.  These days, most every debugger can do this
** with a notable exception being the ULTRIX version of "dbx".
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

int
get_proc()
{
	int	answer;

	dprintf( D_ALWAYS, "Entering function get_proc()\n" );
	delay( 30 );
	return SUCCESS;
}

void
init_ckpt_timer()
{
	char	*value;
	int		answer;

	MyTimer = new CkptTimer( MinCkptInterval, MaxCkptInterval );
}

int
wait_all()
{
	dprintf( D_ALWAYS, "Entering functin wait_all()\n" );
	delay( 30 );
	return ALARM;
}

int
update_all()
{
	dprintf( D_ALWAYS, "Entering function update_all()\n" );
	delay( 30 );
	return SUCCESS;
}

int
send_ckpt_all()
{
	dprintf( D_ALWAYS, "Entering function send_ckpt_all()\n" );
	delay( 30 );
	return DEFAULT;
}

int
cleanup()
{
	dprintf( D_ALWAYS, "Entering function cleanup()\n" );
	delay( 30 );
	return DEFAULT;
}


/*
Delay approximately sec seconds for purposes of debugging.
*/
void
delay( int sec )
{
	int		i;
	int		j, k;
#if defined(R6000)
	int		lim = 250000;
#endif
#if defined(MIPS)
	int		lim = 200000;
#endif

	if( sec == 0 ) {
		return;
	}

	dprintf( D_ALWAYS, "Beginning delay of %d seconds\n", sec );

	for( i=sec; i > 0; i-- ) {
		// dprintf( D_ALWAYS | D_NOHEADER,  "%d\n", i );
		for( j=0; j<lim; j++ ) {
			for( k=0; k<5; k++ );
		}
	}
	// dprintf( D_ALWAYS | D_NOHEADER,  "0\n" );
}

#if 1
int
SetSyscalls( int val )
{
	return val;
}
#endif
