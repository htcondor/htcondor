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
#include "classad_merge.h"
#include "condor_holdcodes.h"
#include "startd_bench_job.h"
#include "ipv6_hostname.h"
#include "expr_analyze.h" // to analyze mismatches in the same way condor_q -better does
#include "directory_util.h"

#include "slot_builder.h"

#include "consumption_policy.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#include "StartdPlugin.h"
#endif

#include "stat_info.h"

#ifndef max
#define max(x,y) (((x) < (y)) ? (y) : (x))
#endif

std::vector<SlotType> SlotType::types(10);
static bool warned_startd_attrs_once = false; // used to prevent repetition of the warning about mixing STARTD_ATTRS and STARTD_EXPRS

const char * SlotType::type_param(const char * name)
{
	slottype_param_map_t::const_iterator it = params.find(name);
	if (it != params.end()) { return it->second.c_str(); }
	return NULL;
}

/*static*/ const char * SlotType::type_param(unsigned int type_id, const char * name)
{
	if (type_id < types.size()) {
		return types[type_id].type_param(name);
	}
	return NULL;
}
/*static*/ const char * SlotType::type_param(CpuAttributes* p_attr, const char * name)
{
	if (p_attr) { return type_param(p_attr->type_id(), name); }
	return NULL;
}

/*static*/ bool SlotType::type_param_boolean(unsigned int type_id, const char * name, bool def_value)
{
	bool result = def_value;
	if (type_id < types.size()) {
		const char * str = types[type_id].type_param(name);
		if ( ! str || ! string_is_boolean_param(str, result))
			result = def_value;
	}
	return result;
}
/*static*/ bool SlotType::type_param_boolean(CpuAttributes* p_attr, const char * name, bool def_value)
{
	if ( ! p_attr) return def_value;
	return type_param_boolean(p_attr->type_id(), name, def_value);
}
/*static*/ long long SlotType::type_param_long(CpuAttributes* p_attr, const char * name, long long def_value)
{
	if ( ! p_attr) return def_value;

	long long result = def_value;
	unsigned int type_id = p_attr->type_id();
	if (type_id < types.size()) {
		const char * str = types[type_id].type_param(name);
		if ( ! str || ! string_is_long_param(str, result))
			result = def_value;
	}
	return result;
}


/*static*/ char * SlotType::param(CpuAttributes* p_attr, const char * name)
{
	const char * str = type_param(p_attr, name);
	if (str) {
		return strdup(str);
	}
	return ::param(name);
}

/*static*/ const char * SlotType::param(std::string &out, CpuAttributes* p_attr, const char * name)
{
	const char * str = type_param(p_attr, name);
	if (str) {
		out = str;
		return out.c_str();
	}
	if (::param(out, name)) {
		return out.c_str();
	}
	out.clear();
	return NULL;
}

// callback for foreach_param_matching that saves SLOT_TYPE_n_* params into a map.
/*static*/ bool SlotType::insert_param(void*, HASHITER & it)
{
	const char * name = hash_iter_key(it);

	// we hit this on iteration so we know the config file has a statement, but we can
	// still get back a NULL from ::param, if so we want to put "" into the slot's param table
	auto_free_ptr res_value(::param(name));
	const char * value = res_value.empty() ? "" : res_value.ptr();

	// extract the number and get a pointer to the remainder
	const char * pnum = name + sizeof("SLOT_TYPE_")-1;
	char * tag;
	int type_id = strtol(pnum, &tag, 10);
	if (tag == pnum)
		return true; // didn't parse a number, this shouldn't happen keep iterating

	if (type_id < 0 || type_id >= (int)types.size()) {
		dprintf(D_FULLDEBUG, "param %s matches SLOT_TYPE_n pattern, but n is out of range, ignoring", name);
	} else {
		SlotType * ptyp = &types[type_id];
		if ( ! *tag) {
			ptyp->shares = value;
		} else if (*tag == '_') {
			ptyp->params[tag+1] = value;
		}
	}
	return true;
}


// clear the table of slot type config info, and then re-initialize it
// by fetching all params that begin with "SLOT_TYPE_n".
/*static*/ void SlotType::init_types(int max_type_id, bool first_init)
{
	if (first_init || (max_type_id > (int)types.size())) {
		types.resize(max_type_id);
	}
	for (size_t ix = 0; ix < types.size(); ++ix) { types[ix].clear(); }
	warned_startd_attrs_once = false; // allow the warning about mixing STARTD_ATTRS and STARTD_EXPRS once again

	// default SLOT_TYPE_0_PARTITIONABLE to true
	// we do this here rather than in the param table so it will not be visible to config_val
	types[0].params["PARTITIONABLE"] = "true";

	Regex re;
	int errcode = 0, erroffset = 0;
	ASSERT(re.compile("^SLOT_TYPE_[0-9]+", &errcode, &erroffset, PCRE2_CASELESS));
	const int iter_options = 0;
	foreach_param_matching(re, iter_options, SlotType::insert_param, 0);
}

// these override the param call, looking the key up first in the per-slot-type config table
// and then in the global param table if it is not found.
char * Resource::param(const char * name) {
	if (r_attr) return SlotType::param(r_attr, name);
	return param(name);
}
const char * Resource::param(std::string& out, const char * name) {
	if (r_attr) return SlotType::param(out, r_attr, name);
	if ( ! ::param(out, name)) return NULL;
	return out.c_str();
}
const char * Resource::param(std::string& out, const char * name, const char * def) {
	const char * val = NULL;
	if (r_attr) val = SlotType::param(out, r_attr, name);
	else if (::param(out, name)) val = out.c_str();
	if ( ! val) out = def;
	return out.c_str();
}

Resource::Resource( CpuAttributes* cap, int rid, Resource* _parent, bool _take_parent_claim )
	: r_state(nullptr)
	, r_config_classad(nullptr)
	, r_classad(nullptr)
	, r_cur(nullptr)
	, r_pre(nullptr)
	, r_pre_pre(nullptr)
	, r_has_cp(false)
	, r_backfill_slot(false)
	, r_cod_mgr(nullptr)
	, r_reqexp(nullptr)
	, r_attr(nullptr)
	, r_load_queue(nullptr)
	, r_name(nullptr)
	, r_id(rid)
	, r_sub_id(0)
	, r_id_str(NULL)
	, m_resource_feature(STANDARD_SLOT)
	, m_parent(nullptr)
	, m_id_dispenser(nullptr)
	, m_acceptedWhileDraining(false)
{
	std::string tmp;

		// we need this before we instantiate any Claim objects...
	if (_parent) { r_sub_id = _parent->m_id_dispenser->next(); }
	const char * name_prefix = SlotType::type_param(cap, "NAME_PREFIX");
	if (name_prefix) {
		tmp = name_prefix;
	} else {
		::param(tmp, "STARTD_RESOURCE_PREFIX", "");
		if (tmp.empty()) tmp = "slot";
	}
	// append slot id
	// for dynamic slots, also append sub_id
	formatstr_cat( tmp, "%d", r_id );
	if  (_parent) { formatstr_cat( tmp, "_%d", r_sub_id ); }
	// save the constucted slot name & id string.
	r_id_str = strdup( tmp.c_str() );

		// we need this before we can call type()...
	r_attr = cap;
	r_attr->attach( this );

	r_backfill_slot = SlotType::type_param_boolean(cap, "BACKFILL", false);

		// we need this before we instantiate the Reqexp object...
	if (SlotType::type_param_boolean(cap, "PARTITIONABLE", false)) {
		set_feature( PARTITIONABLE_SLOT );
		m_id_dispenser = new IdDispenser( 1 );
	}

	// can't bind until after we get a r_sub_id but must happen before creating the Reqexp
	if (_parent) {
		if ( ! r_attr->bind_DevIds(resmgr->m_attr, r_id, r_sub_id, r_backfill_slot, false)) {
			r_attr->unbind_DevIds(resmgr->m_attr, r_id, r_sub_id); // give back the ids that were bound before the failure
			_parent->m_id_dispenser->insert(r_sub_id); // give back the slot id.

			// we can't fail a constructor, so indicate the the slot is unusable instead
			set_feature(BROKEN_SLOT);
			m_parent = nullptr;
			r_cur = nullptr;
			return;
		}
	}
	// set_parent will carve resource quantities out of the parent if it is not NULL
	// this will also set feature to DYNAMIC_SLOT if _parent is non-null
	set_parent( _parent );

	r_state = new ResState( this );
	r_cod_mgr = new CODMgr( this );
	r_reqexp = new Reqexp( this );
	r_load_queue = new LoadQueue( 60 );

    if (get_feature() == PARTITIONABLE_SLOT) {
        // Partitionable slots may support a consumption policy
        // first, determine if a consumption policy is being configured
		r_has_cp = param_boolean("CONSUMPTION_POLICY", false);
		r_has_cp = SlotType::type_param_boolean(cap, "CONSUMPTION_POLICY", r_has_cp);
		if (r_has_cp) {
			unsigned nclaims = 1;
			int ncpus = (int)ceil(r_attr->num_cpus());
			nclaims = param_integer("NUM_CLAIMS", ncpus);
			nclaims = (unsigned) SlotType::type_param_long(cap, "NUM_CLAIMS", nclaims);
			while (r_claims.size() < nclaims) r_claims.insert(new Claim(this));
		}
	}

	if (_parent && _take_parent_claim) {
		r_cur = _parent->r_cur;
		r_cur->setResource(this);
		_parent->r_cur = new Claim(_parent);
	} else {
		r_cur = new Claim(this);
	}

	// make the full slot name "<r_id_str>@<name>"
	// prior to version 9.7.0 machines with a single slot would not include the slot id in the slot name
	tmp = r_id_str;
	tmp += "@";
	if (Name) { tmp += Name; } else { tmp += get_local_fqdn(); }
	r_name = strdup(tmp.c_str());

	// check if slot should be hidden from the collector
	r_no_collector_updates = SlotType::type_param_boolean(cap, "HIDDEN", false);

#ifdef DO_BULK_COLLECTOR_UPDATES
#else
	update_tid = -1;
#endif

#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
#else
	r_cpu_busy = 0;
	r_cpu_busy_start_time = 0;
#endif
	r_last_compute_condor_load = resmgr->now();
	r_suspended_for_cod = false;
	r_hack_load_for_cod = false;
	r_cod_load_hack_tid = -1;
	r_pre_cod_total_load = -1.0;
	r_pre_cod_condor_load = 0.0;
	r_suspended_by_command = false;

#if HAVE_JOB_HOOKS
	m_last_fetch_work_spawned = 0;
	m_last_fetch_work_completed = 0;
	m_next_fetch_work_delay = -1;
	m_next_fetch_work_tid = -1;
	m_currently_fetching = false;
	m_hook_keyword = NULL;
	m_hook_keyword_initialized = false;
#endif

	const char * type_prefix = _parent ? "d" : (is_partitionable_slot() ? "p" : "");
	if( r_attr->type_id() ) {
		dprintf(D_ALWAYS, "New %s%sSlot of type %d allocated\n", r_backfill_slot ? "backfill " : "", type_prefix, r_attr->type_id() );
	} else {
		dprintf(D_ALWAYS, "New %s%sSlot allocated\n", r_backfill_slot ? "backfill " : "", type_prefix);
	}
	tmp = "\t";
	dprintf(D_ALWAYS, "%s\n", r_attr->cat_totals(tmp));

#if 0 // TODO: maybe enable this? do we need to be agressive about this update?
	// when we create a non-backfill d-slot will need to send the collector an update
	// of the backfill p-slot since it will now have fewer resources to report.
	if ( ! r_backfill_slot) { resmgr->walk(&Resource::update_walk_for_backfill_refresh_res); }
#endif
}


Resource::~Resource()
{
#ifdef DO_BULK_COLLECTOR_UPDATES
#else
	if ( update_tid != -1 ) {
		if( daemonCore->Cancel_Timer(update_tid) < 0 ) {
			::dprintf( D_ALWAYS, "failed to cancel update timer (%d): "
					   "daemonCore error\n", update_tid );
		}
		update_tid = -1;
	}
#endif

#if HAVE_JOB_HOOKS
	if (m_next_fetch_work_tid != -1) {
		if (daemonCore->Cancel_Timer(m_next_fetch_work_tid) < 0 ) {
			::dprintf(D_ALWAYS, "failed to cancel update timer (%d): "
					  "daemonCore error\n", m_next_fetch_work_tid);
		}
		m_next_fetch_work_tid = -1;
	}
	if (m_hook_keyword) {
		free(m_hook_keyword); m_hook_keyword = NULL;
	}
#endif /* HAVE_JOB_HOOKS */

	clear_parent();

	if( m_id_dispenser ) {
		delete m_id_dispenser;
		m_id_dispenser = NULL;
	}

	delete r_state; r_state = NULL;
	delete r_cur; r_cur = NULL;
	if( r_pre ) {
		delete r_pre; r_pre = NULL;
	}
	if( r_pre_pre ) {
		delete r_pre_pre; r_pre_pre = NULL;
	}
	if (r_classad) {
		r_classad->Unchain();
		delete r_classad; r_classad = NULL;
	}
	delete r_config_classad; r_config_classad = NULL;
	delete r_cod_mgr; r_cod_mgr = NULL;
	delete r_reqexp; r_reqexp = NULL;
	delete r_attr; r_attr = NULL;
	delete r_load_queue; r_load_queue = NULL;
	free( r_name ); r_name = NULL;
	free( r_id_str ); r_id_str = NULL;

}

void
Resource::clear_parent()
{
	// Note on "&& !m_currently_fetching": A DYNAMIC slot will
	// defer its destruction while it is waiting on a fetch work
	// hook. The only time when a slot with a parent will be
	// destroyed while waiting on a hook is during
	// shutdown. During shutdown there is no need to give
	// resources back to the parent slot, and doing so may
	// actually be dangerous if our parent was deleted first.

	// If we have a parent, return our resources to it
	if( m_parent && !m_currently_fetching ) {
		r_attr->unbind_DevIds(resmgr->m_attr, r_id, r_sub_id);
		*(m_parent->r_attr) += *(r_attr);
		m_parent->m_id_dispenser->insert( r_sub_id );
		resmgr->refresh_classad_resources(m_parent);
		m_parent->update_needed(wf_dslotDelete);
		// TODO: fold this code in to update_needed ?
		resmgr->res_conflict_change(m_parent, true);
		m_parent = NULL;
		set_feature( BROKEN_SLOT );
	}
}

void
Resource::set_parent( Resource* rip )
{
	m_parent = rip;

		// If we have a parent, we consume its resources
	if( m_parent ) {
		// If we have a parent, we are dynamic
		set_feature( DYNAMIC_SLOT );
		*(m_parent->r_attr) -= *(r_attr);
		resmgr->refresh_classad_resources(m_parent);
	}
}


int
Resource::retire_claim( bool reversible )
{
	switch( state() ) {
	case claimed_state:
		if(r_cur) {
			if( !reversible ) {
					// Do not allow backing out of retirement (e.g. if
					// a preempting claim goes away) because this is
					// called for irreversible events such as
					// condor_vacate or PREEMPT.
				r_cur->disallowUnretire();
			}
			if(resmgr->isShuttingDown() && daemonCore->GetPeacefulShutdown()) {
				// Admin is shutting things down but does not want any killing,
				// regardless of MaxJobRetirementTime configuration.
				r_cur->setRetirePeacefully(true);
			}
		}
		change_state( retiring_act );
		break;
	case matched_state:
		change_state( owner_state );
		break;
#if HAVE_BACKFILL
	case backfill_state:
			// we don't want retirement to mean anything special for
			// backfill jobs... they should be killed immediately
		set_destination_state( owner_state );
		break;
#endif /* HAVE_BACKFILL */
	default:
			// For good measure, try directly killing the starter if
			// we're in any other state.  If there's no starter, this
			// will just return without doing anything.  If there is a
			// starter, it shouldn't be there.
		return (int)r_cur->starterKillSoft();
	}
	return TRUE;
}


int
Resource::release_claim( void )
{
	switch( state() ) {
	case claimed_state:
		change_state( preempting_state, vacating_act );
		break;
	case preempting_state:
		if( activity() != killing_act ) {
			change_state( preempting_state, vacating_act );
		}
		break;
	case matched_state:
		change_state( owner_state );
		break;
#if HAVE_BACKFILL
	case backfill_state:
		set_destination_state( owner_state );
		break;
#endif /* HAVE_BACKFILL */
	default:
		return (int)r_cur->starterKillHard();
	}
	return TRUE;
}


int
Resource::kill_claim( void )
{
	switch( state() ) {
	case claimed_state:
	case preempting_state:
			// We might be in preempting/vacating, in which case we'd
			// still want to do the activity change into killing...
			// Added 4/26/00 by Derek Wright <wright@cs.wisc.edu>
		change_state( preempting_state, killing_act );
		break;
	case matched_state:
		change_state( owner_state );
		break;
#if HAVE_BACKFILL
	case backfill_state:
		set_destination_state( owner_state );
		break;
#endif /* HAVE_BACKFILL */
	default:
			// In other states, try direct kill.  See above.
		return (int)r_cur->starterKillHard();
	}
	return TRUE;
}


int
Resource::got_alive( void )
{
	if( state() != claimed_state ) {
		return FALSE;
	}
	if( !r_cur ) {
		dprintf( D_ALWAYS, "Got keep alive with no current match object.\n" );
		return FALSE;
	}
	if( !r_cur->client() ) {
		dprintf( D_ALWAYS, "Got keep alive with no current client object.\n" );
		return FALSE;
	}
	r_cur->alive( true );
	return TRUE;
}


