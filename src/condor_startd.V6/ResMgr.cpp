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
#include "startd.h"
#include "startd_hibernator.h"
#include "startd_named_classad_list.h"
#include "classad_merge.h"
#include "vm_common.h"
#include "VMRegister.h"
#include "overflow.h"
#include <math.h>

#include "slot_builder.h"

ResMgr::ResMgr()
{
	totals_classad = NULL;
	config_classad = NULL;
	up_tid = -1;
	poll_tid = -1;

	draining = false;
	draining_is_graceful = false;
	resume_on_completion_of_draining = false;
	draining_id = 0;
	last_drain_start_time = 0;
	expected_graceful_draining_completion = 0;
	expected_quick_draining_completion = 0;
	expected_graceful_draining_badput = 0;
	expected_quick_draining_badput = 0;
	total_draining_badput = 0;
	total_draining_unclaimed = 0;

	m_attr = new MachAttributes;

#if HAVE_BACKFILL
	m_backfill_mgr = NULL;
	m_backfill_shutdown_pending = false;
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
	MyString	states;
	m_hibernation_manager->getSupportedStates(states);
	dprintf( D_FULLDEBUG,
			 "Detected hibernation states: %s\n", states.Value() );

	m_hibernating = FALSE;
#endif

	id_disp = NULL;

	nresources = 0;
	resources = NULL;
	type_nums = NULL;
	new_type_nums = NULL;
	is_shutting_down = false;
	cur_time = last_in_use = time( NULL );

	max_types = 0;
	num_updates = 0;
	startTime = 0;
	type_strings = NULL;
	m_startd_hook_shutdown_pending = false;
}

void ResMgr::Stats::Init()
{
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", Compute, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", WalkEvalState, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", WalkUpdate, IF_VERBOSEPUB);
   STATS_POOL_ADD(daemonCore->dc_stats.Pool, "ResMgr", WalkOther, IF_VERBOSEPUB);
}

double ResMgr::Stats::BeginRuntime(stats_recent_counter_timer &  /*probe*/)
{
   return UtcTime::getTimeDouble();
}

double ResMgr::Stats::EndRuntime(stats_recent_counter_timer & probe, double before)
{
   double now = UtcTime::getTimeDouble();
   probe.Add(now - before);
   return now;
}

double ResMgr::Stats::BeginWalk(VoidResourceMember  /*memberfunc*/)
{
   return UtcTime::getTimeDouble();
}

double ResMgr::Stats::EndWalk(VoidResourceMember memberfunc, double before)
{
    stats_recent_counter_timer * probe = &WalkOther;
    if (memberfunc == &Resource::update) 
       probe = &WalkUpdate;
    else if (memberfunc == &Resource::eval_state)
       probe = &WalkEvalState;
    return EndRuntime(*probe, before);
}


ResMgr::~ResMgr()
{
	int i;
	if( config_classad ) delete config_classad;
	if( totals_classad ) delete totals_classad;
	if( id_disp ) delete id_disp;
	delete m_attr;

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


	if( resources ) {
		for( i = 0; i < nresources; i++ ) {
			delete resources[i];
		}
		delete [] resources;
	}
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
		MyString default_expr;
		default_expr.formatstr("MY.%s =!= UNDEFINED",ATTR_MACHINE_LAST_MATCH_TIME);
		config_classad->AssignExpr( ATTR_UNHIBERNATE, default_expr.Value() );
	}
#endif /* HAVE_HIBERNATION */

	if( !configInsert( config_classad, ATTR_SLOT_WEIGHT, false ) ) {
		config_classad->AssignExpr( ATTR_SLOT_WEIGHT, ATTR_CPUS );
	}

		// Next, try the IS_OWNER expression.  If it's not there, give
		// them a resonable default, instead of leaving it undefined.
	if( ! configInsert(config_classad, ATTR_IS_OWNER, false) ) {
		config_classad->AssignExpr( ATTR_IS_OWNER, "(START =?= False)" );
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

#if !defined(WANT_OLD_CLASSADS)
	config_classad->AddTargetRefs( TargetJobAttrs, false );
#endif
}


#if HAVE_BACKFILL

void
ResMgr::backfillMgrDone()
{
	ASSERT( m_backfill_mgr );
	dprintf( D_FULLDEBUG, "BackfillMgr now ready to be deleted\n" );
	delete m_backfill_mgr;
	m_backfill_mgr = NULL;
	m_backfill_shutdown_pending = false;

		// We should call backfillConfig() again, since now that the
		// "old" manager is gone, we might want to allocate a new one
	backfillConfig();
}


static bool
verifyBackfillSystem( const char* sys )
{

#if HAVE_BOINC
	if( ! strcasecmp(sys, "BOINC") ) {
		return true;
	}
#endif /* HAVE_BOINC */

	return false;
}


