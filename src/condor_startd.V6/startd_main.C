/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_parameters.h"
/*
 * Main routine and function for the startd.
 */
#define _STARTD_NO_DECLARE_GLOBALS 1
#include "startd.h"
#include "startd_cronmgr.h"

// Define global variables

// windows-specific: notifier for the condor "birdwatcher" (system tray icon)
#ifdef WIN32
CondorSystrayNotifier systray_notifier;
#endif

// Resource manager
ResMgr*	resmgr;			// Pointer to the resource manager object

// Polling variables
int	polling_interval = 0;	// Interval for polling when there are resources in use
int	update_interval = 0;	// Interval to update CM

// Paths
char*	exec_path = NULL;

// String Lists
StringList *startd_job_exprs = NULL;
StringList *startd_vm_exprs = NULL;
static StringList *valid_cod_users = NULL; 

// Hosts
DCCollector*	Collector = NULL;
char*	accountant_host = NULL;

// Others
int		match_timeout;		// How long you're willing to be
							// matched before claimed 
int		killing_timeout;	// How long you're willing to be in
							// preempting/killing before you drop the
							// hammer on the starter
int		max_claim_alives_missed;  // how many keepalives can we miss
								  // until we timeout the claim
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

	// Reset the cron manager to a known state
	Cronmgr = NULL;

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

		// Record the time we started up for use in determining
		// keyboard idle time on SMP machines, etc.
	startd_startup = time( 0 );

		// Instantiate the Resource Manager object.
	resmgr = new ResMgr;

		// find all the starters we care about and get their classads. 
	resmgr->starter_mgr.init();

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

	resmgr->walk( &Resource::init_classad );

	// Startup Cron
	Cronmgr = new StartdCronMgr( );
	Cronmgr->Initialize( );

		// Now that we have our classads, we can compute things that
		// need to be evaluated
	resmgr->walk( &Resource::compute, A_EVALUATED );
	resmgr->walk( &Resource::refresh_classad, A_PUBLIC | A_EVALUATED ); 

		// Now that everything is computed and published, we can
		// finally put in the attrs shared across the different VMs
	resmgr->walk( &Resource::refresh_classad, A_PUBLIC | A_SHARED_VM ); 

		// If we EXCEPT, don't leave any starters lying around.
	_EXCEPT_Cleanup = do_cleanup;

		// register daemoncore stuff

		//////////////////////////////////////////////////
		// Commands
		//////////////////////////////////////////////////

		// These commands all read the ClaimId off the wire, find
		// the resource with that ClaimId, and call appropriate
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

		// the ClassAd-only command
	daemonCore->Register_Command( CA_AUTH_CMD, "CA_AUTH_CMD",
								  (CommandHandler)command_classad_handler,
								  "command_classad_handler", 0, WRITE );
	daemonCore->Register_Command( CA_CMD, "CA_CMD",
								  (CommandHandler)command_classad_handler,
								  "command_classad_handler", 0, WRITE );

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
	resmgr->walk( &Resource::init_classad );  
		// Reset various settings in the ResMgr.
	resmgr->reset_timers();

	dprintf( D_FULLDEBUG, "MainConfig finish\n" );
	Cronmgr->Reconfig(  );
	resmgr->starter_mgr.init();

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
    DCCollector* new_collector = NULL;

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
		new_collector = new DCCollector;
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
		Collector = new DCCollector;
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

	tmp = param( "MAX_CLAIM_ALIVES_MISSED" );
	if( !tmp ) {
		max_claim_alives_missed = 6;
	} else {
		max_claim_alives_missed = atoi( tmp );
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
	} else {
		startd_job_exprs = new StringList();
		startd_job_exprs->initializeFromString( ATTR_JOB_UNIVERSE );
	}

	if( startd_vm_exprs ) {
		delete( startd_vm_exprs );
		startd_vm_exprs = NULL;
	}
	tmp = param( "STARTD_VM_EXPRS" );
	if( tmp ) {
		startd_vm_exprs = new StringList();
		startd_vm_exprs->initializeFromString( tmp );
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

	if( valid_cod_users ) {
		delete( valid_cod_users );
		valid_cod_users = NULL;
	}
	tmp = param( "VALID_COD_USERS" );
	if( tmp ) {
		valid_cod_users = new StringList();
		valid_cod_users->initializeFromString( tmp );
		free( tmp );
	}

	return TRUE;
}