int
Resource::periodic_checkpoint( void )
{
	if( state() != claimed_state ) {
		return FALSE;
	}
	dprintf( D_ALWAYS, "Performing a periodic checkpoint on %s.\n", r_name );
	r_cur->periodicCheckpoint();

		// Now that we updated this time, be sure to insert those
		// attributes into the classad right away so we don't keep
		// periodically checkpointing with stale info.
	r_cur->publish( r_classad );

	return TRUE;
}

// called by the SUSPEND_CLAIM command
// when the suspend is by command, only a command can resume it
int Resource::suspend_claim()
{

	switch( state() ) {
	case claimed_state:
		change_state( suspended_act );
		r_suspended_by_command = true;
		return TRUE;
		break;
	default:
		dprintf( D_ALWAYS, "Can not suspend claim when\n");
		break;
	}
	
	return FALSE;
}

int Resource::continue_claim()
{
	if ( suspended_act == r_state->activity() && r_suspended_by_command )
	{
		if (r_cur->resumeClaim())
		{
			change_state( busy_act );
			r_suspended_by_command = false;
			return TRUE;
		}
	}
	else
	{
		dprintf( D_ALWAYS, "Can not continue_claim\n" );
	}
	
	return FALSE;
}

int
Resource::request_new_proc( void )
{
	if( state() == claimed_state && r_cur->isActive()) {
		return (int)r_cur->starterSignal( SIGHUP );
	} else {
		return FALSE;
	}
}


int
Resource::deactivate_claim( void )
{
	dprintf(D_ALWAYS, "Called deactivate_claim()\n");
	if( state() == claimed_state ) {
		return r_cur->deactivateClaim( true );
	}
	return FALSE;
}


int
Resource::deactivate_claim_forcibly( void )
{
	dprintf(D_ALWAYS, "Called deactivate_claim_forcibly()\n");
	if( state() == claimed_state ) {
		return r_cur->deactivateClaim( false );
	}
	return FALSE;
}


void
Resource::removeClaim( Claim* c )
{
	if( c->type() == CLAIM_COD ) {
		r_cod_mgr->removeClaim( c );
		return;
	}
	if( c == r_pre ) {
		remove_pre();
		r_pre = new Claim( this );
		return;
	}
	if( c == r_pre_pre ) {
		delete r_pre_pre;
		r_pre_pre = new Claim( this );
		return;
	}

	if( c == r_cur ) {
		delete r_cur;
		r_cur = new Claim( this );
		return;
	}
		// we should never get here, this would be a programmer's error:
	EXCEPT( "Resource::removeClaim() called, but can't find the Claim!" );
}


void
Resource::dropAdInLogFile( void )
{
	dprintf(D_ALWAYS, "** BEGIN CLASSAD ** %p\n", this);
	dPrintAd(D_ALWAYS, *r_classad);
	dprintf(D_ALWAYS, "** END CLASSAD ** %p\n", this);
}

extern ExprTree * globalDrainingStartExpr;

void
Resource::shutdownAllClaims( bool graceful, bool reversible )
{
	// shutdown the COD claims
	r_cod_mgr->shutdownAllClaims( graceful );

	// The original code sequence here looked moronic.  This one isn't
	// much better, but here's why we're doing it: void_kill_claim() delete()s
	// _this_ if it's a dynamic slot, so we can't call _any_ member function
	// after we call void_kill_claim().  I don't know if that applies to
	// void_retire_claim(), but the original code implied that it did.
	bool safe = Resource::DYNAMIC_SLOT != get_feature();

	// shutdown our own claims
	if( graceful ) {
		retire_claim(reversible);
	} else {
		kill_claim();
	}

	// if we haven't deleted ourselves, mark ourselves unavailable and
	// update the collector.
	if( safe ) {
		r_reqexp->unavail( globalDrainingStartExpr );
		update_needed(wf_removeClaim);
	}
}

bool
Resource::needsPolling( void )
{
	if( hasOppClaim() ) {
		return true;
	}
#if HAVE_BACKFILL
		/*
		  if we're backfill-enabled, we want to always do polling if
		  we're in the backfill state.  if we're busy/killing, we'll
		  want frequent recompute + eval to make sure we kill backfill
		  jobs when necessary.  even if we're in Backfill/Idle, we
		  want to do polling since we should try to spawn the backfill
		  client quickly after entering Backfill/Idle.
		*/
	if( state() == backfill_state ) {
		return true;
	}
#endif /* HAVE_BACKFILL */
	return false;
}



// This one *only* looks at opportunistic claims
// except for partitionable slots
bool
Resource::hasOppClaim( void )
{
	if (is_partitionable_slot()) {
		return false;
	}
	State s = state();
	if( s == claimed_state || s == preempting_state ) {
		return true;
	}
	return false;
}


// This one checks if the Resource has *any* claims
// except for paritionable slots
bool
Resource::hasAnyClaim( void )
{
	if( r_cod_mgr->hasClaims() ) {
		return true;
	}
#if HAVE_BACKFILL
	if( state() == backfill_state && activity() != idle_act ) {
		return true;
	}
#endif /* HAVE_BACKFILL */
	return hasOppClaim();
}


void
Resource::suspendForCOD( void )
{
	bool did_update = false;
	r_suspended_for_cod = true;
	r_reqexp->unavail();

	beginCODLoadHack();

	switch( r_cur->state() ) {

    case CLAIM_RUNNING:
		dprintf( D_ALWAYS, "State change: Suspending because a COD "
				 "job is now running\n" );
		change_state( suspended_act );
		did_update = TRUE;
		break;

    case CLAIM_VACATING:
    case CLAIM_KILLING:
		dprintf( D_ALWAYS, "A COD job is now running, opportunistic "
				 "claim is already preempting\n" );
		break;

    case CLAIM_SUSPENDED:
		dprintf( D_ALWAYS, "A COD job is now running, opportunistic "
				 "claim is already suspended\n" );
		break;

    case CLAIM_IDLE:
    case CLAIM_UNCLAIMED:
		dprintf( D_ALWAYS, "A COD job is now running, opportunistic "
				 "claim is unavailable\n" );
		break;
	}
	if( ! did_update ) {
		update_needed(wf_cod);
	}
}


void
Resource::resumeForCOD( void )
{
	if( ! r_suspended_for_cod ) {
			// we've already been here, so we can return right away.
			// This could be perfectly normal.  For example, if we
			// suspend a COD job and then deactivate or release that
			// COD claim, we'll get here twice.  We can just ignore
			// the second one, since we'll have already done all the
			// things we need to do when we first got here...
		return;
	}

	bool did_update = false;
	r_suspended_for_cod = false;
	r_reqexp->restore();

	startTimerToEndCODLoadHack();

	switch( r_cur->state() ) {

    case CLAIM_RUNNING:
		dprintf( D_ALWAYS, "ERROR: trying to resume opportunistic "
				 "claim now that there's no COD job, but claim is "
				 "already running!\n" );
		break;

    case CLAIM_VACATING:
    case CLAIM_KILLING:
			// do we even want to print this one?
		dprintf( D_FULLDEBUG, "No running COD job, but opportunistic "
				 "claim is already preempting\n" );
		break;

    case CLAIM_SUSPENDED:
		dprintf( D_ALWAYS, "State Change: No running COD job, "
				 "resuming opportunistic claim\n" );
		change_state( busy_act );
		did_update = TRUE;
		break;

    case CLAIM_IDLE:
    case CLAIM_UNCLAIMED:
		dprintf( D_ALWAYS, "No running COD job, opportunistic "
				 "claim is now available\n" );
		break;
	}
	if( ! did_update ) {
		update_needed(wf_cod);
	}
}


void
Resource::hackLoadForCOD( void )
{
	if( ! r_hack_load_for_cod ) {
		return;
	}

	double load = rint((r_pre_cod_total_load) * 100) / 100.0;
	double c_load = rint((r_pre_cod_condor_load) * 100) / 100.0;

	if( IsDebugVerbose( D_LOAD ) ) {
		if( r_cod_mgr->isRunning() ) {
			dprintf( D_LOAD | D_VERBOSE, "COD job current running, using "
					 "'%s=%f', '%s=%f' for internal policy evaluation\n",
					 ATTR_LOAD_AVG, load, ATTR_CONDOR_LOAD_AVG, c_load );
		} else {
			dprintf( D_LOAD | D_VERBOSE, "COD job recently ran, using '%s=%f', '%s=%f' "
					 "for internal policy evaluation\n",
					 ATTR_LOAD_AVG, load, ATTR_CONDOR_LOAD_AVG, c_load );
		}
	}

	r_classad->Assign( ATTR_LOAD_AVG, load );
	r_classad->Assign( ATTR_CONDOR_LOAD_AVG, c_load );
	r_classad->Assign( ATTR_CPU_IS_BUSY, false );
	r_classad->Assign( ATTR_CPU_BUSY_TIME, 0 );
}


void
Resource::starterExited( Claim* cur_claim )
{
	if( ! cur_claim ) {
		EXCEPT( "Resource::starterExited() called with no Claim!" );
	}

	if( cur_claim->type() == CLAIM_COD ) {
 		r_cod_mgr->starterExited( cur_claim );
		return;
	}

	// Somewhere before the end of this function, this Resource gets
	// delete()d, or at least partially over-written.  (I'm leaning
	// towards deleted, because this Resource isn't in the ResMgr's
	// Resource list when we check at the end of this function.)
	// So decide now if we need to check if we're done draining.
	bool shouldCheckForDrainCompletion = false;
	if(isDraining() && !m_acceptedWhileDraining) {
		shouldCheckForDrainCompletion = true;
	}

		// let our ResState object know the starter exited, so it can
		// deal with destination state stuff...  we'll eventually need
		// to move more of the code from below here into the
		// destination code in ResState, to keep things simple and in
		// 1 place...
	r_state->starterExited();

		// All of the potential paths from here result in a state
		// change, and all of them are triggered by the starter
		// exiting, so let folks know that happened.  The logic in
		// leave_preempting_state() is more complicated, and we'll
		// describe why we make the change we do in there.
	dprintf( D_ALWAYS, "State change: starter exited\n" );

	State s = state();
	Activity a = activity();
	switch( s ) {
	case claimed_state:
		r_cur->client()->c_user = r_cur->client()->c_owner;
		if(a == retiring_act) {
			change_state(preempting_state);
		}
		else {
			change_state( idle_act );
		}
		break;
	case preempting_state:
		leave_preempting_state();
		break;
	default:
		dprintf( D_ALWAYS,
				 "Warning: starter exited while in unexpected state %s\n",
				 state_to_string(s) );
		change_state( owner_state );
		break;
	}

	if( shouldCheckForDrainCompletion ) {
		resmgr->checkForDrainCompletion();
	}
}


Claim*
Resource::findClaimByPid( pid_t starter_pid )
{
		// first, check our opportunistic claim (there's never a
		// starter for r_pre, so we don't have to check that.
	if( r_cur && r_cur->starterPidMatches(starter_pid) ) {
		return r_cur;
	}

		// if it's not there, see if our CODMgr has a Claim with this
		// starter pid.  if it's not there, we'll get NULL back from
		// the CODMgr, which is what we should return, anyway.
	return r_cod_mgr->findClaimByPid( starter_pid );
}


Claim*
Resource::findClaimById( const char* id )
{
	Claim* claim = NULL;

		// first, ask our CODMgr, since most likely, that's what we're
		// looking for
	claim = r_cod_mgr->findClaimById( id );
	if( claim ) {
		return claim;
	}

		// otherwise, try our opportunistic claims
	if( r_cur && r_cur->idMatches(id) ) {
		return r_cur;
	}
	if( r_pre && r_pre->idMatches(id) ) {
		return r_pre;
	}
		// if we're still here, we couldn't find it anywhere
	return NULL;
}


Claim*
Resource::findClaimByGlobalJobId( const char* id )
{
		// first, try our active claim, since that's the only one that
		// should have it...
	if( r_cur && r_cur->globalJobIdMatches(id) ) {
		return r_cur;
	}

	if( r_pre && r_pre->globalJobIdMatches(id) ) {
			// this is bogus, there's no way this should happen, since
			// the globalJobId is never set until a starter is
			// activated, and that requires the claim being r_cur, not
			// r_pre.  so, if for some totally bizzare reason this
			// becomes true, it's a developer error.
		EXCEPT( "Preepmting claims should *never* have a GlobalJobId!" );
	}

		// TODO: ask our CODMgr?

		// if we're still here, we couldn't find it anywhere
	return NULL;
}


bool
Resource::claimIsActive( void )
{
		// for now, just check r_cur.  once we've got multiple
		// claims, we can walk through our list(s).
	if( r_cur && r_cur->isActive() ) {
		return true;
	}
	return false;
}


Claim*
Resource::newCODClaim( int lease_duration )
{
	Claim* claim;
	claim = r_cod_mgr->addClaim(lease_duration);
	if( ! claim ) {
		dprintf( D_ALWAYS, "Failed to create new COD Claim!\n" );
		return NULL;
	}
	dprintf( D_FULLDEBUG, "Created new COD Claim (%s)\n", claim->id() );
	update_needed(wf_cod);
	return claim;
}


/*
   This function is called whenever we're in the preempting state
   without a starter.  This situation occurs b/c either the starter
   has finally exited after being told to go away, or we preempted a
   claim that wasn't active with a starter in the first place.  In any
   event, leave_preempting_state is the one place that does what needs
   to be done to all the current and preempting claims we've got, and
   decides which state we should enter.
*/
void
Resource::leave_preempting_state( void )
{
	bool tmp;

	if (r_cur) { r_cur->vacate(); } // Send a vacate to the client of the claim
	delete r_cur;
	r_cur = NULL;

	// NOTE: all exit paths from this function should call remove_pre()
	// in order to ensure proper cleanup of pre, pre_pre, etc.

	State dest = destination_state();
	switch( dest ) {
	case claimed_state:
			// If the machine is still available....
		if( ! eval_is_owner() ) {
			r_cur = r_pre;
			r_pre = NULL;
			remove_pre(); // do full cleanup of pre stuff
				// STATE TRANSITION preempting -> claimed
				// TLM: STATE TRANSITION #23
				// TLM: STATE TRANSITION #24
			acceptClaimRequest();
			return;
		}
			// Else, fall through, no break.
		set_destination_state( owner_state );
		dest = owner_state;	// So change_state() below will be correct.
		//@fallthrough@
	case owner_state:
		// TLM: STATE TRANSITION #22
		// TLM: STATE TRANSITION #25
	case delete_state:
		// TLM: Undocumented, hopefully on purpose.
		remove_pre();
		change_state( dest );
		return;
		break;
	case no_state:
			// No destination set, use old logic.
		break;
	default:
		EXCEPT( "Unexpected destination (%s) in leave_preempting_state()",
				state_to_string(dest) );
	}

		// Old logic.  This can be ripped out once all the destination
		// state stuff is fully used and implimented.

		// In english:  "If the machine is available and someone
		// is waiting for it..."
	bool allow_it = false;
	if( r_pre && r_pre->requestStream() ) {
		allow_it = true;
		if( (EvalBool("START", r_classad, r_pre->ad(), tmp))
			&& !tmp ) {
				// Only if it's defined and false do we consider the
				// machine busy.  We have a job ad, so local
				// evaluation gotchas don't apply here.
			dprintf( D_ALWAYS,
					 "State change: preempting claim refused - START is false\n" );
			allow_it = false;
		} else {
			dprintf( D_ALWAYS,
					 "State change: preempting claim exists - "
					 "START is true or undefined\n" );
		}
	} else {
		dprintf( D_ALWAYS,
				 "State change: No preempting claim, returning to owner\n" );
	}

	if( allow_it ) {
		r_cur = r_pre;
		r_pre = NULL;
		remove_pre(); // do full cleanup of pre stuff
			// STATE TRANSITION preempting -> claimed
			// TLM: STATE TRANSITION #23
			// TLM: STATE TRANSITION #24
		acceptClaimRequest();
	} else {
			// STATE TRANSITION preempting -> owner
			// TLM: STATE TRANSITION #22
			// TLM: STATE TRANSITION #25
		remove_pre();
		change_state( owner_state );
	}
}


void
Resource::publish_slot_config_overrides(ClassAd * cad)
{
	static const char * const attrs[] = {
		"START",
		"SUSPEND",
		"CONTINUE",
		"PREEMPT",
		"KILL",
		"WANT_SUSPEND",
		"WANT_VACATE",
		"WANT_HOLD",
		"WANT_HOLD_REASON",
		"WANT_HOLD_SUBCODE",
		"CLAIM_WORKLIFE",
		ATTR_MAX_JOB_RETIREMENT_TIME,
		ATTR_MACHINE_MAX_VACATE_TIME,

		// Now, bring in things that we might need
		"PERIODIC_CHECKPOINT",
		"RunBenchmarks",
		ATTR_RANK,
		"SUSPEND_VANILLA",
		"CONTINUE_VANILLA",
		"PREEMPT_VANILLA",
		"KILL_VANILLA",
		"WANT_SUSPEND_VANILLA",
		"WANT_VACATE_VANILLA",
	#if HAVE_BACKFILL
		"START_BACKFILL",
		"EVICT_BACKFILL",
	#endif /* HAVE_BACKFILL */
	#if HAVE_JOB_HOOKS
		ATTR_FETCH_WORK_DELAY,
	#endif /* HAVE_JOB_HOOKS */
	#if HAVE_HIBERNATION
		"HIBERNATE",
		ATTR_UNHIBERNATE,
	#endif /* HAVE_HIBERNATION */
		ATTR_SLOT_WEIGHT,
		ATTR_IS_OWNER,
		ATTR_CPU_BUSY,
		};

	for (size_t ix = 0; ix < sizeof(attrs)/sizeof(attrs[0]); ++ix) {
		const char * val = SlotType::type_param(r_attr, attrs[ix]);
		if (val) cad->AssignExpr(attrs[ix], val);
	}
}