bool
ResMgr::backfillConfig()
{
	if( m_backfill_shutdown_pending ) {
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
				m_backfill_shutdown_pending = true;
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
				m_backfill_shutdown_pending = true;
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


void
ResMgr::init_resources( void )
{
	int i, num_res;
	CpuAttributes** new_cpu_attrs;

    stats.Init();

    m_attr->init_machine_resources();

		// These things can only be set once, at startup, so they
		// don't need to be in build_cpu_attrs() at all.
	if (param_boolean("ALLOW_VM_CRUFT", false)) {
		max_types = param_integer("MAX_SLOT_TYPES",
								  param_integer("MAX_VIRTUAL_MACHINE_TYPES",
												10));
	} else {
		max_types = param_integer("MAX_SLOT_TYPES", 10);
	}

	max_types += 1;

		// The reason this isn't on the stack is b/c of the variable
		// nature of max_types. *sigh*
	type_strings = new StringList*[max_types];
	memset( type_strings, 0, (sizeof(StringList*) * max_types) );

		// Fill in the type_strings array with all the appropriate
		// string lists for each type definition.  This only happens
		// once!  If you change the type definitions, you must restart
		// the startd, or else too much weirdness is possible.
	initTypes( max_types, type_strings, 1 );

		// First, see how many slots of each type are specified.
	num_res = countTypes( max_types, num_cpus(), &type_nums, true );

	if( ! num_res ) {
			// We're not configured to advertise any nodes.
		resources = NULL;
		id_disp = new IdDispenser( num_cpus(), 1 );
		return;
	}

		// See if the config file allows for a valid set of
		// CpuAttributes objects.  Since this is the startup-code
		// we'll let it EXCEPT() if there is an error.
	new_cpu_attrs = buildCpuAttrs( m_attr, max_types, type_strings, num_res, type_nums, true );
	if( ! new_cpu_attrs ) {
		EXCEPT( "buildCpuAttrs() failed and should have already EXCEPT'ed" );
	}

		// Now, we can finally allocate our resources array, and
		// populate it.
	for( i=0; i<num_res; i++ ) {
		addResource( new Resource( new_cpu_attrs[i], i+1, num_res>1 ) );
	}

		// We can now seed our IdDispenser with the right slot id.
	id_disp = new IdDispenser( num_cpus(), i+1 );

		// Finally, we can free up the space of the new_cpu_attrs
		// array itself, now that all the objects it was holding that
		// we still care about are stashed away in the various
		// Resource objects.  Since it's an array of pointers, this
		// won't touch the objects at all.
	delete [] new_cpu_attrs;

#if HAVE_BACKFILL
	backfillConfig();
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr = new StartdHookMgr;
	m_hook_mgr->initialize();
#endif
}


bool
ResMgr::typeNumCmp( int* a, int* b )
{
	int i;
	for( i=0; i<max_types; i++ ) {
		if( a[i] != b[i] ) {
			return false;
		}
	}
	return true;
}

bool
ResMgr::reconfig_resources( void )
{
	int t, i, cur, num;
	CpuAttributes** new_cpu_attrs;
	int max_num = num_cpus();
	int* cur_type_index;
	Resource*** sorted_resources;	// Array of arrays of pointers.
	Resource* rip;

#if HAVE_BACKFILL
	backfillConfig();
#endif

#if HAVE_JOB_HOOKS
	m_hook_mgr->reconfig();
#endif

#if HAVE_HIBERNATION
	updateHibernateConfiguration();
#endif /* HAVE_HIBERNATE */

		// Tell each resource to reconfig itself.
	walk(&Resource::reconfig);

		// See if any new types were defined.  Don't except if there's
		// any errors, just dprintf().
	initTypes( max_types, type_strings, 0 );

		// First, see how many slots of each type are specified.
	num = countTypes( max_types, num_cpus(), &new_type_nums, false );

	if( typeNumCmp(new_type_nums, type_nums) ) {
			// We want the same number of each slot type that we've got
			// now.  We're done!
		delete [] new_type_nums;
		new_type_nums = NULL;
		return true;
	}

		// See if the config file allows for a valid set of
		// CpuAttributes objects.
	new_cpu_attrs = buildCpuAttrs( m_attr, max_types, type_strings, num, new_type_nums, false );
	if( ! new_cpu_attrs ) {
			// There was an error, abort.  We still return true to
			// indicate that we're done doing our thing...
		dprintf( D_ALWAYS, "Aborting slot type reconfig.\n" );
		delete [] new_type_nums;
		new_type_nums = NULL;
		return true;
	}

		////////////////////////////////////////////////////
		// Sort all our resources by type and state.
		////////////////////////////////////////////////////

		// Allocate and initialize our arrays.
	sorted_resources = new Resource** [max_types];
	ASSERT( sorted_resources != NULL );
	for( i=0; i<max_types; i++ ) {
		sorted_resources[i] = new Resource* [max_num];
		memset( sorted_resources[i], 0, (max_num*sizeof(Resource*)) );
	}

	cur_type_index = new int [max_types];
	memset( cur_type_index, 0, (max_types*sizeof(int)) );

		// Populate our sorted_resources array by type.
	for( i=0; i<nresources; i++ ) {
		t = resources[i]->type();
		(sorted_resources[t])[cur_type_index[t]] = resources[i];
		cur_type_index[t]++;
	}

		// Now, for each type, sort our resources by state.
	for( t=0; t<max_types; t++ ) {
		ASSERT( cur_type_index[t] == type_nums[t] );
		qsort( sorted_resources[t], type_nums[t],
			   sizeof(Resource*), &claimedRankCmp );
	}

		////////////////////////////////////////////////////
		// Decide what we need to do.
		////////////////////////////////////////////////////
	cur = -1;
	for( t=0; t<max_types; t++ ) {
		for( i=0; i<new_type_nums[t]; i++ ) {
			cur++;
			if( ! (sorted_resources[t])[i] ) {
					// If there are no more existing resources of this
					// type, we'll need to allocate one.
				alloc_list.Append( new_cpu_attrs[cur] );
				continue;
			}
			if( (sorted_resources[t])[i]->type() ==
				new_cpu_attrs[cur]->type() ) {
					// We've already got a Resource for this slot, so we
					// can delete it.
				delete new_cpu_attrs[cur];
				continue;
			}
		}
			// We're done with the new slots of this type.  See if there
			// are any Resources left over that need to be destroyed.
		for( ; i<max_num; i++ ) {
			if( (sorted_resources[t])[i] ) {
				destroy_list.Append( (sorted_resources[t])[i] );
			} else {
				break;
			}
		}
	}

		////////////////////////////////////////////////////
		// Finally, act on our decisions.
		////////////////////////////////////////////////////

		// Everything we care about in new_cpu_attrs is saved
		// elsewhere, and the rest has already been deleted, so we
		// should now delete the array itself.
	delete [] new_cpu_attrs;

		// Cleanup our memory.
	for( i=0; i<max_types; i++ ) {
		delete [] sorted_resources[i];
	}
	delete [] sorted_resources;
	delete [] cur_type_index;

		// See if there's anything to destroy, and if so, do it.
	destroy_list.Rewind();
	while( destroy_list.Next(rip) ) {
		rip->dprintf( D_ALWAYS,
					  "State change: resource no longer needed by configuration\n" );
		rip->set_destination_state( delete_state );
	}

		// Finally, call our helper, so that if all the slots we need to
		// get rid of are gone by now, we'll allocate the new ones.
	return processAllocList();
}

void
ResMgr::walk( VoidResourceMember memberfunc )
{
	if( ! resources ) {
		return;
	}

    double currenttime = stats.BeginWalk(memberfunc);

		// Because the memberfunc might be an eval function, it can
		// result in resources being deleted. This means a straight
		// for loop on nresources will miss one resource for every one
		// deleted. To combat that, we copy the array and nresources
		// and iterate over it instead.
	int ncache = nresources;
	Resource **cache = new Resource*[ncache];
	memcpy((void*)cache, (void*)resources, (sizeof(Resource*)*ncache));

	for( int i = 0; i < ncache; i++ ) {
		(cache[i]->*(memberfunc))();
	}

	delete [] cache;

    stats.EndWalk(memberfunc, currenttime);
}


void
ResMgr::walk( ResourceMaskMember memberfunc, amask_t mask )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))(mask);
	}
}


