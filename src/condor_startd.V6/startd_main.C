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

/*
 * Main routine and function for the startd.
 */
#define _STARTD_NO_DECLARE_GLOBALS 1
#include "startd.h"
#include "startd_cronmgr.h"

// Define global variables

// Resource manager
ResMgr*	resmgr;			// Pointer to the resource manager object

// Polling variables
int	polling_interval = 0;	// Interval for polling when there are resources in use
int	update_interval = 0;	// Interval to update CM

// Paths
char*	exec_path = NULL;

// String Lists
StringList *startd_job_exprs = NULL;

// Hosts
Daemon*	Collector = NULL;
Daemon* View_Collector = NULL;
char*	condor_view_host = NULL;
char*	accountant_host = NULL;

// Others
int		match_timeout;		// How long you're willing to be
							// matched before claimed 
int		killing_timeout;	// How long you're willing to be in
							// preempting/killing before you drop the
							// hammer on the starter
time_t	startd_startup;		// Time when the startd started up

int		console_vms = 0;	// # of nodes in an SMP that care about
int		keyboard_vms = 0;  //   console and keyboard activity
int		disconnected_keyboard_boost;	// # of seconds before when we
	// started up that we advertise as the last key press for
	// resources that aren't connected to anything.  

int		startd_noclaim_shutdown = 0;	
    // # of seconds we can go without being claimed before we "pull
    // the plug" and tell the master to shutdown.

bool	compute_avail_stats = false;
	// should the startd compute vm availability statistics; currently 
	// false by default

char* Name = NULL;

int		pid_snapshot_interval = 50;
    // How often do we take snapshots of the pid families? 

char*	mySubSystem = "STARTD";

int main_reaper = 0;

// Cron stuff
StartdCronMgr	*Cronmgr;

// Define static variables
static	int old_polling_interval;

/*
 * Prototypes of static functions.
 */

void usage( char* );
int main_init( int argc, char* argv[] );
int init_params(int);
int main_config( bool is_full );
int finish_main_config();
int main_shutdown_fast();
int main_shutdown_graceful();
extern "C" int do_cleanup(int,int,char*);
int reaper( Service*, int pid, int status);
int	check_free();
int	shutdown_reaper( Service*, int pid, int status ); 

void
usage( char* MyName)
{
	fprintf( stderr, "Usage: %s [option]\n", MyName );
	fprintf( stderr, "  where [option] is one of:\n" );
	fprintf( stderr, 
			 "     [-skip-benchmarks]\t(don't run initial benchmarks)\n" );
	DC_Exit( 1 );
}


