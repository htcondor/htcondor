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
#include "classadHistory.h"
#include "misc_utils.h"
#include "slot_builder.h"
#include "history_queue.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#include "StartdPlugin.h"
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

// When this variable is:
//  0 - Advertise STARTD_OLD_ADTYPE for slots, and do not advertise STARTD_DAEMON_ADTYPE
//  1 - Advertise STARTD_SLOT_ADTYPE and STARTD_DAEMON_ADTYPE always
//  2 - Advertise STARTD_SLOT_ADTYPE and STARTD_DAEMON_ADTYPE to collectors that are 23.2 or later
//      and advertise STARTD_OLD_ADTYPE to older collectors
int enable_single_startd_daemon_ad = 0;

// String Lists
std::vector<std::string> startd_job_attrs;
std::vector<std::string> startd_slot_attrs;
static StringList *valid_cod_users = NULL; 

// Hosts
char*	accountant_host = NULL;

// Others
int		match_timeout;		// How long you're willing to be
							// matched before claimed 
int		killing_timeout;	// How long you're willing to be in
							// preempting/killing before you drop the
							// hammer on the starter
int		vm_killing_timeout;	// How long you're willing to be
							// in preempting/killing before you
							// drop the hammer on the starter for VM universe jobs
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

int		docker_cached_image_size_interval = 0; // how often we ask docker for the size of the cache, 0 means don't


char* Name = NULL;

#define DEFAULT_PID_SNAPSHOT_INTERVAL 15
int		pid_snapshot_interval = DEFAULT_PID_SNAPSHOT_INTERVAL;
    // How often do we take snapshots of the pid families? 

int main_reaper = 0;

// Cron stuff
StartdCronJobMgr	*cron_job_mgr;

// Benchmark stuff
StartdBenchJobMgr	*bench_job_mgr;

// remote history queries
HistoryHelperQueue *history_queue_mgr;