float
ResMgr::sum( ResourceFloatMember memberfunc )
{
	if( ! resources ) {
		return 0;
	}
	int i;
	float tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


void
ResMgr::resource_sort( ComparisonFunc compar )
{
	if( ! resources ) {
		return;
	}
	if( nresources > 1 ) {
		qsort( resources, nresources, sizeof(Resource*), compar );
	}
}


// Methods to manipulate the supplemental ClassAd list
int
ResMgr::adlist_register( StartdNamedClassAd *ad )
{
	return extra_ads.Register( ad );
}

int
ResMgr::adlist_replace( const char *name, ClassAd *newAd, bool report_diff )
{
	if( report_diff ) {
		StringList ignore_list;
		MyString ignore = name;
		ignore += "_LastUpdate";
		ignore_list.append( ignore.Value() );
		return extra_ads.Replace( name, newAd, true, &ignore_list );
	}
	else {
		return extra_ads.Replace( name, newAd );
	}
}

int
ResMgr::adlist_delete( const char *name )
{
	return extra_ads.Delete( name );
}

int
ResMgr::adlist_publish( unsigned r_id, ClassAd *resAd, amask_t mask )
{
	// Check the mask
	if (  ( mask & ( A_PUBLIC | A_UPDATE ) ) != ( A_PUBLIC | A_UPDATE )  ) {
		return 0;
	}

	return extra_ads.Publish( resAd, r_id );
}


bool
ResMgr::needsPolling( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->needsPolling() ) {
			return true;
		}
	}
	return false;
}


bool
ResMgr::hasAnyClaim( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->hasAnyClaim() ) {
			return true;
		}
	}
	return false;
}


Claim*
ResMgr::getClaimByPid( pid_t pid )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByPid(pid)) ) {
			return foo;
		}
	}
	return NULL;
}


Claim*
ResMgr::getClaimById( const char* id )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimById(id)) ) {
			return foo;
		}
	}
	return NULL;
}


Claim*
ResMgr::getClaimByGlobalJobId( const char* id )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByGlobalJobId(id)) ) {
			return foo;
		}
	}
	return NULL;
}

Claim *
ResMgr::getClaimByGlobalJobIdAndId( const char *job_id,
									const char *claimId)
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByGlobalJobId(job_id)) ) {
			if( foo == resources[i]->findClaimById(claimId) ) {
				return foo;
			}
		}
	}
	return NULL;

}


