/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "subsystem_info.h"

/*
 * Main routine and function for the startd.
 */
#define _STARTD_NO_DECLARE_GLOBALS 1
#include "startd.h"
#include "vm_common.h"
#include "VMManager.h"
#include "VMRegister.h"
#include "classadHistory.h"
#include "misc_utils.h"
#include "slot_builder.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "StartdPlugin.h"
#endif
#if defined(WIN32)
extern int load_startd_mgmt(void);
#endif
#endif

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
int	update_offset = 0;		// Interval offset to update CM

// String Lists
StringList *startd_job_exprs = NULL;
StringList *startd_slot_attrs = NULL;
static StringList *valid_cod_users = NULL; 

// Hosts
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

int		console_slots = 0;	// # of nodes in an SMP that care about
int		keyboard_slots = 0;  //   console and keyboard activity
int		disconnected_keyboard_boost;	// # of seconds before when we
	// started up that we advertise as the last key press for
	// resources that aren't connected to anything.  

int		startd_noclaim_shutdown = 0;	
    // # of seconds we can go without being claimed before we "pull
    // the plug" and tell the master to shutdown.

bool	compute_avail_stats = false;
	// should the startd compute slot availability statistics; currently 
	// false by default

char* Name = NULL;

#define DEFAULT_PID_SNAPSHOT_INTERVAL 15
int		pid_snapshot_interval = DEFAULT_PID_SNAPSHOT_INTERVAL;
    // How often do we take snapshots of the pid families? 

int main_reaper = 0;

// Cron stuff
StartdCronJobMgr	*cron_job_mgr;

// Benchmark stuff
StartdBenchJobMgr	*bench_job_mgr;

/*
 * Prototypes of static functions.
 */

void usage( char* );
void main_init( int argc, char* argv[] );
int init_params(int);
void main_config();
void finish_main_config();
void main_shutdown_fast();
void main_shutdown_graceful();
extern "C" int do_cleanup(int,int,const char*);
int reaper( Service*, int pid, int status);
int	shutdown_reaper( Service*, int pid, int status ); 

void
usage( char* MyName)
{
	fprintf( stderr, "Usage: %s [option]\n", MyName );
	fprintf( stderr, "  where [option] is one of:\n" );
	fprintf( stderr, 
			 "     [-skip-benchmarks]\t(now a no-op)\n" );
	DC_Exit( 1 );
}