// Cleanup reminders, for things we tried to cleanup but initially failed.
// for instance, the execute directory on Windows when antivirus software is hold a file in it open.
static int cleanup_reminder_timer_id = -1;
int cleanup_reminder_timer_interval = 62; // default to doing at least some cleanup once a minute (ish)
CleanupReminderMap cleanup_reminders;
extern void register_cleanup_reminder_timer();

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
int reaper(int pid, int status);
int	shutdown_reaper(int pid, int status ); 

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
			if (Name) {
				free(Name);
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

#ifdef WIN32
	// get the Windows sysapi load average thread going early
	dprintf(D_FULLDEBUG, "starting Windows load averaging thread\n");
	sysapi_load_avg();
#endif

		// Instantiate the Resource Manager object.
	resmgr = new ResMgr;

		// find all the starters we care about and get their classads. 
	Starter::config();

	ClassAd tmp_classad;
	std::string starter_ability_list;
	Starter::publish(&tmp_classad);
	tmp_classad.LookupString(ATTR_STARTER_ABILITY_LIST, starter_ability_list);
	if( starter_ability_list.find(ATTR_HAS_VM) != std::string::npos ) {
		// Now starter has codes for vm universe.
		resmgr->m_vmuniverse_mgr.setStarterAbility(true);
		// check whether vm universe is available through vmgahp server
		resmgr->m_vmuniverse_mgr.checkVMUniverse( false );
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
	command_x_event(0, 0);
#endif

		// create the class that tracks and limits reqmote history queries
	history_queue_mgr = new HistoryHelperQueue();
	history_queue_mgr->want_startd_history(true);
	history_queue_mgr->setup(1000, param_integer("HISTORY_HELPER_MAX_CONCURRENCY", 50));

		// Instantiate Resource objects in the ResMgr
	resmgr->init_resources();

		// Do a little sanity checking and cleanup
	StringList execute_dirs;
	resmgr->FillExecuteDirsList( &execute_dirs );
	check_execute_dir_perms( execute_dirs );
	cleanup_execute_dirs( execute_dirs );

		// Compute all attributes
	resmgr->compute_static();

		// Startup Cron
	cron_job_mgr = new StartdCronJobMgr( );
	cron_job_mgr->Initialize( "startd" );

		// Startup benchmarking
	bench_job_mgr = new StartdBenchJobMgr( );
	bench_job_mgr->Initialize( "benchmarks" );

		// this does a bit of work that isn't valid yet 
	resmgr->update_cur_time();
	resmgr->initResourceAds();

	// Now that we have our classads, we can compute things that
		// need to be evaluated
	resmgr->compute_dynamic(true);

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
		// you need DAEMON permission.
	daemonCore->Register_Command( ALIVE, "ALIVE", 
								  command_handler,
								  "command_handler", DAEMON ); 
	daemonCore->Register_Command( DEACTIVATE_CLAIM,
								  "DEACTIVATE_CLAIM",  
								  command_handler,
								  "command_handler", DAEMON );
	daemonCore->Register_Command( DEACTIVATE_CLAIM_FORCIBLY, 
								  "DEACTIVATE_CLAIM_FORCIBLY", 
								  command_handler,
								  "command_handler", DAEMON );
	daemonCore->Register_Command( PCKPT_FRGN_JOB, "PCKPT_FRGN_JOB", 
								  command_handler,
								  "command_handler", DAEMON );
	daemonCore->Register_Command( REQ_NEW_PROC, "REQ_NEW_PROC", 
								  command_handler,
								  "command_handler", DAEMON );

		// These commands are special and need their own handlers
		// READ permission commands
	daemonCore->Register_Command( GIVE_STATE,
								  "GIVE_STATE",
								  command_give_state,
								  "command_give_state", READ );
	daemonCore->Register_Command( GIVE_TOTALS_CLASSAD,
								  "GIVE_TOTALS_CLASSAD",
								  command_give_totals_classad,
								  "command_give_totals_classad", READ );
	daemonCore->Register_Command( QUERY_STARTD_ADS, "QUERY_STARTD_ADS",
								  command_query_ads,
								  "command_query_ads", READ );
	if (history_queue_mgr) {
		daemonCore->Register_CommandWithPayload(GET_HISTORY,
			"GET_HISTORY",
			(CommandHandlercpp)&HistoryHelperQueue::command_handler,
			"command_get_history", history_queue_mgr, READ);
	}

		// DAEMON permission commands
	daemonCore->Register_Command( ACTIVATE_CLAIM, "ACTIVATE_CLAIM",
								  command_activate_claim,
								  "command_activate_claim", DAEMON );
	daemonCore->Register_Command( REQUEST_CLAIM, "REQUEST_CLAIM", 
								  command_request_claim,
								  "command_request_claim", DAEMON );
	daemonCore->Register_Command( RELEASE_CLAIM, "RELEASE_CLAIM", 
								  command_release_claim,
								  "command_release_claim", DAEMON );
	daemonCore->Register_Command( SUSPEND_CLAIM, "SUSPEND_CLAIM", 
								  command_suspend_claim,
								  "command_suspend_claim", DAEMON );
	daemonCore->Register_Command( CONTINUE_CLAIM, "CONTINUE_CLAIM", 
								  command_continue_claim,
								  "command_continue_claim", DAEMON );	
	
	daemonCore->Register_Command( X_EVENT_NOTIFICATION,
								  "X_EVENT_NOTIFICATION",
								  command_x_event,
								  "command_x_event", ALLOW ); 
	daemonCore->Register_Command( PCKPT_ALL_JOBS, "PCKPT_ALL_JOBS", 
								  command_pckpt_all,
								  "command_pckpt_all", DAEMON );
	daemonCore->Register_Command( PCKPT_JOB, "PCKPT_JOB", 
								  command_name_handler,
								  "command_name_handler", DAEMON );
#if !defined(WIN32)
	daemonCore->Register_Command( DELEGATE_GSI_CRED_STARTD, "DELEGATE_GSI_CRED_STARTD",
	                              command_delegate_gsi_cred,
	                              "command_delegate_gsi_cred", DAEMON );
#endif

	daemonCore->Register_Command( COALESCE_SLOTS, "COALESCE_SLOTS",
								  command_coalesce_slots,
								  "command_coalesce_slots", DAEMON );

		// ex-OWNER permission commands, now ADMINISTRATOR
	daemonCore->Register_Command( VACATE_ALL_CLAIMS,
								  "VACATE_ALL_CLAIMS",
								  command_vacate_all,
								  "command_vacate_all", ADMINISTRATOR );
	daemonCore->Register_Command( VACATE_ALL_FAST,
								  "VACATE_ALL_FAST",
								  command_vacate_all,
								  "command_vacate_all", ADMINISTRATOR );
	daemonCore->Register_Command( VACATE_CLAIM,
								  "VACATE_CLAIM",
								  command_name_handler,
								  "command_name_handler", ADMINISTRATOR );
	daemonCore->Register_Command( VACATE_CLAIM_FAST,
								  "VACATE_CLAIM_FAST",
								  command_name_handler,
								  "command_name_handler", ADMINISTRATOR );

		// NEGOTIATOR permission commands
	daemonCore->Register_Command( MATCH_INFO, "MATCH_INFO",
								  command_match_info,
								  "command_match_info", NEGOTIATOR );

		// the ClassAd-only command
	daemonCore->Register_Command( CA_AUTH_CMD, "CA_AUTH_CMD",
								  command_classad_handler,
								  "command_classad_handler", WRITE );
	daemonCore->Register_Command( CA_CMD, "CA_CMD",
								  command_classad_handler,
								  "command_classad_handler", WRITE );

		// Commands from starter for VM universe
	daemonCore->Register_Command( VM_UNIV_GAHP_ERROR, 
								"VM_UNIV_GAHP_ERROR",
								command_vm_universe, 
								"command_vm_universe", DAEMON );
	daemonCore->Register_Command( VM_UNIV_VMPID, 
								"VM_UNIV_VMPID",
								command_vm_universe, 
								"command_vm_universe", DAEMON );
	daemonCore->Register_Command( VM_UNIV_GUEST_IP, 
								"VM_UNIV_GUEST_IP",
								command_vm_universe, 
								"command_vm_universe", DAEMON );
	daemonCore->Register_Command( VM_UNIV_GUEST_MAC, 
								"VM_UNIV_GUEST_MAC",
								command_vm_universe, 
								"command_vm_universe", DAEMON );

	daemonCore->Register_CommandWithPayload( DRAIN_JOBS,
								  "DRAIN_JOBS",
								  command_drain_jobs,
											 "command_drain_jobs", ADMINISTRATOR);
	daemonCore->Register_CommandWithPayload( CANCEL_DRAIN_JOBS,
								  "CANCEL_DRAIN_JOBS",
								  command_cancel_drain_jobs,
								  "command_cancel_drain_jobs", ADMINISTRATOR);

		//////////////////////////////////////////////////
		// Reapers 
		//////////////////////////////////////////////////
	main_reaper = daemonCore->Register_Reaper( "reaper_starters", 
		reaper, "reaper" );
	ASSERT(main_reaper != FALSE);

	daemonCore->Set_Default_Reaper( main_reaper );

#if defined(WIN32)
		// Pretend we just got an X event so we think our console idle
		// is something, even if we haven't heard from the kbdd yet.
		// We do this on Win32 since Win32 uses last_x_event
		// variable in a similar fasion to the X11 condor_kbdd, and
		// thus it must be initialized.
	command_x_event(0, 0);
#endif

	resmgr->start_sweep_timer();
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
#if defined(UNIX)
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
		// Reread config file for global settings.
	init_params(0);

		// Process any changes in the slot type specifications
		// note that reconfig_resources will mark all slots as dirty
		// and force collector update "soon"
	resmgr->reconfig_resources();
	finish_main_config();
}


void
finish_main_config( void ) 
{
#if defined(WIN32)
	resmgr->reset_credd_test_throttle();
#endif
		// Recompute machine-wide attributes object.
	resmgr->compute_static();
		// Rebuild ads for each resource.  
	resmgr->initResourceAds();
		// Reset various settings in the ResMgr.
	resmgr->reset_timers();

	dprintf( D_FULLDEBUG, "MainConfig finish\n" );
	cron_job_mgr->Reconfig(  );
	bench_job_mgr->Reconfig(  );
	Starter::config();

#if HAVE_HIBERNATION
	resmgr->updateHibernateConfiguration();
#endif /* HAVE_HIBERNATION */
}


int
init_params( int first_time)
{
	static bool match_password_enabled = false;

	if (first_time) {
		std::string func_name("SlotEval");
		classad::FunctionCall::RegisterFunction( func_name, OtherSlotEval );
	}

	resmgr->init_config_classad();

	polling_interval = param_integer( "POLLING_INTERVAL", 5 );

	update_interval = param_integer( "UPDATE_INTERVAL", 300, 1 );

	// TODO: change this default to True or Auto
	enable_single_startd_daemon_ad = 0;
	auto_free_ptr send_daemon_ad(param("ENABLE_STARTD_DAEMON_AD"));
	if (send_daemon_ad) {
		bool bval = false;
		if (string_is_boolean_param(send_daemon_ad, bval)) {
			enable_single_startd_daemon_ad = bval ? 1 : 0;
		} else if (YourStringNoCase("auto") == send_daemon_ad) {
			enable_single_startd_daemon_ad = 2;
		}
	}
	dprintf(D_STATUS, "ENABLE_STARTD_DAEMON_AD=%d (%s)\n", enable_single_startd_daemon_ad,
		send_daemon_ad.ptr() ? send_daemon_ad.ptr() : "");

	if( accountant_host ) {
		free( accountant_host );
	}
	accountant_host = param("ACCOUNTANT_HOST");

	match_timeout = param_integer( "MATCH_TIMEOUT", 120 );

	killing_timeout = param_integer( "KILLING_TIMEOUT", 30 );
	vm_killing_timeout = param_integer( "VM_KILLING_TIMEOUT", 60);
	if (vm_killing_timeout < killing_timeout) { vm_killing_timeout = killing_timeout; } // use the larger of the two for VM universe

	max_claim_alives_missed = param_integer( "MAX_CLAIM_ALIVES_MISSED", 6 );

	sysapi_reconfig();

	// Fill in *_JOB_ATTRS
	//
	startd_job_attrs.clear();
	param_and_insert_unique_items("STARTD_JOB_ATTRS", startd_job_attrs);
	// merge in the deprecated _EXPRS config
	param_and_insert_unique_items("STARTD_JOB_EXPRS", startd_job_attrs);
	// Now merge in the attrs required by HTCondor - this knob is a secret from users
	param_and_insert_unique_items("SYSTEM_STARTD_JOB_ATTRS", startd_job_attrs);

	// Fill in *_SLOT_ATTRS
	//
	startd_slot_attrs.clear();
	param_and_insert_unique_items("STARTD_SLOT_ATTRS", startd_slot_attrs);
	param_and_insert_unique_items("STARTD_SLOT_EXPRS", startd_slot_attrs);

	// now insert attributes needed by HTCondor
	param_and_insert_unique_items("SYSTEM_STARTD_SLOT_ATTRS", startd_slot_attrs);

	console_slots = param_integer( "SLOTS_CONNECTED_TO_CONSOLE", 0);
	keyboard_slots = param_integer( "SLOTS_CONNECTED_TO_KEYBOARD", 0);
	disconnected_keyboard_boost = param_integer( "DISCONNECTED_KEYBOARD_IDLE_BOOST", 1200 );

	startd_noclaim_shutdown = param_integer( "STARTD_NOCLAIM_SHUTDOWN", 0 );

	// how often we query docker for the size of the image cache, 0 is never
	docker_cached_image_size_interval = param_integer("DOCKER_CACHE_ADVERTISE_INTERVAL", 1200);

	// a 0 or negative value for the timer interval will disable cleanup reminders entirely
	cleanup_reminder_timer_interval = param_integer( "STARTD_CLEANUP_REMINDER_TIMER_INTERVAL", 62 );

	auto_free_ptr tmp(param("STARTD_NAME"));
	if (tmp) {
		if( Name ) {
			free(Name);
		}
		Name = build_valid_daemon_name(tmp);
		dprintf( D_FULLDEBUG, "Using %s for name\n", Name );
	}

	pid_snapshot_interval = param_integer( "PID_SNAPSHOT_INTERVAL", DEFAULT_PID_SNAPSHOT_INTERVAL );

	if( valid_cod_users ) {
		delete( valid_cod_users );
		valid_cod_users = NULL;
	}
	tmp.set(param("VALID_COD_USERS"));
	if (tmp) {
		valid_cod_users = new StringList();
		valid_cod_users->initializeFromString( tmp );
	}

	InitJobHistoryFile( "STARTD_HISTORY" , "STARTD_PER_JOB_HISTORY_DIR");

	bool new_match_password = param_boolean( "SEC_ENABLE_MATCH_PASSWORD_AUTHENTICATION", true );
	if ( new_match_password != match_password_enabled ) {
		IpVerify* ipv = daemonCore->getIpVerify();
		if ( new_match_password ) {
			ipv->PunchHole( DAEMON, SUBMIT_SIDE_MATCHSESSION_FQU );
			ipv->PunchHole( CLIENT_PERM, SUBMIT_SIDE_MATCHSESSION_FQU );
		} else {
			ipv->FillHole( DAEMON, SUBMIT_SIDE_MATCHSESSION_FQU );
			ipv->FillHole( CLIENT_PERM, SUBMIT_SIDE_MATCHSESSION_FQU );
		}
		match_password_enabled = new_match_password;
	}


	return TRUE;
}

// implement an exponential backoff by skipping some iterations
// the algorithm is even powers of 2 until iteration 60, then once per 'hour' for 24 hours, then once per 'day' forever
// The algorithm assumes that the iteration tick rate is once per minute.
// if it is not then 'hour' or 'day' will be faster or slower to match the tick rate.
//
static bool retry_on_this_iter(int iter, CleanupReminder::category /*cat*/)
{
	// try once a day once we the we have been at it longer than a day.
	if (iter > 60*24) return (iter % (60*24)) == 0;

	// try once an hour once we the we have been at it longer than an hour
	if (iter > 60) return (iter % 60) == 0;

	// do exponential back off for the first hour
	if ((iter & (iter-1)) == 0)
		return true;
	return false;
}

// process the cleanup reminders.
void CleanupReminderTimerCallback()
{
	dprintf(D_FULLDEBUG, "In CleanupReminderTimerCallback() there are %d reminders\n", (int)cleanup_reminders.size());

	for (auto jt = cleanup_reminders.begin(); jt != cleanup_reminders.end(); /* advance in the loop */) {
		auto it = jt++; // so we can remove the current item if we manage to clean it up
		it->second += 1; // record that we looked at this.
		bool erase_it = false; // set this to true when we succeed (or don't need to try anymore)

		const CleanupReminder & cr = it->first; // alias the CleanupReminder so that the code below is clearer

		bool retry_now = retry_on_this_iter(it->second, cr.cat);
		dprintf(D_FULLDEBUG, "cleanup_reminder %s, iter %d, retry_now = %d\n", cr.name.c_str(), it->second, retry_now);

		// if our exponential backoff says we should retry this time, attempt the cleanup.
		if (retry_now) {
			int err=0;
			switch (cr.cat) {
			case CleanupReminder::category::exec_dir:
				if (retry_cleanup_execute_dir(cr.name, cr.opt, err)) {
					dprintf(D_ALWAYS, "Retry of directory delete '%s' succeeded. removing it from the retry list\n", cr.name.c_str());
					erase_it = true;
				} else {
					dprintf(D_ALWAYS, "Retry of directory delete '%s' failed with error %d. will try again later\n", cr.name.c_str(), err);
				}
				break;
			case CleanupReminder::category::account:
				if (retry_cleanup_user_account(cr.name, cr.opt, err)) {
					dprintf(D_ALWAYS, "Retry of account cleanup for '%s' succeeded. removing it from the retry list\n", cr.name.c_str());
					erase_it = true;
				} else {
					dprintf(D_ALWAYS, "Retry of account cleanup '%s' failed with error %d. will try again later\n", cr.name.c_str(), err);
				}
				break;
			}

		}

		// if we successfully cleaned up, or cleanup is now moot, remove the item from the list.
		if (erase_it) {
			cleanup_reminders.erase(it);
		}
	}

	// if the collection of things to try and clean up is empty, turn off the timer
	// it will get turned back on the next time an item is added to the collection
	if (cleanup_reminders.empty() && cleanup_reminder_timer_id != -1) {
		daemonCore->Cancel_Timer(cleanup_reminder_timer_id);
		cleanup_reminder_timer_id = -1;
		dprintf( D_ALWAYS, "Cancelling timer for cleanup reminders - collection is empty.\n" );
	}
}

// register a timer to process the cleanup reminders, if the timer is not already registered
void register_cleanup_reminder_timer()
{
	// a timer interval of 0 or negative will disable cleanup reminders
	if (cleanup_reminder_timer_interval <= 0)
		return;
	if (cleanup_reminder_timer_id < 0) {
		dprintf( D_ALWAYS, "Starting timer for cleanup reminders.\n" );
		int id = daemonCore->Register_Timer(cleanup_reminder_timer_interval,
								cleanup_reminder_timer_interval,
								(TimerHandler)CleanupReminderTimerCallback,
								"CleanupReminderTimerCallback");
		if  (id < 0) {
			EXCEPT( "Can't register DaemonCore timer for cleanup reminders" );
		}
		cleanup_reminder_timer_id = id;
	}
}

void PREFAST_NORETURN
startd_exit() 
{
	// print resources into log.  we need to do this before we free them
	if(param_boolean("STARTD_PRINT_ADS_ON_SHUTDOWN", false)) {
		auto_free_ptr slot_types(param("STARTD_PRINT_ADS_FILTER"));
		dprintf(D_ALWAYS, "*** BEGIN AD DUMP ***\n");
		resmgr->printSlotAds(slot_types);
		dprintf(D_ALWAYS, "*** END AD DUMP ***\n");
	}

	// Shut down the cron logic
	if( cron_job_mgr ) {
		dprintf( D_FULLDEBUG, "Forcing Shutdown of cron job manager\n" );
		cron_job_mgr->Shutdown( true );
		delete cron_job_mgr;
	}

	// Shut down the benchmark job manager
	if( bench_job_mgr ) {
		dprintf( D_FULLDEBUG, "Forcing Shutdown of benchmark job mgr\n" );
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
		std::string filename;
		for( i = 0; i <= resmgr->numSlots(); i++ ) { 
			filename = startdClaimIdFile( i );
			if (unlink(filename.c_str()) < 0) {
				dprintf( D_FULLDEBUG, "startd_exit: Failed to remove file '%s'\n", filename.c_str());
			}
		}

		delete resmgr;
		resmgr = NULL;
	}

#ifdef WIN32
	systray_notifier.notifyCondorOff();
#endif

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	StartdPluginManager::Shutdown();
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
								 shutdown_reaper,
								 "shutdown_reaper" );

		// Quickly kill all the starters that are running
	resmgr->killAllClaims();

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
								 shutdown_reaper,
								 "shutdown_reaper" );

		// Release all claims, active or not
	resmgr->releaseAllClaims();

	daemonCore->Register_Timer( 0, 5, 
								startd_check_free,
								 "startd_check_free" );
}