int
main_init( int, char* argv[] )
{
	int		skip_benchmarks = FALSE;
	char*	tmp = NULL;
	char**	ptr; 

		// Process command line args.
	for(ptr = argv + 1; *ptr; ptr++) {
		if(ptr[0][0] != '-') {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
		case 's':
			skip_benchmarks = TRUE;
			break;
		case 'n':
			ptr++;
			if( !(ptr && *ptr) ) {
                EXCEPT( "-n requires another arugment" );
            }
            Name = build_valid_daemon_name( *ptr );
            dprintf( D_ALWAYS, "Using name: %s\n", Name );
            break;
		default:
			fprintf( stderr, "Error:  Unknown option %s\n", *ptr );
			usage( argv[0] );
		}
	}

		// Seed the random number generator for capability generation.
	set_seed( 0 );

		// Record the time we started up for use in determining
		// keyboard idle time on SMP machines, etc.
	startd_startup = time( 0 );

		// Instantiate the Resource Manager object.
	resmgr = new ResMgr;

		// Read in global parameters from the config file.
		// We do this after we instantiate the resmgr, so we can know
		// what num_cpus is, but before init_resources(), so we can
		// use polling_interval to figure out how big to make each
		// Resource's LoadQueue object.
	init_params(1);		// The 1 indicates that this is the first time

#if defined( OSF1 ) || defined (IRIX62) || defined(WIN32)
		// Pretend we just got an X event so we think our console idle
		// is something, even if we haven't heard from the kbdd yet.
		// We do this on Win32 as well, since Win32 uses last_x_event
		// variable in a similar fasion to the X11 condor_kbdd, and
		// thus it must be initialized.
	command_x_event( 0, 0, 0 );
#endif

		// Do a little sanity checking and cleanup
	check_perms();
	cleanup_execute_dir( 0 );	// 0 indicates we should clean everything.

		// Instantiate Resource objects in the ResMgr
	resmgr->init_resources();

	resmgr->init_socks();

		// Compute all attributes
	resmgr->compute( A_ALL );

	if( (tmp = param("RunBenchmarks")) ) {
		if( (!skip_benchmarks) &&
			(*tmp != 'F' && *tmp != 'f') ) {
			// There's an expression defined to have us periodically
			// run benchmarks, so run them once here at the start.
			// Added check so if people want no benchmarks at all,
			// they just comment RunBenchmarks out of their config
			// file, or set it to "False". -Derek Wright 10/20/98
			dprintf( D_ALWAYS, "About to run initial benchmarks.\n" ); 
			resmgr->force_benchmark();
			dprintf( D_ALWAYS, "Completed initial benchmarks.\n" );
		}
		free( tmp );
	}

	resmgr->walk( &(Resource::init_classad) );

	// Startup Cron
	Cronmgr = new StartdCronMgr( );
	Cronmgr->Reconfig( );

		// Now that we have our classads, we can compute things that
		// need to be evaluated
	resmgr->walk( &(Resource::compute), A_EVALUATED );
	resmgr->walk( &(Resource::refresh_classad), A_PUBLIC | A_EVALUATED ); 

		// If we EXCEPT, don't leave any starters lying around.
	_EXCEPT_Cleanup = do_cleanup;

		// register daemoncore stuff

		//////////////////////////////////////////////////
		// Commands
		//////////////////////////////////////////////////

		// These commands all read the capability off the wire, find
		// the resource with that capability, and call appropriate
		// action on that resource.  Plus, all of these commands only
		// make sense when we're in the claimed state.  So, we can
		// handle them all with a common handler.  For all of them,
		// you need WRITE permission.
	daemonCore->Register_Command( ALIVE, "ALIVE", 
								  (CommandHandler)command_handler,
								  "command_handler", 0, DAEMON,
								  D_FULLDEBUG ); 
	daemonCore->Register_Command( RELEASE_CLAIM, "RELEASE_CLAIM", 
								  (CommandHandler)command_handler,
								  "command_handler", 0, DAEMON );
	daemonCore->Register_Command( DEACTIVATE_CLAIM,
								  "DEACTIVATE_CLAIM",  
								  (CommandHandler)command_handler,
								  "command_handler", 0, DAEMON );
	daemonCore->Register_Command( DEACTIVATE_CLAIM_FORCIBLY, 
								  "DEACTIVATE_CLAIM_FORCIBLY", 
								  (CommandHandler)command_handler,
								  "command_handler", 0, DAEMON );
	daemonCore->Register_Command( PCKPT_FRGN_JOB, "PCKPT_FRGN_JOB", 
								  (CommandHandler)command_handler,
								  "command_handler", 0, DAEMON );
	daemonCore->Register_Command( REQ_NEW_PROC, "REQ_NEW_PROC", 
								  (CommandHandler)command_handler,
								  "command_handler", 0, DAEMON );

		// These commands are special and need their own handlers
		// READ permission commands
	daemonCore->Register_Command( GIVE_STATE,
								  "GIVE_STATE",
								  (CommandHandler)command_give_state,
								  "command_give_state", 0, READ );
	daemonCore->Register_Command( GIVE_TOTALS_CLASSAD,
								  "GIVE_TOTALS_CLASSAD",
								  (CommandHandler)command_give_totals_classad,
								  "command_give_totals_classad", 0, READ );
	daemonCore->Register_Command( GIVE_REQUEST_AD, "GIVE_REQUEST_AD",
								  (CommandHandler)command_give_request_ad,
								  "command_give_request_ad", 0, READ );
	daemonCore->Register_Command( QUERY_STARTD_ADS, "QUERY_STARTD_ADS",
								  (CommandHandler)command_query_ads,
								  "command_query_ads", 0, READ );

		// WRITE permission commands
	daemonCore->Register_Command( ACTIVATE_CLAIM, "ACTIVATE_CLAIM",
								  (CommandHandler)command_activate_claim,
								  "command_activate_claim", 0, DAEMON );
	daemonCore->Register_Command( REQUEST_CLAIM, "REQUEST_CLAIM", 
								  (CommandHandler)command_request_claim,
								  "command_request_claim", 0, DAEMON );
	daemonCore->Register_Command( X_EVENT_NOTIFICATION,
								  "X_EVENT_NOTIFICATION",
								  (CommandHandler)command_x_event,
								  "command_x_event", 0, DAEMON,
								  D_FULLDEBUG ); 
	daemonCore->Register_Command( PCKPT_ALL_JOBS, "PCKPT_ALL_JOBS", 
								  (CommandHandler)command_pckpt_all,
								  "command_pckpt_all", 0, DAEMON );
	daemonCore->Register_Command( PCKPT_JOB, "PCKPT_JOB", 
								  (CommandHandler)command_name_handler,
								  "command_name_handler", 0, DAEMON );

		// OWNER permission commands
	daemonCore->Register_Command( VACATE_ALL_CLAIMS,
								  "VACATE_ALL_CLAIMS",
								  (CommandHandler)command_vacate_all,
								  "command_vacate_all", 0, OWNER );
	daemonCore->Register_Command( VACATE_ALL_FAST,
								  "VACATE_ALL_FAST",
								  (CommandHandler)command_vacate_all,
								  "command_vacate_all", 0, OWNER );
	daemonCore->Register_Command( VACATE_CLAIM,
								  "VACATE_CLAIM",
								  (CommandHandler)command_name_handler,
								  "command_name_handler", 0, OWNER );
	daemonCore->Register_Command( VACATE_CLAIM_FAST,
								  "VACATE_CLAIM_FAST",
								  (CommandHandler)command_name_handler,
								  "command_name_handler", 0, OWNER );

		// NEGOTIATOR permission commands
	daemonCore->Register_Command( MATCH_INFO, "MATCH_INFO",
								  (CommandHandler)command_match_info,
								  "command_match_info", 0, NEGOTIATOR );

		//////////////////////////////////////////////////
		// Reapers 
		//////////////////////////////////////////////////
	main_reaper = daemonCore->Register_Reaper( "reaper_starters", 
		(ReaperHandler)reaper, "reaper" );
	assert(main_reaper != FALSE);

	daemonCore->Set_Default_Reaper( main_reaper );

#if defined( OSF1 ) || defined (IRIX) || defined(WIN32)
		// Pretend we just got an X event so we think our console idle
		// is something, even if we haven't heard from the kbdd yet.
		// We do this on Win32 as well, since Win32 uses last_x_event
		// variable in a similar fasion to the X11 condor_kbdd, and
		// thus it must be initialized.
	command_x_event( 0, 0, 0 );
#endif

	resmgr->start_update_timer();

		// Evaluate the state of all resources and update CM 
		// We don't just call eval_and_update_all() b/c we don't need
		// to recompute anything.
	resmgr->first_eval_and_update_all();

	return TRUE;
}


