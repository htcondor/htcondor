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
#include "condor_state.h"
#include "condor_environ.h"
#include "startd.h"
#include "ipv6_hostname.h" // for get_local_fqdn
#include "startd_hibernator.h"
#include "startd_named_classad_list.h"
#include "overflow.h"
#include "credmon_interface.h"
#include "condor_auth_passwd.h"
#include "token_utils.h"
#ifdef HAVE_DATA_REUSE_DIR
#include "data_reuse.h"
#endif
#include <algorithm>
#include "dc_schedd.h"

#include "slot_builder.h"

#include "strcasestr.h"

struct slotOrderSorter {
   bool operator()(const Resource *r1, const Resource *r2) {
		if (r1->r_id < r2->r_id) {
			return true;
		}
		if (r1->r_id > r2->r_id) {
			return false;
		}
		if (r1->r_sub_id < r2->r_sub_id) {
			return true;
		} else {
			return false;
		}
	}
};


ResMgr::ResMgr() :
	extras_classad( NULL ),
	m_lastDirectAttachToSchedd(0),
	m_token_requester(&ResMgr::token_request_callback, this),
	max_job_retirement_time_override(-1)
{
	totals_classad = NULL;
	config_classad = NULL;
	up_tid = -1;		   // periodic timer for triggering updates
	poll_tid = -1;
	m_cred_sweep_tid = -1;
	send_updates_tid = -1; // one shot, short period timer for actually talking to the collector
	send_updates_whyfor_mask = (1<<Resource::WhyFor::wf_doUpdate); // initial update should be a full update

	draining = false;
	draining_is_graceful = false;
	on_completion_of_draining = DRAIN_NOTHING_ON_COMPLETION;
	draining_id = 0;
	drain_deadline = 0;
	last_drain_start_time = 0;
	last_drain_stop_time = 0;
	expected_graceful_draining_completion = 0;
	expected_quick_draining_completion = 0;
	expected_graceful_draining_badput = 0;
	expected_quick_draining_badput = 0;
	total_draining_badput = 0;
	total_draining_unclaimed = 0;

	m_attr = new MachAttributes;

#if HAVE_BACKFILL
	m_backfill_mgr = NULL;
	m_backfill_mgr_shutdown_pending = false;
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr = NULL;
#endif

#if HAVE_HIBERNATION
	m_netif = NetworkAdapterBase::createNetworkAdapter(
		daemonCore->InfoCommandSinfulString (), false );
	StartdHibernator	*hibernator = new StartdHibernator;
	m_hibernation_manager = new HibernationManager( hibernator );
	if ( m_netif ) {
		m_hibernation_manager->addInterface( *m_netif );
	}
	m_hibernate_tid = -1;
	NetworkAdapterBase	*primary = m_hibernation_manager->getNetworkAdapter();
	if ( NULL == primary ) {
		dprintf( D_FULLDEBUG,
				 "No usable network interface: hibernation disabled\n" );
	}
	else {
		dprintf( D_FULLDEBUG, "Using network interface %s for hibernation\n",
				 primary->interfaceName() );
	}
	m_hibernation_manager->initialize( );
	std::string	states;
	m_hibernation_manager->getSupportedStates(states);
	dprintf( D_FULLDEBUG,
			 "Detected hibernation states: %s\n", states.c_str() );

	m_hibernating = FALSE;
#endif

	id_disp = NULL;

	type_nums = NULL;
	new_type_nums = NULL;
	is_shutting_down = false;
	cur_time = last_in_use = time( NULL );
	startTime = cur_time;
	const char *death_time = getenv(ENV_DAEMON_DEATHTIME);
	if (death_time && death_time[0]) {
		deathTime = atoi(death_time);
		dprintf( D_ALWAYS, ENV_DAEMON_DEATHTIME " Env set to %s (%lld seconds from now)\n",
			death_time, (long long)time_to_live());
	}

	max_types = 0;
	num_updates = 0;
	type_strings = NULL;
	m_startd_hook_shutdown_pending = false;
}

void ResMgr::Stats::Init()
{
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", Compute, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", WalkEvalState, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", WalkUpdate, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", WalkOther, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", Drain, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", SendUpdates, IF_VERBOSEPUB);
}

double ResMgr::Stats::BeginRuntime(stats_recent_counter_timer &  /*probe*/)
{
   return _condor_debug_get_time_double();
}

double ResMgr::Stats::EndRuntime(stats_recent_counter_timer & probe, double before)
{
   double now = _condor_debug_get_time_double();
   probe.Add(now - before);
   return now;
}

double ResMgr::Stats::BeginWalk(VoidResourceMember  /*memberfunc*/)
{
   return _condor_debug_get_time_double();
}

double ResMgr::Stats::EndWalk(VoidResourceMember memberfunc, double before)
{
    stats_recent_counter_timer * probe = &WalkOther;
    if (memberfunc == &Resource::update_walk_for_timer)
       probe = &WalkUpdate;
    else if (memberfunc == &Resource::eval_state)
       probe = &WalkEvalState;
    return EndRuntime(*probe, before);
}


ResMgr::~ResMgr()
{
	int i;
	if( extras_classad ) delete extras_classad;
	if( config_classad ) delete config_classad;
	if( totals_classad ) delete totals_classad;
	if( id_disp ) delete id_disp;

#if HAVE_BACKFILL
	if( m_backfill_mgr ) {
		delete m_backfill_mgr;
	}
#endif

#if HAVE_JOB_HOOKS
	if (m_hook_mgr) {
		delete m_hook_mgr;
	}
#endif

#if HAVE_HIBERNATION
	cancelHibernateTimer();
	if (m_hibernation_manager) {
		delete m_hibernation_manager;
	}

	if ( m_netif ) {
		delete m_netif;
		m_netif = NULL;
	}
#endif /* HAVE_HIBERNATION */


	for (auto rip : slots) { delete rip; }
	slots.clear();
	for( i=0; i<max_types; i++ ) {
		if( type_strings[i] ) {
			delete type_strings[i];
		}
	}
	delete [] type_strings;
	delete [] type_nums;
	if( new_type_nums ) {
		delete [] new_type_nums;
	}
	delete m_attr;
}


void
ResMgr::init_config_classad( void )
{
	if( config_classad ) delete config_classad;
	config_classad = new ClassAd();

		// First, bring in everything we know we need
	configInsert( config_classad, "START", true );
	configInsert( config_classad, "SUSPEND", true );
	configInsert( config_classad, "CONTINUE", true );
	configInsert( config_classad, "PREEMPT", true );
	configInsert( config_classad, "KILL", true );
	configInsert( config_classad, "WANT_SUSPEND", true );
	configInsert( config_classad, "WANT_VACATE", true );
	if( !configInsert( config_classad, "WANT_HOLD", false ) ) {
			// default's to false if undefined
		config_classad->AssignExpr("WANT_HOLD","False");
	}
	configInsert( config_classad, "WANT_HOLD_REASON", false );
	configInsert( config_classad, "WANT_HOLD_SUBCODE", false );
	configInsert( config_classad, "CLAIM_WORKLIFE", false );
	configInsert( config_classad, ATTR_MAX_JOB_RETIREMENT_TIME, false );
	configInsert( config_classad, ATTR_MACHINE_MAX_VACATE_TIME, true );

		// Now, bring in things that we might need
	configInsert( config_classad, "PERIODIC_CHECKPOINT", false );
	configInsert( config_classad, "RunBenchmarks", false );
	configInsert( config_classad, ATTR_RANK, false );
	configInsert( config_classad, "SUSPEND_VANILLA", false );
	configInsert( config_classad, "CONTINUE_VANILLA", false );
	configInsert( config_classad, "PREEMPT_VANILLA", false );
	configInsert( config_classad, "KILL_VANILLA", false );
	configInsert( config_classad, "WANT_SUSPEND_VANILLA", false );
	configInsert( config_classad, "WANT_VACATE_VANILLA", false );
#if HAVE_BACKFILL
	configInsert( config_classad, "START_BACKFILL", false );
	configInsert( config_classad, "EVICT_BACKFILL", false );
#endif /* HAVE_BACKFILL */
#if HAVE_JOB_HOOKS
	configInsert( config_classad, ATTR_FETCH_WORK_DELAY, false );
#endif /* HAVE_JOB_HOOKS */
#if HAVE_HIBERNATION
	configInsert( config_classad, "HIBERNATE", false );
	if( !configInsert( config_classad, ATTR_UNHIBERNATE, false ) ) {
		std::string default_expr;
		formatstr(default_expr, "MY.%s =!= UNDEFINED",ATTR_MACHINE_LAST_MATCH_TIME);
		config_classad->AssignExpr( ATTR_UNHIBERNATE, default_expr.c_str() );
	}
#endif /* HAVE_HIBERNATION */

	if( !configInsert( config_classad, ATTR_SLOT_WEIGHT, false ) ) {
		config_classad->AssignExpr( ATTR_SLOT_WEIGHT, ATTR_CPUS );
	}

		// First, try the IsOwner expression.  If it's not there, try
		// what's defined in IS_OWNER (for backwards compatibility).
		// If that's not there, give them a reasonable default.
	if( ! configInsert(config_classad, ATTR_IS_OWNER, false) ) {
		if( ! configInsert(config_classad, "IS_OWNER", ATTR_IS_OWNER, false) ) {
			config_classad->AssignExpr( ATTR_IS_OWNER, "(START =?= False)" );
		}
	}
		// Next, try the CpuBusy expression.  If it's not there, try
		// what's defined in cpu_busy (for backwards compatibility).
		// If that's not there, give them a default of "False",
		// instead of leaving it undefined.
	if( ! configInsert(config_classad, ATTR_CPU_BUSY, false) ) {
		if( ! configInsert(config_classad, "cpu_busy", ATTR_CPU_BUSY,
						   false) ) {
			config_classad->Assign( ATTR_CPU_BUSY, false );
		}
	}
	
	// Publish all DaemonCore-specific attributes, which also handles
	// STARTD_ATTRS for us.
	daemonCore->publish(config_classad);
}

void
ResMgr::final_update_daemon_ad()
{
	ClassAd invalidate_ad;
	std::string exprstr, name, escaped_name;

	if (Name) { name = Name; }
	else { name = get_local_fqdn(); }

	// Set the correct types
	SetMyTypeName( invalidate_ad, QUERY_ADTYPE );
	invalidate_ad.Assign(ATTR_TARGET_TYPE, STARTD_DAEMON_ADTYPE);

	/*
	* NOTE: the collector depends on the data below for performance reasons
	* if you change here you will need to CollectorEngine::remove (AdTypes t_AddType, const ClassAd & c_query)
	* the IP was added to allow the collector to create a hash key to delete in O(1).
	*/
	exprstr = std::string("TARGET." ATTR_NAME " == ") + QuoteAdStringValue(name.c_str(), escaped_name);
	invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, exprstr.c_str() );
	invalidate_ad.Assign( ATTR_NAME, name );
	invalidate_ad.Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr());

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	StartdPluginManager::Invalidate(&invalidate_ad);
#endif

	resmgr->send_update( INVALIDATE_STARTD_ADS, &invalidate_ad, NULL, false );
}

void
ResMgr::publish_daemon_ad(ClassAd & ad)
{
	SetMyTypeName(ad, STARTD_DAEMON_ADTYPE);
	daemonCore->publish(&ad);
	if (Name) { ad.Assign(ATTR_NAME, Name); }
	else { ad.Assign(ATTR_NAME, get_local_fqdn()); }


	// cribbed from Resource::publish_static
	ad.Assign(ATTR_IS_LOCAL_STARTD, param_boolean("IS_LOCAL_STARTD", false));
	publish_static(&ad); // publish stuff we learned from the Starter

	m_attr->publish_static(&ad);
	// TODO: move ATTR_CONDOR_SCRATCH_DIR out of m_attr->publish_static
	ad.Delete(ATTR_CONDOR_SCRATCH_DIR);

	primary_res_in_use.Publish(ad, "TotalInUse");
	backfill_res_in_use.Publish(ad, "TotalBackfillInUse");
	excess_backfill_res.Publish(ad, "ExcessBackfill");

	// static information about custom resources
	// this does not include the non fungible resource properties
	ad.Update(m_attr->machres_attrs());

	// gloal dynamic information. offline resources, WINREG values
	m_attr->publish_common_dynamic(&ad, true);
	

	//PRAGMA_REMIND("TJ: write this")
	// m_attr->publish_EP_dynamic(&ad);

	publish_resmgr_dynamic(&ad, true);

	publish_draining_attrs(nullptr, &ad);

	// Publish the supplemental Class Ads IS_UPDATE
	extra_ads.Publish(&ad, 0, "daemon");

	// Publish the monitoring information ALWAYS
	daemonCore->dc_stats.Publish(ad);
	daemonCore->monitor_data.ExportData(&ad);

}

#if HAVE_BACKFILL

void
ResMgr::backfillMgrDone()
{
	ASSERT( m_backfill_mgr );
	dprintf( D_FULLDEBUG, "BackfillMgr now ready to be deleted\n" );
	delete m_backfill_mgr;
	m_backfill_mgr = NULL;
	m_backfill_mgr_shutdown_pending = false;

		// We should call backfillConfig() again, since now that the
		// "old" manager is gone, we might want to allocate a new one
	backfillMgrConfig();
}