Resource*
ResMgr::findRipForNewCOD( ClassAd* ad )
{
	if( ! resources ) {
		return NULL;
	}
	int requirements;
	int i;

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

		// sort resources based on the above order
	resource_sort( newCODClaimCmp );

		// find the first one that matches our requirements
	for( i = 0; i < nresources; i++ ) {
		if( ad->EvalBool( ATTR_REQUIREMENTS, resources[i]->r_classad,
						  requirements ) == 0 ) {
			requirements = 0;
		}
		if( requirements ) {
			return resources[i];
		}
	}
	return NULL;
}



Resource*
ResMgr::get_by_cur_id( char* id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->idMatches(id) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_id( char* id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->idMatches(id) ) {
			return resources[i];
		}
		if( resources[i]->r_pre &&
			resources[i]->r_pre->idMatches(id) ) {
			return resources[i];
		}
		if( resources[i]->r_pre_pre &&
			resources[i]->r_pre_pre->idMatches(id) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_name( char* name )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( !strcmp(resources[i]->r_name, name) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_slot_id( int id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_id == id ) {
			return resources[i];
		}
	}
	return NULL;
}


State
ResMgr::state( void )
{
	if( ! resources ) {
		return owner_state;
	}
	State s = no_state;
	Resource* rip;
	int i, is_owner = 0;
	for( i = 0; i < nresources; i++ ) {
		rip = resources[i];
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
	if( ! resources ) {
		return;
	}
	walk( &Resource::final_update );
}


int
ResMgr::send_update( int cmd, ClassAd* public_ad, ClassAd* private_ad,
					 bool nonblock )
{
		// Increment the resmgr's count of updates.
	num_updates++;
		// Actually do the updates, and return the # of updates sent.
	return daemonCore->sendUpdates(cmd, public_ad, private_ad, nonblock);
}


void
ResMgr::update_all( void )
{
	num_updates = 0;

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

		// Evaluate the state of this resource.
	walk( &Resource::eval_state );
		// If we didn't update b/c of the eval_state, we need to
		// actually do the update now.
	walk( &Resource::update );

	report_updates();
	check_polling();
	check_use();
}


void
ResMgr::eval_and_update_all( void )
{
#if HAVE_HIBERNATION
	if ( !hibernating () ) {
#endif
		compute( A_TIMEOUT | A_UPDATE );
		update_all();
#if HAVE_HIBERNATION
	}
#endif
}


void
ResMgr::eval_all( void )
{
#if HAVE_HIBERNATION
	if ( !hibernating () ) {
#endif
		num_updates = 0;
		compute( A_TIMEOUT );
		walk( &Resource::eval_state );
		report_updates();
		check_polling();
#if HAVE_HIBERNATION
	}
#endif
}


void
ResMgr::report_updates( void )
{
	if( !num_updates ) {
		return;
	}

	CollectorList* collectors = daemonCore->getCollectorList();
	if( collectors ) {
		MyString list;
		Daemon * collector;
		collectors->rewind();
		while (collectors->next (collector)) {
			list += collector->fullHostname();
			list += " ";
		}
		dprintf( D_FULLDEBUG,
				 "Sent %d update(s) to the collector (%s)\n",
				 num_updates, list.Value());
	}
}

void
ResMgr::refresh_benchmarks()
{
	if( ! resources ) {
		return;
	}

	walk( &Resource::refresh_classad, A_TIMEOUT );
}

void
ResMgr::compute( amask_t how_much )
{
	if( ! resources ) {
		return;
	}

    double runtime = stats.BeginRuntime(stats.Compute);

		// Since lots of things want to know this, just get it from
		// the kernel once and share the value...
	cur_time = time( 0 );

	compute_draining_attrs( how_much );

	m_attr->compute( (how_much & ~(A_SUMMED)) | A_SHARED );
	walk( &Resource::compute, (how_much & ~(A_SHARED)) );
	m_attr->compute( (how_much & ~(A_SHARED)) | A_SUMMED );
	walk( &Resource::compute, (how_much | A_SHARED) );

		// Sort the resources so when we're assigning owner load
		// average and keyboard activity, we get to them in the
		// following state order: Owner, Unclaimed, Matched, Claimed
		// Preempting
	resource_sort( ownerStateCmp );

	assign_load();
	assign_keyboard();

	if( vmapi_is_virtual_machine() == TRUE ) {
		ClassAd* host_classad;
		vmapi_request_host_classAd();
		host_classad = vmapi_get_host_classAd();
		if( host_classad ) {
			int i;
			for( i = 0; i < nresources; i++ ) {
				if( resources[i]->r_classad )
					MergeClassAds( resources[i]->r_classad, host_classad, true );
			}
		}

	}

		// Now that everything has actually been computed, we can
		// refresh our internal classad with all the current values of
		// everything so that when we evaluate our state or any other
		// expressions, we've got accurate data to evaluate.
	walk( &Resource::refresh_classad, how_much );

		// Now that we have an updated internal classad for each
		// resource, we can "compute" anything where we need to
		// evaluate classad expressions to get the answer.
	walk( &Resource::compute, A_EVALUATED );

		// Next, we can publish any results from that to our internal
		// classads to make sure those are still up-to-date
	walk( &Resource::refresh_classad, A_EVALUATED );

		// Finally, now that all the internal classads are up to date
		// with all the attributes they could possibly have, we can
		// publish the cross-slot attributes desired from
		// STARTD_SLOT_ATTRS into each slots's internal ClassAd.
	walk( &Resource::refresh_classad, A_SHARED_SLOT );

		// Now that we're done, we can display all the values.
	walk( &Resource::display, how_much );

    stats.EndRuntime(stats.Compute, runtime);
}


void
ResMgr::publish( ClassAd* cp, amask_t how_much )
{
	if( IS_UPDATE(how_much) && IS_PUBLIC(how_much) ) {
		cp->Assign(ATTR_TOTAL_SLOTS, numSlots());
		if (param_boolean("ALLOW_VM_CRUFT", false)) {
			cp->Assign(ATTR_TOTAL_VIRTUAL_MACHINES, numSlots());
		}
	}

	starter_mgr.publish( cp, how_much );
	m_vmuniverse_mgr.publish(cp, how_much);

#if HAVE_HIBERNATION
    m_hibernation_manager->publish( *cp );
#endif

}


void
ResMgr::publishSlotAttrs( ClassAd* cap )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		resources[i]->publishSlotAttrs( cap );
	}
}