void
main_init( int, char* argv[] )
{
	char**	ptr; 

	// Reset the cron & benchmark managers to a known state
	cron_job_mgr = NULL;
	bench_job_mgr = NULL;

		// Process command line args.
	for(ptr = argv + 1; *ptr; ptr++) {
		if(ptr[0][0] != '-') {
			usage( argv[0] );
		}
		switch( ptr[0][1] ) {
		case 's':
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

	ClassAd tmp_classad;
	MyString starter_ability_list;
	resmgr->starter_mgr.publish(&tmp_classad, A_STATIC | A_PUBLIC);
	tmp_classad.LookupString(ATTR_STARTER_ABILITY_LIST, starter_ability_list);
	if( starter_ability_list.find(ATTR_HAS_VM) >= 0 ) {
		// Now starter has codes for vm universe.
		resmgr->m_vmuniverse_mgr.setStarterAbility(true);
		// check whether vm universe is available through vmgahp server
		resmgr->m_vmuniverse_mgr.checkVMUniverse();
	}

		// Read in global parameters from the config file.
		// We do this after we instantiate the resmgr, so we can know
		// what num_cpus is, but before init_resources(), so we can
		// use polling_interval to figure out how big to make each
		// Resource's LoadQueue object.
	init_params(1);		// The 1 indicates that this is the first time

#if defined(WIN32)
		// We do this on Win32 since Win32 uses last_x_event
		// variable in a similar fasion to the X11 condor_kbdd, and
		// thus it must be initialized.
	command_x_event( 0, 0, 0 );
#endif

		// Instantiate Resource objects in the ResMgr
	resmgr->init_resources();

		// Do a little sanity checking and cleanup
	StringList execute_dirs;
	resmgr->FillExecuteDirsList( &execute_dirs );
	check_execute_dir_perms( execute_dirs );
	cleanup_execute_dirs( execute_dirs );

		// Compute all attributes
	resmgr->compute( A_ALL );

	resmgr->walk( &Resource::init_classad );

		// Startup Cron
	cron_job_mgr = new StartdCronJobMgr( );
	cron_job_mgr->Initialize( "startd" );

		// Startup benchmarking
	bench_job_mgr = new StartdBenchJobMgr( );
	bench_job_mgr->Initialize( "benchmarks" );

		// Now that we have our classads, we can compute things that
		// need to be evaluated
	resmgr->walk( &Resource::compute, A_EVALUATED );
	resmgr->walk( &Resource::refresh_classad, A_PUBLIC | A_EVALUATED ); 

		// Now that everything is computed and published, we can
		// finally put in the attrs shared across the different slots
	resmgr->walk( &Resource::refresh_classad, A_PUBLIC | A_SHARED_SLOT ); 

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
	daemonCore->Register_Command( RELEASE_CLAIM, "RELEASE_CLAIM", 
								  (CommandHandler)command_release_claim,
								  "command_release_claim", 0, DAEMON );
	daemonCore->Register_Command( SUSPEND_CLAIM, "SUSPEND_CLAIM", 
								  (CommandHandler)command_suspend_claim,
								  "command_suspend_claim", 0, DAEMON );
	daemonCore->Register_Command( CONTINUE_CLAIM, "CONTINUE_CLAIM", 
								  (CommandHandler)command_continue_claim,
								  "command_continue_claim", 0, DAEMON );	
	
	daemonCore->Register_Command( X_EVENT_NOTIFICATION,
								  "X_EVENT_NOTIFICATION",
								  (CommandHandler)command_x_event,
								  "command_x_event", 0, ALLOW,
								  D_FULLDEBUG ); 
	daemonCore->Register_Command( PCKPT_ALL_JOBS, "PCKPT_ALL_JOBS", 
								  (CommandHandler)command_pckpt_all,
								  "command_pckpt_all", 0, DAEMON );
	daemonCore->Register_Command( PCKPT_JOB, "PCKPT_JOB", 
								  (CommandHandler)command_name_handler,
								  "command_name_handler", 0, DAEMON );
#if !defined(WIN32)
	daemonCore->Register_Command( DELEGATE_GSI_CRED_STARTD, "DELEGATE_GSI_CRED_STARTD",
	                              (CommandHandler)command_delegate_gsi_cred,
	                              "command_delegate_gsi_cred", 0, DAEMON );
#endif



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
	
	// Virtual Machine commands
	if( vmapi_is_host_machine() == TRUE ) {
		daemonCore->Register_Command( VM_REGISTER,
				"VM_REGISTER",
				(CommandHandler)command_vm_register,
				"command_vm_register", 0, DAEMON,
				D_FULLDEBUG );
	}

		// Commands from starter for VM universe
	daemonCore->Register_Command( VM_UNIV_GAHP_ERROR, 
								"VM_UNIV_GAHP_ERROR",
								(CommandHandler)command_vm_universe, 
								"command_vm_universe", 0, DAEMON, 
								D_FULLDEBUG );
	daemonCore->Register_Command( VM_UNIV_VMPID, 
								"VM_UNIV_VMPID",
								(CommandHandler)command_vm_universe, 
								"command_vm_universe", 0, DAEMON, 
								D_FULLDEBUG );
	daemonCore->Register_Command( VM_UNIV_GUEST_IP, 
								"VM_UNIV_GUEST_IP",
								(CommandHandler)command_vm_universe, 
								"command_vm_universe", 0, DAEMON, 
								D_FULLDEBUG );
	daemonCore->Register_Command( VM_UNIV_GUEST_MAC, 
								"VM_UNIV_GUEST_MAC",
								(CommandHandler)command_vm_universe, 
								"command_vm_universe", 0, DAEMON, 
								D_FULLDEBUG );

	daemonCore->Register_CommandWithPayload( DRAIN_JOBS,
								  "DRAIN_JOBS",
								  (CommandHandler)command_drain_jobs,
								  "command_drain_jobs", 0, ADMINISTRATOR);
	daemonCore->Register_CommandWithPayload( CANCEL_DRAIN_JOBS,
								  "CANCEL_DRAIN_JOBS",
								  (CommandHandler)command_cancel_drain_jobs,
								  "command_cancel_drain_jobs", 0, ADMINISTRATOR);


		//////////////////////////////////////////////////
		// Reapers 
		//////////////////////////////////////////////////
	main_reaper = daemonCore->Register_Reaper( "reaper_starters", 
		(ReaperHandler)reaper, "reaper" );
	ASSERT(main_reaper != FALSE);

	daemonCore->Set_Default_Reaper( main_reaper );

#if defined(WIN32)
		// Pretend we just got an X event so we think our console idle
		// is something, even if we haven't heard from the kbdd yet.
		// We do this on Win32 since Win32 uses last_x_event
		// variable in a similar fasion to the X11 condor_kbdd, and
		// thus it must be initialized.
	command_x_event( 0, 0, 0 );
#endif

	resmgr->start_update_timer();

#if HAVE_HIBERNATION
	resmgr->updateHibernateConfiguration();
#endif /* HAVE_HIBERNATION */

		// Evaluate the state of all resources and update CM 
		// We don't just call eval_and_update_all() b/c we don't need
		// to recompute anything.
		// This is now called by a timer registered by start_update_timer()
	//resmgr->update_all();

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN)
   StartdPluginManager::Load();
#elif defined(WIN32)
	load_startd_mgmt();
#endif
   StartdPluginManager::Initialize();
#endif
}


void
main_config()
{
	bool done_allocating;

		// Reread config file for global settings.
	init_params(0);
		// Process any changes in the slot type specifications
	done_allocating = resmgr->reconfig_resources();
	if( done_allocating ) {
		finish_main_config();
	}
}


void
finish_main_config( void ) 
{
#if defined(WIN32)
	resmgr->reset_credd_test_throttle();
#endif
		// Recompute machine-wide attributes object.
	resmgr->compute( A_ALL );
		// Rebuild ads for each resource.  
	resmgr->walk( &Resource::init_classad );  
		// Reset various settings in the ResMgr.
	resmgr->reset_timers();

	dprintf( D_FULLDEBUG, "MainConfig finish\n" );
	cron_job_mgr->Reconfig(  );
	bench_job_mgr->Reconfig(  );
	resmgr->starter_mgr.init();

#if HAVE_HIBERNATION
	resmgr->updateHibernateConfiguration();
#endif /* HAVE_HIBERNATION */

		// Re-evaluate and update the CM for each resource (again, we
		// don't need to recompute, since we just did that, so we call
		// the special case version).
		// This is now called by a timer registered by reset_timers()
	//resmgr->update_all();
}


int
init_params( int /* first_time */)
{
	char *tmp;


	resmgr->init_config_classad();

	polling_interval = param_integer( "POLLING_INTERVAL", 5 );

	update_interval = param_integer( "UPDATE_INTERVAL", 300, 1 );
	update_offset = param_integer( "UPDATE_OFFSET", 0, 0 );

	if( accountant_host ) {
		free( accountant_host );
	}
	accountant_host = param("ACCOUNTANT_HOST");

	match_timeout = param_integer( "MATCH_TIMEOUT", 120 );

	killing_timeout = param_integer( "KILLING_TIMEOUT", 30 );

	max_claim_alives_missed = param_integer( "MAX_CLAIM_ALIVES_MISSED", 6 );

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

	if( startd_slot_attrs ) {
		delete( startd_slot_attrs );
		startd_slot_attrs = NULL;
	}
	tmp = param( "STARTD_SLOT_ATTRS" );
	if (!tmp) {
		tmp = param( "STARTD_SLOT_EXPRS" );
	}
	if (param_boolean("ALLOW_VM_CRUFT", false) && !tmp) {
		tmp = param( "STARTD_VM_ATTRS" );
		if (!tmp) {
			tmp = param( "STARTD_VM_EXPRS" );
		}
	}
	if( tmp ) {
		startd_slot_attrs = new StringList();
		startd_slot_attrs->initializeFromString( tmp );
		free( tmp );
	}

	console_slots = param_integer( "SLOTS_CONNECTED_TO_CONSOLE",
                    param_integer( "VIRTUAL_MACHINES_CONNECTED_TO_CONSOLE",
                    param_integer( "CONSOLE_VMS",
                    param_integer( "CONSOLE_CPUS",
                    resmgr->m_attr->num_cpus()))));

	keyboard_slots = param_integer( "SLOTS_CONNECTED_TO_KEYBOARD",
                     param_integer( "VIRTUAL_MACHINES_CONNECTED_TO_KEYBOARD",
                     param_integer( "KEYBOARD_VMS",
                     param_integer( "KEYBOARD_CPUS", 1))));

	disconnected_keyboard_boost = param_integer( "DISCONNECTED_KEYBOARD_IDLE_BOOST", 1200 );

	startd_noclaim_shutdown = param_integer( "STARTD_NOCLAIM_SHUTDOWN", 0 );

	compute_avail_stats = false;
	compute_avail_stats = param_boolean( "STARTD_COMPUTE_AVAIL_STATS", false );

	tmp = param( "STARTD_NAME" );
	if( tmp ) {
		if( Name ) {
			delete [] Name;
		}
		Name = build_valid_daemon_name( tmp );
		dprintf( D_FULLDEBUG, "Using %s for name\n", Name );
		free( tmp );
	}

	pid_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", DEFAULT_PID_SNAPSHOT_INTERVAL );

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

	if( vmapi_is_virtual_machine() == TRUE ) {
		vmapi_destroy_vmregister();
	}
	tmp = param( "VMP_HOST_MACHINE" );
	if( tmp ) {
		if( vmapi_is_my_machine(tmp) ) {
			dprintf( D_ALWAYS, "WARNING: VMP_HOST_MACHINE should be the hostname of host machine. In host machine, it doesn't need to be defined\n");
		} else {
			vmapi_create_vmregister(tmp);
		}
		free(tmp);
	}

	if( vmapi_is_host_machine() == TRUE ) {
		vmapi_destroy_vmmanager();
	}
	tmp = param( "VMP_VM_LIST" );
	if( tmp ) {
		if( vmapi_is_virtual_machine() == TRUE ) {
			dprintf( D_ALWAYS, "WARNING: both VMP_HOST_MACHINE and VMP_VM_LIST are defined. Assuming this machine is a virtual machine\n");
		}else {
			vmapi_create_vmmanager(tmp);
		}
		free(tmp);
	}

	InitJobHistoryFile( "STARTD_HISTORY" , "STARTD_PER_JOB_HISTORY_DIR");

	return TRUE;
}


void PREFAST_NORETURN
startd_exit() 
{
	// Shut down the cron logic
	if( cron_job_mgr ) {
		dprintf( D_ALWAYS, "Deleting cron job manager\n" );
		cron_job_mgr->Shutdown( true );
		delete cron_job_mgr;
	}

	// Shut down the benchmark job manager
	if( bench_job_mgr ) {
		dprintf( D_ALWAYS, "Deleting benchmark job mgr\n" );
		bench_job_mgr->Shutdown( true );
		delete bench_job_mgr;
	}

	// Cleanup the resource manager
	if ( resmgr ) {
#if HAVE_HIBERNATION
		// don't want the final update, since it will overwrite 
		// our off-line ad
		if ( !resmgr->hibernating () ) {
#else
		if ( true ) {
#endif /* HAVE_HIBERNATION */
			dprintf( D_FULLDEBUG, "About to send final update to the central manager\n" );	
			resmgr->final_update();
			if ( resmgr->m_attr ) {
				resmgr->m_attr->final_idle_dprintf();
			}
		}
		
			// clean-up stale claim-id files
		int i;
		char* filename;
		for( i = 0; i <= resmgr->numSlots(); i++ ) { 
			filename = startdClaimIdFile( i );
			if (unlink(filename) < 0) {
				dprintf( D_FULLDEBUG, "startd_exit: Failed to remove file '%s'\n", filename );
			}
			free( filename );
			filename = NULL;
		}

		delete resmgr;
		resmgr = NULL;
	}

#ifdef WIN32
	systray_notifier.notifyCondorOff();
#endif

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	StartdPluginManager::Shutdown();
#endif
#endif

	dprintf( D_ALWAYS, "All resources are free, exiting.\n" );
	DC_Exit(0);
}

void
main_shutdown_fast()
{
	dprintf( D_ALWAYS, "shutdown fast\n" );

		// Shut down the cron logic
	if( cron_job_mgr ) {
		cron_job_mgr->Shutdown( true );
	}

		// Shut down the benchmark logic
	if( bench_job_mgr ) {
		bench_job_mgr->Shutdown( true );
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
								startd_check_free,
								 "startd_check_free" );
}


void
main_shutdown_graceful()
{
	dprintf( D_ALWAYS, "shutdown graceful\n" );

		// Shut down the cron logic
	if( cron_job_mgr ) {
		cron_job_mgr->Shutdown( false );
	}

		// Shut down the benchmark logic
	if( bench_job_mgr ) {
		bench_job_mgr->Shutdown( false );
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
								startd_check_free,
								 "startd_check_free" );
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

	// Adjust info for vm universe
	resmgr->m_vmuniverse_mgr.freeVM(pid);

	foo = resmgr->getClaimByPid(pid);
	if( foo ) {
		foo->starterExited(status);
	} else {
		dprintf(D_FAILURE|D_ALWAYS, "Warning: Starter pid %d is not associated with an claim. A slot may fail to transition to Idle.\n", pid);
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
do_cleanup(int,int,const char*)
{
	static int already_excepted = FALSE;

	if ( already_excepted == FALSE ) {
		already_excepted = TRUE;
			// If the machine is already free, we can exit right away.
		startd_check_free();		
			// Otherwise, quickly kill all the active starters.
		resmgr->walk( &Resource::void_kill_claim );
		dprintf( D_FAILURE|D_ALWAYS, "startd exiting because of fatal exception.\n" );
	}

	return TRUE;
}


void
startd_check_free()
{	
	if ( cron_job_mgr && ( ! cron_job_mgr->ShutdownOk() ) ) {
		return;
	}
	if ( bench_job_mgr && ( ! bench_job_mgr->ShutdownOk() ) ) {
		return;
	}
	if ( ! resmgr ) {
		startd_exit();
	}
	if( ! resmgr->hasAnyClaim() ) {
		startd_exit();
	}
	return;
}


int
main( int argc, char **argv )
{
	set_mySubSystem( "STARTD", SUBSYSTEM_TYPE_STARTD );

	dc_main_init = main_init;
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	return dc_main( argc, argv );
}


bool
authorizedForCOD( const char* owner )
{
	if( ! valid_cod_users ) {
		return false;
	}
	return valid_cod_users->contains( owner );
}