// this is called on startup AND ON RECONFIG!
void
Resource::init_classad()
{
	ASSERT( resmgr->config_classad );
	if (r_classad) {
		r_classad->Unchain();
		delete r_classad;
	}
	if (r_config_classad) {
		delete r_config_classad;
	}
	r_config_classad = new ClassAd( *resmgr->config_classad );

	// make an ephemeral ad that we will occasionally discard
	// this catches all state updates
	r_classad = new ClassAd();
	r_classad->ChainToAd(r_config_classad);

		// put in slottype overrides of the config_classad
	this->publish_slot_config_overrides(r_config_classad);
	this->publish_static(r_config_classad);
#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
	// the latches need access to the r_config_classad?
	this->reconfig_latches();
#endif

	// TODO: remove publish_dynamic here, it should happen later?
	// Publish everything we know about.
	this->publish_dynamic(r_classad);
	// this will publish empty child rollup on startup since no d-slots will exist yet
	// but it *may* publish non-empty rollup when we reconfig
	if (is_partitionable_slot()) { this->publishDynamicChildSummaries(r_classad); }

	// NOTE: we don't publish cross-slot attrs here, it's too soon to look at other slots
}

void Resource::initial_compute(Resource * pslot)
{
	r_attr->compute_virt_mem_share(resmgr->m_attr->virt_mem());
	// set dslot total disk from pslot total disk (if appropriate)
	if ( ! resmgr->m_attr->always_recompute_disk()) {
		r_attr->init_total_disk(pslot->r_attr);
	}
	r_attr->compute_disk();
	r_reqexp->config();
}

void Resource::compute_unshared()
{
	r_attr->set_condor_load(compute_condor_usage());

	// this either sets total disk into the slot, or causes it to be recomputed.
	// TODO: !!!! do not recompute LVM free space for each slot when always_recompute_disk is true
	r_attr->set_total_disk(
		resmgr->m_attr->total_disk(),
		resmgr->m_attr->always_recompute_disk(),
		resmgr->getVolumeManager());

	r_attr->compute_disk();
}

void Resource::compute_evaluated()
{
#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
	evaluate_latches();
#else
	// Evaluate the CpuBusy expression and compute CpuBusyTime
	// and CpuIsBusy.
	compute_cpu_busy();
#endif
}

int
Resource::benchmarks_started( void )
{
	return 0;
}

int
Resource::benchmarks_finished( void )
{
	resmgr->m_attr->benchmarks_finished( this );
	return 0;
}


// called by ResState when the state or activity changes in a notable way
// this gives us a chance to refresh the slot classad and possibly trigger a collector update
// this can get called as a side effect of Resource::eval_state
void Resource::state_or_activity_changed(time_t now, bool /*state_changed*/, bool /*activity_changed*/)
{
	r_state->publish(r_classad);

	// hack! TJ has seen ads where EnterredCurrentActivity is
	// ahead of MyCurrentTime and that confuses condor_status.
	// TODO: fix things so that can't happen and then remove this...
	if (now > resmgr->now()) { r_classad->Assign(ATTR_MY_CURRENT_TIME, now); }

	// We want to update the CM on every state or activity change (actually we don't...)
	// TODO: rethink update triggering code so that the collector gets a coherent view of things
	update_needed(wf_stateChange);
}

void
Resource::eval_state( void )
{
	// we may need to modify the load average in our internal
	// policy classad if we're currently running a COD job or have
	// been running 1 in the last minute.  so, give our rip a
	// chance to modify the load, if necessary, before we evaluate
	// anything.  
	hackLoadForCOD();

	// before we evaluate our state, we should refresh cross-slot attrs
	//PRAGMA_REMIND("tj: revisit this with SlotEval?")
	resmgr->publishSlotAttrs( r_classad );

	r_state->eval_policy();
};


void
Resource::reconfig( void )
{
	r_attr->reconfig_DevIds(resmgr->m_attr, r_id, r_sub_id);
#if HAVE_JOB_HOOKS
	if (m_hook_keyword) {
		free(m_hook_keyword);
		m_hook_keyword = NULL;
	}
	m_hook_keyword_initialized = false;
#endif /* HAVE_JOB_HOOKS */
}


void
Resource::update_needed( WhyFor why )
{
	if (r_no_collector_updates)
		return;

	//dprintf(D_ZKM, "Resource::update_needed(%d) %s\n",
	//	why, update_tid < 0 ? "queuing timer" : "timer already queued");

#ifdef DO_BULK_COLLECTOR_UPDATES
	r_update_is_for |= (1<<why);
	time_t now = resmgr->rip_update_needed(r_update_is_for);
	if ( ! r_update_is_due) {
		r_update_is_due = now;
	}
#else
	// If we haven't already queued an update, queue one.
	int delay = 0;
	int updateSpreadTime = param_integer( "UPDATE_SPREAD_TIME", 0 );
	if( update_tid == -1 ) {
		if( r_id > 0 && updateSpreadTime > 0 ) {
			// If we were doing rate limiting, this would be integer
			// division, instead.
			delay += (r_id - 1) % updateSpreadTime;
		}

		update_tid = daemonCore->Register_Timer(
						delay,
						(TimerHandlercpp)&Resource::do_update,
						"do_update",
						this );
	}

	if ( update_tid < 0 ) {
		// Somehow, the timer could not be set.  Ick!
		update_tid = -1;
	}
#endif
}

// Process SlotEval and StartdCron aggregation
// inject attributes that the update ad needs to see, but that are not necessarily configured
//
void
Resource::process_update_ad(ClassAd & public_ad, int snapshot) // change the update ad before we send it 
{
	// this special unparser works only in the Startd.
	// it evaluates SlotEval as it unparses
	classad::SlotEvalUnParser unparser(&public_ad);
	unparser.SetOldClassAd(true, true);
	std::string unparse_buffer;

	// Modifying the ClassAds we're sending in ResMgr::send_update()
	// would be evil, so do the collector filtering here.  Also,
	// ClassAd iterators are broken, so delay attribute deletion until
	// after the loop.
	//
	// It looks like attributes you add during an iteration may also be
	// seen during that iteration.  This could cause problems later, but
	// doesn't seem like it's worth fixing now.
	//
	// Actually, adding attributes during an iteration can force a rehash,
	// which invalidates the iterator.  Let's just cache the keys before we
	// actually do anything with them.

	std::vector<std::string> attrNames;
	for( const auto & ad : public_ad ) {
		attrNames.push_back(ad.first);
	}

	// This isn't necessary to work around the iterators any more, but
	// not deleting attributes is handy when taking snapshots.
	std::vector< std::string > deleteList;
	for( const auto & name : attrNames ) {
		ExprTree * value = public_ad.Lookup(name);
		if(value == NULL) { continue; }

		// if this attribute has an expression with the SlotEval function
		// flatten that function and write the flattened expression into the ad
		//
		if (ExprHasSlotEval(value)) {
			unparse_buffer.clear();
			if (ExprTreeIsSlotEval(value)) {
				// if the entire expression is a single SlotEval call, just flatten it
				//    Foo = SlotEval("slot1",expr)
				// becomes
				//    Foo = <value of expr evaluated against slot1>
				unparser.setIndirectThroughAttr(false);
				unparser.Unparse(unparse_buffer, value);
			} else {
				// if the expression *contains* a SlotEval, the unparser will
				// create an intermediate attribute to contain the value.
				//    Foo = SlotEval("slot1",Activity) == "Idle"
				// becomes
				//    Foo = slot1_Activity == "Idle"
				//    slot1_Activity == <evaluated value of Activity attribute from slot1>
				//
				// slot1_Activity is stored in the unparser, we will insert it into the ad
				// after this loop exits
				unparser.setIndirectThroughAttr(true);
				unparser.Unparse(unparse_buffer, value);
			}
			public_ad.AssignExpr(name, unparse_buffer.c_str());
		}

		//
		// Arguably, this code here and the code in Resource::publish()
		// would benefit from a startd-global (?) list of the names of all
		// custom global resources; that way, we could confirm that this
		// Uptime* attribute actually corresponded to a custom global
		// resource and wasn't something from somewhere else (for instance,
		// some other startd cron job).
		//
		if( name.find( "Uptime" ) == 0 ) {
			std::string resourceName;
			if( StartdCronJobParams::attributeIsPeakMetric( name ) ) {
				// Convert Uptime<Resource>PeakUsage to
				// Device<Resource>PeakUsage to match the
				// Device<Resource>AverageUsage computed below.
				std::string computedName = name;
				computedName.replace( 0, 6, "Device" );
				// There's no rename primitive, so we have to copy from the
				// original and then delete it.
				CopyAttribute( computedName, public_ad, name, public_ad );
				deleteList.push_back( name );
				continue;
			}
			if(! StartdCronJobParams::attributeIsSumMetric( name ) ) { continue; }
			if(! StartdCronJobParams::getResourceNameFromAttributeName( name, resourceName )) { continue; }
			if(! param_boolean( "ADVERTISE_CMR_UPTIME_SECONDS", false )) {
				deleteList.push_back( name );
			}

			classad::Value v;
			double uptimeValue;
			value->Evaluate( v );
			if(! v.IsNumber( uptimeValue )) {
				dprintf( D_ALWAYS, "Metric %s is not a number, ignoring.\n", name.c_str() );
				continue;
			}

			auto birth = daemonCore->getStartTime();
			int duration = time(NULL) - birth;
			double average = uptimeValue / duration;
			// Since we don't have a whole-machine ad, we won't bother to
			// include the device name in this attribute name; people will
			// have to check the AssignedGPUs attribute.  This will suck
			// for partitionable slots, but what can you do?  Advertise each
			// GPU in every slot?
			std::string computedName = "Device" + resourceName + "AverageUsage";
			public_ad.Assign( computedName, average );

		} else if (name.find("StartOfJob") == 0) {

			// Compute the SUM metrics' *Usage values.  The PEAK metrics
			// have already inserted their *Usage values into the ad.
			std::string usageName;
			std::string uptimeName = name.substr(10);
			if (! StartdCronJobParams::getResourceNameFromAttributeName(uptimeName, usageName)) { continue; }
			usageName += "AverageUsage";

			std::string lastUpdateName = "LastUpdate" + uptimeName;
			std::string firstUpdateName = "FirstUpdate" + uptimeName;

			// Note that we calculate the usage rate only for full
			// sample intervals.  This eliminates the imprecision of
			// the sample interval in which the job started; since we
			// can't include the usage from the sample interval in
			// which the job ended, we also don't include the time
			// the job was running in that interval.  The computation
			// is thus exact for its time period.
			std::string usageExpr;
			formatstr(usageExpr, "(%s - %s)/(%s - %s)",
				uptimeName.c_str(), name.c_str(),
				lastUpdateName.c_str(), firstUpdateName.c_str());

			classad::Value v;
			if (! public_ad.EvaluateExpr(usageExpr, v)) { continue; }
			double usageValue;
			if (! v.IsNumber(usageValue)) { continue; }
			public_ad.Assign(usageName, usageValue);

			deleteList.push_back(uptimeName);
			deleteList.push_back(name);
			deleteList.push_back(lastUpdateName);
			deleteList.push_back(firstUpdateName);

		} else if( name.find( "StartOfJobUptime" ) == 0
		  || (name != "LastUpdate" && name.find( "LastUpdate" ) == 0)
		  || name.find( "FirstUpdate" ) == 0 ) {
			deleteList.push_back( name );
		}

	}

	if ( ! snapshot) {
		for (auto i = deleteList.begin(); i != deleteList.end(); ++i) {
			public_ad.Delete(* i);
		}
	}

	// Assign temporary intermediate attributes that SlotEval processing generated
	// then clear the list of them.  (we don't actually need to clear explicitly here)
	unparser.AssignSlotAttrs(public_ad);
	unparser.ClearSlotAttrs();

	// Make sure there is a max job retirement time expression for the negotiator to see
	// TODO: tj 2020, this is what the code did, it's not clear why??
	if ( ! public_ad.Lookup(ATTR_MAX_JOB_RETIREMENT_TIME)) {
		public_ad.Assign(ATTR_MAX_JOB_RETIREMENT_TIME, 0);
	}
	if ( ! public_ad.Lookup(ATTR_RANK)) {
		public_ad.Assign(ATTR_RANK, 0);
	}
}

#ifdef DO_BULK_COLLECTOR_UPDATES
void
Resource::get_update_ads(ClassAd & public_ad, ClassAd & private_ad)
{
#else
void
Resource::do_update( int /* timerID */ )
{
	int rval;
	ClassAd private_ad;
	ClassAd public_ad;
#endif

	// Get the public and private ads
	publish_single_slot_ad(public_ad, 0, Resource::Purpose::for_update);

		// refresh the machine ad in the job sandbox
	refresh_sandbox_ad(&public_ad);

	publish_private(&private_ad);

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	StartdPluginManager::Update(&public_ad, &private_ad);
#endif

#ifdef DO_BULK_COLLECTOR_UPDATES
	r_update_is_due = 0;
	r_update_is_for = 0;
#else

		// Send class ads to owning collector(s)
	rval = resmgr->send_update( UPDATE_STARTD_AD, &public_ad,
								&private_ad, true );
	if( rval ) {
		dprintf( D_FULLDEBUG, "Sent update to %d collector(s)\n", rval );
	} else {
		dprintf( D_ALWAYS, "Error sending update to collector(s)\n" );
	}

	// If we have a temporary CM, send update there, too
	if (!r_cur->c_working_cm.empty()) {
		// TODO: create the collectorList when the working_cm is registered.
		// Recreating the collectorList over and over is broken because
		// the update sequence numbers will never increment
		CollectorList *workingCollectors = CollectorList::create(r_cur->c_working_cm.c_str());
		workingCollectors->sendUpdates(UPDATE_STARTD_AD, &public_ad, &private_ad, true);
		delete workingCollectors;
	}

	// We _must_ reset update_tid to -1 before we return so
	// the class knows there is no pending update.
	update_tid = -1;
#endif
}

// build a slot ad from whole cloth, used for updating the collector, etc
// it is an ERROR to pass r_classad as input ad here!!
void Resource::publish_single_slot_ad(ClassAd & ad, time_t last_heard_from, Purpose purpose)
{
	ASSERT(&ad != r_classad && &ad != r_config_classad);

	publish_static(&ad);
	publish_dynamic(&ad);
	// the collector will set this, but for direct query, we have to set this ourselves
	if (last_heard_from) { ad.Assign(ATTR_LAST_HEARD_FROM, last_heard_from); }

	switch (purpose) {
	case Purpose::for_update:
		if (is_partitionable_slot()) { publishDynamicChildSummaries(&ad); }
		resmgr->publishSlotAttrs(&ad);
		process_update_ad(ad);
		break;
	case Purpose::for_cod:
	case Purpose::for_req_claim:
		if (is_partitionable_slot()) { publishDynamicChildSummaries(&ad); }
		resmgr->publishSlotAttrs(&ad);
		process_update_ad(ad);
		break;
	case Purpose::for_workfetch:
		if (is_partitionable_slot()) { publishDynamicChildSummaries(&ad); }
		resmgr->publishSlotAttrs(&ad);
		process_update_ad(ad);
		break;
	case Purpose::for_query:
		if (is_partitionable_slot()) { publishDynamicChildSummaries(&ad); }
		resmgr->publishSlotAttrs(&ad);
		process_update_ad(ad);
		break;
	case Purpose::for_snap:
		process_update_ad(ad);
		break;
	}
}


void
Resource::final_update( void )
{
	if (r_no_collector_updates) return;

	ClassAd invalidate_ad;
	std::string exprstr, escaped_name;

		// Set the correct types
	SetMyTypeName( invalidate_ad, QUERY_ADTYPE );
	if (enable_single_startd_daemon_ad) {
		invalidate_ad.Assign(ATTR_TARGET_TYPE, STARTD_SLOT_ADTYPE);
	} else {
		invalidate_ad.Assign(ATTR_TARGET_TYPE, STARTD_OLD_ADTYPE);
	}

	/*
	 * NOTE: the collector depends on the data below for performance reasons
	 * if you change here you will need to CollectorEngine::remove (AdTypes t_AddType, const ClassAd & c_query)
	 * the IP was added to allow the collector to create a hash key to delete in O(1).
     */
	 
     exprstr = std::string("TARGET." ATTR_NAME " == ") + QuoteAdStringValue(r_name, escaped_name);
     invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, exprstr.c_str() );
     invalidate_ad.Assign( ATTR_NAME, r_name );
     invalidate_ad.Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr());

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
	StartdPluginManager::Invalidate(&invalidate_ad);
#endif

	resmgr->send_update( INVALIDATE_STARTD_ADS, &invalidate_ad, NULL, false );
}