void
ResMgr::assign_load( void )
{
	if( ! resources ) {
		return;
	}

	int i;
	float total_owner_load = m_attr->load() - m_attr->condor_load();
	if( total_owner_load < 0 ) {
		total_owner_load = 0;
	}
	if( is_smp() ) {
			// Print out the totals we already know.
		if( IsDebugLevel( D_LOAD ) ) {
			dprintf( D_FULLDEBUG,
					 "%s %.3f\t%s %.3f\t%s %.3f\n",
					 "SystemLoad:", m_attr->load(),
					 "TotalCondorLoad:", m_attr->condor_load(),
					 "TotalOwnerLoad:", total_owner_load );
		}

			// Initialize everything to 0.  Only need this for SMP
			// machines, since on single CPU machines, we just assign
			// all OwnerLoad to the 1 CPU.
		for( i = 0; i < nresources; i++ ) {
			resources[i]->set_owner_load( 0 );
		}
	}

		// So long as there's at least two more resources and the
		// total owner load is greater than 1.0, assign an owner load
		// of 1.0 to each CPU.  Once we get below 1.0, we assign all
		// the rest to the next CPU.  So, for non-SMP machines, we
		// never hit this code, and always assign all owner load to
		// cpu1 (since i will be initialized to 0 but we'll never
		// enter the for loop).
	for( i = 0; i < (nresources - 1) && total_owner_load > 1; i++ ) {
		resources[i]->set_owner_load( 1.0 );
		total_owner_load -= 1.0;
	}
	resources[i]->set_owner_load( total_owner_load );
}


void
ResMgr::assign_keyboard( void )
{
	if( ! resources ) {
		return;
	}

	int i;
	time_t console = m_attr->console_idle();
	time_t keyboard = m_attr->keyboard_idle();
	time_t max;

		// First, initialize all CPUs to the max idle time we've got,
		// which is some configurable amount of minutes longer than
		// the time since we started up.
	max = (cur_time - startd_startup) + disconnected_keyboard_boost;
	for( i = 0; i < nresources; i++ ) {
		resources[i]->r_attr->set_console( max );
		resources[i]->r_attr->set_keyboard( max );
	}

		// Now, assign console activity to all CPUs that care.
		// Notice, we should also assign keyboard here, since if
		// there's console activity, there's (by definition) keyboard
		// activity as well.
	for( i = 0; i < console_slots  && i < nresources; i++ ) {
		resources[i]->r_attr->set_console( console );
		resources[i]->r_attr->set_keyboard( console );
	}

		// Finally, assign keyboard activity to all CPUS that care.
	for( i = 0; i < keyboard_slots && i < nresources; i++ ) {
		resources[i]->r_attr->set_keyboard( keyboard );
	}
}


void
ResMgr::check_polling( void )
{
	if( ! resources ) {
		return;
	}

	if( needsPolling() || m_attr->condor_load() > 0 ) {
		start_poll_timer();
	} else {
		cancel_poll_timer();
	}
}