static bool
verifyBackfillSystem( const char* sys )
{

#if HAVE_BOINC
	if( ! strcasecmp(sys, "BOINC") ) {
		return true;
	}
#else
	(void)sys;
#endif /* HAVE_BOINC */

	return false;
}


bool
ResMgr::backfillMgrConfig()
{
	if( m_backfill_mgr_shutdown_pending ) {
			/*
			  we're already in the middle of trying to reconfig the
			  backfill manager, anyway.  we can only get to this point
			  if we had 1 backfill system running, then we either
			  change the system we want or disable backfill entirely,
			  and while we're waiting for the old system to cleanup,
			  we get *another* reconfig.  in this case, we do NOT want
			  to act on the new reconfig until the old reconfig had a
			  chance to complete.  since we'll call backfillConfig()
			  from backfillMgrDone(), anyway, there's no harm in just
			  returning immediately at this point, and plenty of harm
			  that could come from trying to proceed. ;)
			*/
		dprintf( D_ALWAYS, "Got another reconfig while waiting for the old "
				 "backfill system to finish cleaning up, delaying\n" );
		return true;
	}

	if( ! param_boolean("ENABLE_BACKFILL", false) ) {
		if( m_backfill_mgr ) {
			dprintf( D_ALWAYS,
					 "ENABLE_BACKFILL is false, destroying BackfillMgr\n" );
			if( m_backfill_mgr->destroy() ) {
					// nothing else to cleanup now, we can delete it
					// immediately...
				delete m_backfill_mgr;
				m_backfill_mgr = NULL;
			} else {
					// backfill_mgr told us we have to wait, so just
					// return for now and we'll finish deleting this
					// in ResMgr::backfillMgrDone().
				dprintf( D_ALWAYS, "BackfillMgr still has cleanup to "
						 "perform, postponing delete\n" );
				m_backfill_mgr_shutdown_pending = true;
			}
		}
		return false;
	}

	char* new_system = param( "BACKFILL_SYSTEM" );
	if( ! new_system ) {
		dprintf( D_ALWAYS, "ERROR: ENABLE_BACKFILL is TRUE, but "
				 "BACKFILL_SYSTEM is undefined!\n" );
		return false;
	}
	if( ! verifyBackfillSystem(new_system) ) {
		dprintf( D_ALWAYS,
				 "ERROR: BACKFILL_SYSTEM '%s' not supported, ignoring\n",
				 new_system );
		free( new_system );
		return false;
	}

	if( m_backfill_mgr ) {
		if( ! strcasecmp(new_system, m_backfill_mgr->backfillSystemName()) ) {
				// same as before
			free( new_system );
				// since it's already here and we're keeping it, tell
				// it to reconfig (if that matters)
			m_backfill_mgr->reconfig();
				// we're done
			return true;
		} else {
				// different!
			dprintf( D_ALWAYS, "BACKFILL_SYSTEM has changed "
					 "(old: '%s', new: '%s'), re-initializing\n",
					 m_backfill_mgr->backfillSystemName(), new_system );
			if( m_backfill_mgr->destroy() ) {
					// nothing else to cleanup now, we can delete it
					// immediately...
				delete m_backfill_mgr;
				m_backfill_mgr = NULL;
			} else {
					// backfill_mgr told us we have to wait, so just
					// return for now and we'll finish deleting this
					// in ResMgr::backfillMgrDone().
				dprintf( D_ALWAYS, "BackfillMgr still has cleanup to "
						 "perform, postponing delete\n" );
				m_backfill_mgr_shutdown_pending = true;
				free( new_system );
				return true;
			}
		}
	}

		// if we got this far, it means we've got a valid system, but
		// no manager object.  so, depending on the system,
		// instantiate the right thing.
#if HAVE_BOINC
	if( ! strcasecmp(new_system, "BOINC") ) {
		m_backfill_mgr = new BOINC_BackfillMgr();
		if( ! m_backfill_mgr->init() ) {
			dprintf( D_ALWAYS, "ERROR initializing BOINC_BackfillMgr\n" );
			delete m_backfill_mgr;
			m_backfill_mgr = NULL;
			free( new_system );
			return false;
		}
	}
#endif /* HAVE_BOINC */

	if( ! m_backfill_mgr ) {
			// this is impossible, since we've already verified above
		EXCEPT( "IMPOSSILE: unrecognized BACKFILL_SYSTEM: '%s'",
				new_system );
	}

	dprintf( D_ALWAYS, "Created a %s Backfill Manager\n",
			 m_backfill_mgr->backfillSystemName() );

	free( new_system );

	return true;
}



#endif /* HAVE_BACKFILL */


// one time initialization/creation of static and partitionable slots
void
ResMgr::init_resources( void )
{
	int i, num_res;
	CpuAttributes** new_cpu_attrs;
	bool *bkfill_bools = nullptr;

	m_execution_xfm.config("JOB_EXECUTION");

#ifdef LINUX
	if (!param_boolean("STARTD_ENFORCE_DISK_LIMITS", false)) {
		dprintf(D_STATUS, "Startd will not enforce disk limits via logical volume management.\n");
		m_volume_mgr.reset(nullptr);
	} else {
		m_volume_mgr.reset(new VolumeManager());
		m_volume_mgr->CleanupLVs();
	}
#endif // LINUX

    stats.Init();

    m_attr->init_machine_resources();

		// These things can only be set once, at startup, so they
		// don't need to be in build_cpu_attrs() at all.
	max_types = param_integer("MAX_SLOT_TYPES", 10);

	max_types += 1;

		// The reason this isn't on the stack is b/c of the variable
		// nature of max_types. *sigh*
	type_strings = new StringList*[max_types];
	memset( type_strings, 0, (sizeof(StringList*) * max_types) );

		// Fill in the type_strings array with all the appropriate
		// string lists for each type definition.  This only happens
		// once!  If you change the type definitions, you must restart
		// the startd, or else too much weirdness is possible.
	SlotType::init_types(max_types, true);
	initTypes( max_types, type_strings, 1 );

		// First, see how many slots of each type are specified.
	num_res = countTypes( max_types, num_cpus(), &type_nums, &bkfill_bools, true );

	if( ! num_res ) {
			// We're not configured to advertise any nodes.
		slots.clear();
		delete [] bkfill_bools;
		id_disp = new IdDispenser( 1 );
		return;
	}

		// See if the config file allows for a valid set of
		// CpuAttributes objects.  Since this is the startup-code
		// we'll let it EXCEPT() if there is an error.
	new_cpu_attrs = buildCpuAttrs( m_attr, max_types, type_strings, num_res, type_nums, bkfill_bools, true );
	if( ! new_cpu_attrs ) {
		EXCEPT( "buildCpuAttrs() failed and should have already EXCEPT'ed" );
	}

		// Now, we can finally allocate our resources array, and
		// populate it.
	for( i=0; i<num_res; i++ ) {
		addResource( new Resource( new_cpu_attrs[i], i+1 ) );
	}

		// We can now seed our IdDispenser with the right slot id.
	id_disp = new IdDispenser( i+1 );

	// Do any post slot setup, currently used for determining the excess backfill resources
	_post_init_resources();

		// Finally, we can free up the space of the new_cpu_attrs
		// array itself, now that all the objects it was holding that
		// we still care about are stashed away in the various
		// Resource objects.  Since it's an array of pointers, this
		// won't touch the objects at all.
	delete [] new_cpu_attrs;
	delete [] bkfill_bools;

#if HAVE_BACKFILL
		// enable the pluggable backfill manager, aka. BACKFILL_SYSTEM  used for BOINC
		// Note that this is distinct from backfill slots.  
	backfillMgrConfig();
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr = new StartdHookMgr;
	m_hook_mgr->initialize();
#endif

#ifdef HAVE_DATA_REUSE_DIR
	std::string reuse_dir;
	if (param(reuse_dir, "DATA_REUSE_DIRECTORY")) {
		if (!m_reuse_dir.get() || (m_reuse_dir->GetDirectory() != reuse_dir)) {
			m_reuse_dir.reset(new htcondor::DataReuseDirectory(reuse_dir, true));
		}
	} else {
		m_reuse_dir.reset();
	}
#endif
}


bool
ResMgr::typeNumCmp( const int* a, const int* b ) const
{
	int i;
	for( i=0; i<max_types; i++ ) {
		if( a[i] != b[i] ) {
			return false;
		}
	}
	return true;
}

void
ResMgr::reconfig_resources( void )
{
	dprintf(D_ALWAYS, "beginning reconfig_resources\n");

#if HAVE_BACKFILL
	backfillMgrConfig();
#endif

#if HAVE_JOB_HOOKS
	if ( m_hook_mgr ) {
		m_hook_mgr->reconfig();
	}
#endif

#if HAVE_HIBERNATION
	updateHibernateConfiguration();
#endif /* HAVE_HIBERNATE */

	m_attr->ReconfigOfflineDevIds();

		// Tell each resource to reconfig itself.
	walk(&Resource::reconfig);

		// See if any new types were defined.  Don't except if there's
		// any errors, just dprintf().
	ASSERT(max_types > 0);
	SlotType::init_types(max_types, false);

#ifdef HAVE_DATA_REUSE_DIR
	std::string reuse_dir;
	if (param(reuse_dir, "DATA_REUSE_DIRECTORY")) {
		if (!m_reuse_dir.get() || (m_reuse_dir->GetDirectory() != reuse_dir)) {
			m_reuse_dir.reset(new htcondor::DataReuseDirectory(reuse_dir, true));
		}
	} else {
		m_reuse_dir.reset();
	}
#endif

		// mark all resources as dirty (i.e. needing update)
		// TODO: change to update walk for reconfig?
	walk(&Resource::update_walk_for_timer);
}


void
ResMgr::walk( VoidResourceMember memberfunc )
{
	if ( ! numSlots()) {
		return;
	}

	double currenttime = stats.BeginWalk(memberfunc);

	// walk the walk.
	// Note that memberfunc might be an eval function, which can
	// result in resources being deleted while we walk. To avoid having our
	// iterator be invalidated, removeResource will do everything except
	// removing the resource from the slots vector and deleting the Resource object
	if (++in_walk > 1) { dprintf(D_ZKM | D_BACKTRACE, "recursive ResMgr::walk\n"); }
	for(Resource* rip : slots) {
		if ( ! rip) continue;
		(rip->*(memberfunc))();
	}
	if (--in_walk <= 0) { _complete_removes(); }

	stats.EndWalk(memberfunc, currenttime);
}

double
ResMgr::sum( ResourceFloatMember memberfunc )
{
	// walk the walk.
	double tot = 0.0;
	for(Resource* rip : slots) {
		if ( ! rip) continue;
		tot += (rip->*(memberfunc))();
	}
	return tot;
}

// Methods to manipulate the supplemental ClassAd list
int
ResMgr::adlist_register( StartdNamedClassAd *ad )
{
	return extra_ads.Register( ad );
}

StartdNamedClassAd *
ResMgr::adlist_find( const char * name )
{
	NamedClassAd * nad = extra_ads.Find(name);
	return dynamic_cast<StartdNamedClassAd*>(nad);
}

bool ResMgr::needsPolling(void) { return call_until<bool>(&Resource::needsPolling); }
bool ResMgr::hasAnyClaim(void) {  return call_until<bool>(&Resource::hasAnyClaim); }
Claim* ResMgr::getClaimByPid(pid_t pid)     { return call_until<Claim*>(&Resource::findClaimByPid, pid); }
Claim* ResMgr::getClaimById(const char* id) { return call_until<Claim*>(&Resource::findClaimById, id); }
Claim* ResMgr::getClaimByGlobalJobId(const char* id) {
	return call_until<Claim*>(&Resource::findClaimByGlobalJobId, id);
}

Claim *
ResMgr::getClaimByGlobalJobIdAndId( const char *job_id,
									const char *claimId)
{
	Claim* foo = NULL;
	for (Resource * rip : slots) {
		if ( ! rip) continue;
		if( (foo = rip->findClaimByGlobalJobId(job_id)) ) {
			if( foo == rip->findClaimById(claimId) ) {
				return foo;
			}
		}
	}
	return NULL;

}


Resource*
ResMgr::findRipForNewCOD( ClassAd* ad )
{
	if ( ! numSlots()) {
		return NULL;
	}

		/*
          We always ensure that the request's Requirements, if any,
		  are met.  Other than that, we give out COD claims to
		  Resources in the following order:

		  1) the Resource with the least # of existing COD claims (to
  		     ensure round-robin across resources
		  2) in case of a tie, the Resource in the best state (owner
   		     or unclaimed, not claimed)
		  3) in case of a tie, the Claimed resource with the lowest
  		     value of machine Rank for its claim
		*/

	auto CODLessThan = [](const Resource *r1, const Resource *r2) {
		int numCOD1 = r1->r_cod_mgr->numClaims();
		int numCOD2 = r2->r_cod_mgr->numClaims();

		if (numCOD1 < numCOD2) {
			return true;
		}

		if (numCOD1 > numCOD2) {
			return false;
		}

		if (r1->state() < r2->state()) {
			return true;
		}
		if (r1->state() > r2->state()) {
			return false;
		}

		State s = r1->state();
		if ((s == claimed_state) || (s == preempting_state)) {
			if (r1->r_cur->rank() < r2->r_cur->rank()) {
				return true;
			}
			if (r1->r_cur->rank() > r2->r_cur->rank()) {
				return false;
			}
		}
		// Otherwise, by pointer, just to avoid loops
		return r1 < r2;
	};

	// sort resources based on the above order
	std::vector<Resource*> cods(slots);
	std::sort(cods.begin(), cods.end(), CODLessThan);
	for (Resource* rip : cods) {
		if ( ! rip) continue;
		bool val = false;
		if (EvalBool(ATTR_REQUIREMENTS, ad, rip->r_classad, val) && val) {
			return rip;
		}
	}

	return nullptr;
}