int
main_config( bool is_full )
{
	bool done_allocating;

		// Stash old interval so we know if it's changed.
	old_polling_interval = polling_interval;
		// Reread config file for global settings.
	init_params(0);
		// Process any changes in the VM type specifications
	done_allocating = resmgr->reconfig_resources();
	if( done_allocating ) {
		return finish_main_config();
	}
	return TRUE;
}


int
finish_main_config( void ) 
{
		// Recompute machine-wide attributes object.
	resmgr->compute( A_ALL );
		// Rebuild ads for each resource.  
	resmgr->walk( &(Resource::init_classad) );  
		// Reset various settings in the ResMgr.
	resmgr->init_socks();
	resmgr->reset_timers();
	if( old_polling_interval != polling_interval ) {
		resmgr->walk( &(Resource::resize_load_queue) );
	}
	dprintf( D_FULLDEBUG, "MainConfig finish\n" );
	Cronmgr->Reconfig( );

		// Re-evaluate and update the CM for each resource (again, we
		// don't need to recompute, since we just did that, so we call
		// the special case version).
	resmgr->first_eval_and_update_all();
	return TRUE;
}


int
init_params( int first_time)
{
	char *tmp;
    Daemon *new_collector = NULL;

	resmgr->init_config_classad();

	if( exec_path ) {
		free( exec_path );
	}
	exec_path = param( "EXECUTE" );
	if( exec_path == NULL ) {
		EXCEPT( "No Execute file specified in config file" );
	}

#if defined(WIN32)
	int i;
	// switch delimiter char in exec path on WIN32
	for (i=0; exec_path[i]; i++) {
		if (exec_path[i] == '/') {
			exec_path[i] = '\\';
		}
	}
#endif

	if( Collector ) {
			// See if we changed collectors.  If so, invalidate our
			// ads at the old one, and start using the new info.
		new_collector = new Daemon( DT_COLLECTOR );
		if( strcmp(new_collector->addr(), 
				   Collector->addr()) != MATCH ) {
				// The addresses are different, so we must really have
				// a new collector...
			resmgr->final_update();
			delete Collector;
			Collector = new_collector;
		} else {
				// They're the same, just flush the new object.
			delete new_collector;
		}
	} else {
			// No Collector yet, there's nothing to do except create a
			// new object.
		Collector = new Daemon( DT_COLLECTOR );
	}


	if( condor_view_host ) {
		free( condor_view_host );
	}
	condor_view_host = param( "CONDOR_VIEW_HOST" );

    if (View_Collector) {
        delete View_Collector;
    }
    if (condor_view_host) {
        View_Collector = new Daemon(condor_view_host, CONDOR_VIEW_PORT);
    }

	tmp = param( "POLLING_INTERVAL" );
	if( tmp == NULL ) {
		polling_interval = 5;
	} else {
		polling_interval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "UPDATE_INTERVAL" );
	if( tmp == NULL ) {
		update_interval = 300;
	} else {
		update_interval = atoi( tmp );
		free( tmp );
	}

	tmp = param( "STARTD_DEBUG" );
	if( tmp == NULL ) {
		EXCEPT( "STARTD_DEBUG not defined in config file" );
	}
	free( tmp );

	if( accountant_host ) {
		free( accountant_host );
	}
	accountant_host = param("ACCOUNTANT_HOST");

	tmp = param( "MATCH_TIMEOUT" );
	if( !tmp ) {
		match_timeout = 120;
	} else {
		match_timeout = atoi( tmp );
		free( tmp );
	}

	tmp = param( "KILLING_TIMEOUT" );
	if( !tmp ) {
		killing_timeout = 30;
	} else {
		killing_timeout = atoi( tmp );
		free( tmp );
	}

	sysapi_reconfig();

	if( startd_job_exprs ) {
		delete( startd_job_exprs );
		startd_job_exprs = NULL;
	}
	tmp = param( "STARTD_JOB_EXPRS" );
	if( tmp ) {
		startd_job_exprs = new StringList();
		startd_job_exprs->initializeFromString( tmp );
		free( tmp );
	}

	tmp = param( "VIRTUAL_MACHINES_CONNECTED_TO_CONSOLE" );
	if( !tmp ) {
		tmp = param( "CONSOLE_VMS" );
	}
	if( !tmp ) {
		tmp = param( "CONSOLE_CPUS" );
	}
	if( !tmp ) {
		console_vms = resmgr->m_attr->num_cpus();
	} else {
		console_vms = atoi( tmp );
		free( tmp );
	}

	tmp = param( "VIRTUAL_MACHINES_CONNECTED_TO_KEYBOARD" );
	if( !tmp ) {
		tmp = param( "KEYBOARD_VMS" );
	}
	if( !tmp ) {
		tmp = param( "KEYBOARD_CPUS" );
	}
	if( !tmp ) {
		keyboard_vms = 1;
	} else {
		keyboard_vms = atoi( tmp );
		free( tmp );
	}

	tmp = param( "DISCONNECTED_KEYBOARD_IDLE_BOOST" );
	if( !tmp ) {
		disconnected_keyboard_boost = 1200;
	} else {
		disconnected_keyboard_boost = atoi( tmp );
		free( tmp );
	}

	startd_noclaim_shutdown = 0;
	tmp = param( "STARTD_NOCLAIM_SHUTDOWN" );
	if( tmp ) {
		startd_noclaim_shutdown = atoi( tmp );
		free( tmp );
	}

	compute_avail_stats = false;
	tmp = param( "STARTD_COMPUTE_AVAIL_STATS" );
	if( tmp ) {
		if( tmp[0] == 'T' || tmp[0] == 't' ) {
			compute_avail_stats = true;
		}
		free( tmp );
	}

	tmp = param( "STARTD_NAME" );
	if( tmp ) {
		if( Name ) {
			delete [] Name;
		}
		Name = build_valid_daemon_name( tmp );
		dprintf( D_FULLDEBUG, "Using %s for name\n", Name );
		free( tmp );
	}

	tmp = param( "PID_SNAPSHOT_INTERVAL" );
	if( tmp ) {
		pid_snapshot_interval = atoi( tmp );
		free( tmp );
	}

	return TRUE;
}