int
reaper(int pid, int status)
{

	if( WIFSIGNALED(status) ) {
		dprintf(D_ERROR | D_EXCEPT, "Starter pid %d died on signal %d (%s)\n",
				pid, WTERMSIG(status), daemonCore->GetExceptionString(status));
	} else {
		int d_stat = status ? D_ERROR : D_ALWAYS;
		dprintf(d_stat, "Starter pid %d exited with status %d\n",
				pid, WEXITSTATUS(status));
	}

	// Adjust info for vm universe
	resmgr->m_vmuniverse_mgr.freeVM(pid);

	Starter * starter = findStarterByPid(pid);
	Claim* claim = resmgr->getClaimByPid(pid);
	if (claim) {
		// this will call the starter->exited method also
		claim->starterExited(starter, status);
	} else if (starter) {
		// claim is gone, we want to call the starter->exited method ourselves.
		starter->exited(NULL, status);
		delete starter;
	} else {
		dprintf(D_ERROR, "ERROR: Starter pid %d is not associated with a Starter object or a Claim.\n", pid);
	}
	return TRUE;
}


int
shutdown_reaper(int pid, int status)
{
	reaper(pid,status);
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
		const bool fast = true;
		resmgr->vacate_all(fast);
		dprintf( D_ERROR | D_EXCEPT, "startd exiting because of fatal exception.\n" );
	}

	return TRUE;
}


void
startd_check_free(int /* tid */)
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
	// TODO This check ignores claimed pslots, thus we won't send a
	//   RELEASE_CLAIM for those before shutting down.
	//   Today, those messages would fail, as the schedd doesn't keep
	//   track of claimed pslots. We expect this to change in the future.
	if( ! resmgr->hasAnyClaim() ) {
		startd_exit();
	}
	return;
}


int
main( int argc, char **argv )
{
	set_mySubSystem( "STARTD", true, SUBSYSTEM_TYPE_STARTD );

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