int
ResMgr::start_update_timer( void )
{
	int		initial_interval;

	if ( update_offset ) {
		initial_interval = update_offset;
		dprintf( D_FULLDEBUG, "Delaying initial update by %d seconds\n",
				 update_offset );
	} else {
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
	if ( update_offset ) {
		dprintf( D_FULLDEBUG,
				 "Delaying update after reconfig by %d seconds\n",
				 update_offset );
	}
	if( poll_tid != -1 ) {
		daemonCore->Reset_Timer( poll_tid, polling_interval,
								 polling_interval );
	}
	if( up_tid != -1 ) {
		daemonCore->Reset_Timer( up_tid, update_offset,
								 update_interval );
	}

#if HAVE_HIBERNATION
	resetHibernateTimer();
#endif /* HAVE_HIBERNATE */

}


void
ResMgr::addResource( Resource *rip )
{
	Resource** new_resources = NULL;

	if( !rip ) {
		EXCEPT("Error: attempt to add a NULL resource\n");
	}

	calculateAffinityMask(rip);

	new_resources = new Resource*[nresources + 1];
	if( !new_resources ) {
		EXCEPT("Failed to allocate memory for new resource\n");
	}

		// Copy over the old Resource pointers.  If nresources is 0
		// (b/c we used to be configured to have no slots), this won't
		// copy anything (and won't seg fault).
	memcpy( (void*)new_resources, (void*)resources,
			(sizeof(Resource*)*nresources) );

	new_resources[nresources] = rip;


	if( resources ) {
		delete [] resources;
	}

	resources = new_resources;
	nresources++;

}


bool
ResMgr::removeResource( Resource* rip )
{
	int i, j;
	Resource** new_resources = NULL;
	Resource* rip2;

	if( nresources > 1 ) {
			// There are still more resources after this one is
			// deleted, so we'll need to make a new resources array
			// without this resource.
		new_resources = new Resource* [ nresources - 1 ];
		j = 0;
		for( i = 0; i < nresources; i++ ) {
			if( resources[i] != rip ) {
				new_resources[j++] = resources[i];
			}
		}

		if ( j == nresources ) { // j == i would work too
				// The resource was not found, which should never happen
			delete [] new_resources;
			return false;
		}
	}

		// Remove this rip from our destroy_list.
	destroy_list.Rewind();
	while( destroy_list.Next(rip2) ) {
		if( rip2 == rip ) {
			destroy_list.DeleteCurrent();
			break;
		}
	}

		// Now, delete the old array and start using the new one, if
		// it's there at all.  If not, it'll be NULL and this will all
		// still be what we want.
	delete [] resources;
	resources = new_resources;
	nresources--;

		// Return this Resource's ID to the dispenser.
		// If it is a dynamic slot it's reusing its partitionable
		// parent's id, so we don't want to free the id.
	if( Resource::DYNAMIC_SLOT != rip->get_feature() ) {
		id_disp->insert( rip->r_id );
	}

		// Tell the collector this Resource is gone.
	rip->final_update();

		// Log a message that we're going away
	rip->dprintf( D_ALWAYS, "Resource no longer needed, deleting\n" );

		// At last, we can delete the object itself.
	delete rip;

	return true;
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

	for( int i = 0; i < nresources; i++ ) {
		std::list<int> *cores = resources[i]->get_affinity_set();
		for (std::list<int>::iterator it = cores->begin();
			 it != cores->end(); it++) {

			int used_core_num = *it;
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
		dprintf(D_ALWAYS, "Setting cpu affinity for %s to cpu id %d\n", rip->r_name, leastUsedCore);
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
		EXCEPT( "ResMgr::deleteResource() failed: couldn't find resource" );
	}

		// Now that a Resource is gone, see if we're done deleting
		// Resources and see if we should allocate any.
	if( processAllocList() ) {
			// We're done allocating, so we can finish our reconfig.
		finish_main_config();
	}
}


void
ResMgr::makeAdList( ClassAdList *list, ClassAd *  /*pqueryAd =NULL*/ )
{
	ClassAd* ad;
	int i;

		// Make sure everything is current
	compute( A_TIMEOUT | A_UPDATE );

		// We want to insert ATTR_LAST_HEARD_FROM into each ad.  The
		// collector normally does this, so if we're servicing a
		// QUERY_STARTD_ADS commannd, we need to do this ourselves or
		// some timing stuff won't work.
	for( i=0; i<nresources; i++ ) {
		ad = new ClassAd;
		resources[i]->publish( ad, A_ALL_PUB );
		ad->Assign( ATTR_LAST_HEARD_FROM, (int)cur_time );
		list->Insert( ad );
	}
}


bool
ResMgr::processAllocList( void )
{
	if( ! destroy_list.IsEmpty() ) {
			// Can't start allocating until everything has been
			// destroyed.
		return false;
	}
	if( alloc_list.IsEmpty() ) {
		return true;  // Since there's nothing to allocate...
	}

		// We're done destroying, and there's something to allocate.

	bool multiple_slots = (alloc_list.Number() + numSlots()) > 1;

		// Create the new Resource objects.
	CpuAttributes* cap;
	alloc_list.Rewind();
	while( alloc_list.Next(cap) ) {
		addResource( new Resource( cap, nextId(), multiple_slots ) );
		alloc_list.DeleteCurrent();
	}

	delete [] type_nums;
	type_nums = new_type_nums;
	new_type_nums = NULL;

	return true; 	// Since we're done allocating.
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
ResMgr::allHibernating( MyString &target ) const
{
    	// fail if there is no resource or if we are
		// configured not to hibernate
	if (   !resources  ||  !m_hibernation_manager->wantsHibernate()  ) {
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
	MyString str;
	int level = 0;
	bool activity = false;
	for( int i = 0; i < nresources; i++ ) {

		str = "";
		if ( !resources[i]->evaluateHibernate ( str ) ) {
			return 0;
		}

		int tmp = m_hibernation_manager->stringToSleepState (
			str.Value () );

		dprintf ( D_FULLDEBUG,
			"allHibernating: resource #%d: '%s' (0x%x)\n",
			i + 1, str.Value (), tmp );

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
ResMgr::checkHibernate( void )
{

		// If we have already issued the command to hibernate, then
		// don't bother re-entering the check/evaluation.
	if ( hibernating () ) {
		return;
	}

		// If all resources have gone unused for some time
		// then put the machine to sleep
	MyString	target;
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
#     if !defined( WIN32 )
		sleep(10);
        m_hibernation_manager->setTargetState ( HibernatorBase::NONE );
        for ( int i = 0; i < nresources; ++i ) {
            resources[i]->enable();
            resources[i]->update();
			m_hibernating = false;
	    }

#     endif
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
ResMgr::disableResources( const MyString &state_str )
{

	dprintf (
		D_FULLDEBUG,
		"In ResMgr::disableResources ()\n" );

	int i; /* stupid VC6 */

	/* set the sleep state so the plugin will pickup on the
	fact that we are sleeping */
	m_hibernation_manager->setTargetState ( state_str.Value() );

	/* update the CM */
	bool ok = true;
	for ( i = 0; i < nresources && ok; ++i ) {
		ok = resources[i]->update_with_ack();
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
		   matchable while offline.  The negotiator knows to treat this
		   state specially. */
		for ( i = 0; i < nresources; ++i ) {
			resources[i]->disable();
		}
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
	int current_time = time(NULL);
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


int
ownerStateCmp( const void* a, const void* b )
{
	const Resource *rip1, *rip2;
	int val1, val2, diff;
	float fval1, fval2;
	State s;
	rip1 = *((Resource* const *)a);
	rip2 = *((Resource* const *)b);
		// Since the State enum is already in the "right" order for
		// this kind of sort, we don't need to do anything fancy, we
		// just cast the state enum to an int and we're done.
	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val1 - val2;
	if( diff ) {
		return diff;
	}
		// We're still here, means we've got the same state.  If that
		// state is "Claimed" or "Preempting", we want to break ties
		// w/ the Rank expression, else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval1 - fval2);
		return diff;
	}
	return 0;
}


// This is basically the same as above, except we want it in exactly
// the opposite order, so reverse the signs.
int
claimedRankCmp( const void* a, const void* b )
{
	const Resource *rip1, *rip2;
	int val1, val2, diff;
	float fval1, fval2;
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


/*
  Sort resource so their in the right order to give out a new COD
  Claim.  We give out COD claims in the following order:
  1) the Resource with the least # of existing COD claims (to ensure
     round-robin across resources
  2) in case of a tie, the Resource in the best state (owner or
     unclaimed, not claimed)
  3) in case of a tie, the Claimed resource with the lowest value of
     machine Rank for its claim
*/
int
newCODClaimCmp( const void* a, const void* b )
{
	const Resource *rip1, *rip2;
	int val1, val2, diff;
	int numCOD1, numCOD2;
	float fval1, fval2;
	State s;
	rip1 = *((Resource* const *)a);
	rip2 = *((Resource* const *)b);

	numCOD1 = rip1->r_cod_mgr->numClaims();
	numCOD2 = rip2->r_cod_mgr->numClaims();

		// In the first case, sort based on # of COD claims
	diff = numCOD1 - numCOD2;
	if( diff ) {
		return diff;
	}

		// If we're still here, we've got same # of COD claims, so
		// sort based on State.  Since the state enum is already in
		// the "right" order for this kind of sort, we don't need to
		// do anything fancy, we just cast the state enum to an int
		// and we're done.
	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val1 - val2;
	if( diff ) {
		return diff;
	}

		// We're still here, means we've got the same number of COD
		// claims and the same state.  If that state is "Claimed" or
		// "Preempting", we want to break ties w/ the Rank expression,
		// else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval1 - fval2);
		return diff;
	}
	return 0;
}


void
ResMgr::FillExecuteDirsList( class StringList *list )
{
	ASSERT( list );
	for( int i=0; i<nresources; i++ ) {
		if( resources[i] ) {
			char const *execute_dir = resources[i]->executeDir();
			if( !list->contains( execute_dir ) ) {
				list->append(execute_dir);
			}
		}
	}
}

bool
ResMgr::startDraining(int how_fast,bool resume_on_completion,ExprTree *check_expr,std::string &new_request_id,std::string &error_msg,int &error_code)
{
	if( draining ) {
		new_request_id = "";
		error_msg = "Draining already in progress.";
		error_code = DRAINING_ALREADY_IN_PROGRESS;
		return false;
	}

	if( check_expr ) {
		for( int i = 0; i < nresources; i++ ) {
			classad::Value v;
			bool check_ok = false;
			classad::EvalState eval_state;
			eval_state.SetScopes( resources[i]->r_classad );
			if( !check_expr->Evaluate( eval_state, v ) ) {
				formatstr(error_msg,"Failed to evaluate draining check expression against %s.", resources[i]->r_name );
				error_code = DRAINING_CHECK_EXPR_FAILED;
				return false;
			}
			if( !v.IsBooleanValue(check_ok) ) {
				formatstr(error_msg,"Draining check expression does not evaluate to a bool on %s.", resources[i]->r_name );
				error_code = DRAINING_CHECK_EXPR_FAILED;
				return false;
			}
			if( !check_ok ) {
				formatstr(error_msg,"Draining check expression is false on %s.", resources[i]->r_name );
				error_code = DRAINING_CHECK_EXPR_FAILED;
				return false;
			}
		}
	}

	draining = true;
	last_drain_start_time = time(NULL);
	draining_id += 1;
	formatstr(new_request_id,"%d",draining_id);
	this->resume_on_completion_of_draining = resume_on_completion;

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

		walk(&Resource::releaseAllClaimsReversibly);
	}
	else if( how_fast <= DRAIN_QUICK ) {
			// retirement time will not be honored, but vacate time will
		dprintf(D_ALWAYS,"Initiating quick draining.\n");
		draining_is_graceful = false;
		walk(&Resource::setBadputCausedByDraining);
		walk(&Resource::releaseAllClaims);
	}
	else if( how_fast > DRAIN_QUICK ) { // DRAIN_FAST
			// neither retirement time nor vacate time will be honored
		dprintf(D_ALWAYS,"Initiating fast draining.\n");
		draining_is_graceful = false;
		walk(&Resource::setBadputCausedByDraining);
		walk(&Resource::killAllClaims);
	}

	update_all();
	return true;
}

bool
ResMgr::cancelDraining(std::string request_id,std::string &error_msg,int &error_code)
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

	walk(&Resource::enable);
	update_all();
	return true;
}

bool
ResMgr::isSlotDraining(Resource * /*rip*/)
{
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
	for( int i = 0; i < nresources; i++ ) {
		int retirement_remaining = resources[i]->evalRetirementRemaining();
		if( retirement_remaining > longest_retirement_remaining ) {
			longest_retirement_remaining = retirement_remaining;
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

	for( int i = 0; i < nresources; i++ ) {
		if( resources[i]->state() != drained_state ) {
			return false;
		}
	}
	return true;
}

bool
ResMgr::considerResumingAfterDraining()
{
	if( !draining || !resume_on_completion_of_draining ) {
		return false;
	}

	for( int i = 0; i < nresources; i++ ) {
		if( resources[i]->state() != drained_state ||
			resources[i]->activity() != idle_act )
		{
			return false;
		}
	}

	dprintf(D_ALWAYS,"As specified in draining request, resuming normal operation after completion of draining.\n");
	std::string error_msg;
	int error_code = 0;
	if( !cancelDraining("",error_msg,error_code) ) {
			// should never happen!
		EXCEPT("failed to cancel draining: (code %d) %s",error_code,error_msg.c_str());
	}
	return true;
}

bool
ResMgr::getDrainingRequestId( Resource * /*rip*/, std::string &request_id )
{
	if( !draining ) {
		return false;
	}
	formatstr(request_id,"%d",draining_id);
	return true;
}

void
ResMgr::publish_draining_attrs( Resource *rip, ClassAd *cap, amask_t mask )
{
	if( !IS_PUBLIC(mask) ) {
		return;
	}

	if( isSlotDraining(rip) ) {
		cap->Assign( ATTR_DRAINING, true );

		std::string request_id;
		resmgr->getDrainingRequestId( rip, request_id );
		cap->Assign( ATTR_DRAINING_REQUEST_ID, request_id );
	}
	else {
		cap->Delete( ATTR_DRAINING );
		cap->Delete( ATTR_DRAINING_REQUEST_ID );
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
		cap->Assign( ATTR_LAST_DRAIN_START_TIME, (int)last_drain_start_time );
	}
}

void
ResMgr::compute_draining_attrs( int /*how_much*/ )
{
		// Using long long for int math in this function so
		// MaxJobRetirementTime=MAX_INT or MaxVacateTime=MAX_INT do
		// not cause overflow.
	long long ll_expected_graceful_draining_completion = 0;
	long long ll_expected_quick_draining_completion = 0;
	long long ll_expected_graceful_draining_badput = 0;
	long long ll_expected_quick_draining_badput = 0;
	long long ll_total_draining_unclaimed = 0;

	for( int i = 0; i < nresources; i++ ) {
		Resource *rip = resources[i];
		if( rip->r_cur ) {
			long long runtime = rip->r_cur->getJobTotalRunTime();
			long long retirement_remaining = rip->evalRetirementRemaining();
			long long max_vacate_time = rip->evalMaxVacateTime();
			long long cpus = rip->r_attr->num_cpus();

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

		// convert time estimates from relative time to absolute time
	ll_expected_graceful_draining_completion += cur_time;
	ll_expected_quick_draining_completion += cur_time;

	expected_graceful_draining_completion = cap_int(ll_expected_graceful_draining_completion);
	expected_quick_draining_completion = cap_int(ll_expected_quick_draining_completion);
	expected_graceful_draining_badput = cap_int(ll_expected_graceful_draining_badput);
	expected_quick_draining_badput = cap_int(ll_expected_quick_draining_badput);
	total_draining_unclaimed = cap_int(ll_total_draining_unclaimed);
}

void
ResMgr::addToDrainingBadput( int badput )
{
	total_draining_badput += badput;
}