void
startd_exit() 
{
	if ( resmgr ) {
		dprintf( D_FULLDEBUG, "About to send final update to the central manager\n" );	
		resmgr->final_update();
		if ( resmgr->m_attr ) {
			resmgr->m_attr->final_idle_dprintf();
		}
		delete resmgr;
		resmgr = NULL;
	}

	dprintf( D_ALWAYS, "All resources are free, exiting.\n" );
#ifdef WIN32
	KBShutdown();
#endif
	DC_Exit(0);
}

int
main_shutdown_fast()
{
		// If the machine is free, we can just exit right away.
	check_free();

		// Remember that we're in shutdown-mode so we will refuse
		// various commands. 
	resmgr->markShutdown();

	daemonCore->Reset_Reaper( main_reaper, "shutdown_reaper", 
								 (ReaperHandler)shutdown_reaper,
								 "shutdown_reaper" );

		// Quickly kill all the starters that are running
	resmgr->walk( &(Resource::kill_claim) );

	daemonCore->Register_Timer( 0, 5, 
								(TimerHandler)check_free,
								 "check_free" );
	return TRUE;
}


int
main_shutdown_graceful()
{
		// If the machine is free, we can just exit right away.
	check_free();

		// Remember that we're in shutdown-mode so we will refuse
		// various commands. 
	resmgr->markShutdown();

	daemonCore->Reset_Reaper( main_reaper, "shutdown_reaper", 
								 (ReaperHandler)shutdown_reaper,
								 "shutdown_reaper" );

		// Release all claims, active or not
	resmgr->walk( &(Resource::release_claim) );

	daemonCore->Register_Timer( 0, 5, 
								(TimerHandler)check_free,
								 "check_free" );
	return TRUE;
}