int
Resource::update_with_ack( void )
{
    const int timeout = 5;
    Daemon    collector ( DT_COLLECTOR );

    if ( !collector.locate () ) {

        dprintf (
            D_FULLDEBUG,
            "Failed to locate collector host.\n" );

        return FALSE;

    }

    const char *address = collector.addr ();
    ReliSock *socket  = (ReliSock*) collector.startCommand (
        UPDATE_STARTD_AD_WITH_ACK );

    if ( !socket ) {

        dprintf (
            D_FULLDEBUG,
            "update_with_ack: "
            "Failed to send UPDATE_STARTD_AD_WITH_ACK command "
            "to collector host %s.\n",
            address );

        return FALSE;

	}

    socket->timeout ( timeout );
    socket->encode ();

    ClassAd public_ad,
            private_ad;

	publish_single_slot_ad(public_ad, 0, Resource::Purpose::for_update);

	publish_private(&private_ad);


    if ( !putClassAd ( socket, public_ad ) ) {

        dprintf (
            D_FULLDEBUG,
            "update_with_ack: "
            "Failed to send public ad to collector host %s.\n",
            address );

        return FALSE;

    }

    if ( !putClassAd ( socket, private_ad ) ) {

		dprintf (
            D_FULLDEBUG,
            "update_with_ack: "
            "Failed to send private ad to collector host %s.\n",
            address );

        return FALSE;

    }

    if ( !socket->end_of_message() ) {

        dprintf (
            D_FULLDEBUG,
            "update_with_ack: "
            "Failed to send update EOM to collector host %s.\n",
            address );

        return FALSE;

	}

    socket->timeout ( timeout ); /* still more research... */
	socket->decode ();

    int ack     = 0,
        success = TRUE;

    if ( !socket->code ( ack ) ) {

        dprintf (
            D_FULLDEBUG,
            "update_with_ack: "
            "Failed to send query EOM to collector host %s.\n",
            address );

        /* looks like we didn't get the ack, so we need to fail so
        that we don't enter hibernation and eventually become
        unreachable because our machine ad is invalidated by the
        collector */

        success = FALSE;

    }

    socket->end_of_message();

    return success;

}

void
Resource::hold_job( bool soft )
{
	std::string hold_reason;
	int hold_subcode = 0;

	(void) EvalString("WANT_HOLD_REASON", r_classad, r_cur->ad(), hold_reason);
	if( hold_reason.empty() ) {
		ExprTree *want_hold_expr;
		std::string want_hold_str;

		want_hold_expr = r_classad->LookupExpr("WANT_HOLD");
		if( want_hold_expr ) {
			formatstr( want_hold_str, "%s = %s", "WANT_HOLD",
					ExprTreeToString( want_hold_expr ) );
		}

		hold_reason = "The startd put this job on hold.  (At preemption time, WANT_HOLD evaluated to true: ";
		hold_reason += want_hold_str;
		hold_reason += ")";
	}

	EvalInteger("WANT_HOLD_SUBCODE", r_classad, r_cur->ad(), hold_subcode);

	r_cur->starterHoldJob(hold_reason.c_str(),CONDOR_HOLD_CODE::StartdHeldJob,hold_subcode,soft);
}

int
Resource::wants_hold( void )
{
	int want_hold = eval_expr("WANT_HOLD",false,false);

	if( want_hold == -1 ) {
		want_hold = 0;
		dprintf(D_ALWAYS,"State change: WANT_HOLD is undefined --> FALSE.\n");
	}
	else {
		dprintf( D_ALWAYS, "State change: WANT_HOLD is %s\n",
				 want_hold ? "TRUE" : "FALSE" );
	}
	return want_hold;
}


int
Resource::wants_vacate( void )
{
	bool want_vacate = false;
	bool unknown = true;

	if( ! claimIsActive() ) {
			// There's no job here, so chances are good that some of
			// the job attributes that WANT_VACATE might be defined in
			// terms of won't exist.  So, instead of getting
			// undefined, just return True, since w/o a job, vacating
			// a job is basically a no-op.
			// Derek Wright <wright@cs.wisc.edu>, 6/21/00
		dprintf( D_FULLDEBUG,
				 "In Resource::wants_vacate() w/o a job, returning TRUE\n" );
		dprintf( D_ALWAYS, "State change: no job, WANT_VACATE considered TRUE\n" );
		return 1;
	}

	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( EvalBool("WANT_VACATE_VANILLA", r_classad,
								r_cur->ad(),
								want_vacate ) ) {
			dprintf( D_ALWAYS, "State change: WANT_VACATE_VANILLA is %s\n",
					 want_vacate ? "TRUE" : "FALSE" );
			unknown = false;
		}
	}
	if( r_cur->universe() == CONDOR_UNIVERSE_VM ) {
		if( EvalBool("WANT_VACATE_VM", r_classad,
								r_cur->ad(),
								want_vacate ) ) {
			dprintf( D_ALWAYS, "State change: WANT_VACATE_VM is %s\n",
					 want_vacate ? "TRUE" : "FALSE" );
			unknown = false;
		}
	}
	if( unknown ) {
		if( EvalBool( "WANT_VACATE", r_classad,
								 r_cur->ad(),
								 want_vacate ) == 0) {

			dprintf( D_ALWAYS,
					 "In Resource::wants_vacate() with undefined WANT_VACATE\n" );
			dprintf( D_ALWAYS, "INTERNAL AD:\n" );
			dPrintAd( D_ALWAYS, *r_classad );
			if( r_cur->ad() ) {
				dprintf( D_ALWAYS, "JOB AD:\n" );
				dPrintAd( D_ALWAYS, *r_cur->ad() );
			} else {
				dprintf( D_ALWAYS, "ERROR! No job ad!!!!\n" );
			}

				// This should never happen, since we already check
				// when we're constructing the internal config classad
				// if we've got this defined. -Derek Wright 4/12/00
			EXCEPT( "WANT_VACATE undefined in internal ClassAd" );
		}
		dprintf( D_ALWAYS, "State change: WANT_VACATE is %s\n",
				 want_vacate ? "TRUE" : "FALSE" );
	}
	return (int)want_vacate;
}


int
Resource::wants_suspend( void )
{
	bool want_suspend;
	bool unknown = true;
	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( EvalBool("WANT_SUSPEND_VANILLA", r_classad,
								r_cur->ad(),
								want_suspend) ) {
			unknown = false;
		}
	}
	if( r_cur->universe() == CONDOR_UNIVERSE_VM ) {
		if( EvalBool("WANT_SUSPEND_VM", r_classad,
								r_cur->ad(),
								want_suspend) ) {
			unknown = false;
		}
	}
	if( unknown ) {
		if( EvalBool( "WANT_SUSPEND", r_classad,
								   r_cur->ad(),
								   want_suspend ) == 0) {
				// UNDEFINED means FALSE for WANT_SUSPEND
			want_suspend = false;
		}
	}
	return (int)want_suspend;
}


int
Resource::wants_pckpt( void )
{
	switch( r_cur->universe() ) {
		case CONDOR_UNIVERSE_VANILLA: {
			ClassAd * jobAd = r_cur->ad();
			bool wantCheckpoint = false;
			jobAd->LookupBool( ATTR_WANT_CHECKPOINT_SIGNAL, wantCheckpoint );
			if( ! wantCheckpoint ) { return FALSE; }
			} break;

		case CONDOR_UNIVERSE_VM:
			break;

		default:
			return FALSE;
	}

	bool want_pckpt;
	if( EvalBool( "PERIODIC_CHECKPOINT", r_classad,
				r_cur->ad(),
				want_pckpt ) == 0) { 
		// Default to no, if not defined.
		want_pckpt = false;
	}

	return want_pckpt;
}

int
Resource::hasPreemptingClaim()
{
	return (r_pre && r_pre->requestStream());
}

int
Resource::mayUnretire()
{
	if(!isDraining() && r_cur && r_cur->mayUnretire()) {
		if(!hasPreemptingClaim()) {
			// preempting claim has gone away
			return 1;
		}
	}
	return 0;
}

bool
Resource::inRetirement()
{
	return hasPreemptingClaim() || !mayUnretire();
}

int
Resource::preemptWasTrue() const
{
	if(r_cur) return r_cur->preemptWasTrue();
	return 0;
}

void
Resource::preemptIsTrue()
{
	if(r_cur) r_cur->preemptIsTrue();
}

bool
Resource::curClaimIsClosing()
{
	return
		hasPreemptingClaim() ||
		activity() == retiring_act ||
		state() == preempting_state ||
		claimWorklifeExpired() ||
		isDraining();
}

bool
Resource::isDraining()
{
	return resmgr->isSlotDraining(this);
}

bool
Resource::claimWorklifeExpired()
{
	//This function evaulates to true if the claim has been alive
	//for longer than the CLAIM_WORKLIFE expression dictates.

	if( r_cur && r_cur->activationCount() > 0 ) {
		int ClaimWorklife = 0;

		//look up the maximum retirement time specified by the startd
		if(!r_classad->LookupInteger(
				  "CLAIM_WORKLIFE",
		          ClaimWorklife)) {
			ClaimWorklife = -1;
		}

		int ClaimAge = r_cur->getClaimAge();

		if(ClaimWorklife >= 0) {
			dprintf(D_FULLDEBUG,"Computing claimWorklifeExpired(); ClaimAge=%d, ClaimWorklife=%d\n",ClaimAge,ClaimWorklife);
			return (ClaimAge > ClaimWorklife);
		}
	}
	return false;
}

int
Resource::evalRetirementRemaining()
{
	int MaxJobRetirementTime = 0;
	int JobMaxJobRetirementTime = 0;
	int JobAge = 0;

	if (r_cur && r_cur->isActive() && r_cur->ad()) {
		//look up the maximum retirement time specified by the startd
		if(!r_classad->LookupExpr(ATTR_MAX_JOB_RETIREMENT_TIME) ||
		   !EvalInteger(
		          ATTR_MAX_JOB_RETIREMENT_TIME, r_classad,
		          r_cur->ad(),
		          MaxJobRetirementTime)) {
			MaxJobRetirementTime = 0;
		}
		// GT#6701: Allow job policy to be effective even during peaceful
		// retirement (e.g., just because I'm turning the machine off
		// doesn't mean you get to use too much RAM).
		//
		// GT#7034: A computed MJRT of -1 means preempt immediately, even
		// if you're retiring peacefully.  Otherwise, the job gets all
		// all the time in the world.
		if( r_cur->getRetirePeacefully() ) {
			if( MaxJobRetirementTime != -1 ) {
				MaxJobRetirementTime = INT_MAX;
			}
		}
		//look up the maximum retirement time specified by the job
		if(r_cur->ad()->LookupExpr(ATTR_MAX_JOB_RETIREMENT_TIME) &&
		   EvalInteger(
		          ATTR_MAX_JOB_RETIREMENT_TIME, r_cur->ad(),
		          r_classad,
		          JobMaxJobRetirementTime)) {
			if(JobMaxJobRetirementTime < MaxJobRetirementTime) {
				//The job wants _less_ retirement time than the startd offers,
				//so let it have its way.
				MaxJobRetirementTime = JobMaxJobRetirementTime;
			}
		}
	}

	if (r_cur && r_cur->isActive()) {
		JobAge = r_cur->getJobTotalRunTime();
	}
	else {
		// There is no job running, so there is no point in waiting any longer
		MaxJobRetirementTime = 0;
	}

	int remaining = MaxJobRetirementTime - JobAge;
	return (remaining < 0) ? 0 : remaining;
}

bool
Resource::retirementExpired()
{
	//
	// GT#6697: Allow job policy to be effective while draining.  Only
	// attempt to synchronize retirement if an individual job's computed
	// retirement is <= 0 and that job either (a) was accepted while
	// draining or (b) matches the new START expression (that is, is
	// interruptible).  This design intends to allow job policy to work
	// without churning backfill jobs (by letting them run as long as the
	// machine is waiting for non-interruptible jobs to run).
	//
	// Problem: when the slot transitions from Owner to Claimed/Idle,
	// ResState::eval() checks to see if the slot is in retirement and
	// if it is, if the retirement has expired.  We end up claiming it has
	// because, of course, idle slots don't need any retirement time. This
	// didn't used to happen because retirementExpired() didn't used to
	// respect MJRTs of 0 for individual slots -- instead, if wer were in
	// retirement, the largest inividual retirement would be returned.
	//
	// So, during retirement, if we're Claimed/Idle, return the coordinated
	// draining time instead of anything else.
	//

	int retirement_remaining = evalRetirementRemaining();
	if( isDraining() && r_state->state() == claimed_state && r_state->activity() == idle_act ) {
		retirement_remaining = resmgr->gracefulDrainingTimeRemaining( this ) ;
	} else if( isDraining() && retirement_remaining > 0 ) {
		bool jobMatches = false;
		ClassAd * machineAd = r_classad;
		ClassAd * jobAd = r_cur->ad();
		if( machineAd != NULL && jobAd != NULL ) {
			// Assumes EvalBool() doesn't modify its output argument on failure.
			(void) EvalBool( ATTR_START, machineAd, jobAd, jobMatches );
		}

		if( jobMatches || wasAcceptedWhileDraining() ) {
			retirement_remaining = resmgr->gracefulDrainingTimeRemaining( this ) ;
		}
	}

	if( retirement_remaining <= 0 ) {
		return true;
	}

	int max_vacate_time = evalMaxVacateTime();
	if( max_vacate_time >= retirement_remaining ) {
			// the goal is to begin evicting the job before the end of
			// retirement so that if the job uses the full eviction
			// time, it will finish by the end of retirement

		dprintf(D_ALWAYS,"Evicting job with %ds of retirement time "
				"remaining in order to accommodate this job's "
				"max vacate time of %ds.\n",
				retirement_remaining,max_vacate_time);
		return true;
	}
	return false;
}

int
Resource::evalMaxVacateTime()
{
	int MaxVacateTime = 0;

	if (r_cur && r_cur->isActive() && r_cur->ad()) {
		// Look up the maximum vacate time specified by the startd.
		// This was evaluated at claim activation time.
		int MachineMaxVacateTime = r_cur->getPledgedMachineMaxVacateTime();

		MaxVacateTime = MachineMaxVacateTime;
		int JobMaxVacateTime = MaxVacateTime;

		//look up the maximum vacate time specified by the job
		if(r_cur->ad()->LookupExpr(ATTR_JOB_MAX_VACATE_TIME)) {
			if( !EvalInteger(
					ATTR_JOB_MAX_VACATE_TIME, r_cur->ad(),
					r_classad,
					JobMaxVacateTime) )
			{
				JobMaxVacateTime = 0;
			}
		}
		else if( r_cur->ad()->LookupExpr(ATTR_KILL_SIG_TIMEOUT) ) {
				// the old way of doing things prior to JobMaxVacateTime
			if( !EvalInteger(
					ATTR_KILL_SIG_TIMEOUT, r_cur->ad(),
					r_classad,
					JobMaxVacateTime) )
			{
				JobMaxVacateTime = 0;
			}
		}
		if(JobMaxVacateTime <= MaxVacateTime) {
				//The job wants _less_ time than the startd offers,
				//so let it have its way.
			MaxVacateTime = JobMaxVacateTime;
		}
		else {
				// The job wants more vacate time than the startd offers.
				// See if the job can use some of its remaining retirement
				// time as vacate time.

			int retirement_remaining = evalRetirementRemaining();
			if( retirement_remaining >= JobMaxVacateTime ) {
					// there is enough retirement time left to
					// give the job the vacate time it wants
				MaxVacateTime = JobMaxVacateTime;
			}
			else if( retirement_remaining > MaxVacateTime ) {
					// There is not enough retirement time left to
					// give the job all the vacate time it wants,
					// but there is enough to give it more than
					// what the machine would normally offer.
				MaxVacateTime = retirement_remaining;
			}
		}
	}

	return MaxVacateTime;
}

// returns -1 on undefined, 0 on false, 1 on true
int
Resource::eval_expr( const char* expr_name, bool fatal, bool check_vanilla )
{
	int tmp;
	if( check_vanilla && r_cur && r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		std::string tmp_expr_name = expr_name;
		tmp_expr_name += "_VANILLA";
		tmp = eval_expr( tmp_expr_name.c_str(), false, false );
		if( tmp >= 0 ) {
				// found it, just return the value;
			return tmp;
		}
			// otherwise, fall through and try the non-vanilla version
	}
	if( check_vanilla && r_cur && r_cur->universe() == CONDOR_UNIVERSE_VM ) {
		std::string tmp_expr_name = expr_name;
		tmp_expr_name += "_VM";
		tmp = eval_expr( tmp_expr_name.c_str(), false, false );
		if( tmp >= 0 ) {
				// found it, just return the value;
			return tmp;
		}
			// otherwise, fall through and try the non-vm version
	}
	bool btmp;
	if( (EvalBool(expr_name, r_classad, r_cur ? r_cur->ad() : NULL , btmp) ) == 0 ) {
		
		char *p = param(expr_name);

			// Only issue warning if we are trying to define the expression
		if (p) {
        	dprintf( D_ALWAYS, "WARNING: EvalBool of %s resulted in ERROR or UNDEFINED\n", expr_name );
			free(p);
		}
        
        if( fatal ) {
			dprintf(D_ALWAYS, "Can't evaluate %s in the context of following ads\n", expr_name );
			dPrintAd(D_ALWAYS, *r_classad);
			dprintf(D_ALWAYS, "=============================\n");
			if ( r_cur && r_cur->ad() ) {
				dPrintAd(D_ALWAYS, *r_cur->ad());
			} else {
				dprintf( D_ALWAYS, "<no job ad>\n" );
			}
			EXCEPT( "Invalid evaluation of %s was marked as fatal", expr_name );
		} else {
				// anything else for here?
			return -1;
		}
	}
		// EvalBool returned success, we can just return the value
	return (int)btmp;
}


#if HAVE_HIBERNATION