void
startd_exit() 
{
	// Shut down the cron logic
	if( Cronmgr ) {
		dprintf( D_ALWAYS, "Deleting Cronmgr\n" );
		Cronmgr->Shutdown( true );
		delete Cronmgr;
	}

	// Cleanup the resource manager
	if ( resmgr ) {
		dprintf( D_FULLDEBUG, "About to send final update to the central manager\n" );	
		resmgr->final_update();
		if ( resmgr->m_attr ) {
			resmgr->m_attr->final_idle_dprintf();
		}
		delete resmgr;
		resmgr = NULL;
	}

#ifdef WIN32
	systray_notifier.notifyCondorOff();
#endif

	dprintf( D_ALWAYS, "All resources are free, exiting.\n" );
	DC_Exit(0);
}

int
main_shutdown_fast()
{
	dprintf( D_ALWAYS, "shutdown fast\n" );

	// Shut down the cron logic
	if( Cronmgr ) {
		Cronmgr->Shutdown( true );
	}

		// If the machine is free, we can just exit right away.
	startd_check_free();

		// Remember that we're in shutdown-mode so we will refuse
		// various commands. 
	resmgr->markShutdown();

	daemonCore->Reset_Reaper( main_reaper, "shutdown_reaper", 
								 (ReaperHandler)shutdown_reaper,
								 "shutdown_reaper" );

		// Quickly kill all the starters that are running
	resmgr->walk( &Resource::killAllClaims );

	daemonCore->Register_Timer( 0, 5, 
								(TimerHandler)startd_check_free,
								 "startd_check_free" );
	return TRUE;
}


int
main_shutdown_graceful()
{
	dprintf( D_ALWAYS, "shutdown graceful\n" );

	// Shut down the cron logic
	if( Cronmgr ) {
		Cronmgr->Shutdown( false );
	}

		// If the machine is free, we can just exit right away.
	startd_check_free();

		// Remember that we're in shutdown-mode so we will refuse
		// various commands. 
	resmgr->markShutdown();

	daemonCore->Reset_Reaper( main_reaper, "shutdown_reaper", 
								 (ReaperHandler)shutdown_reaper,
								 "shutdown_reaper" );

		// Release all claims, active or not
	resmgr->walk( &Resource::releaseAllClaims );

	daemonCore->Register_Timer( 0, 5, 
								(TimerHandler)startd_check_free,
								 "startd_check_free" );
	return TRUE;
}


int
reaper(Service *, int pid, int status)
{
	Claim* foo;

	if( WIFSIGNALED(status) ) {
		dprintf(D_FAILURE|D_ALWAYS, "Starter pid %d died on signal %d (%s)\n",
				pid, WTERMSIG(status), daemonCore->GetExceptionString(status));
	} else {
		dprintf(D_FAILURE|D_ALWAYS, "Starter pid %d exited with status %d\n",
				pid, WEXITSTATUS(status));
	}
	foo = resmgr->getClaimByPid(pid);
	if( foo ) {
		foo->starterExited();
	}		
	return TRUE;
}


int
shutdown_reaper(Service *, int pid, int status)
{
	reaper(NULL,pid,status);
	startd_check_free();
	return TRUE;
}


int
do_cleanup(int,int,char*)
{
	static int already_excepted = FALSE;

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
			// If the machine is already free, we can exit right away.
		startd_check_free();		
			// Otherwise, quickly kill all the active starters.
		resmgr->walk( &Resource::kill_claim );
		dprintf( D_FAILURE|D_ALWAYS, "startd exiting because of fatal exception.\n" );
	}

	return TRUE;
}


int
startd_check_free()
{	
	if ( Cronmgr && ( ! Cronmgr->ShutdownOk() ) ) {
		return FALSE;
	}
	if ( ! resmgr ) {
		startd_exit();
	}
	if( ! resmgr->hasAnyClaim() ) {
		startd_exit();
	}
	return TRUE;
}


void
main_pre_dc_init( int argc, char* argv[] )
{
}


void
main_pre_command_sock_init( )
{
}


bool
authorizedForCOD( const char* owner )
{
	if( ! valid_cod_users ) {
		return false;
	}
	return valid_cod_users->contains( owner );
}