int
reaper(Service *, int pid, int status)
{
	Resource* rip;

	if( WIFSIGNALED(status) ) {
		dprintf(D_FAILURE|D_ALWAYS, "starter pid %d died on signal %d (%s)\n",
				pid, WTERMSIG(status), daemonCore->GetExceptionString(status));
	} else {
		dprintf(D_FAILURE|D_ALWAYS, "starter pid %d exited with status %d\n",
				pid, WEXITSTATUS(status));
	}
	rip = resmgr->get_by_pid(pid);
	if( rip ) {
		rip->starter_exited();
	}		
	return TRUE;
}


int
shutdown_reaper(Service *, int pid, int status)
{
	reaper(NULL,pid,status);
	check_free();
	return TRUE;
}


int
do_cleanup(int,int,char*)
{
	static int already_excepted = FALSE;

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
			// If the machine is already free, we can exit right away.
		check_free();		
			// Otherwise, quickly kill all the active starters.
		resmgr->walk( &(Resource::kill_claim) );
		dprintf( D_FAILURE|D_ALWAYS, "startd exiting because of fatal exception.\n" );
	}

#ifdef WIN32
	// Detach our keyboard hook
	KBShutdown();
#endif

	return TRUE;
}


int
check_free()
{	
	if ( ! resmgr ) {
		startd_exit();
	}
	if( ! resmgr->in_use() ) {
		startd_exit();
	} 
	return TRUE;
}