bool
Resource::evaluateHibernate( std::string &state_str ) const
{
	ClassAd *ad = NULL;
	if ( NULL != r_cur ) {
		ad = r_cur->ad();
	}

	if ( EvalString( "HIBERNATE", r_classad, ad, state_str ) ) {
		return true;
	}
	return false;
}

#endif /* HAVE_HIBERNATION */


int
Resource::eval_kill()
{
	return eval_expr( "KILL", false, true );
}


int
Resource::eval_preempt( void )
{
	return eval_expr( "PREEMPT", false, true );
}


int
Resource::eval_suspend( void )
{
	return eval_expr( "SUSPEND", false, true );
}


int
Resource::eval_continue( void )
{
	return (r_suspended_by_command)?false:eval_expr( "CONTINUE", false, true );
}


int
Resource::eval_is_owner( void )
{
	// fatal if undefined, don't check vanilla
	return eval_expr( ATTR_IS_OWNER, true, false );
}


int
Resource::eval_start( void )
{
	// -1 if undefined, don't check vanilla
	return eval_expr( "START", false, false );
}


#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
#else

int
Resource::eval_cpu_busy( void )
{
	int rval = 0;
	if( ! r_classad ) {
			// We don't have our classad yet, so just return that
			// we're not busy.
		return 0;
	}
		// not fatal, don't check vanilla
	rval = eval_expr( ATTR_CPU_BUSY, false, false );
	if( rval < 0 ) {
			// Undefined, try "cpu_busy"
		rval = eval_expr( "CPU_BUSY", false, false );
	}
	if( rval < 0 ) {
			// Totally undefined, return false;
		return 0;
	}
	return rval;
}

#endif

#if HAVE_BACKFILL

int
Resource::eval_start_backfill( void )
{
	int rval = 0;
	rval = eval_expr( "START_BACKFILL", false, false );
	if( rval < 0 ) {
			// Undefined, return false
		return 0;
	}
	return rval;
}


int
Resource::eval_evict_backfill( void )
{
		// return -1 on undefined (not fatal), don't check vanilla
	return eval_expr( "EVICT_BACKFILL", false, false );
}


bool
Resource::start_backfill( void )
{
	return resmgr->m_backfill_mgr->start(r_id);
}


bool
Resource::softkill_backfill( void )
{
	return resmgr->m_backfill_mgr->softkill(r_id);
}


bool
Resource::hardkill_backfill( void )
{
	return resmgr->m_backfill_mgr->hardkill(r_id);
}


#endif /* HAVE_BACKFILL */

void Resource::refresh_draining_attrs() {
	// this needs to refresh 
	if (r_classad) {
		r_classad->InsertAttr( "AcceptedWhileDraining", m_acceptedWhileDraining );
		if( resmgr->getMaxJobRetirementTimeOverride() >= 0 ) {
			r_classad->InsertAttr( ATTR_MAX_JOB_RETIREMENT_TIME, resmgr->getMaxJobRetirementTimeOverride() );
		} else {
			caRevertToParent(r_classad, ATTR_MAX_JOB_RETIREMENT_TIME);
		}
	}
}
void Resource::refresh_startd_cron_attrs() {
	if (r_classad) {
		// Publish the supplemental Class Ads IS_UPDATE
		resmgr->adlist_publish(r_id, r_classad, r_id_str );
	}
}

void Resource::refresh_classad_slot_attrs() {
	if (r_classad) {
		resmgr->publishSlotAttrs(r_classad);
	}
}

void Resource::publish_static(ClassAd* cap)
{
	bool internal_ad = (cap == r_config_classad || cap == r_classad);
	bool wrong_internal_ad = (cap == r_classad);
	//if (param_boolean("STARTD_TRACE_PUBLISH_CALLS", false)) {
	//	dprintf(D_ALWAYS | D_BACKTRACE, "in Resource::publish_static(%p) for %s classad=%p, base=%p\n", cap, r_name, r_classad, r_config_classad);
	//}
	if (wrong_internal_ad) {
		dprintf(D_ALWAYS, "ERROR! updating the wrong internal ad!\n");
	} else {
		dprintf(D_TEST | D_VERBOSE, "Resource::publish_static, %s ad\n", internal_ad ? "internal" : "external");
	}

	// Set the correct types on the ClassAd
	dprintf(D_ZKM | D_VERBOSE, "Resource::publish_static send_daemon_ad=%d\n", enable_single_startd_daemon_ad);
	if (enable_single_startd_daemon_ad) {
		cap->Assign(ATTR_HAS_START_DAEMON_AD, true);
		SetMyTypeName( *cap,STARTD_SLOT_ADTYPE );
	} else {
		SetMyTypeName( *cap,STARTD_OLD_ADTYPE );
	}
	cap->Assign(ATTR_TARGET_TYPE, JOB_ADTYPE); // because matchmaking before 23.0 needs this

	// We need these for both public and private ads
	cap->Assign(ATTR_STARTD_IP_ADDR, daemonCore->InfoCommandSinfulString());
	cap->Assign(ATTR_NAME, r_name);

	cap->Assign(ATTR_IS_LOCAL_STARTD, param_boolean("IS_LOCAL_STARTD", false));

	{
		// Since the Rank expression itself only lives in the
		// config file and the r_classad (not any obejects), we
		// have to insert it here from r_classad.  If Rank is
		// undefined in r_classad, we need to insert a default
		// value, since we don't want to use the job ClassAd's
		// Rank expression when we evaluate our Rank value.
		if( !internal_ad && !caInsert(cap, r_classad, ATTR_RANK) ) {
			cap->Assign( ATTR_RANK, 0.0 );
		}

		// Similarly, the CpuBusy expression only lives in the
		// config file and in the r_classad.  So, we have to
		// insert it here, too.  This is just the expression that
		// defines what "CpuBusy" means, not the current value of
		// it and how long it's been true.  Those aren't static,
		// and need to be re-published after they're evaluated.
		if( !internal_ad && !caInsert(cap, r_classad, ATTR_CPU_BUSY) ) {
			EXCEPT( "%s not in internal resource classad, but default "
				"should be added by ResMgr!", ATTR_CPU_BUSY );
		}

		if (!internal_ad) { caInsert(cap, r_classad, ATTR_SLOT_WEIGHT); }

#if HAVE_HIBERNATION
		if (!internal_ad) { caInsert(cap, r_classad, ATTR_UNHIBERNATE); }
#endif

		// this sets STARTD_ATTRS, but ignores the SLOT_TYPE_n override values
		daemonCore->publish(cap);
		// remove time from static ad?
		//if (cap == r_config_classad) { cap->Delete(ATTR_MY_CURRENT_TIME); }

		// Put in cpu-specific attributes that can only change on reconfig
		resmgr->publish_static_slot_resources(this, cap);

		// Put in machine-wide attributes that can only change on reconfig
		resmgr->m_attr->publish_static(cap);
		resmgr->publish_static(cap);

		// now override the global STARTD_ATTRS values with SLOT_TYPE_n_* or SLOTn_* values if they exist.
		// the list of attributes can also be ammended at this level, but note that the list can only be expanded
		// it cannot be fully overridden via a SLOT override. (it will just be left with daemonCore->publish values)

		// use the slot name prefix up to the first _ as a config prefix, we do this so dynamic slots
		// get the same values as their parent slots.
		std::string slot_name(r_id_str);
		size_t iUnderPos = slot_name.find('_');
		if (iUnderPos != std::string::npos) { slot_name.erase (iUnderPos); }

		// load the relevant param that controls the attribute list.  
		// We preserve the behavior of config_fill_ad here so the effective list is STARTD_ATTRS + SLOTn_STARTD_ATTRS
		// but because param is overridden in this class the actual effect is that if SLOT_TYPE_n_* attributes
		// are defined, they are loaded INSTEAD of global params without the SLOT_TYPE_n_ prefix
		// so this CAN generate a completely different list than config_fill_ad uses.  When that happens
		// the values set in daemonCore->publish will be left alone <sigh>
		//
		std::vector<std::string> slot_attrs;
		auto_free_ptr tmp(param("STARTD_ATTRS"));
		if ( ! tmp.empty()) {
			for (const auto& item: StringTokenIterator(tmp)) {
				slot_attrs.emplace_back(item);
			}
		}
		std::string param_name(slot_name); param_name += "_STARTD_ATTRS";
		tmp.set(param(param_name.c_str()));
		if ( ! tmp.empty()) {
			for (const auto& item: StringTokenIterator(tmp)) {
				slot_attrs.emplace_back(item);
			}
		}

	#ifdef USE_STARTD_LATCHES
		// append latch expressions
		tmp.set(param("STARTD_LATCH_EXPRS"));
		if ( ! tmp.empty()) {
			for (const auto& item: StringTokenIterator(tmp)) {
				slot_attrs.emplace_back(item);
			}
		}
	#endif

		// check for obsolete STARTD_EXPRS and generate a warning if both STARTD_ATTRS and STARTD_EXPRS is set.
		if ( ! slot_attrs.empty() && ! warned_startd_attrs_once)
		{
			std::string tname(slot_name); tname += "_STARTD_EXPRS";
			auto_free_ptr tmp2(param(tname.c_str()));
			if ( ! tmp2.empty()) {
				dprintf(D_ALWAYS, "WARNING: config contains obsolete %s or SLOT_TYPE_n_%s which will be (partially) ignored! use *_STARTD_ATTRS instead.\n", tname.c_str(), tname.c_str());
			}
			tmp2.set(param("STARTD_EXPRS"));
			if ( ! tmp2.empty()) {
				dprintf(D_ALWAYS, "WARNING: config contains obsolete STARTD_EXPRS or SLOT_TYPE_n_STARTD_EXPRS which will be (partially) ignored! use STARTD_ATTRS instead.\n");
			}
			warned_startd_attrs_once = true;
		}

		// now append any attrs needed by HTCondor itself
		tmp.set(param("SYSTEM_STARTD_ATTRS"));
		if ( ! tmp.empty()) {
			for (const auto& item: StringTokenIterator(tmp)) {
				slot_attrs.emplace_back(item);
			}
		}

	#ifdef USE_STARTD_LATCHES
		// append system latch expressions
		tmp.set(param("SYSTEM_STARTD_LATCH_EXPRS"));
		if ( ! tmp.empty()) {
			for (const auto& item: StringTokenIterator(tmp)) {
				slot_attrs.emplace_back(item);
			}
		}
	#endif

		for (const auto& attr: slot_attrs) {
			formatstr(param_name, "%s_%s", slot_name.c_str(), attr.c_str());
			tmp.set(param(param_name.c_str()));
			if ( ! tmp) { tmp.set(param(attr.c_str())); } // if no SLOTn_attr config definition, lookup just attr
			if ( ! tmp) continue;  // no definition of either type, skip this attribute.

			if (tmp.empty()) {
				// interpret explicit empty values as 'remove the attribute' (should we set to undefined instead?)
				cap->Delete(attr);
			} else {
				if ( ! cap->AssignExpr(attr, tmp.ptr()) ) {
					dprintf(D_ERROR,
						"CONFIGURATION PROBLEM: Failed to insert ClassAd attribute %s = %s."
						"  The most common reason for this is that you forgot to quote a string value in the list of attributes being added to the %s ad.\n",
						attr.c_str(), tmp.ptr(), slot_name.c_str() );
				}
			}
		}

		// Also, include a slot ID attribute, since it's handy for
		// defining expressions, and other things.
		cap->Assign(ATTR_SLOT_ID, r_id);

		if (r_backfill_slot) cap->Assign(ATTR_SLOT_BACKFILL, true);

		// include any attributes set via local resource inventory
		cap->Update(resmgr->m_attr->machres_attrs());

		// advertise the slot type id number, as in SLOT_TYPE_<N>
		cap->Assign(ATTR_SLOT_TYPE_ID, r_attr->type_id() );

		switch (get_feature()) {
		case PARTITIONABLE_SLOT:
			cap->Assign(ATTR_SLOT_PARTITIONABLE, true);
			cap->Assign(ATTR_SLOT_TYPE, "Partitionable");
			if (enable_claimable_partitionable_slots) {
				int lease = param_integer("MAX_PARTITIONABLE_SLOT_CLAIM_TIME", 3600);
				cap->Assign(ATTR_MAX_CLAIM_TIME, lease);
			}
			break;
		case DYNAMIC_SLOT:
			cap->Assign(ATTR_SLOT_DYNAMIC, true);
			cap->Assign(ATTR_SLOT_TYPE, "Dynamic");
			cap->Assign(ATTR_PARENT_SLOT_ID, r_id);
			cap->Assign(ATTR_DSLOT_ID, r_sub_id);
			if ( param_boolean("ADVERTISE_PSLOT_ROLLUP_INFORMATION", true) ) {
				// the Negotiator uses this to determine if the p-slot will have rollup from the d-slot
				cap->Assign(ATTR_PSLOT_ROLLUP_INFORMATION, true);
			}
			break;
		default:
			cap->Assign(ATTR_SLOT_TYPE, "Static");
			break; // Do nothing
		}
	}

	// slot weight and consumption policy
	{
		std::string pname;
		std::string expr;
		// A negative slot type indicates a d-slot, in which case I want to use
		// the slot-type of its parent for inheriting CP-related configurations:
		unsigned int slot_type = type_id();

		if ( ! SlotType::param(expr, r_attr, "SLOT_WEIGHT") &&
			 ! SlotType::param(expr, r_attr, "SlotWeight")) {
			expr = "Cpus";
		}

        if (!cap->AssignExpr(ATTR_SLOT_WEIGHT, expr.c_str())) {
            EXCEPT("Bad slot weight expression: '%s'", expr.c_str());
        }

        if (r_has_cp || (m_parent && m_parent->r_has_cp)) {
            dprintf(D_FULLDEBUG, "Acquiring consumption policy configuration for slot type %d\n", slot_type);
            // If we have a consumption policy, then we acquire config for it
            // cpus, memory and disk always exist, and have asset-specific defaults:
            string mrv;
            if (!cap->LookupString(ATTR_MACHINE_RESOURCES, mrv)) {
                EXCEPT("Resource ad missing %s attribute", ATTR_MACHINE_RESOURCES);
            }

            for (const auto& asset: StringTokenIterator(mrv)) {
                if (MATCH == strcasecmp(asset.c_str(), "swap")) continue;

				pname = "CONSUMPTION_"; pname += asset;
				if ( ! SlotType::param(expr, r_attr, pname.c_str())) {
					formatstr(expr, "ifthenelse(target.%s%s =?= undefined, 0, target.%s%s)", ATTR_REQUEST_PREFIX, asset.c_str(), ATTR_REQUEST_PREFIX, asset.c_str());
				}

                string rattr;
                formatstr(rattr, "%s%s", ATTR_CONSUMPTION_PREFIX, asset.c_str());
                if (!cap->AssignExpr(rattr, expr.c_str())) {
                    EXCEPT("Bad consumption policy expression: '%s'", expr.c_str());
                }
            }
        }
    }
}


void
Resource::publish_dynamic(ClassAd* cap)
{
	bool internal_ad = (cap == r_config_classad || cap == r_classad);
	bool wrong_internal_ad = (cap == r_config_classad);
	//if (param_boolean("STARTD_TRACE_PUBLISH_CALLS", false)) {
	//	dprintf(D_ALWAYS | D_BACKTRACE, "in Resource::publish_dynamic(%p) for %s classad=%p, base=%p\n", cap, r_name, r_classad, r_config_classad);
	//}
	if (wrong_internal_ad) {
		dprintf(D_ALWAYS | D_BACKTRACE, "ERROR! updating the wrong internal ad!\n");
	} else {
		dprintf(D_TEST | D_VERBOSE, "Resource::publish_dynamic, %s ad\n", internal_ad ? "internal" : "external");
	}

	cap->Assign(ATTR_MY_CURRENT_TIME, resmgr->now());

	// If we're claimed or preempting, handle anything listed
	// in STARTD_JOB_ATTRS.
	// Our current claim object might be gone though, so make
	// sure we have the object before we try to use it.
	// Also, our current claim object might not have a
	// ClassAd, so be careful about that, too.
	State s = this->state();
	if (s == claimed_state || s == preempting_state) {
		if (r_cur && r_cur->ad()) {
			for (const auto& attr: startd_job_attrs) {
				caInsert(cap, r_cur->ad(), attr.c_str());
			}
		}
	}

	if (is_partitionable_slot()) {
		if (r_has_cp) cap->Assign(ATTR_NUM_CLAIMS, (long long)r_claims.size());
	}

	// Put in cpu-specific attributes (TotalDisk, LoadAverage)
	r_attr->publish_dynamic(cap);

	// Put in machine-wide dynamic attributes (Mips, OfflineGPUs)
	resmgr->m_attr->publish_common_dynamic(cap);
	// Put in per-slot dynamic (AvailableGpus, ResourceConflict)
	resmgr->m_attr->publish_slot_dynamic(cap, r_id, r_sub_id, r_backfill_slot, m_res_conflict);

	// Put in ResMgr-specific attributes hibernation, TTL, STARTD_CRON
	resmgr->publish_resmgr_dynamic(cap);

	resmgr->publish_draining_attrs(this, cap);	// always

	// put in evaluated attributes e.g. CpuBusyTime
	publish_evaluated(cap);

	// Put in state info	   A_ALWAYS
	r_state->publish( cap );

	// This is a bit of a mess, but unwinding all of the call sites of this function will have to
	// wait for a future refactoring pass.  for now, if the caller passes the internal r_classad
	// we call the ReqExp::publish function that knows how to manage the internal ads, otherwise
	// we just blast stuff into the add that was passed in.
	//
	if (internal_ad) {
		r_reqexp->publish(this);
	} else {
		r_reqexp->publish_external(cap);
	}

	cap->Assign( ATTR_RETIREMENT_TIME_REMAINING, evalRetirementRemaining() );
	if (! internal_ad) { caInsert(cap, r_classad, ATTR_MACHINE_MAX_VACATE_TIME); }

#if HAVE_JOB_HOOKS
	cap->Assign( ATTR_LAST_FETCH_WORK_SPAWNED, m_last_fetch_work_spawned );
	cap->Assign( ATTR_LAST_FETCH_WORK_COMPLETED, m_last_fetch_work_completed );
	cap->Assign( ATTR_NEXT_FETCH_WORK_DELAY, m_next_fetch_work_delay );
#endif /* HAVE_JOB_HOOKS */

	// Update info from the current Claim object, if it exists.
	if( r_cur ) {
		r_cur->publish( cap );
		if (state() == claimed_state) {
			cap->Assign(ATTR_PUBLIC_CLAIM_ID, r_cur->publicClaimId());
			cap->Assign(ATTR_CLAIM_END_TIME, r_cur->getLeaseEndtime());
		}

	}
	if( r_pre ) {
		r_pre->publishPreemptingClaim( cap );
	}

	r_cod_mgr->publish(cap);  // should probably be IS_STATIC?? or IS_TIMER?

	// Publish the supplemental Class Ads IS_UPDATE
	resmgr->adlist_publish(r_id, cap, r_id_str);

	// Publish the monitoring information ALWAYS
	daemonCore->dc_stats.Publish(*cap);
	daemonCore->monitor_data.ExportData( cap );

	cap->InsertAttr( "AcceptedWhileDraining", m_acceptedWhileDraining );
	if( resmgr->getMaxJobRetirementTimeOverride() >= 0 ) {
		cap->Assign( ATTR_MAX_JOB_RETIREMENT_TIME, resmgr->getMaxJobRetirementTimeOverride() );
	} else {
		if (cap == r_classad) {
			caRevertToParent(cap, ATTR_MAX_JOB_RETIREMENT_TIME);
		} else if ( ! internal_ad) {
			caInsert(cap, r_classad, ATTR_MAX_JOB_RETIREMENT_TIME);
		}
	}

	//TODO: move this and startd_slot_attrs publishing out of here an into a separate pass
	//      for now this must be here so that when resemgr does Walk(&Resource::refresh_classad_slot_attrs)
	//      it can see the rollup
	if (is_partitionable_slot()) { publishDynamicChildSummaries(cap); }
}