Resource*
ResMgr::get_by_cur_id(const char* id )
{
	for (Resource * rip : slots) {
		if (!rip) continue;
		if( rip->r_cur->idMatches(id) ) {
			return rip;
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_id(const char* id, bool move_cp_claim )
{
	for (Resource * rip : slots) {
		if (!rip) continue;
		if (rip->r_cur->idMatches(id)) {
			return rip;
		}
		if (rip->r_pre && rip->r_pre->idMatches(id)) {
			return rip;
		}
		if (rip->r_pre_pre && rip->r_pre_pre->idMatches(id) ) {
			return rip;
		}
		if (rip->r_has_cp) {
			for (Resource::claims_t::iterator j(rip->r_claims.begin());  j != rip->r_claims.end();  ++j) {
				if ((*j)->idMatches(id)) {
					if ( move_cp_claim ) {
						delete rip->r_cur;
						rip->r_cur = *j;
						rip->r_claims.erase(*j);
						rip->r_claims.insert(new Claim(rip));
					}
					return rip;
				}
			}
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_name(const char* name )
{
	for (Resource * rip : slots) {
		if (!rip) continue;
		if( !strcmp(rip->r_name, name) ) {
			return rip;
		}
	}
	return NULL;
}

Resource*
ResMgr::get_by_name_prefix(const char* name )
{
	int len = (int)strlen(name);
	for (Resource * rip : slots) {
		if (!rip) continue;
		const char * pat = strchr(rip->r_name, '@');
		if (pat && (int)(pat - rip->r_name) == len && strncasecmp(name, rip->r_name, len) == MATCH) {
			return rip;
		}
	}

	// not found, print possible names
	StringList names;
	for (Resource * rip : slots) {
		if (!rip) continue;
		names.append(rip->r_name);
		if( !strcmp(rip->r_name, name) ) {
			return rip;
		}
	}
	auto_free_ptr namelist(names.print_to_string());
	dprintf(D_ALWAYS, "%s not found, slot names are %s\n", name, namelist ? namelist.ptr() : "<empty>");

	return NULL;
}


Resource*
ResMgr::get_by_slot_id( int id )
{
	for (Resource * rip : slots) {
		if (!rip) continue;
		if( rip->r_id == id ) {
			return rip;
		}
	}
	return NULL;
}

State
ResMgr::state( void )
{
	State s = no_state;
	int is_owner = 0;
	for (Resource * rip : slots) {
		if (!rip) continue;
			// if there are *any* COD claims at all (active or not),
			// we should say this slot is claimed so preen doesn't
			// try to clean up directories for the COD claim(s).
		if( rip->r_cod_mgr->numClaims() > 0 ) {
			return claimed_state;
		}
		s = rip->state();
		switch( s ) {
		case claimed_state:
		case preempting_state:
			return s;
			break;
		case owner_state:
			is_owner = 1;
			break;
		default:
			break;
		}
	}
	if( is_owner ) {
		return owner_state;
	} else {
		return s;
	}
}


void
ResMgr::final_update( void )
{
	if (numSlots()) {
		walk( &Resource::final_update );
	}
#ifdef DO_BULK_COLLECTOR_UPDATES
	if (enable_single_startd_daemon_ad) {
		final_update_daemon_ad();
	}
#endif
}

int
ResMgr::send_update( int cmd, ClassAd* public_ad, ClassAd* private_ad,
					 bool nonblock )
{
	static bool first_time = true;

		// Increment the resmgr's count of updates.
	num_updates++;

	int res = daemonCore->sendUpdates(cmd, public_ad, private_ad, nonblock, &m_token_requester,
		DCTokenRequester::default_identity, "ADVERTISE_STARTD");

	if (first_time) {
		first_time = false;
		dprintf( D_ALWAYS, "Initial update sent to collector(s)\n");
		if ( ! param_boolean("STARTD_SEND_READY_AFTER_FIRST_UPDATE", true)) return res;

		// send a DC_SET_READY message to the master to indicate the STARTD is ready to go
		const char* master_sinful(daemonCore->InfoCommandSinfulString(-2));
		if ( master_sinful ) {
			dprintf( D_ALWAYS, "Sending DC_SET_READY message to master %s\n", master_sinful);
			ClassAd readyAd;
			readyAd.Assign("DaemonPID", getpid());
			readyAd.Assign("DaemonName", "STARTD"); // fix to use the environment
			readyAd.Assign("DaemonState", "Ready");
			classy_counted_ptr<Daemon> dmn = new Daemon(DT_ANY,master_sinful);
			classy_counted_ptr<ClassAdMsg> msg = new ClassAdMsg(DC_SET_READY, readyAd);
			dmn->sendMsg(msg.get());
		}
	}

	return res;
}


void
ResMgr::update_all( int /* timerID */ )
{
	num_updates = 0;
	// make sure that the send_updates timer is queued
	rip_update_needed(1<<Resource::WhyFor::wf_doUpdate);

		// NOTE: We do *NOT* eval_state and update in the same walk
		// over the resources. The reason we do not is the eval_state
		// may result in the deletion of a resource, e.g. if it ends
		// up in the delete_state. In such a case we'll be calling
		// eval_state on a resource we delete and then call update on
		// the same resource. As a result, you might be lucky enough
		// to get a SEGV immediately, but if you aren't you'll get a
		// SEGV later on when the timer Resource::update registers
		// fires. That delay will make it rather difficult to find the
		// root cause of the SEGV, believe me. Generally, nothing
		// should mess with a resource immediately after eval_state is
		// called on it. To avoid this problem, the eval and update
		// process is split here. The Resource::update will only be
		// called on resources that are still alive. - matt 1 Oct 09

		// Evaluate the state change policy expressions (like PREEMPT)
		// For certain changes this will trigger an update to the collector
		// (all that really does is register a timer)
	walk( &Resource::eval_state );

		// If we didn't update b/c of the eval_state, we need to
		// actually do the update now. Tj 2020 sez: this is a lie, was it ever true?
		// What this actually does is insure that the update timers have been registered for all slots
	walk( &Resource::update_walk_for_timer );

	directAttachToSchedd();
	report_updates();
	check_polling();
	check_use();
}

#ifdef DO_BULK_COLLECTOR_UPDATES
// Evaluate and send updates for dirty resources, and clear update dirty bits
void ResMgr::send_updates_and_clear_dirty(int /*timerID = -1*/)
{
	const unsigned int whyfor_mask = send_updates_whyfor_mask;
	send_updates_tid = -1;
	send_updates_whyfor_mask = 0;

	double currenttime = stats.BeginRuntime(stats.SendUpdates);

	ClassAd public_ad, private_ad;

	// TODO: figure out collector version and implement ENABLE_STARTD_DAEMON_AD=auto

	const unsigned int send_daemon_ad_mask = (1<<Resource::WhyFor::wf_doUpdate)
		| (1<<Resource::WhyFor::wf_hiberChange)
		| (1<<Resource::WhyFor::wf_cronRequest);
	if (enable_single_startd_daemon_ad && (whyfor_mask & send_daemon_ad_mask)) {
		publish_daemon_ad(public_ad);
		send_update(UPDATE_STARTD_AD, &public_ad, nullptr, true);
	}

	// force update of backfill p-slot whenever a non-backfill d-slot is created or deleted
	// TODO: maybe update this to handle the case of claiming a non-backfill static slot?
	const unsigned int dslot_mask = (1<<Resource::WhyFor::wf_dslotCreate) | (1<<Resource::WhyFor::wf_dslotDelete);
	bool send_backfill_slots = false;
	if (whyfor_mask & dslot_mask) {
		for (Resource * rip : slots) {
			// if we have created or deleted a primary d-slot, then we want to always update the backfill
			// p-slots because primary d-slot resources are deducted from the backfill p-slots advertised values
			if (rip->is_partitionable_slot() && ! rip->r_backfill_slot && 
				(rip->update_is_needed() & dslot_mask) != 0) {
				send_backfill_slots = true;
				break;
			}
		}
	}

	for(Resource* rip : slots) {
		if ( ! rip) continue;
		if (rip->update_is_needed() ||
			(send_backfill_slots && rip->is_partitionable_slot() && rip->r_backfill_slot)) {
			public_ad.Clear(); private_ad.Clear();
			rip->get_update_ads(public_ad, private_ad); // this clears update_is_needed
			send_update(UPDATE_STARTD_AD, &public_ad, &private_ad, true);
		}
	}

	stats.EndRuntime(stats.SendUpdates, currenttime);
}

// called when Resource::update_needed is called
time_t ResMgr::rip_update_needed(unsigned int whyfor_bits)
{
	send_updates_whyfor_mask |= whyfor_bits;

	dprintf(D_FULLDEBUG, "ResMgr  update_needed(0x%x) -> 0x%x %s\n", whyfor_bits,
		send_updates_whyfor_mask, send_updates_tid < 0 ? "queuing timer" : "timer already queued");

	if (send_updates_tid < 0) {
		send_updates_tid = daemonCore->Register_Timer(1,0,
			(TimerHandlercpp)&ResMgr::send_updates_and_clear_dirty,
			"send_updates_and_clear_dirty",
			this );
	}

	return cur_time;
}

#endif


void
ResMgr::eval_and_update_all( int /* timerID */ )
{
#if HAVE_HIBERNATION
	if ( !hibernating () ) {
#endif
		compute_dynamic(true);
		update_all();
#if HAVE_HIBERNATION
	}
#endif
}


void
ResMgr::eval_all( int /* timerID */ )
{
#if HAVE_HIBERNATION
	if ( !hibernating () ) {
#endif
		num_updates = 0;
		compute_dynamic(false);
		walk( &Resource::eval_state );
		report_updates();
		check_polling();
#if HAVE_HIBERNATION
	}
#endif
}

void
ResMgr::directAttachToSchedd()
{
	std::string schedd_name;
	std::string schedd_pool;
	std::string offer_submitter;
	int interval = 0;

	param(schedd_name, "STARTD_DIRECT_ATTACH_SCHEDD_NAME");
	param(schedd_pool, "STARTD_DIRECT_ATTACH_SCHEDD_POOL");
	param(offer_submitter, "STARTD_DIRECT_ATTACH_SUBMITTER_NAME");

	if ( schedd_name.empty() ) {
		dprintf(D_FULLDEBUG, "No direct attach schedd configured\n");
		return;
	}

	interval = param_integer("STARTD_DIRECT_ATTACH_INTERVAL", 300);
	if ( m_lastDirectAttachToSchedd + interval > time(NULL) ) {
		dprintf(D_FULLDEBUG," Delaying direct attach to schedd\n");
		return;
	}

	std::vector<Resource*> offer_resources;

	for (auto resource: slots) {
		if ( resource->state() != unclaimed_state ) {
			continue;
		}
		offer_resources.push_back(resource);
	}

	if ( offer_resources.empty() ) {
		dprintf(D_FULLDEBUG, "No unclaimed slots, nothing to offer to schedd\n");
		return;
	}
	dprintf(D_FULLDEBUG, "Found %d slots to offer to schedd\n", (int)offer_resources.size());

	// Do we need this if we only trigger when updating the collector?
	compute_dynamic(true);

	m_lastDirectAttachToSchedd = time(NULL);

	int timeout = 30;
	DCSchedd schedd(schedd_name.c_str(), schedd_pool.empty() ? nullptr : schedd_pool.c_str());
	ReliSock *sock = schedd.reliSock(timeout);
	if ( ! sock ) {
		dprintf(D_FULLDEBUG, "Failed to contact schedd for offer\n");
		return;
	}
	if (!schedd.startCommand(DIRECT_ATTACH, sock, timeout)) {
		dprintf(D_FULLDEBUG, "Failed to send DIRECT_ATTACH command to %s\n",
		        schedd_name.c_str());
		delete sock;
		return;
	}

	sock->encode();
	ClassAd cmd_ad;
	cmd_ad.InsertAttr(ATTR_NUM_ADS, (long)offer_resources.size());
	if (!offer_submitter.empty()) {
		cmd_ad.InsertAttr(ATTR_SUBMITTER, offer_submitter);
	}
	if ( !putClassAd(sock, cmd_ad) ) {
		dprintf(D_FULLDEBUG, "Failed to send GIVE_ADS ad to %s\n",
		        schedd_name.c_str());
		delete sock;
		return;
	}

	for ( auto slot: offer_resources ) {
		ClassAd offer_ad;
		slot->publish_single_slot_ad(offer_ad, time(NULL), Resource::Purpose::for_query);
			// TODO This assumes the resource has no preempting claimids,
			//   because we're only looking at unclaimed slots.
		std::string claimid = slot->r_cur->id();

		if ( !sock->put_secret(claimid) ||
		     !putClassAd(sock, offer_ad) )
		{
			dprintf(D_FULLDEBUG, "Failed to send offer ad to %s\n",
			        schedd_name.c_str());
			delete sock;
			return;
		}
	}

	if ( !sock->end_of_message() ) {
		dprintf(D_FULLDEBUG, "Failed to send eom to %s\n",
		        schedd_name.c_str());
	}

	sock->decode();
	ClassAd reply_ad;
	if (!getClassAd(sock, reply_ad) || !sock->end_of_message()) {
		dprintf(D_FULLDEBUG, "Failed to read reply from %s\n", schedd_name.c_str());
		delete sock;
		return;
	}

	int reply_code = NOT_OK;
	reply_ad.LookupInteger(ATTR_ACTION_RESULT, reply_code);
	if (reply_code != OK) {
		dprintf(D_FULLDEBUG, "Schedd returned error\n");
	}

	delete sock;
}

bool
ResMgr::AllocVM(pid_t starter_pid, ClassAd & vm_classad, Resource* rip)
{
	if ( ! m_vmuniverse_mgr.allocVM(starter_pid, vm_classad, rip->executeDir())) {
		return false;
	}
	// update VM related info in our slot and our parent slot (if any)
	Resource* parent = rip->get_parent();
	if (parent) { parent->update_needed(Resource::WhyFor::wf_vmChange); }
	rip->update_needed(Resource::WhyFor::wf_vmChange);

	// if the number of VMs allowed is limited,  then we need to update other static slots and p-slots
	// so that the negotiator knows the new limit.. TJ sez, um why the hurry?
	if (m_vmuniverse_mgr.hasVMLimit()) {
		walk([rip](Resource * itr) {
			if (itr && itr != rip && ! itr->is_dynamic_slot() && ! itr->is_broken_slot()) {
				itr->update_needed(Resource::WhyFor::wf_vmChange);
			}
			});
	}
	return true;
}


void
ResMgr::report_updates( void ) const
{
	if( !num_updates ) {
		return;
	}

	CollectorList* collectors = daemonCore->getCollectorList();
	if( collectors ) {
		std::string list;
		for (auto& collector : collectors->getList()) {
			const char* host = collector->fullHostname();
			list += host ? host : "";
			list += " ";
		}
		dprintf( D_FULLDEBUG,
				 "Sent %d update(s) to the collector (%s)\n",
				 num_updates, list.c_str());
	}
}

void ResMgr::compute_static()
{
	// each time we reconfig (or on startup) we must populate
	// static machine attributes and per-slot config that depends on resource allocation
	m_attr->compute_config();
	walk(&Resource::initial_compute);
}

// Called to refresh dynamic slot attributes
// when a d-slot is created and when any slot is Activated
//
void
ResMgr::compute_and_refresh(Resource * rip)
{
	if ( ! rip) {
		return;
	}

	Resource * parent = rip->get_parent();

	// the policy computations may need to have access to updated MyCurrentTime
	time_t now = time(nullptr);
	if (now != cur_time) {
		cur_time = now;
		//TODO: maybe refresh all slots instead? using refresh_cur_time();
		rip->r_classad->Assign(ATTR_MY_CURRENT_TIME, cur_time);
	}
	compute_resource_conflicts();

#if 0 // TJ: these recompute things which we don't need/want to refresh on slot creation or activation
	compute_draining_attrs();
	// for updates, we recompute some machine attributes (like virtual mem)
	// and that may require a recompute of the resources that reference them
	m_attr->compute_for_update();
#endif
	// calculate slot and parent's virtual memory.
	// TODO: can we get rid of CpuAttributes::c_virt_mem_fraction ?
	rip->compute_shared();
	if (parent) parent->compute_shared();

	// update global machine load and idle values, also dynamic WinReg attributes
	m_attr->compute_for_policy();

	// update per-slot disk and cpu usage/load values
	rip->compute_unshared();
	if (parent) parent->compute_unshared();

	// now sum the updated slot load values to get a system wide load value
	m_attr->update_condor_load(sum(&Resource::condor_load));

	// refresh attributes in the slot and the parent
	rip->refresh_classad_dynamic();
	if (parent) parent->refresh_classad_dynamic();

	// recompute and re-publish evaluated attributes that may depend on the above dynamic attributes
	rip->compute_evaluated();
	if (parent) parent->compute_evaluated();
	rip->refresh_classad_evaluated();
	if (parent) parent->refresh_classad_evaluated();

	// TODO: it's hard to know what the correct order of thse two is
	// it depends on specifically *what* slot attrs that we want to cross post
	rip->refresh_classad_slot_attrs();
	if (parent) parent->refresh_classad_slot_attrs();
}

void
ResMgr::compute_dynamic(bool for_update)
{
	if ( ! numSlots()) {
		return;
	}

	//PRAGMA_REMIND("tj: is this where we clear out the r_classad ?")
	//tj: Not until we make it so rollup happens in a separate layer and cross-slot doesn't depend on stale values to work

    double runtime = stats.BeginRuntime(stats.Compute);

	// make sure that slot are in the normal order before we do the updates below
	std::sort(slots.begin(), slots.end(), slotOrderSorter{});

		// Since lots of things want to know this, just get it from
		// the kernel once and share the value. we want to stuff the value
		// into the classads as well since we are about to evaluate expressions
	update_cur_time(true);

	compute_resource_conflicts();
	compute_draining_attrs();

	// for updates, we recompute some machine attributes (like virtual mem)
	// and that may require a recompute of the resources that reference them
	if (for_update) {
		m_attr->compute_for_update();
		walk(&Resource::compute_shared);
	}

	// update machine load and idle values, also dynamic WinReg attributes
	m_attr->compute_for_policy();
	// the above might take a few seconds, so update the value of now again
	update_cur_time(true);

	// update per-slot disk and cpu usage/load values
	walk(&Resource::compute_unshared);	// how_much & ~(A_SHARED)

	// now sum the updated slot load values to get a system wide load value
	m_attr->update_condor_load(sum(&Resource::condor_load));
	// and then assign the load to slots
	// TODO: rethink this.  the way load is assigned is very pre-p-slot
	assign_load_and_idle();

	// refresh the main resource classad from the internal Resource members
	walk( [](Resource * rip) { rip->refresh_classad_dynamic(); } );

		// Now that we have an updated internal classad for each
		// resource, we can "compute" anything where we need to
		// evaluate classad expressions to get the answer.
	walk( &Resource::compute_evaluated );

		// Next, we can publish any results from that to our internal
		// classads to make sure those are still up-to-date
	walk( [](Resource* rip) { rip->refresh_classad_evaluated(); } );

		// Finally, now that all the internal classads are up to date
		// with all the attributes they could possibly have, we can
		// publish the cross-slot attributes desired from
		// STARTD_SLOT_ATTRS into each slots's internal ClassAd.
	walk( &Resource::refresh_classad_slot_attrs );

	// And last, do some logging
	//
	if (IsFulldebug(D_FULLDEBUG) && for_update && m_attr->always_recompute_disk()) {
		// on update (~10min) we report the new value of DISK 
		walk(&Resource::display_total_disk);
	}
	if (IsDebugLevel(D_LOAD) || IsDebugLevel(D_KEYBOARD)) {
		// Now that we're done, we can display all the values.
		// for updates, we want to log this on normal, all other times, we log at VERBOSE 
		if (for_update) {
			walk(&Resource::display_load);
		} else if (IsDebugVerbose(D_LOAD) || IsDebugVerbose(D_KEYBOARD)) {
			walk(&Resource::display_load_as_D_VERBOSE);
		}
	}

    stats.EndRuntime(stats.Compute, runtime);
}


void
ResMgr::publish_resmgr_dynamic(ClassAd* cp, bool /* daemon_ad =false*/)
{
	cp->Assign(ATTR_TOTAL_SLOTS, numSlots());
#ifdef HAVE_DATA_REUSE_DIR
	if (m_reuse_dir && ! daemon_ad) {
		m_reuse_dir->Publish(*cp);
	}
#endif
	m_vmuniverse_mgr.publish(cp);
	startd_stats.Publish(*cp, 0);
	startd_stats.Tick(time(0));

	// daemonCore->publish sets this, but it will be stale
	// we want to use a consistent value for any given ad,
	// and ideally the same value for a set of ads that are published together
	cp->Assign(ATTR_MY_CURRENT_TIME, cur_time);

	time_t ttl = time_to_live();
	if (ttl < 0) ttl = 0;
	cp->Assign(ATTR_TIME_TO_LIVE, ttl);

#if HAVE_HIBERNATION
    m_hibernation_manager->publish(*cp);
#endif

	if (extras_classad) { cp->Update(*extras_classad); }
}


void
ResMgr::updateExtrasClassAd( ClassAd * cap ) {
	// It turns out to be colossal pain to use the ClassAd for persistence.
	static classad::References offlineUniverses;

	if( ! cap ) { return; }
	if( ! extras_classad ) { extras_classad = new ClassAd(); }

	int topping = 0;
	int obsolete_univ = false;

	//
	// The startd maintains the set offline universes, and the offline
	// universe timestamps.
	//
	// We start with the current set of offline universes, and add or remove
	// universes as directed by the update ad.  This obviates the need for
	// the need for the starter to know anything about the state of the rest
	// of the machine.
	//

	ExprTree * expr = NULL;
	const char * attr = NULL;
	for ( auto itr = cap->begin(); itr != cap->end(); itr++ ) {
		attr = itr->first.c_str();
		expr = itr->second;
		//
		// Copy the whole ad over, excepting special or computed attributes.
		//
		if( strcasecmp( attr, "MyType" ) == 0 ) { continue; }
		if( strcasecmp( attr, "TargetType" ) == 0 ) { continue; }
		if( strcasecmp( attr, "OfflineUniverses" ) == 0 ) { continue; }
		if( strcasestr( attr, "OfflineReason" ) != NULL ) { continue; }
		if( strcasestr( attr, "OfflineTime" ) != NULL ) { continue; }

		ExprTree * copy = expr->Copy();
		extras_classad->Insert( attr, copy );

		//
		// Adjust OfflineUniverses based on the Has<Universe> attributes.
		//
		const char * uo = strcasestr( attr, "Has" );
		if( uo != attr ) { continue; }

		std::string universeName( attr + 3 );
		int univ = CondorUniverseInfo( universeName.c_str(), &topping, &obsolete_univ );
		if( univ == 0 || obsolete_univ) {
			continue;
		}

		// convert universe name to canonical form
		universeName = CondorUniverseOrToppingName(univ, topping);

		std::string reasonTime = universeName + "OfflineTime";
		std::string reasonName = universeName + "OfflineReason";

		bool universeOnline = false;
		ASSERT( cap->LookupBool( attr, universeOnline ) );
		if( ! universeOnline ) {
			offlineUniverses.insert( universeName );
			extras_classad->Assign( reasonTime, time( NULL ) );

			std::string reason = "[unknown reason]";
			cap->LookupString( reasonName, reason );
			extras_classad->Assign( reasonName, reason );
		} else {
			// The universe is online, so it can't have an offline reason
			// or a time that it entered the offline state.
			offlineUniverses.erase( universeName );
			extras_classad->AssignExpr( reasonTime, "undefined" );
			extras_classad->AssignExpr( reasonName, "undefined" );
		}
	}

	//
	// Construct the OfflineUniverses attribute and set it in extras_classad.
	//
	std::string ouListString;
	ouListString.reserve(10 + offlineUniverses.size()*20);
	ouListString = "{";
	classad::References::const_iterator i = offlineUniverses.begin();
	for( ; i != offlineUniverses.end(); ++i ) {
		int univ = CondorUniverseInfo( i->c_str(), &topping, &obsolete_univ );
		if ( ! univ || obsolete_univ) { continue; }

		if (ouListString.size() > 1) { ouListString += ", "; }
		formatstr_cat(ouListString, "\"%s\"", i->c_str() );
		if ( ! topping) { formatstr_cat(ouListString, ",%d", univ ); }
	}
	ouListString += "}";
	dprintf( D_ALWAYS, "OfflineUniverses = %s\n", ouListString.c_str() );
	extras_classad->AssignExpr( "OfflineUniverses", ouListString.c_str() );
}

void
ResMgr::publishSlotAttrs( ClassAd* cap )
{
	if ( ! numSlots()) {
		return;
	}
	// experimental flags new for 8.9.7, evaluate STARTD_SLOT_ATTRS and insert valid literals only
	bool as_literal = param_boolean("STARTD_EVAL_SLOT_ATTRS", false);
	bool valid_only = ! param_boolean("STARTD_EVAL_SLOT_ATTRS_DEBUG", false);
	for (Resource* rip : slots) {
		rip->publish_SlotAttrs( cap, as_literal, valid_only );
	}
}

void ResMgr::assign_load_and_idle()
{
	// make a copy of the slot pointers vector so we sort it and assign load and idle
	// first to the slots that are running jobs.
	std::vector<Resource*> active(slots);

	// Sort the resources so when we're assigning owner load
	// average and keyboard activity, we get to them in the
	// following state order: Owner, Unclaimed, Matched, Claimed
	// Preempting

	// sort first by state, in enum order, then rank, if claimed
	auto ownerStateLessThan = [](const Resource *r1, const Resource *r2) {
		if (r1->state() < r2->state()) {
			return true;
		}
		if (r1->state() > r2->state()) {
			return false;
		}

		State s = r1->state();
		if ((s == claimed_state) || (s == preempting_state)) {
			if (r1->r_cur->rank() < r2->r_cur->rank()) {
				return true;
			}
			if (r1->r_cur->rank() > r2->r_cur->rank()) {
				return false;
			}
		}
		// Otherwise, by id and sub-id, just to avoid loops
		return (r1->r_id*10000)+r1->r_sub_id < (r2->r_id*10000)+r2->r_sub_id;
	};

	std::sort(active.begin(), active.end(), ownerStateLessThan);

	double total_owner_load = m_attr->load() - m_attr->condor_load();
	if( total_owner_load < 0 ) {
		total_owner_load = 0;
	}

	// Print out the totals we already know.
	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_LOAD | D_VERBOSE,
			"%s %.3f\t%s %.3f\t%s %.3f\n",
			"SystemLoad:", m_attr->load(),
			"TotalCondorLoad:", m_attr->condor_load(),
			"TotalOwnerLoad:", total_owner_load );
	}

	// Distribute the owner load over the slots, assign an owner load of 1.0
	// to each slot until the remainer is less than 1.0.  then assign the remainder
	// to the next slot, and 0 to all of the remaining slots.
	// Note that before HTCondor 10.x we would assign *all* of the remainder to the last slot
	// even if the value was greater than 1.0, but other than that this algorithm is
	// the same as before.  This algorithm doesn't make a lot of sense for multi-core slots
	// but it's the way it has always worked so...
        for (Resource* rip : active) {
                long long cpus = rip->r_attr->num_cpus();
                if (total_owner_load < cpus) {
                        rip->set_owner_load(total_owner_load);
                        total_owner_load = 0;
                } else {
                        rip->set_owner_load(cpus);
                        total_owner_load -= cpus;
                }
        }

	// assign keyboard and console idle
	time_t console = m_attr->console_idle();
	time_t keyboard = m_attr->keyboard_idle();
	time_t max = (cur_time - startd_startup) + disconnected_keyboard_boost;

	// Assign console idle and keyboard idle activity to all slots connected to keyboard/console
	// and the startd lifetime + boost to all other slots
	int num_console = console_slots;
	int num_keyboard = keyboard_slots;
	for (Resource* rip : active) {
		// don't count p-slots as slots for purposes of decrementing the slot count
		// but do set their keyboard and console idle value.
		if ( ! rip->is_partitionable_slot()) {
			if (--num_console < 0) { console = max; }
			if (--num_keyboard < 0) { keyboard = console; }
		}
		rip->r_attr->set_console(console);
		rip->r_attr->set_keyboard(keyboard);
	}
}

void
ResMgr::check_polling( void )
{
	if ( ! numSlots()) {
		return;
	}

	if( needsPolling() || m_attr->condor_load() > 0 ) {
		start_poll_timer();
	} else {
		cancel_poll_timer();
	}
}


void
ResMgr::sweep_timer_handler( int /* timerID */ ) const
{
	dprintf(D_FULLDEBUG, "STARTD: calling and resetting sweep_timer_handler()\n");
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	credmon_sweep_creds(cred_dir, credmon_type_KRB);
	int sec_cred_sweep_interval = param_integer("SEC_CREDENTIAL_SWEEP_INTERVAL", 300);
	daemonCore->Reset_Timer (m_cred_sweep_tid, sec_cred_sweep_interval, sec_cred_sweep_interval);
}

int
ResMgr::start_sweep_timer( void )
{
	// only sweep if we have a cred dir
	auto_free_ptr p(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
	if(!p) {
		return TRUE;
	}

	dprintf(D_FULLDEBUG, "STARTD: setting start_sweep_timer()\n");
	int sec_cred_sweep_interval = param_integer("SEC_CREDENTIAL_SWEEP_INTERVAL", 300);
	m_cred_sweep_tid = daemonCore->Register_Timer( sec_cred_sweep_interval, sec_cred_sweep_interval,
							(TimerHandlercpp)&ResMgr::sweep_timer_handler,
							"sweep_timer_handler", this );
	return TRUE;
}


int
ResMgr::start_update_timer( void )
{
	int initial_interval;

	int update_offset =  param_integer("UPDATE_OFFSET",0,0); // knob to delay initial update
	if (update_offset) {
		initial_interval = update_offset;
		dprintf(D_FULLDEBUG, "Delaying initial update by %d seconds\n", update_offset);
	} else {
		// if we are not delaying the initial update, trigger an update now
		// and then schedule the timer to do updates periodically from now on
		initial_interval = update_interval;
		update_all( );
	}

	up_tid = daemonCore->Register_Timer(
		initial_interval,
		update_interval,
		(TimerHandlercpp)&ResMgr::eval_and_update_all,
		"eval_and_update_all",
		this );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
ResMgr::start_poll_timer( void )
{
	if( poll_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	poll_tid =
		daemonCore->Register_Timer( polling_interval,
							polling_interval,
							(TimerHandlercpp)&ResMgr::eval_all,
							"poll_resources", this );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	dprintf( D_FULLDEBUG, "Started polling timer.\n" );
	return TRUE;
}


void
ResMgr::cancel_poll_timer( void )
{
	int rval;
	if( poll_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( poll_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel polling timer (%d): "
					 "daemonCore error\n", poll_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled polling timer (%d)\n",
					 poll_tid );
		}
		poll_tid = -1;
	}
}


void
ResMgr::reset_timers( void )
{
	if( poll_tid != -1 ) {
		daemonCore->Reset_Timer( poll_tid, polling_interval,
								 polling_interval );
	}
	if( up_tid != -1 ) {
		daemonCore->Reset_Timer_Period( up_tid, update_interval );
	}

	int sec_cred_sweep_interval = param_integer("SEC_CREDENTIAL_SWEEP_INTERVAL", 300);
	if( m_cred_sweep_tid != -1 ) {
		daemonCore->Reset_Timer( m_cred_sweep_tid, sec_cred_sweep_interval,
								 sec_cred_sweep_interval );
	}

#if HAVE_HIBERNATION
	resetHibernateTimer();
#endif /* HAVE_HIBERNATE */

		// Clear out any pending token requests.
	m_token_client_id = "";
	m_token_request_id = "";

		// This is a borrowed reference; do not delete.
	m_token_daemon = nullptr;
}


void
ResMgr::addResource( Resource *rip )
{
	if( !rip ) {
		EXCEPT("Error: attempt to add a NULL resource");
	}

	calculateAffinityMask(rip);

	slots.push_back(rip);

	// If this newly added slot is dynamic, add it to
	// its parent's children

	if( rip->get_feature() == Resource::DYNAMIC_SLOT) {
		Resource *parent = rip->get_parent();
		if (parent) {
			parent->add_dynamic_child(rip);
		}
	}

#ifdef LINUX
	if (!m_volume_mgr) {
		rip->setVolumeManager(m_volume_mgr.get());
	}
#endif // LINUX
}

// private helper functions for removing slots while walking
void ResMgr::_remove_and_delete_slot_res(Resource * rip)
{
	// remove the resource from our slot collection
	auto last = std::remove(slots.begin(), slots.end(), rip);
	if (last != slots.end()) { slots.erase(last, slots.end()); }

	// and now we can delete the object itself.
	delete rip;
}
void ResMgr::_complete_removes()
{
	ASSERT( ! in_walk);
	std::vector<Resource*> removes(_pending_removes);
	_pending_removes.clear();
	for (Resource * rip : removes) { _remove_and_delete_slot_res(rip); }
	// in case we want to null out pointers in the slots vector when they are first removed
	auto last = std::remove_if(slots.begin(), slots.end(), [](const Resource*rip) { return !rip; });
	slots.erase(last, slots.end());
}

bool
ResMgr::removeResource( Resource* rip )
{
	// Tell the collector this Resource is gone.
	rip->final_update();

	// If this was a dynamic slot, remove it from parent
	// Otherwise return this Resource's ID to the dispenser.
	Resource *parent = rip->get_parent();
	if (parent) {
		parent->remove_dynamic_child(rip);
		rip->clear_parent(); // this turns a DYNAMIC_SLOT into a BROKEN_SLOT
	} else if ( ! rip->is_dynamic_slot() && ! rip->is_broken_slot()) {
		id_disp->insert( rip->r_id );
	}

	// Log a message that we're going away
	rip->dprintf( D_ALWAYS, "Slot %s no longer needed, deleting\n", rip->r_id_str );

	// we might be in the process of iterating the slots. If we are we want to
	// leave the resource in the slot collection until after we are done iterating.
	if (in_walk) {
		dprintf(D_ZKM | D_BACKTRACE, "removeResource called while walking depth=%d\n", in_walk);
		// we can't actually delete it now or remove it from the slot vector
		// so add it to the _pending_removes so it will be removed after the walk
		for (Resource * it : _pending_removes) { if (it == rip) return false; } // safety check
		_pending_removes.push_back(rip);
	} else {
		//dprintf(D_ZKM | D_BACKTRACE, "removeResource while not walking\n");
		_remove_and_delete_slot_res(rip);
		return true;
	}

	return false;
}


void
ResMgr::calculateAffinityMask( Resource *rip) {
	int firstCore = 0;
	int numCores  = m_attr->num_real_cpus();
	numCores -= firstCore;

	int *coreOccupancy = new int[numCores];
	for (int i = 0; i < numCores; i++) {
		coreOccupancy[i] = 0;
	}

	// Invert the slots' affinity mask to figure out
	// which cpu core is already used the least.
	for (Resource* slot : slots) {
		for (int used_core_num : *slot->get_affinity_set()) {
			if (used_core_num < numCores) {
				coreOccupancy[used_core_num]++;
			}
		}
	}

	int coresToAssign = rip->r_attr->num_cpus();
	while (coresToAssign--) {
		int leastUsedCore = 0;
		int leastUsedCoreUsage = coreOccupancy[0];

		for (int i = 0; i < numCores; i++) {
			if (coreOccupancy[i] < leastUsedCoreUsage) {
				leastUsedCore = i;
				leastUsedCoreUsage = coreOccupancy[i];
			}
		}

		rip->get_affinity_set()->push_back(leastUsedCore);
		coreOccupancy[leastUsedCore]++;
	}

	delete [] coreOccupancy;
}

void
ResMgr::deleteResource( Resource* rip )
{
	if( ! removeResource( rip ) ) {
			// Didn't find it.  This is where we'll hit if resources
			// is NULL.  We should never get here, anyway (we'll never
			// call deleteResource() if we don't have any resources.
		dprintf(D_ERROR, "ResMgr::deleteResource() failed: couldn't find resource\n" );
	}

}

// return the count of claims on this machine associated with this user
// used to decide when to delete credentials
int ResMgr::claims_for_this_user(const std::string &user)
{
	if (user.empty()) {
		return 0;
	}
	int num_matches = 0;

	for (const Resource *res : slots) {
		if (res && res->r_cur && res->r_cur->client() && !res->r_cur->client()->c_user.empty()) {
			if (user == res->r_cur->client()->c_user) {
				num_matches += 1;
			}
		}
	}
	return num_matches;
}

static void clean_private_attrs(ClassAd & ad)
{
	for (auto i = ad.begin(); i != ad.end(); ++i) {
		const std::string & name = i->first;

		if (ClassAdAttributeIsPrivateAny(name)) {
			// TODO: redact these while still providing some info, perhaps return the HASH?
			ad.Assign(name, "<redacted>");
		}
	}
}

void
ResMgr::makeAdList( ClassAdList & list, ClassAd & queryAd )
{

	std::string stats_config;
	int      dc_publish_flags = daemonCore->dc_stats.PublishFlags;
	queryAd.LookupString("STATISTICS_TO_PUBLISH",stats_config);
	if ( ! stats_config.empty()) {
#if 0 // HACK to test swapping claims without a schedd
		dprintf(D_ALWAYS, "Got QUERY_STARTD_ADS with stats config: %s\n", stats_config.c_str());
		if (starts_with_ignore_case(stats_config.c_str(), "swap:")) {
			StringList swap_args(stats_config.c_str()+5);
			hack_test_claim_swap(swap_args);
		} else
#endif
			daemonCore->dc_stats.PublishFlags = 
			generic_stats_ParseConfigString(stats_config.c_str(), 
				"DC", "DAEMONCORE", 
				dc_publish_flags);
	}

	bool snapshot = false;
	if (!queryAd.LookupBool("Snapshot", snapshot)) {
		snapshot = false;
	}
	int limit_results = -1;
	if (!queryAd.LookupInteger(ATTR_LIMIT_RESULTS, limit_results)) {
		limit_results = -1;
	}

		// Make sure everything is current unless we have been asked for a snapshot of the current internal state
	Resource::Purpose purp = Resource::Purpose::for_query;
	if (snapshot) {
		purp = Resource::Purpose::for_snap;
	} else {
		purp = Resource::Purpose::for_query;
		compute_dynamic(true);
	}

	// TODO: use ATTR_TARGET_TYPE of the queryAd to restrict what ads are created here?

	// we will put the Machine ads we intend to return here temporarily
	std::map <YourString, ClassAd*, CaseIgnLTYourString> ads;
	// these get filled in with Resource and Job(Claim) ads only when snapshot == true
	std::map <YourString, ClassAd*, CaseIgnLTYourString> res_ads;
	std::map <YourString, ClassAd*, CaseIgnLTYourString> cfg_ads;
	std::map <YourString, ClassAd*, CaseIgnLTYourString> claim_ads;

		// We want to insert ATTR_LAST_HEARD_FROM into each ad.  The
		// collector normally does this, so if we're servicing a
		// QUERY_STARTD_ADS commannd, we need to do this ourselves or
		// some timing stuff won't work.
	int num_ads = 0;
	for (Resource * rip : slots) {
		if (limit_results >= 0 && num_ads >= limit_results) {
			dprintf(D_ALWAYS, "result limit of %d reached, completing direct query\n", num_ads);
			break;
		}

		ClassAd * res_ad = NULL;
		if (snapshot && rip->r_classad) {
			rip->r_classad->Unchain();
			res_ad = new ClassAd(*rip->r_classad);
			rip->r_classad->ChainToAd(rip->r_config_classad);
			SetMyTypeName(*res_ad, "Slot.State");
			res_ad->Assign(ATTR_NAME, rip->r_name); // stuff a name because the name attribute is in the base ad
		}
		ClassAd * cfg_ad = NULL;
		if (snapshot && rip->r_config_classad) {
			cfg_ad = new ClassAd(*rip->r_config_classad);
			SetMyTypeName(*cfg_ad, "Slot.Config");
		}
		ClassAd * claim_ad = NULL;
		if (snapshot && rip->r_cur && rip->r_cur->ad()) {
			claim_ad = new ClassAd(*rip->r_cur->ad());
			clean_private_attrs(*claim_ad);
			SetMyTypeName(*claim_ad, "Slot.Claim");
		}

		ClassAd * ad = new ClassAd;
		rip->publish_single_slot_ad(*ad, cur_time, purp);

		if (IsAConstraintMatch(&queryAd, ad) /* || (claim_ad && IsAConstraintMatch(&queryAd, claim_ad))*/) {
			ads[rip->r_name] = ad;
			if (res_ad) { res_ads[rip->r_name] = res_ad; }
			if (cfg_ad) { cfg_ads[rip->r_name] = cfg_ad; }
			if (claim_ad) { claim_ads[rip->r_name] = claim_ad; }
			++num_ads;
		} else {
			delete ad;
			delete res_ad;
			delete cfg_ad;
			delete claim_ad;
		}
	}

	// put Machine ads and their associated snapshot ads into the return
	// as we do this we erase the snap ads so that we can detect any leftover snap ads
	if ( ! ads.empty()) {
		for (auto it = ads.begin(); it != ads.end(); ++it) {
			list.Insert(it->second);
			auto foundb = cfg_ads.find(it->first);
			if (foundb != cfg_ads.end()) {
				list.Insert(foundb->second);
				cfg_ads.erase(foundb);
			}
			auto foundr = res_ads.find(it->first);
			if (foundr != res_ads.end()) {
				list.Insert(foundr->second);
				res_ads.erase(foundr);
			}
			auto foundj = claim_ads.find(it->first);
			if (foundj != claim_ads.end()) {
				list.Insert(foundj->second);
				claim_ads.erase(foundj);
			}
		}
	}

	// also return any leftover snap ads, this puts leftover snap ads at the end
	for (auto it = res_ads.begin(); it != res_ads.end(); ++it) {
		list.Insert(it->second);
	}
	for (auto it = cfg_ads.begin(); it != cfg_ads.end(); ++it) {
		list.Insert(it->second);
	}
	for (auto it = claim_ads.begin(); it != claim_ads.end(); ++it) {
		list.Insert(it->second);
	}

	// also return the raw STARTD cron ads
	if (snapshot) {
		for (auto it = extra_ads.Enum().begin(); it != extra_ads.Enum().end(); ++it) {
			ClassAd * named_ad = (*it)->GetAd();
			if (named_ad) {
				ClassAd * ad = new ClassAd(*named_ad);
				SetMyTypeName(*ad, "Machine.Extra");
				ad->Assign(ATTR_NAME, (*it)->GetName());
				list.Insert(ad);
			}
		}
	}

	// restore the dc stats publish flags
	if ( ! stats_config.empty()) {
		daemonCore->dc_stats.PublishFlags = dc_publish_flags;
	}

}



#if HAVE_HIBERNATION

HibernationManager const& ResMgr::getHibernationManager(void) const
{
	return *m_hibernation_manager;
}


void ResMgr::updateHibernateConfiguration() {
	m_hibernation_manager->update();
	if ( m_hibernation_manager->wantsHibernate() ) {
		if ( -1 == m_hibernate_tid ) {
			startHibernateTimer();
		}
	} else {
		if ( -1 != m_hibernate_tid ) {
			cancelHibernateTimer();
		}
	}
}


int
ResMgr::allHibernating( std::string &target ) const
{
    	// fail if there is no resource or if we are
		// configured not to hibernate
	if ( ! numSlots()  ||  !m_hibernation_manager->wantsHibernate()  ) {
		dprintf( D_FULLDEBUG, "allHibernating: doesn't want hibernate\n" );
		return 0;
	}
		// The following may evaluate to true even if there
		// is a claim on one or more of the resources, so we
		// don't bother checking for claims first.
		//
		// We take largest value as the representative
		// hibernation level for this machine
	target = "";
	std::string str;
	int level = 0;
	bool activity = false;
	for (Resource * rip : slots) {
		str = "";
		if ( !rip->evaluateHibernate ( str ) ) {
			return 0;
		}

		int tmp = m_hibernation_manager->stringToSleepState (
			str.c_str () );

		dprintf ( D_FULLDEBUG,
			"allHibernating: slot %s: '%s' (0x%x)\n",
			rip->r_id_str, str.c_str (), tmp );

		if ( 0 == tmp ) {
			activity = true;
		}

		if ( tmp > level ) {
			target = str;
			level = tmp;
		}
	}
	return activity ? 0 : level;
}


void
ResMgr::checkHibernate( int /* timerID */ )
{

		// If we have already issued the command to hibernate, then
		// don't bother re-entering the check/evaluation.
	if ( hibernating () ) {
		return;
	}

		// If all resources have gone unused for some time
		// then put the machine to sleep
	std::string target;
	int level = allHibernating( target );
	if( level > 0 ) {

        if( !m_hibernation_manager->canHibernate() ) {
            dprintf ( D_ALWAYS, "ResMgr: ERROR: Ignoring "
                "HIBERNATE: Machine does not support any "
                "sleep states.\n" );
            return;
        }

        if( !m_hibernation_manager->canWake() ) {
			NetworkAdapterBase	*netif =
				m_hibernation_manager->getNetworkAdapter();
			if ( param_boolean( "HIBERNATION_OVERRIDE_WOL", false ) ) {
				dprintf ( D_ALWAYS,
						  "ResMgr: "
						  "HIBERNATE: Machine cannot be woken by its "
						  "public network adapter (%s); hibernating anyway\n",
						  netif->interfaceName() );
			}
			else {
				dprintf ( D_ALWAYS, "ResMgr: ERROR: Ignoring "
						  "HIBERNATE: Machine cannot be woken by its "
						  "public network adapter (%s).\n",
						  netif->interfaceName() );
				return;
			}
		}

		dprintf ( D_ALWAYS, "ResMgr: This machine is about to "
        		"enter hibernation\n" );

        //
		// Set the hibernation state, shutdown the machine's slot
	    // and hibernate the machine. We turn off the local slots
	    // so the StartD will remove any jobs that are currently
	    // running as well as stop accepting new ones, since--on
	    // Windows anyway--there is the possibility that a job
	    // may be matched to this machine between the time it
	    // is told hibernate and the time it actually does.
		//
	    // Setting the state here also ensures the Green Computing
	    // plug-in will know the this ad belongs to it when the
	    // Collector invalidates it.
	    //
		if ( disableResources( target ) ) {
			m_hibernation_manager->switchToTargetState( );
		}
	#if !defined( WIN32 )
		sleep(10);
		m_hibernation_manager->setTargetState ( HibernatorBase::NONE );
		for (Resource * rip : slots) {
			rip->enable();
			rip->update_needed(Resource::WhyFor::wf_hiberChange);
			m_hibernating = false;
		}

	#endif
    }
}


int
ResMgr::startHibernateTimer( void )
{
	int interval = m_hibernation_manager->getCheckInterval();
	m_hibernate_tid = daemonCore->Register_Timer(
		interval, interval,
		(TimerHandlercpp)&ResMgr::checkHibernate,
		"ResMgr::startHibernateTimer()", this );
	if( m_hibernate_tid < 0 ) {
		EXCEPT( "Can't register hibernation timer" );
	}
	dprintf( D_FULLDEBUG, "Started hibernation timer.\n" );
	return TRUE;
}


void
ResMgr::resetHibernateTimer( void )
{
	if ( m_hibernation_manager->wantsHibernate() ) {
		if( m_hibernate_tid != -1 ) {
			int interval = m_hibernation_manager->getCheckInterval();
			daemonCore->Reset_Timer(
				m_hibernate_tid,
				interval, interval );
		}
	}
}


void
ResMgr::cancelHibernateTimer( void )
{
	int rval;
	if( m_hibernate_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( m_hibernate_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel hibernation timer (%d): "
				"daemonCore error\n", m_hibernate_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled hibernation timer (%d)\n",
				m_hibernate_tid );
		}
		m_hibernate_tid = -1;
	}
}


int
ResMgr::disableResources( const std::string &state_str )
{

	dprintf (
		D_FULLDEBUG,
		"In ResMgr::disableResources ()\n" );

	/* set the sleep state so the plugin will pickup on the
	fact that we are sleeping */
	m_hibernation_manager->setTargetState ( state_str.c_str() );

	/* update the CM */
	bool ok = true;
	for (Resource * rip : slots) {
		ok = rip->update_with_ack();
		if ( ! ok) break;
	}

	dprintf (
		D_FULLDEBUG,
		"All resources disabled: %s.\n",
		ok ? "yes" : "no" );

	/* if any of the updates failed, then re-enable all the
	resources and try again later (next time HIBERNATE evaluates
	to a value>0) */
	if ( !ok ) {
		m_hibernation_manager->setTargetState (
			HibernatorBase::NONE );
	}
	else {
		/* Boot off any running jobs and disable all resource on this
		   machine so we don't allow new jobs to start while we are in
		   the middle of hibernating.  We disable _after_ sending our
		   update_with_ack(), because we want our machine to still be
		   matchable while broken.  The negotiator knows to treat this
		   state specially. */
		for (Resource * rip : slots) { rip->disable(); }
	}

	dprintf ( 
		D_FULLDEBUG,
		"All resources disabled: %s.\n", 
		ok ? "yes" : "no" );

	/* record if we we are hibernating or not */
	m_hibernating = ok;

	return ok;
}


bool ResMgr::hibernating () const {
	return m_hibernating;
}

#endif /* HAVE_HIBERNATION */


void
ResMgr::check_use( void )
{
	time_t current_time = time(NULL);
	if( hasAnyClaim() ) {
		last_in_use = current_time;
	}
	if( ! startd_noclaim_shutdown ) {
			// Nothing to do.
		return;
	}
	if( current_time - last_in_use > startd_noclaim_shutdown ) {
			// We've been unused for too long, send a SIGTERM to our
			// parent, the condor_master.
		dprintf( D_ALWAYS,
				 "No resources have been claimed for %d seconds\n",
				 startd_noclaim_shutdown );
		dprintf( D_ALWAYS, "Shutting down Condor on this machine.\n" );
		daemonCore->Send_Signal( daemonCore->getppid(), SIGTERM );
	}
}

#if 0 // not currently used
// Comparison function for sorting resources:
// Sort on State, with Claimed state resources coming first.  Break
// ties with the value of the Rank expression for Claimed resources.
int
claimedRankCmp( const void* a, const void* b )
{
	const Resource *rip1, *rip2;
	int val1, val2, diff;
	double fval1, fval2;
	State s;
	rip1 = *((Resource* const *)a);
	rip2 = *((Resource* const *)b);

	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val2 - val1;
	if( diff ) {
		return diff;
	}
		// We're still here, means we've got the same state.  If that
		// state is "Claimed" or "Preempting", we want to break ties
		// w/ the Rank expression, else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval2 - fval1);
		return diff;
	}
	return 0;
}
#endif

void
ResMgr::FillExecuteDirsList( class StringList *list )
{
	if ( ! numSlots())
		return;

	ASSERT( list );
	for (Resource * rip : slots) {
		if (rip) {
			const char * execute_dir = rip->executeDir();
			if( !list->contains( execute_dir ) ) {
				list->append(execute_dir);
			}
		}
	}
}

// private helper function called after all static and partitionable slots have been created on startup
void ResMgr::_post_init_resources()
{
	// summarize the slot config
	//
	static int res_summary_log_level = D_ZKM /*|D_VERBOSE*/;
	if (IsDebugCatAndVerbosity(res_summary_log_level)) {
		std::string buf;
		for (Resource * rip : slots) {
			formatstr_cat(buf, "\t%s type%d : ", rip->r_id_str, rip->type_id());
			rip->r_attr->cat_totals(buf);
			buf += "\n";
		}
		if ( ! buf.empty()) { dprintf(res_summary_log_level, "Slot Config:\n%s", buf.c_str()); }
	}

	// calculate the resource difference between all backfill slots and all primary slots
	// we need to take this constant difference into account when calculating resource conflicts

	excess_backfill_res.reset();

	int num_bkfill = 0;
	for (Resource * rip : slots) {
		if (rip->r_backfill_slot) {
			num_bkfill += 1;
			excess_backfill_res += *rip->r_attr;
		} else {
			excess_backfill_res -= *rip->r_attr;
		}
	}

	std::string names;
	bool has_excess = excess_backfill_res.excess(&names);
	if (has_excess && num_bkfill) {
		dprintf(D_STATUS, "WARNING: Configured Backfill slots have more %s than primary slots\n", names.c_str());
	}
	names.clear();
	if (excess_backfill_res.underrun(&names) && num_bkfill) {
		dprintf(D_STATUS, "WARNING: Configured Backfill slots have less %s than primary slots\n", names.c_str());
	}
	names.clear();

	// we want to use the excess in the calculation, but ignore the underun.
	excess_backfill_res.clear_underrun();
	if ((num_bkfill && has_excess) || IsFulldebug(D_FULLDEBUG)) {
		std::string buf;
		dprintf(D_STATUS, "Backfill excess: %s\n", excess_backfill_res.dump(buf));
	}
}


bool
ResMgr::compute_resource_conflicts()
{
	dprintf(D_ZKM | D_VERBOSE, "ResMgr::compute_resource_conflicts\n");

	ResBag totbag(excess_backfill_res); // totals start with difference between total backfill resources and total primary resources
	std::string dumptmp;
	std::deque<Resource*> active;
	int num_nft_conflict = 0; // number of active backfill slots that already have non-fungible resource conflicts

	primary_res_in_use.reset();
	backfill_res_in_use.reset();

	// Build up a bag of unclaimed resources
	// and a list of active backfill slots
	// TODO: fix for claimed p-slots when that changes
	for (Resource * rip : slots) {
		if ( ! rip) continue;
		State state = rip->state();
		bool is_claimed = state == claimed_state || state == preempting_state;
		if (rip->r_sub_id > 0 || (is_claimed && ! rip->is_partitionable_slot())) {
			if (rip->r_backfill_slot) {
				if (rip->has_nft_conflicts(resmgr->m_attr)) {
					++num_nft_conflict;
					active.push_front(rip);
				} else {
					active.push_back(rip);
				}
				backfill_res_in_use += *rip->r_attr;
			} else {
				primary_res_in_use += *rip->r_attr;
			}
			// subtract active slots from the resource bag
			totbag -= *rip->r_attr;
			dprintf(D_ZKM | D_VERBOSE, "conflicts SUB %s %s\n", rip->r_id_str, totbag.dump(dumptmp));
		} else if ( ! rip->r_backfill_slot && rip->r_sub_id == 0) {
			// add primary p-slots to the resource bag
			totbag += *rip->r_attr;
			dprintf(D_ZKM | D_VERBOSE, "conflicts ADD %s %s\n", rip->r_id_str, totbag.dump(dumptmp));
		}
	}

	// if we get to here with a negative value in the bag of unclaimed resources
	// we need to start kicking off backfill slots.  For now, we do that by
	// assigning the resource conflicts to specific backfill slots and trusting PREEMPT to kick off the jobs

	// If there were fungible resource conflicts, totbag will be in an underflow state
	std::string conflicts;
	bool has_conflicts = totbag.underrun(&conflicts);

	// go back through the active backfill slots and assign the conflicts
	// the active backfill slot collection will have slots that have non-fungible resource conflicts first
	if (has_conflicts) {
		dprintf(D_ZKM, "resource_conflicts(%d NFT of %d active) : %s\n",  num_nft_conflict, (int)active.size(), conflicts.c_str());

		// assign fungible res conflicts to the slots that have non-fungible conflicts first
		for (auto rip : active) {
			int d_verb = (conflicts.empty() ? D_VERBOSE : 0);
			bool nft_conflict = num_nft_conflict-- > 0;
			dprintf(D_ZKM | d_verb, "assigning conflicts %s to %s%s\n",
				conflicts.c_str(), rip->r_name,
				nft_conflict ? "  which already has NFT conflicts" : "");

			rip->set_res_conflict(conflicts);
			totbag += *rip->r_attr;
			dprintf(D_ZKM | d_verb, "conflicts ADD %s %s\n", rip->r_name, totbag.dump(dumptmp));
			conflicts.clear();
			totbag.underrun(&conflicts);
		}
	} else {
		dprintf(D_ZKM | D_VERBOSE, "resource_conflicts(%d NFT of %d active) : %s\n",  num_nft_conflict, (int)active.size(), conflicts.c_str());
	}

	return false;
}


ExprTree * globalDrainingStartExpr = NULL;

bool
ResMgr::startDraining(
	int how_fast,
	time_t deadline,
	const std::string & reason,
	int on_completion,
	ExprTree *check_expr,
	ExprTree *start_expr,
	std::string &new_request_id,
	std::string &error_msg,
	int &error_code)
{
	// For now, let's assume that that you never want to change the start
	// expression while draining.
	if( draining ) {
		new_request_id = "";
		error_msg = "Draining already in progress.";
		error_code = DRAINING_ALREADY_IN_PROGRESS;
		return false;
	}

	if( check_expr ) {
		for (Resource * rip : slots) {
			classad::Value v;
			bool check_ok = false;
			classad::EvalState eval_state;
			eval_state.SetScopes( rip->r_classad );
			if( !check_expr->Evaluate( eval_state, v ) ) {
				formatstr(error_msg,"Failed to evaluate draining check expression against %s.", rip->r_name );
				error_code = DRAINING_CHECK_EXPR_FAILED;
				return false;
			}
			if( !v.IsBooleanValue(check_ok) ) {
				formatstr(error_msg,"Draining check expression does not evaluate to a bool on %s.", rip->r_name );
				error_code = DRAINING_CHECK_EXPR_FAILED;
				return false;
			}
			if( !check_ok ) {
				formatstr(error_msg,"Draining check expression is false on %s.", rip->r_name );
				error_code = DRAINING_CHECK_EXPR_FAILED;
				return false;
			}
		}
	}

	draining = true;
	last_drain_start_time = time(NULL);
	draining_id += 1;
	formatstr(new_request_id,"%d",draining_id);
	this->on_completion_of_draining = on_completion;
	this->drain_reason = reason;
	this->drain_deadline = deadline;

	// Insert draining attributes into the resource ads, in case the
	// retirement expression uses them.
	for (Resource * rip : slots) {
		ClassAd &ad = *(rip->r_classad);
		// put these into the resources ClassAd now, they are also set by this->publish
		ad.InsertAttr( ATTR_DRAIN_REASON, reason );
		ad.InsertAttr( ATTR_DRAINING, true );
		ad.InsertAttr( ATTR_DRAINING_REQUEST_ID, new_request_id );
		if (deadline) { ad.InsertAttr(ATTR_DRAINING_DEADLINE, deadline); } else { ad.Delete(ATTR_DRAINING_DEADLINE); }
		ad.InsertAttr( ATTR_LAST_DRAIN_START_TIME, last_drain_start_time );
		ad.InsertAttr( ATTR_LAST_DRAIN_STOP_TIME, last_drain_stop_time );
	}

	if( how_fast <= DRAIN_GRACEFUL ) {
			// retirement time and vacate time are honored
		draining_is_graceful = true;
		int graceful_retirement = gracefulDrainingTimeRemaining();
		dprintf(D_ALWAYS,"Initiating graceful draining.\n");
		if( graceful_retirement > 0 ) {
			dprintf(D_ALWAYS,
					"Coordinating retirement of draining slots; retirement of all draining slots ends in %ds.\n",
					graceful_retirement);
		}

			// we do not know yet if these jobs will be evicted or if
			// they will finish within their retirement time, so do
			// not call setBadputCausedByDraining() yet

		// Even if we could pass start_expr through walk(), it turns out,
		// beceause ResState::enter_action() calls unavail() as well, we
		// really do need to keep the global.  To that end, we /want/ to
		// assign the NULL value here if that's what we got, so that we
		// do the right thing if we drain without a START expression after
		// draining with one.
		delete globalDrainingStartExpr;
		if (start_expr) {
			globalDrainingStartExpr = start_expr->Copy();
		} else {
			ConstraintHolder start(param("DEFAULT_DRAINING_START_EXPR"));
			if (!start.empty() && !start.Expr()) {
				dprintf(D_ALWAYS, "Warning: DEFAULT_DRAINING_START_EXPR is not valid : %s\n", start.c_str());
			}
			// if empty or invalid, detach() returns NULL, which is what we want here if the expr is invalid
			globalDrainingStartExpr = start.detach();
		}
		releaseAllClaimsReversibly();
	}
	else if( how_fast <= DRAIN_QUICK ) {
			// retirement time will not be honored, but vacate time will
		dprintf(D_ALWAYS,"Initiating quick draining.\n");
		draining_is_graceful = false;
		walk(&Resource::setBadputCausedByDraining);
		releaseAllClaims();
	}
	else if( how_fast > DRAIN_QUICK ) { // DRAIN_FAST
			// neither retirement time nor vacate time will be honored
		dprintf(D_ALWAYS,"Initiating fast draining.\n");
		draining_is_graceful = false;
		walk(&Resource::setBadputCausedByDraining);
		killAllClaims();
	}

	update_all();
	return true;
}

bool
ResMgr::cancelDraining(std::string request_id,bool reconfig,std::string &error_msg,int &error_code)
{
	if( !this->draining ) {
		if( request_id.empty() ) {
			return true;
		}
	}

	if( !request_id.empty() && atoi(request_id.c_str()) != this->draining_id ) {
		formatstr(error_msg,"No matching draining request id %s.",request_id.c_str());
		error_code = DRAINING_NO_MATCHING_REQUEST_ID;
		return false;
	}

	draining = false;
	drain_reason.clear();
	// If we want to record when a non-resuming drain actually finished, we
	// should only call this here if we've started draining since the last
	// time we stopped.
	// if( last_drain_start_time > last_drain_stop_time ) { setLastDrainStopTime(); }
	setLastDrainStopTime();

	walk(&Resource::enable);
	if (reconfig) {
		main_config();
	} else {
		update_all();
	}
	return true;
}

bool
ResMgr::isSlotDraining(Resource * /*rip*/) const
{
	// NOTE: passed in rip will be NULL when building the daemon ad
	return draining;
}

int
ResMgr::gracefulDrainingTimeRemaining()
{
	return gracefulDrainingTimeRemaining(NULL);
}

int
ResMgr::gracefulDrainingTimeRemaining(Resource * /*rip*/)
{
	if( !draining || !draining_is_graceful ) {
		return 0;
	}

		// If we have 100s of slots, we may want to cache the
		// result of the following computation to avoid
		// poor scaling.  For now, we just compute it every
		// time.

	int longest_retirement_remaining = 0;
	for (Resource * rip : slots) {
		// The max job retirement time of jobs accepted while draining is
		// implicitly zero.  Otherwise, we'd need to record the result of
		// this computation at the instant we entered draining state and
		// set a timer to vacate all slots at that point.  This would
		// probably be more efficient, but would be a small semantic change,
		// because jobs would no longer be able to voluntarily reduce their
		// max job retirement time after retirement began.
		if(! rip->wasAcceptedWhileDraining()) {
			int retirement_remaining = rip->evalRetirementRemaining();
			if( retirement_remaining > longest_retirement_remaining ) {
				longest_retirement_remaining = retirement_remaining;
			}
		}
	}
	return longest_retirement_remaining;
}

bool
ResMgr::drainingIsComplete(Resource * /*rip*/)
{
	if( !draining ) {
		return false;
	}

	for (Resource * rip : slots) {
		if (rip->state() != drained_state) { return false; }
	}
	return true;
}

bool
ResMgr::considerResumingAfterDraining()
{
	if( !draining || !on_completion_of_draining ) {
		return false;
	}

	for (Resource * rip : slots) {
		if (rip->state() != drained_state || rip->activity() != idle_act) { return false; }
	}

	bool reconfig = false;
	if (on_completion_of_draining == DRAIN_RECONFIG_ON_COMPLETION) {
		reconfig = true;
	}
	else
	if (on_completion_of_draining != DRAIN_RESUME_ON_COMPLETION) {
		bool restart = (on_completion_of_draining != DRAIN_EXIT_ON_COMPLETION);
		dprintf(D_ALWAYS,"As specified in draining request, %s after completion of draining.\n",
			restart ? "restarting" : "exiting");
		const bool fast = false;
		daemonCore->beginDaemonRestart(fast, restart);
		return true;
	}

	dprintf(D_ALWAYS,"As specified in draining request, resuming normal operation after completion of draining.\n");
	std::string error_msg;
	int error_code = 0;
	if( !cancelDraining("",reconfig,error_msg,error_code) ) {
			// should never happen!
		EXCEPT("failed to cancel draining: (code %d) %s",error_code,error_msg.c_str());
	}
	return true;
}

void
ResMgr::publish_draining_attrs(Resource *rip, ClassAd *cap)
{
	if( isSlotDraining(rip) ) {
		cap->Assign( ATTR_DRAINING, true );
		cap->Assign(ATTR_DRAIN_REASON, this->drain_reason);

		std::string request_id;
		if (draining) { formatstr(request_id, "%d", draining_id); }
		cap->Assign( ATTR_DRAINING_REQUEST_ID, request_id );
	}
	else {
		// in case we are writing into resource->r_classad, do a deep delete
		caDeleteThruParent(cap, ATTR_DRAINING);
		caDeleteThruParent(cap, ATTR_DRAIN_REASON);
		caDeleteThruParent(cap, ATTR_DRAINING_REQUEST_ID );
	}

	cap->Assign( ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_BADPUT, expected_graceful_draining_badput );
	cap->Assign( ATTR_EXPECTED_MACHINE_QUICK_DRAINING_BADPUT, expected_quick_draining_badput );
	cap->Assign( ATTR_EXPECTED_MACHINE_GRACEFUL_DRAINING_COMPLETION, expected_graceful_draining_completion );
	cap->Assign( ATTR_EXPECTED_MACHINE_QUICK_DRAINING_COMPLETION, expected_quick_draining_completion );
	if( total_draining_badput ) {
		cap->Assign( ATTR_TOTAL_MACHINE_DRAINING_BADPUT, total_draining_badput );
	}
	if( total_draining_unclaimed ) {
		cap->Assign( ATTR_TOTAL_MACHINE_DRAINING_UNCLAIMED_TIME, total_draining_unclaimed );
	}
	if( last_drain_start_time != 0 ) {
		cap->Assign( ATTR_LAST_DRAIN_START_TIME, last_drain_start_time );
	}
	if( last_drain_stop_time != 0 ) {
	    cap->Assign( ATTR_LAST_DRAIN_STOP_TIME, last_drain_stop_time );
	}
}

void
ResMgr::compute_draining_attrs()
{
		// Using long long for int math in this function so
		// MaxJobRetirementTime=MAX_INT or MaxVacateTime=MAX_INT do
		// not cause overflow.
	long long ll_expected_graceful_draining_completion = 0;
	long long ll_expected_quick_draining_completion = 0;
	long long ll_expected_graceful_draining_badput = 0;
	long long ll_expected_quick_draining_badput = 0;
	long long ll_total_draining_unclaimed = 0;
	bool is_drained = true;

	for (Resource * rip : slots) {
		if( rip->r_cur ) {
			long long runtime = rip->r_cur->getJobTotalRunTime();
			long long retirement_remaining = rip->evalRetirementRemaining();
			long long max_vacate_time = rip->evalMaxVacateTime();
			long long cpus = rip->r_attr->num_cpus();

			if (rip->r_cur->isActive()) { is_drained = false; }

			ll_expected_quick_draining_badput += cpus*(runtime + max_vacate_time);
			ll_expected_graceful_draining_badput += cpus*runtime;

			int graceful_time_remaining;
			if( retirement_remaining < max_vacate_time ) {
					// vacate would happen immediately
				graceful_time_remaining = max_vacate_time;
			}
			else {
					// vacate would be delayed to finish by end of retirement
				graceful_time_remaining = retirement_remaining;
			}

			ll_expected_graceful_draining_badput += cpus*graceful_time_remaining;
			if( graceful_time_remaining > ll_expected_graceful_draining_completion ) {
				ll_expected_graceful_draining_completion = graceful_time_remaining;
			}
			if( max_vacate_time > ll_expected_quick_draining_completion ) {
				ll_expected_quick_draining_completion = max_vacate_time;
			}

			ll_total_draining_unclaimed += rip->r_state->timeDrainingUnclaimed();
		}
	}

	if (is_drained) {
		// once the slot is drained we only want to change the expected completion time
		// if we have never set it before, or if we finished draining early.
		if (0 == expected_graceful_draining_completion || expected_graceful_draining_completion > cur_time)
			expected_graceful_draining_completion = cur_time;
		if (0 == expected_quick_draining_completion || expected_quick_draining_completion > cur_time)
			expected_quick_draining_completion = cur_time;
	} else {
			// convert time estimates from relative time to absolute time
		ll_expected_graceful_draining_completion += cur_time;
		ll_expected_quick_draining_completion += cur_time;
		expected_graceful_draining_completion = cap_int(ll_expected_graceful_draining_completion);
		expected_quick_draining_completion = cap_int(ll_expected_quick_draining_completion);
	}

	expected_graceful_draining_badput = cap_int(ll_expected_graceful_draining_badput);
	expected_quick_draining_badput = cap_int(ll_expected_quick_draining_badput);
	total_draining_unclaimed = cap_int(ll_total_draining_unclaimed);
}

void
ResMgr::addToDrainingBadput( int badput )
{
	total_draining_badput += badput;
}

void
ResMgr::adlist_reset_monitors( unsigned r_id, ClassAd * forWhom ) {
	extra_ads.reset_monitors( r_id, forWhom );
}

void
ResMgr::adlist_unset_monitors( unsigned r_id, ClassAd * forWhom ) {
	extra_ads.unset_monitors( r_id, forWhom );
}

void
ResMgr::checkForDrainCompletion() {
	if ( ! numSlots()) { return; }

	bool allAcceptedWhileDraining = true;
	for (Resource * rip : slots) {
		if(! rip->wasAcceptedWhileDraining()) {
			// Not sure how COD and draining are supposed to interact, but
			// the partitionable slot is never accepted-while-draining,
			// nor should it block drain from completing.
			if(! rip->hasAnyClaim()) { continue; }
			if(rip->is_partitionable_slot()) { continue; }
			allAcceptedWhileDraining = false;
		}
	}
	if(! allAcceptedWhileDraining) { return; }

	dprintf( D_ALWAYS, "Initiating final draining (all original jobs complete).\n" );
	// This (auto-reversibly) sets START to false when we release all claims.
	delete globalDrainingStartExpr;
	globalDrainingStartExpr = NULL;
	// Invalidate all claim IDs.  This prevents the schedd from claiming
	// resources that were negotiated before draining finished.
	walk( &Resource::invalidateAllClaimIDs );
	// Set MAXJOBRETIREMENTTIME to 0.  This will be reset in ResState::eval()
	// when draining completes.
	this->max_job_retirement_time_override = 0;
	walk( & Resource::refresh_draining_attrs );
	// Initiate final draining.
	releaseAllClaimsReversibly();
}

void
ResMgr::printSlotAds(const char * slot_types) const
{
	// potentially filter by types if the types are defined.  otherwise print all.
	std::set<Resource::ResourceFeature> filter;
	if (slot_types) {
		// check the filter to see if we will print
		dprintf(D_FULLDEBUG, "Filtering ads to %s\n", slot_types);
		StringList sl(slot_types);
		if(sl.contains_anycase("static")) { filter.insert(Resource::STANDARD_SLOT); }
		if(sl.contains_anycase("partitionable")) { filter.insert(Resource::PARTITIONABLE_SLOT); }
		if(sl.contains_anycase("dynamic")) { filter.insert(Resource::DYNAMIC_SLOT); }
	}

	for (Resource * rip : slots) {
		if (filter.empty() || filter.count(rip->get_feature())) {
			rip->dropAdInLogFile();
		}
	}
}


void
ResMgr::token_request_callback(bool success, void *miscdata)
{
	auto self = reinterpret_cast<ResMgr *>(miscdata);
		// In the successful case, trigger an update to the collector for all ads
	if (success) {
		self->eval_and_update_all();
	}
}

bool OtherSlotEval( const char * name,
	const classad::ArgumentList &arg_list,
	classad::EvalState &state,
	classad::Value &result)
{
	classad::Value arg;
	std::string slotname;

	ASSERT( resmgr );

	dprintf(D_MACHINE|D_VERBOSE, "OtherSlotEval called\n");

	// Must have two argument
	if ( arg_list.size() != 2 ) {
		result.SetErrorValue();
		return( true );
	}

	// Evaluate slotname argument
	if( !arg_list[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue();
		return false;
	}

	// If argument isn't a string, then the result is an error.
	if( !arg.IsStringValue( slotname ) ) {
		result.SetErrorValue();
		return true;
	}

	// this is an invocation intended to produce a slot<n>_<attr> name
	if (*name == '*') {
		classad::ExprTree * expr = arg_list[1];
		if (! expr) {
			result.SetErrorValue();
		} else {
			std::string attr("");
			if (!ExprTreeIsAttrRef(expr, attr)) {
				attr = "expr_";
			}
			slotname += "_";
			slotname += attr;
			result.SetStringValue(slotname);
		}
		return true;
	}

	Resource* res = resmgr->get_by_name_prefix(slotname.c_str());
	if (! res) {
		result.SetUndefinedValue();
		dprintf(D_MACHINE|D_VERBOSE, "OtherSlotEval(%s) - slot not found\n", slotname.c_str());
	} else {
		classad::ExprTree * expr = arg_list[1];
		if (! expr) {
			result.SetErrorValue();
			dprintf(D_MACHINE|D_VERBOSE, "OtherSlotEval(%s) - empty expr\n", slotname.c_str());
		} else {
			std::string attr("");
			if (ExprTreeIsAttrRef(expr, attr) && starts_with_ignore_case(attr, "Child") && false) {	 // fetch attr, but disable special Child* processing
				attr = attr.substr(5); // strip "Child" prefix
			#if 0 // TODO: parse expr and insert it into result value, or change rollup so it returns an ExprList?
				std::string expr;
				res->rollupChildAttrs(expr, attr);
				classad_shared_ptr<classad::ExprList> lst( new classad::ExprList() );
				ASSERT(lst);
				//lst->push_back(classad::Literal::MakeLiteral(first));
				result.SetSListValue(lst);
			#endif
			} else {
				const classad::ClassAd * parent = expr->GetParentScope();
				res->r_classad->EvaluateExpr(expr, result);
				expr->SetParentScope(parent); // put the parent scope back to where it was

				if (IsDebugCatAndVerbosity((D_MACHINE|D_VERBOSE))) {
					dprintf(D_MACHINE|D_VERBOSE, "OtherSlotEval(%s,expr) %s evalutes to %s\n",
							slotname.c_str(), attr.c_str(), ClassAdValueToString(result));
				}
			}
		}
	}
	return true;
}

// check to see if an expr tree is just a single SlotEval function call
bool ExprTreeIsSlotEval(classad::ExprTree * tree)
{
	if (! tree || tree->GetKind() != classad::ExprTree::FN_CALL_NODE)
		return false;
	std::string fnName;
	std::vector<classad::ExprTree*> args;
	((const classad::FunctionCall*)tree)->GetComponents( fnName, args );
	return (MATCH == strcasecmp(fnName.c_str(), "SlotEval"));
}

// walk an ExprTree, calling a function each time a ATTRREF_NODE is found.
//
int ExprHasSlotEval(classad::ExprTree * tree)
{
	int iret = 0;
	if ( ! tree) return 0;
	switch (tree->GetKind()) {
	case classad::ExprTree::LITERAL_NODE:
	break;

	case classad::ExprTree::ATTRREF_NODE: {
		const classad::AttributeReference* atref = reinterpret_cast<const classad::AttributeReference*>(tree);
		classad::ExprTree *expr;
		std::string ref;
		std::string tmp;
		bool absolute;
		atref->GetComponents(expr, ref, absolute);
		// if there is a non-trivial left hand side (something other than X from X.Y attrib ref)
		// then recurse it.
		if (expr && ! ExprTreeIsAttrRef(expr, tmp)) {
			iret += ExprHasSlotEval(expr);
		}
	}
	break;

	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind	op;
		classad::ExprTree *t1, *t2, *t3;
		((const classad::Operation*)tree)->GetComponents( op, t1, t2, t3 );
		if (t1) iret += ExprHasSlotEval(t1);
		//if (iret && stop_on_first_match) return iret;
		if (t2) iret += ExprHasSlotEval(t2);
		//if (iret && stop_on_first_match) return iret;
		if (t3) iret += ExprHasSlotEval(t3);
	}
	break;

	case classad::ExprTree::FN_CALL_NODE: {
		std::string fnName;
		std::vector<classad::ExprTree*> args;
		((const classad::FunctionCall*)tree)->GetComponents( fnName, args );
		if (MATCH == strcasecmp(fnName.c_str(), "SlotEval")) {
			iret += 1;
			break; // no need to look deeper
		}
		for (std::vector<classad::ExprTree*>::iterator it = args.begin(); it != args.end(); ++it) {
			iret += ExprHasSlotEval(*it);
			if (iret) return iret;
		}
	}
	break;

	case classad::ExprTree::CLASSAD_NODE: {
		std::vector< std::pair<std::string, classad::ExprTree*> > attrs;
		((const classad::ClassAd*)tree)->GetComponents(attrs);
		for (std::vector< std::pair<std::string, classad::ExprTree*> >::iterator it = attrs.begin(); it != attrs.end(); ++it) {
			iret += ExprHasSlotEval(it->second);
			if (iret) return iret;
		}
	}
	break;

	case classad::ExprTree::EXPR_LIST_NODE: {
		std::vector<classad::ExprTree*> exprs;
		((const classad::ExprList*)tree)->GetComponents( exprs );
		for (std::vector<classad::ExprTree*>::iterator it = exprs.begin(); it != exprs.end(); ++it) {
			iret += ExprHasSlotEval(*it);
			if (iret) return iret;
		}
	}
	break;

	case classad::ExprTree::EXPR_ENVELOPE: {
		classad::ExprTree * expr = SkipExprEnvelope(const_cast<classad::ExprTree*>(tree));
		if (expr) iret += ExprHasSlotEval(expr);
	}
	break;

	default:
		// unknown or unallowed node.
		ASSERT(0);
		break;
	}
	return iret;
}