void Resource::publish_evaluated(ClassAd * cap)
{
	time_t now = resmgr->now();
#ifdef USE_STARTD_LATCHES
	publish_latches(cap, now);
#else
	cap->Assign(ATTR_CPU_BUSY_TIME, cpu_busy_time(now));
	cap->Assign(ATTR_CPU_IS_BUSY, r_cpu_busy ? true : false);
#endif
}

void Resource::refresh_sandbox_ad(ClassAd*cap)
{
	// If we have a starter that is alive enough to have sent it's first update.
	// And it has not yet sent the final update when we may want to refresh the .update.ad
	// Don't bother to write an ad to disk that won't include the extras ads.
	// Also only write the ad to disk when the claim has a ClassAd and the
	// starter knows where the execute directory is.  Empirically, this set
	// of conditions also ensures that reset_monitor() has been called, so
	// the first ad we write will include the StartOfJob* attribute(s).
	Starter * starter = NULL;
	if (r_cur && r_cur->ad() && r_cur->starterPID()) {
		starter = findStarterByPid(r_cur->starterPID());
		if (starter && ( ! starter->got_update() || starter->got_final_update())) {
			dprintf(D_FULLDEBUG, "Skipping refresh of .update.ad because Starter is alive, but %s\n",
				starter->got_final_update() ? "sent final update" : "has not yet sent any updates");
			starter = NULL;
		}
	}
	if (starter && starter->executeDir()) {

		std::string updateAdDir;
		formatstr( updateAdDir, "%s%cdir_%d", starter->executeDir(), DIR_DELIM_CHAR, r_cur->starterPID() );

		// Write to a temporary file first and then rename it
		// to ensure atomic updates.
		std::string updateAdTmpPath;
		dircat(updateAdDir.c_str(), ".update.ad.tmp", updateAdTmpPath);

		FILE * updateAdFile = NULL;
#if defined(WINDOWS)
		// use don't set_user_priv on windows because we don't have an easy way to figure
		// out what user to use and it matters a lot less that we do so. 
		updateAdFile = safe_fopen_wrapper_follow( updateAdTmpPath.c_str(), "w" );
		if (updateAdFile) {
			// If directory encryption is on, we want to make sure that this file is not encryped
			// since it would be encrypted by LOCAL_SYSTEM and not readable by others
			DecryptFile(updateAdTmpPath.c_str(), 0);
		}
#else
		StatInfo si( updateAdDir.c_str() );
		if((!si.Error()) && (si.GetOwner() > 0) && (si.GetGroup() > 0)) {
			set_user_ids( si.GetOwner(), si.GetGroup() );
			TemporaryPrivSentry p( PRIV_USER, true );
			updateAdFile = safe_fopen_wrapper_follow( updateAdTmpPath.c_str(), "w" );
		}
#endif

		if( updateAdFile ) {
			classad::References r;
			sGetAdAttrs( r, * cap, true, NULL );

			std::string adstring;
			sPrintAdAttrs( adstring, * cap, r );

			// It's important for ToE tag updating that this does NOT
			// produce an extra trailing newline.
			fprintf( updateAdFile, "%s", adstring.c_str() );

			fclose( updateAdFile );


			// Rename the temporary.
			std::string updateAdPath;
			dircat(updateAdDir.c_str(), ".update.ad", updateAdPath);

#if defined(WINDOWS)
			rotate_file( updateAdTmpPath.c_str(), updateAdPath.c_str() );
#else
			StatInfo si( updateAdDir.c_str() );
			if(! si.Error()) {
				set_user_ids( si.GetOwner(), si.GetGroup() );
				TemporaryPrivSentry p(PRIV_USER, true);
				if (rename(updateAdTmpPath.c_str(), updateAdPath.c_str()) < 0) {
					dprintf(D_ALWAYS, "Failed to rename update ad from  %s to %s\n", updateAdTmpPath.c_str(), updateAdPath.c_str());
				}
			}
#endif
		} else {
			dprintf( D_ALWAYS, "Failed to open '%s' for writing update ad: %s (%d).\n",
				updateAdTmpPath.c_str(), strerror( errno ), errno );
		}
	}
}

void
Resource::publish_private( ClassAd *ad )
{
		// Needed by the collector to correctly respond to queries
		// for private ads.  As of 7.2.0, the 
	if (enable_single_startd_daemon_ad) {
		SetMyTypeName( *ad, STARTD_SLOT_ADTYPE );
	} else {
		SetMyTypeName( *ad, STARTD_OLD_ADTYPE );

		// For backward compatibility with pre 7.2.0 collectors, send
		// name and IP address in private ad (needed to match up the
		// private ad with the public ad in the negotiator).
		// As of 7.2.0, the collector automatically propagates this
		// info from the public ad to the private ad.  Also as of
		// 7.2.0, the negotiator doesn't even care about STARTD_IP_ADDR.
		// It looks at MY_ADDRESS instead, which is propagated to
		// the private ad by the collector (and which is also inserted
		// by the startd before sending this ad for compatibility with
		// older collectors).

		ad->Assign(ATTR_STARTD_IP_ADDR, daemonCore->InfoCommandSinfulString() );
		ad->Assign(ATTR_NAME, r_name );

		// CRUFT: This shouldn't still be called ATTR_CAPABILITY in
		// the ClassAd, but for backwards compatibility it is.  As of
		// 7.1.3, the negotiator accepts ATTR_CLAIM_ID or
		// ATTR_CAPABILITY, so once we no longer care about
		// compatibility with negotiators older than that, we can
		// ditch ATTR_CAPABILITY and switch the following over to
		// ATTR_CLAIM_ID.  That will slightly simplify
		// claimid-specific logic elsewhere, such as the private
		// attributes in ClassAds.
		ad->AssignExpr( ATTR_CAPABILITY, ATTR_CLAIM_ID );
	}

		// Add ClaimId.  If r_pre exists, we need to advertise it's
		// ClaimId.  Otherwise, we should get the ClaimId from r_cur.
	if( r_pre_pre ) {
		ad->Assign( ATTR_CLAIM_ID, r_pre_pre->id() );
	} else if( r_pre ) {
		ad->Assign( ATTR_CLAIM_ID, r_pre->id() );
	} else if( r_cur ) {
		ad->Assign( ATTR_CLAIM_ID, r_cur->id() );
	}

    if (r_has_cp) {
        string claims;
        for (claims_t::iterator j(r_claims.begin());  j != r_claims.end();  ++j) {
            claims += " ";
            claims += (*j)->id();
        }
        ad->Assign(ATTR_CLAIM_ID_LIST, claims);
        ad->Assign(ATTR_NUM_CLAIMS, (long long)r_claims.size());
    }

	if (is_partitionable_slot()) {
		ad->AssignExpr(ATTR_CHILD_CLAIM_IDS, makeChildClaimIds().c_str());
		ad->Assign(ATTR_NUM_DYNAMIC_SLOTS, (long long)m_children.size());
	}
}

std::string
Resource::makeChildClaimIds() {
		std::string attrValue = "{";
		bool firstTime = true;

		for (std::set<Resource *,ResourceLess>::iterator i(m_children.begin());  i != m_children.end();  i++) {
			std::string buf = "";
			if (firstTime) {
				firstTime = false;
			} else {
				attrValue += ", ";
			}
			Resource *child = (*i);
			if (child->r_pre_pre) {
				attrValue += QuoteAdStringValue( child->r_pre_pre->id(), buf );
			} else if (child->r_pre) {
				attrValue += QuoteAdStringValue( child->r_pre->id(), buf );
			} else if (child->r_cur) {
				attrValue += QuoteAdStringValue( child->r_cur->id(), buf );
			}
		}
		attrValue += "}";
		return attrValue;
}


void
Resource::publish_SlotAttrs( ClassAd* cap, bool as_literal, bool only_valid_values )
{
	if( startd_slot_attrs.empty() ) {
		return;
	}
	if( ! cap ) {
		return;
	}
	if( ! r_classad ) {
		return;
	}
	if (as_literal) {
		std::string slot_attr; slot_attr.reserve(64);
		classad::Value val;
		classad::ExprList * lstval;
		classad::ClassAd * adval;

		for (const auto& attr: startd_slot_attrs) {
			if (r_classad->EvaluateAttr(attr, val) && ! val.IsErrorValue() && ( ! only_valid_values || ! val.IsUndefinedValue())) {
				slot_attr = r_id_str;
				slot_attr += "_";
				slot_attr += attr;
				if (val.IsListValue(lstval)) {
					cap->Insert(slot_attr, lstval->Copy());
				} else if (val.IsClassAdValue(adval)) {
					cap->Insert(slot_attr, adval->Copy());
				} else {
					cap->Insert(slot_attr, classad::Literal::MakeLiteral(val));
				}
			}
		}
	} else {
		std::string prefix = r_id_str;
		prefix += '_';
		for (const auto& attr: startd_slot_attrs) {
			caInsert(cap, r_classad, attr.c_str(), prefix.c_str());
		}
	}
}


void
Resource::dprintf_va( int flags, const char* fmt, va_list args ) const
{
	const DPF_IDENT ident = 0; // REMIND: maybe something useful here??
	std::string fmt_str( r_id_str );
	fmt_str += ": ";
	fmt_str += fmt;
	::_condor_dprintf_va( flags, ident, fmt_str.c_str(), args );
}


void
Resource::dprintf( int flags, const char* fmt, ... ) const
{
	va_list args;
	va_start( args, fmt );
	this->dprintf_va( flags, fmt, args );
	va_end( args );
}


// Update the Cpus and Memory usage values of the starter on the active claim
// and compute the condor load average from those numbers.
double
Resource::compute_condor_usage( void )
{
	double cpu_usage, avg, max, load;
	int numcpus = resmgr->num_real_cpus();

	time_t now = resmgr->now();
	int num_since_last = now - r_last_compute_condor_load;
	if( num_since_last < 1 ) {
		num_since_last = 1;
	}
	if( num_since_last > polling_interval ) {
		num_since_last = polling_interval;
	}

	// this will have the cpus usage value that we use for calculating the load average
	cpu_usage = 0.0;

	if (r_cur) {
		// update the Cpus and Memory usage numbers of the active claim.
		// the claim will cache the values returned here and we can fetch them again later.
		double pctCpu = 0.0;
		long long imageSize = 0;
		r_cur->updateUsage(pctCpu, imageSize);

		// we only consider the opportunistic Condor claim for
		// CondorLoadAvg, not any of the COD claims...
		if (r_cur->isActive()) {
			cpu_usage = pctCpu;
		}
	}

	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_LOAD | D_VERBOSE, "LoadQueue: Adding %d entries of value %f\n",
				 num_since_last, cpu_usage );
	}
	r_load_queue->push( num_since_last, cpu_usage );

	avg = (r_load_queue->avg() / numcpus);

	if( IsDebugVerbose( D_LOAD ) ) {
		r_load_queue->display( this );
		dprintf( D_LOAD | D_VERBOSE,
				 "LoadQueue: Size: %d  Avg value: %.2f  "
				 "Share of system load: %.2f\n",
				 r_load_queue->size(), r_load_queue->avg(), avg );
	}

	r_last_compute_condor_load = now;

	max = MAX( numcpus, resmgr->m_attr->load() );
	load = (avg * max) / 100;
		// Make sure we don't think the CondorLoad on 1 node is higher
		// than the total system load.
	return MIN( load, resmgr->m_attr->load() );
}


#ifdef USE_STARTD_LATCHES  // more generic mechanism for CpuBusy
void Resource::init_latches()
{
	latches.clear();
	reconfig_latches();
}

void Resource::reconfig_latches()
{
	// we want to preserve order here, but also uniqueness of names
	// so build an ordered list, we will ignore duplicates as we iterate
	std::vector<std::string> items;
	auto_free_ptr names(param("STARTD_LATCH_EXPRS"));
	if (names) {
		items = split(names);
	}
	names.set(::param("SYSTEM_STARTD_LATCH_EXPRS"));
	if (names) {
		for (const auto& item: StringTokenIterator(names)) {
			items.emplace_back(item);
		}
	}

	if ( ! items.size()) {
		latches.clear();
		return;
	}
	latches.reserve(items.size());

	std::set<YourStringNoCase> attrs; // for uniqueness testing
	std::map<std::string, AttrLatch, classad::CaseIgnLTStr> saved; // in case we have to rebuild

	auto it = latches.begin();
	for (const auto& attr: items) {
		if (attrs.count(attr.c_str())) continue; // ignore name repeats, we use the first one only
		attrs.insert(attr.c_str());

		bool publish_value = false;
		if (r_config_classad) {
			classad::ExprTree* expr = r_config_classad->Lookup(attr);
			if ( ! expr) {
				// TODO: figure out a better way to do this...
				if (YourStringNoCase(ATTR_NUM_DYNAMIC_SLOTS) != attr) {
					dprintf(D_FULLDEBUG, "Warning : Latch expression %s not found in config\n", attr.c_str());
				}
			} else {
				classad::Value val;
				publish_value = ! ExprTreeIsLiteral(expr, val);
			}
		}

		if (it != latches.end()) {
			// if we get to here, we are modifying an existing list, we start by
			// being optimistic that the list has not changed
			if (YourStringNoCase(attr.c_str()) == it->attr) {
				// optimism warranted, keep going.
				it->publish_last_value = publish_value;
				++it; continue;
			}
			// optimism was unfounded, save off the remaining latch values and truncate
			// we will have to rebuild from the saved latch data from here on
			for (auto & jt = it; jt != latches.end(); ++jt) { saved[attr] = *jt; }
			latches.erase(it, latches.end());
			it = latches.end();
		}

		auto found = saved.find(attr);
		if (found != saved.end()) {
			it = latches.emplace(it, found->second);
			// saved.erase(found);
		} else {
			it = latches.emplace(it, attr.c_str());
		}
		it->publish_last_value = publish_value;
		++it;
	}
	// if we aren't at the end of the array, erase the remainder.
	latches.erase(it, latches.end());
	dump_latches(D_ZKM | D_VERBOSE);
}

void Resource::evaluate_latches()
{
	if ( ! r_classad) {
		// We don't have our classad yet, so just quit
		return;
	}

	for (AttrLatch & latch : latches) {
		int64_t value = latch.last_value; 
		bool changed = false;
		if (r_classad->EvaluateAttrNumber(latch.attr, value)) {
			changed = (value != latch.last_value) || ! latch.is_defined;
			latch.is_defined = true;
			latch.last_value = value;
		} else {
			//dprintf(D_ZKM, "evaluate_latches: %s does not evaluate to a number\n", latch.attr.c_str());
			changed = latch.is_defined;
			latch.is_defined = false;
		}
		if (changed) {
			latch.change_time = resmgr->now();
		}
	}
}

void Resource::publish_latches(ClassAd * cap, time_t /*now*/)
{
	std::string buf;
	std::string attr;
	for (const AttrLatch & latch : latches) {
		attr = latch.attr; attr += "Value";
		if (latch.publish_last_value) {
			if (latch.is_defined) {
				cap->Assign(attr, latch.last_value);
			} else {
				cap->Delete(attr);
			}
		}
		attr = latch.attr; attr += "Time";
		if (latch.change_time > 0) {
			cap->Assign(attr, latch.change_time);
		} else {
			cap->Delete(attr);
		}
		//if (IsDebugCatAndVerbosity(D_ZKM)) {
		//	if (!buf.empty()) buf += "  ";
		//	latch.repl(buf);
		//}
	}

	//dprintf(D_ZKM, "published_latches: %s\n", buf.c_str());
}

void Resource::dump_latches(int dpf_level)
{
	std::string buf;
	for (const AttrLatch & latch : latches) {
		if (!buf.empty()) buf += " ";
		latch.repl(buf);
	}
	dprintf(dpf_level, "Latches: %s\n", buf.c_str());
}

#else
void
Resource::compute_cpu_busy( void )
{
	int busy = eval_cpu_busy();

	// It's busy now and it wasn't before, or idle and wasn't before so set the start time to now
	if ((busy?true:false) != (r_cpu_busy?true:false)) {
		r_cpu_busy_start_time = resmgr->now();
	}
	r_cpu_busy = busy;
}


time_t
Resource::cpu_busy_time(time_t now)
{
	time_t elapsed = 0;
	if (r_cpu_busy_start_time > 0) {
		elapsed = now - r_cpu_busy_start_time;
		if (elapsed < 0) elapsed = 0;
	}
	return elapsed;
}
#endif

void
Resource::log_ignore( int cmd, State s )
{
	dprintf( D_ALWAYS, "Got %s while in %s state, ignoring.\n",
			 getCommandString(cmd), state_to_string(s) );
}


void
Resource::log_ignore( int cmd, State s, Activity a )
{
	dprintf( D_ALWAYS, "Got %s while in %s/%s state, ignoring.\n",
			 getCommandString(cmd), state_to_string(s),
			 activity_to_string(a) );
}


void
Resource::log_shutdown_ignore( int cmd )
{
	dprintf( D_ALWAYS, "Got %s while shutting down, ignoring.\n",
			 getCommandString(cmd) );
}


void
Resource::remove_pre( void )
{
	if( r_pre ) {
		if( r_pre->requestStream() ) {
			r_pre->refuseClaimRequest();
		}
		delete r_pre;
		r_pre = NULL;
	}
	if( r_pre_pre ) {
		delete r_pre_pre;
		r_pre_pre = NULL;
	}
}


void
Resource::beginCODLoadHack( void )
{
		// set our bool, so we use the pre-COD load for policy
		// evaluations
	r_hack_load_for_cod = true;

		// if we have a value for the pre-cod-load, we want to
		// maintain it.  the only case where this would happen is if a
		// COD job had finished in the last minute, we were still
		// reporting the pre-cod-load, and a new cod job started up.
		// if that happens, we don't want to use the current load,
		// since that'll have some residual COD in it.  instead, we
		// just want to use the load from *before* any COD happened.
		// only if we've been free of COD for over a minute (and
		// therefore, we're completely out of COD-load hack), do we
		// want to record the real system load as the "pre-COD" load.
	if( r_pre_cod_total_load < 0.0 ) {
		r_pre_cod_total_load = r_attr->total_load();
		r_pre_cod_condor_load = r_attr->condor_load();
	} else {
		ASSERT( r_cod_load_hack_tid != -1 );
	}

		// if we had a timer set to turn off this hack, cancel it,
		// since we're back in hack mode...
	if( r_cod_load_hack_tid != -1 ) {
		if( daemonCore->Cancel_Timer(r_cod_load_hack_tid) < 0 ) {
			::dprintf( D_ALWAYS, "failed to cancel COD Load timer (%d): "
					   "daemonCore error\n", r_cod_load_hack_tid );
		}
		r_cod_load_hack_tid = -1;
	}
}


void
Resource::startTimerToEndCODLoadHack( void )
{
	ASSERT( r_cod_load_hack_tid == -1 );
	r_cod_load_hack_tid = daemonCore->Register_Timer( 60, 0,
					(TimerHandlercpp)&Resource::endCODLoadHack,
					"endCODLoadHack", this );
	if( r_cod_load_hack_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
}


void
Resource::endCODLoadHack( int /* timerID */ )
{
		// our timer went off, so we can clear our tid
	r_cod_load_hack_tid = -1;

		// now, reset all the COD-load hack state
	r_hack_load_for_cod = false;
	r_pre_cod_total_load = -1.0;
	r_pre_cod_condor_load = 0.0;
}


bool
Resource::acceptClaimRequest()
{
	bool accepted = false;
	switch (r_cur->type()) {
	case CLAIM_OPPORTUNISTIC:
		if (r_cur->requestStream()) {
				// We have a pending opportunistic claim, try to accept it.
			accepted = accept_request_claim(r_cur);
		}
		break;

#if HAVE_JOB_HOOKS
	case CLAIM_FETCH:
			// Enter Claimed/Idle will trigger all the actions we need.
		change_state(claimed_state);
		accepted = true;
		break;
#endif /* HAVE_JOB_HOOKS */

	case CLAIM_COD:
			// TODO?
		break;

	default:
		EXCEPT("Inside Resource::acceptClaimRequest() "
			   "with unexpected claim type: %s",
			   getClaimTypeString(r_cur->type()));
		break;
	}
	return accepted;
}

const char * Resource::analyze_match(
	std::string & buf,
	ClassAd* request_ad,
	bool slot_requirements,
	bool job_requirements)
{
	buf.reserve(1000);

	if (slot_requirements) {
		classad::References inline_attrs, target_refs;

		inline_attrs.insert(ATTR_START);
		inline_attrs.insert(ATTR_WITHIN_RESOURCE_LIMITS);

		// print analysis
		std::vector<ClassAd*> jobs; jobs.push_back(request_ad);
		anaFormattingOptions fmt = { 100,
			detail_analyze_each_sub_expr | detail_inline_std_slot_exprs | detail_smart_unparse_expr
			| detail_suppress_tall_heading | detail_append_to_buf /* | detail_show_all_subexprs */,
			"Requirements", "Slot", "Job" };
		AnalyzeRequirementsForEachTarget(r_classad, ATTR_REQUIREMENTS, inline_attrs, jobs, buf, fmt);
		chomp(buf); buf += "\n--------------------------\n";

		// print relevent slot attrs
		formatstr_cat(buf, "  Slot %s has the following attributes:\n", r_id_str);
		ExprTree * tree = r_classad->LookupExpr(ATTR_START);
		if (tree) {
			buf += "    " ATTR_START " = ";
			PrettyPrintExprTree(tree, buf, 4, 128);
		}
		if ( ! ends_with(buf, "\n")) buf += "\n";
		tree = r_classad->LookupExpr(ATTR_WITHIN_RESOURCE_LIMITS);
		if (tree) {
			buf += "    " ATTR_WITHIN_RESOURCE_LIMITS " = ";
			PrettyPrintExprTree(tree, buf, 4, 128);
		}
		if ( ! ends_with(buf, "\n")) buf += "\n";
		AddReferencedAttribsToBuffer(r_classad, ATTR_REQUIREMENTS, inline_attrs, target_refs, true, "    ", buf);

		// print targeted job attrs
		std::string temp, target_name;
		if (AddTargetAttribsToBuffer(target_refs, r_classad, request_ad, false, "    ", temp, target_name)) {
			formatstr_cat(buf, "  %s has the following attributes:\n%s", target_name.c_str(), temp.c_str());
		}
		chomp(buf);

	}

	if (job_requirements) {
		// TODO: write this one.
	}

	return buf.c_str();
}

bool
Resource::willingToRun(ClassAd* request_ad)
{
	bool slot_requirements = true, req_requirements = true;

		// First, verify that the slot and job meet each other's
		// requirements at all.
	if (request_ad) {
		r_reqexp->restore();
		if (EvalBool(ATTR_REQUIREMENTS, r_classad,
								request_ad, slot_requirements) == 0) {
				// Since we have the request ad, treat UNDEFINED as FALSE.
			slot_requirements = false;
		}

			// The following dprintfs are only done if request_ad !=
			// NULL, because this function is frequently called with
			// request_ad==NULL when the startd is evaluating its
			// state internally, and it is not unexpected for START to
			// locally evaluate to false in that case.

		// Possibly print out the ads we just got to the logs.
		if (IsDebugLevel(D_JOB)) {
			std::string adbuf;
			dprintf(D_JOB, "REQ_CLASSAD:\n%s", formatAd(adbuf, *request_ad, "\t"));
		}
		if (IsDebugLevel(D_MACHINE)) {
			std::string adbuf;
			dprintf(D_MACHINE, "MACHINE_CLASSAD:\n%s", formatAd(adbuf, *r_classad, "\t"));
		}

		// on match failure, print the job and machine ad if we did not print them already
		if (!slot_requirements || !req_requirements) {
			if ( ! IsDebugLevel(D_JOB)) { // skip if we already printed the job ad
				dprintf(D_FULLDEBUG, "Job ad was ============================\n");
				dPrintAd(D_FULLDEBUG, *request_ad);
			}
			if ( ! IsDebugLevel(D_MACHINE)) { // skip if we already printed the machine ad
				dprintf(D_FULLDEBUG, "Slot ad was ============================\n");
				dPrintAd(D_FULLDEBUG, *r_classad);
			}
			if (!req_requirements) { dprintf(D_ERROR, "Job requirements not satisfied.\n"); }
			if (!slot_requirements) {
				std::string anabuf;
				analyze_match(anabuf, request_ad, !slot_requirements, false);
				dprintf(D_ERROR, "Slot Requirements not satisfied. Analysis:\n%s\n", anabuf.c_str());
			}
		}
	}
	else {
			// All we can do is locally evaluate START.  We don't want
			// the full-blown ATTR_REQUIREMENTS since that includes
			// the valid checkpoint platform clause, which will always
			// be undefined (and irrelevant for our decision here).
		if (r_classad->LookupBool(ATTR_START, slot_requirements) == 0) {
				// Without a request classad, treat UNDEFINED as TRUE.
			slot_requirements = true;
		}
	}

	if (!slot_requirements || !req_requirements) {
			// Not willing -- no sense checking state, RANK, etc.
		return false;
	}

		// TODO: check state, RANK, etc.?
	return true;
}


#if HAVE_JOB_HOOKS

void
Resource::createOrUpdateFetchClaim(ClassAd* job_ad, double rank)
{
	if (state() == claimed_state && activity() == idle_act
		&& r_cur && r_cur->type() == CLAIM_FETCH)
	{
			// We're currently claimed with a fetch claim, and we just
			// fetched another job. Instead of generating a new Claim,
			// we just need to update r_cur with the new job ad.
		r_cur->setjobad(job_ad);
		r_cur->setrank(rank);
	}
	else {
			// We're starting a new claim for this fetched work, so
			// create a new Claim object and initialize it.
		createFetchClaim(job_ad, rank);
	}
		// Either way, maybe we should initialize the Client object, too?
		// TODO-fetch
}

void
Resource::createFetchClaim(ClassAd* job_ad, double rank)
{
	Claim* new_claim = new Claim(this, CLAIM_FETCH);
	new_claim->setjobad(job_ad);
	new_claim->setrank(rank);

	if (state() == claimed_state) {
		remove_pre();
		r_pre = new_claim;
	}
	else {
		delete r_cur;
		r_cur = new_claim;
	}
}


bool
Resource::spawnFetchedWork(void)
{
	Starter* tmp_starter = new Starter;

		// Update the claim object with info from the job classad stored in the Claim object
		// Then spawn the given starter.
		// If the starter was spawned, we no longer own the tmp_starter object
	ASSERT(r_cur->ad() != NULL);
	if ( ! r_cur->spawnStarter(tmp_starter, NULL)) {
		delete tmp_starter; tmp_starter = NULL;

		dprintf(D_ERROR, "ERROR: Failed to spawn starter for fetched work request, aborting.\n");
		change_state(owner_state);
		return false;
	}

	change_state(busy_act);
	return true;
}


void
Resource::terminateFetchedWork(void)
{
	change_state(preempting_state, vacating_act);
}


void
Resource::startedFetch(void)
{
		// Record that we just fetched.
	m_last_fetch_work_spawned = time(NULL);
	m_currently_fetching = true;

}


void
Resource::fetchCompleted(void)
{
	m_currently_fetching = false;

		// Record that we just finished fetching.
	m_last_fetch_work_completed = time(NULL);

		// Now that a fetch hook returned, (re)set our timer to try
		// fetching again based on the delay expression.
	resetFetchWorkTimer();

		// If we are a dynamically created slot, it is possible that
		// we became unclaimed while waiting for the fetch to
		// complete. Now that it has we can reevaluate our state and
		// potentially delete ourself.
	if ( get_feature() == DYNAMIC_SLOT ) {
		// WARNING: This must be the last thing done in response to a
		// hook exiting, if it isn't then there is a chance we will be
		// referenced after we are deleted.
		eval_state();
	}
}


int
Resource::evalNextFetchWorkDelay(void)
{
	static bool warned_undefined = false;
	int value = 0;
	ClassAd* job_ad = NULL;
	if (r_cur) {
		job_ad = r_cur->ad();
	}
	if (EvalInteger(ATTR_FETCH_WORK_DELAY, r_classad, job_ad, value) == 0) {
			// If undefined, default to 5 minutes (300 seconds).
		if (!warned_undefined) {
			dprintf(D_FULLDEBUG,
					"%s is UNDEFINED, using 5 minute default delay.\n",
					ATTR_FETCH_WORK_DELAY);
			warned_undefined = true;
		}
		value = 300;
	}
	m_next_fetch_work_delay = value;
	return value;
}


void
Resource::tryFetchWork( int /* timerID */ )
{
		// First, make sure we're configured for fetching at all.
	if (!getHookKeyword()) {
			// No hook keyword for ths slot, bail out.
		return;
	}

		// Then, make sure we're not currently fetching.
	if (m_currently_fetching) {
			// No need to log a message about this, it's not an error.
		return;
	}

		// Now, make sure we  haven't fetched too recently.
	evalNextFetchWorkDelay();
	if (m_next_fetch_work_delay > 0) {
		time_t now = time(NULL);
		time_t delta = now - m_last_fetch_work_completed;
		if (delta < m_next_fetch_work_delay) {
				// Throttle is defined, and the time since we last
				// fetched is shorter than the desired delay. Reset
				// our timer to go off again when we think we'd be
				// ready, and bail out.
			resetFetchWorkTimer(m_next_fetch_work_delay - delta);
			return;
		}
	}

		// Finally, ensure the START expression isn't locally FALSE.
	if (!willingToRun(NULL)) {
			// We're not currently willing to run any jobs at all, so
			// don't bother invoking the hook. However, we know the
			// fetching delay was already reached, so we should reset
			// our timer for another full delay.
		resetFetchWorkTimer();
		return;
	}

		// We're ready to invoke the hook. The timer to re-fetch will
		// be reset once the hook completes.
	resmgr->m_hook_mgr->invokeHookFetchWork(this);
	return;
}


void
Resource::resetFetchWorkTimer(int next_fetch)
{
	int next = 1;  // Default if there's no throttle set
	if (next_fetch) {
			// We already know how many seconds we want to wait until
			// the next fetch, so just use that.
		next = next_fetch;
	}
	else {
			// A fetch invocation just completed, we don't need to
			// recompute any times and deltas.  We just need to
			// reevaluate the delay expression and set a timer to go
			// off in that many seconds.
		evalNextFetchWorkDelay();
		if (m_next_fetch_work_delay > 0) {
			next = m_next_fetch_work_delay;
		}
	}

	if (m_next_fetch_work_tid == -1) {
			// Never registered.
		m_next_fetch_work_tid = daemonCore->Register_Timer(
			next, 100000, (TimerHandlercpp)&Resource::tryFetchWork,
			"Resource::tryFetchWork", this);
	}
	else {
			// Already registered, just reset it.
		daemonCore->Reset_Timer(m_next_fetch_work_tid, next, 100000);
	}
}


char*
Resource::getHookKeyword()
{
	if (!m_hook_keyword_initialized) {
		std::string param_name;
		formatstr(param_name, "%s_JOB_HOOK_KEYWORD", r_id_str);
		m_hook_keyword = param(param_name.c_str());
		if (!m_hook_keyword) {
			m_hook_keyword = param("STARTD_JOB_HOOK_KEYWORD");
		}
		m_hook_keyword_initialized = true;
	}
	return m_hook_keyword;
}

void Resource::enable()
{
    /* let the negotiator match jobs to this slot */
	r_reqexp->restore ();

}

void Resource::disable()
{

    /* kill the claim */
	kill_claim ();

	/* let the negotiator know not to match any new jobs to
    this slot */
	r_reqexp->unavail ();

}


double
Resource::compute_rank( ClassAd* req_classad ) {

	double rank;

	if( EvalFloat( ATTR_RANK, r_classad, req_classad, rank ) == 0 ) {
		ExprTree *rank_expr = r_classad->LookupExpr("RANK");
		dprintf( D_ALWAYS, "Error evaluating machine rank expression: %s\n", ExprTreeToString(rank_expr));
		dprintf( D_ALWAYS, "Setting RANK to 0.0\n");
		rank = 0.0;
	}
	return rank;
}


#endif /* HAVE_JOB_HOOKS */

// partially evaluate the Require<tag> expression so it can be used by bind_DevIds
bool Resource::fix_require_tag_expr(const ExprTree * expr, ClassAd * request_ad, std::string & require)
{
	if ( ! expr) return false;

	// skip over parens and envelope nodes, then copy the expr so we can mutate it
	expr = SkipExprParens(expr);
	ConstraintHolder constr(expr->Copy());

	// remove TARGET prefix (which would be JOB refs for CountMatches())
	// so that flattenAndInline will flatten them as job refs
	NOCASE_STRING_MAP mapping;
	mapping["TARGET"] = "";
	RewriteAttrRefs(constr.Expr(), mapping);

	ExprTree * flattened = nullptr;
	classad::Value val;
	if (request_ad->FlattenAndInline(constr.Expr(), val, flattened) && flattened) {
		constr.set(flattened);
	}
	ExprTreeToString(constr.Expr(), require);

	return true;
}

//
// Create dynamic slot from p-slot
//
Resource * create_dslot(Resource * rip, ClassAd * req_classad, bool take_parent_claim)
{
	ASSERT(rip);
	ASSERT(req_classad);
	// for legacy use case, a static slot or d-slot could be passed in and the code would expect it 
	// to come right back out the other side.
	if ( ! rip->can_create_dslot()) {
		return rip;
	}

	// for logging convenience, extract the "jobid" from the request classad.
	JOB_ID_KEY jid;
	req_classad->LookupInteger(ATTR_CLUSTER_ID, jid.cluster);
	req_classad->LookupInteger(ATTR_PROC_ID, jid.proc);
	std::string req_jobid;
	jid.sprint(req_jobid);

	std::string tmpbuf;
	dprintf(D_ZKM, "Create dSlot for %s from %s : %s\n", req_jobid.c_str(), rip->r_id_str, rip->r_attr->cat_totals(tmpbuf));

	// dummy {} to avoid unnecessary git diffs
	{
		ClassAd	*mach_classad = rip->r_classad;
		CpuAttributes *cpu_attrs = nullptr;
		CpuAttributes::_slot_request cpu_attrs_request;
		int cpus;
		long long disk, memory, swap;
		bool must_modify_request = param_boolean("MUST_MODIFY_REQUEST_EXPRS",false,false,req_classad,mach_classad);
		ClassAd *unmodified_req_classad = NULL;

			// Modify the requested resource attributes as per config file.
			// If must_modify_request is false (the default), then we only modify the request _IF_
			// the result still matches.  So is must_modify_request is false, we first backup
			// the request classad before making the modification - if after modification matching fails,
			// fall back on the original backed-up ad.
		if ( !must_modify_request ) {
				// save an unmodified backup copy of the req_classad
			unmodified_req_classad = new ClassAd( *req_classad );  
		}

			// Now make the modifications.
		static const char* resources[] = {ATTR_REQUEST_CPUS, ATTR_REQUEST_DISK, ATTR_REQUEST_MEMORY, NULL};
		for (int i=0; resources[i]; i++) {
			std::string knob("MODIFY_REQUEST_EXPR_");
			knob += resources[i];
			auto_free_ptr exprstr(rip->param(knob.c_str()));
			if (exprstr) {
				ExprTree *tree = NULL;
				classad::Value result;
				long long val = 0;
				ParseClassAdRvalExpr(exprstr, tree);
				if ( tree &&
					 EvalExprToNumber(tree,req_classad,mach_classad,result) &&
					 result.IsIntegerValue(val) )
				{
					req_classad->Assign(resources[i],val);
				}
				if (tree) delete tree;
			}
		}

			// Now make sure the partitionable slot itself is satisfied by
			// the job. If not there is no point in trying to
			// partition it. This check also prevents
			// over-partitioning. The acceptability of the dynamic
			// slot and job will be checked later, in the normal
			// course of accepting the claim.
		bool mach_requirements = true;
		do {
			rip->r_reqexp->restore();
			if (EvalBool( ATTR_REQUIREMENTS, mach_classad, req_classad, mach_requirements) == 0) {
				dprintf(D_ALWAYS,
					"STARTD Requirements expression no longer evaluates to a boolean %s MODIFY_REQUEST_EXPR_ edits\n",
					unmodified_req_classad ? "with" : "w/o"
					);
				mach_requirements = false;  // If we can't eval it as a bool, treat it as false
			}
				// If the pslot cannot support this request, ABORT iff there is not
				// an unmodified_req_classad backup copy we can try on the next iteration of
				// the while loop
			if (mach_requirements == false) {

				std::string buf;
				rip->analyze_match(buf, req_classad, !mach_requirements, false);
				dprintf(D_ALWAYS, "STARTD Requirements do not match, %s MODIFY_REQUEST_EXPR_ edits. Analysis:\n%s\n",
					unmodified_req_classad ? "with" : "w/o", buf.c_str());

				if (IsDebugVerbose(D_MATCH)) {
					dprintf(D_MATCH | D_FULLDEBUG,
						"STARTD Requirements do not match, %s MODIFY_REQUEST_EXPR_ edits. Job ad was ============================\n", 
						unmodified_req_classad ? "with" : "w/o");
					dPrintAd(D_MATCH | D_FULLDEBUG, *req_classad);
					dprintf(D_MATCH | D_FULLDEBUG, "Machine ad was ============================\n");
					dPrintAd(D_MATCH | D_FULLDEBUG, *mach_classad);
				}
				if (unmodified_req_classad) {
					// our modified req_classad no longer matches, put back the original
					// so we can try again.
					dprintf(D_ALWAYS, 
						"Job no longer matches partitionable slot after MODIFY_REQUEST_EXPR_ edits, retrying w/o edits\n");
					req_classad->CopyFrom(*unmodified_req_classad);	// put back original					
					delete unmodified_req_classad;
					unmodified_req_classad = NULL;
				} else {
					rip->dprintf(D_ALWAYS, 
					  "Partitionable slot can't be split to allocate a dynamic slot large enough for the claim\n" );
					return NULL;
				}
			}
		} while (mach_requirements == false);

			// No longer need this, make sure to free the memory.
		if (unmodified_req_classad) {
			delete unmodified_req_classad;
			unmodified_req_classad = NULL;
		}

		// Pull out the requested attribute values.  If specified, we go with whatever
		// the schedd wants, which is in request attributes prefixed with
		// "_condor_".  This enables to schedd to request something different than
		// what the end user specified, and yet still preserve the end-user's
		// original request. If the schedd does not specify, go with whatever the user
		// placed in the ad, aka the ATTR_REQUEST_* attributes itself.  If that does
		// not exist, we either cons up a default or refuse the claim.
		std::string schedd_requested_attr;

        if (cp_supports_policy(*mach_classad)) {
            // apply consumption policy
            consumption_map_t consumption;
            cp_compute_consumption(*req_classad, *mach_classad, consumption);

            for (consumption_map_t::iterator j(consumption.begin());  j != consumption.end();  ++j) {
                if (YourStringNoCase("disk") == j->first) {
                    // int denom = (int)(ceil(rip->r_attr->total_cpus()));
                    double total_disk = rip->r_attr->get_total_disk();
                    double disk_slop = max(1024, total_disk / (1024*1024));
                    cpu_attrs_request.disk_fraction = max(0, j->second + disk_slop) / total_disk;
                } else if (YourStringNoCase("swap") == j->first) {
                    cpu_attrs_request.virt_mem_fraction = j->second;
                } else if (YourStringNoCase("cpus") == j->first) {
                    cpu_attrs_request.num_cpus = MAX(1.0/128.0, j->second);
                } else if (YourStringNoCase("memory") == j->first) {
                    cpu_attrs_request.num_phys_mem = int(j->second);
                } else {
                    cpu_attrs_request.slotres[j->first] = j->second;
                }
            }
        } else {
                // Look to see how many CPUs are being requested.
            schedd_requested_attr = "_condor_";
            schedd_requested_attr += ATTR_REQUEST_CPUS;
            if( !EvalInteger( schedd_requested_attr.c_str(), req_classad, mach_classad, cpus ) ) {
                if( !EvalInteger( ATTR_REQUEST_CPUS, req_classad, mach_classad, cpus ) ) {
                    cpus = 1; // reasonable default, for sure
                }
            }
            cpu_attrs_request.num_cpus = cpus;

                // Look to see how much MEMORY is being requested.
            schedd_requested_attr = "_condor_";
            schedd_requested_attr += ATTR_REQUEST_MEMORY;
            if( !EvalInteger( schedd_requested_attr.c_str(), req_classad, mach_classad, memory ) ) {
                if( !EvalInteger( ATTR_REQUEST_MEMORY, req_classad, mach_classad, memory ) ) {
                        // some memory size must be available else we cannot
                        // match, plus a job ad without ATTR_MEMORY is sketchy
                    rip->dprintf( D_ALWAYS,
                                  "No memory request in incoming ad, aborting...\n" );
                    return NULL;
                }
            }
            cpu_attrs_request.num_phys_mem = memory;

                // Look to see how much DISK is being requested.
            schedd_requested_attr = "_condor_";
            schedd_requested_attr += ATTR_REQUEST_DISK;
            if( !EvalInteger( schedd_requested_attr.c_str(), req_classad, mach_classad, disk ) ) {
                if( !EvalInteger( ATTR_REQUEST_DISK, req_classad, mach_classad, disk ) ) {
                        // some disk size must be available else we cannot
                        // match, plus a job ad without ATTR_DISK is sketchy
                    rip->dprintf( D_FULLDEBUG,
                                  "No disk request in incoming ad, aborting...\n" );
                    return NULL;
                }
            }
            // convert disk request into a fraction for the slot splitting code
            //int denom = (int)(ceil(rip->r_attr->total_cpus()));
            double total_disk = rip->r_attr->get_total_disk();
            double disk_slop = max(1024, total_disk / (1024*1024));
            double disk_fraction = max(0, disk + disk_slop) / total_disk;

            cpu_attrs_request.disk_fraction = disk_fraction;

                // Look to see how much swap is being requested.
            schedd_requested_attr = "_condor_";
            schedd_requested_attr += ATTR_REQUEST_VIRTUAL_MEMORY;
			double total_virt_mem = resmgr->m_attr->virt_mem();
			bool set_swap = true;

            if( !EvalInteger( schedd_requested_attr.c_str(), req_classad, mach_classad, swap ) ) {
                if( !EvalInteger( ATTR_REQUEST_VIRTUAL_MEMORY, req_classad, mach_classad, swap ) ) {
						// Schedd didn't set it, user didn't request it
					if (param_boolean("PROPORTIONAL_SWAP_ASSIGNMENT", false)) {
						// set swap to same percentage of swap as we have of physical memory
						cpu_attrs_request.virt_mem_fraction = double(memory) / double(resmgr->m_attr->phys_mem());
						set_swap = false;
					} else {
						// Fall back to you get everything and don't pay anything
						set_swap = false;
					}
                }
            }

			if (set_swap) {
				cpu_attrs_request.virt_mem_fraction = swap / total_virt_mem;
			}

			for (CpuAttributes::slotres_map_t::const_iterator j(rip->r_attr->get_slotres_map().begin());  j != rip->r_attr->get_slotres_map().end();  ++j) {
				std::string reqname = ATTR_REQUEST_PREFIX + j->first;
				double reqval = 0;
				if (!EvalFloat(reqname.c_str(), req_classad, mach_classad, reqval)) reqval = 0.0;
				cpu_attrs_request.slotres[j->first] = reqval;
				if (reqval > 0.0) {
					// check for a constraint on the resource
					reqname = ATTR_REQUIRE_PREFIX + j->first;
					ExprTree * constr = req_classad->Lookup(reqname);
					if (constr) {
						rip->fix_require_tag_expr(constr, req_classad, cpu_attrs_request.slotres_constr[j->first]);
					}
				}
			}
		}

		if (IsDebugVerbose(D_FULLDEBUG)) {
			std::string resources;
			cpu_attrs_request.dump(resources);
			rip->dprintf(D_FULLDEBUG, "Job %s requesting resources: %s\n", req_jobid.c_str(), resources.c_str());
		}

		if (cpu_attrs_request.num_cpus > 0.0) { // quick and dirty check for for non-empty request
			// EXECUTE and SLOT<n>_EXECUTE will always give d-slots the same execute dir as the p-slot
			const char * executeDir = rip->r_attr->executeDir();
			const char * partitionId = rip->r_attr->executePartitionID();
			if ( ! cpu_attrs_request.allow_fractional_cpus) {
				cpu_attrs_request.num_cpus = MAX(cpu_attrs_request.num_cpus, 1.0);
				cpu_attrs_request.num_phys_mem = MAX(cpu_attrs_request.num_phys_mem, 1);
			}
			cpu_attrs = new CpuAttributes(rip->type_id(), cpu_attrs_request, executeDir, partitionId);
		} else {
			rip->dprintf( D_ALWAYS,
						  "Failed to parse attributes for request, aborting\n" );
			return NULL;
		}

		Resource * new_rip = new Resource(cpu_attrs, rip->r_id, rip, take_parent_claim);
		if( ! new_rip || new_rip->is_broken_slot()) {
			rip->dprintf( D_ALWAYS,
						  "Failed to build new resource for request, aborting\n" );
			return NULL;
		}

		// update the standard value for "now" for all of the subsequent slot attributes
		resmgr->update_cur_time();

			// Initialize the rest of the Resource
		new_rip->initial_compute(rip);
		new_rip->init_classad();
		new_rip->refresh_classad_slot_attrs(); 

			// Recompute the partitionable slot's resources
			// Call update() in case we were never matched, i.e. no state change
			// Note: update() may create a new claim if pass thru Owner state
		rip->update_needed(Resource::WhyFor::wf_dslotCreate);

		resmgr->addResource( new_rip );

			// refresh most attributes. many of these were set in initial_compute
			// but may need to be refreshed now that the slot has been added.
			// One thing this doesn't update is owner load and keyboard
			// note that compute_dynamic will refresh both the d-slot and p-slot
		resmgr->compute_and_refresh(new_rip);

		return new_rip;
	}
}

/*
Create multiple dynamic slots for a single request ad
*/
std::vector<Resource*> create_dslots(Resource* rip, ClassAd * req_classad, int num_dslots, bool take_parent_claim)
{
	std::vector<Resource*> dslots(num_dslots);

	dslots.clear();
	for (int ix = 0; ix < num_dslots; ++ix) {
		Resource * dslot = create_dslot(rip, req_classad, take_parent_claim);
		if ( ! dslot) break;
		dslots.push_back(dslot);
	}

	return dslots;
}






void
Resource::publishDynamicChildSummaries(ClassAd *cap)
{
	if (!is_partitionable_slot()) {
		// TODO:  Add rollup by slot_type
		dprintf(D_ALWAYS | D_BACKTRACE, "publishDynamicChildSummaries called for non p-slot\n");
		return;
	}

	cap->Assign(ATTR_NUM_DYNAMIC_SLOTS, (long long)m_children.size());

		// If not set, turn off the whole thing
	if (param_boolean("ADVERTISE_PSLOT_ROLLUP_INFORMATION", true) == false) {
		return;
	}

	cap->Assign(ATTR_PSLOT_ROLLUP_INFORMATION, true);

	//cap->AssignExpr(ATTR_CHILD_CLAIM_IDS, makeChildClaimIds().c_str());

	// List of attrs to rollup from dynamic ads into lists
	// in the partitionable ad

	classad::References attrs;
	attrs.insert(ATTR_NAME);
	attrs.insert(ATTR_DSLOT_ID);
	attrs.insert(ATTR_CURRENT_RANK);
	attrs.insert(ATTR_REMOTE_USER);
	attrs.insert(ATTR_REMOTE_OWNER);
	attrs.insert(ATTR_ACCOUNTING_GROUP);
	attrs.insert(ATTR_STATE);
	attrs.insert(ATTR_ACTIVITY);
	attrs.insert(ATTR_ENTERED_CURRENT_STATE);
	attrs.insert(ATTR_RETIREMENT_TIME_REMAINING);

	attrs.insert(ATTR_CPUS);
	attrs.insert(ATTR_MEMORY);
	attrs.insert(ATTR_DISK);

	MachAttributes::slotres_map_t machres_map = resmgr->m_attr->machres();
    for (auto j(machres_map.begin());  j != machres_map.end();  j++) {
        attrs.insert(j->first);
    }

	// The admin can add additional ones
	param_and_insert_attrs("STARTD_PARTITIONABLE_SLOT_ATTRS", attrs);

	std::string attrName;
	std::string attrValue;
	attrValue.reserve(100);

	for (auto it(attrs.begin()); it != attrs.end(); it++) {
		attrName = "Child" + (*it);
		attrValue.clear();
		rollupAttrs(attrValue, m_children, (*it));
		cap->AssignExpr(attrName, attrValue.c_str());
	}
}

/* static */ void
Resource::rollupAttrs(std::string &attrValue, std::set<Resource *,ResourceLess> & children, const std::string &name)
{
	classad::Value val;

	classad::ClassAdUnParser unparse;
	unparse.SetOldClassAd(true, true);

	attrValue = "{";

	bool firstTime = true;

	for (auto i(children.begin());  i != children.end();  i++) {
		if (firstTime) {
			firstTime = false;
		} else {
			attrValue += ", ";
		}
		if ((*i)->r_classad->EvaluateAttr(name, val)) {
			unparse.Unparse(attrValue,val);
		} else {
			attrValue += "undefined";
		}
	}
	attrValue += "}";

	return;
}

void
Resource::invalidateAllClaimIDs() {
	if( r_pre ) { r_pre->invalidateID(); }
	if( r_pre_pre ) { r_pre_pre->invalidateID(); }
	if( r_cur ) { r_cur->invalidateID(); }
}
