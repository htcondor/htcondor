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
#include "condor_environ.h"
#include "classad_merge.h"
#include "vm_common.h"
#include "VMRegister.h"
#include "file_sql.h"
#include "condor_holdcodes.h"
#include "startd_bench_job.h"

#include "slot_builder.h"

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
#include "StartdPlugin.h"
#endif
#endif

#ifndef max
#define max(x,y) (((x) < (y)) ? (y) : (x))
#endif

extern FILESQL *FILEObj;

Resource::Resource( CpuAttributes* cap, int rid, bool multiple_slots, Resource* _parent )
{
	MyString tmp;
	const char* tmpName;

		// we need this before we instantiate any Claim objects...
	r_id = rid;
	char* name_prefix = param( "STARTD_RESOURCE_PREFIX" );
	if( name_prefix ) {
		tmp = name_prefix;
		free( name_prefix );
	} else {
		tmp = "slot";
	}
	if( _parent ) {
		r_sub_id = _parent->m_id_dispenser->next();
		tmp.formatstr_cat( "%d_%d", r_id, r_sub_id );
	} else {
		tmp.formatstr_cat( "%d", r_id );
	}
	r_id_str = strdup( tmp.Value() );

		// we need this before we can call type()...
	r_attr = cap;
	r_attr->attach( this );

	m_id_dispenser = NULL;

		// we need this before we instantiate the Reqexp object...
	tmp.formatstr( "SLOT_TYPE_%d_PARTITIONABLE", type() );
	if( param_boolean( tmp.Value(), false ) ) {
		set_feature( PARTITIONABLE_SLOT );

		m_id_dispenser = new IdDispenser( 3, 1 );
	} else {
		set_feature( STANDARD_SLOT );
	}

		// This must happen before creating the Reqexp
	set_parent( _parent );

	prevLHF = 0;
	r_classad = NULL;
	r_state = new ResState( this );
	r_cur = new Claim( this );
	r_pre = NULL;
	r_pre_pre = NULL;
	r_cod_mgr = new CODMgr( this );
	r_reqexp = new Reqexp( this );
	r_load_queue = new LoadQueue( 60 );

	if( Name ) {
		tmpName = Name;
	} else {
		tmpName = my_full_hostname();
	}
	if( multiple_slots || get_feature() == PARTITIONABLE_SLOT ) {
		tmp.formatstr( "%s@%s", r_id_str, tmpName );
		r_name = strdup( tmp.Value() );
	} else {
		r_name = strdup( tmpName );
	}

	update_tid = -1;

		// Set ckpt filename for avail stats here, since this object
		// knows the resource id, and we need to use a different ckpt
		// file for each resource.
	if( compute_avail_stats ) {
		char *log = param("LOG");
		if (log) {
			MyString avail_stats_ckpt_file(log);
			free(log);
			tmp.formatstr( "%c.avail_stats.%d", DIR_DELIM_CHAR, rid);
			avail_stats_ckpt_file += tmp;
			r_avail_stats.checkpoint_filename(avail_stats_ckpt_file);
		}
	}

	r_cpu_busy = 0;
	r_cpu_busy_start_time = 0;
	r_last_compute_condor_load = resmgr->now();
	r_suspended_for_cod = false;
	r_hack_load_for_cod = false;
	r_cod_load_hack_tid = -1;
	r_pre_cod_total_load = 0.0;
	r_pre_cod_condor_load = 0.0;
	m_bUserSuspended = false;

#if HAVE_JOB_HOOKS
	m_last_fetch_work_spawned = 0;
	m_last_fetch_work_completed = 0;
	m_next_fetch_work_delay = -1;
	m_next_fetch_work_tid = -1;
	m_currently_fetching = false;
	m_hook_keyword = NULL;
	m_hook_keyword_initialized = false;
#endif

	if( r_attr->type() ) {
		dprintf( D_ALWAYS, "New machine resource of type %d allocated\n",
				 r_attr->type() );
	} else {
		dprintf( D_ALWAYS, "New machine resource allocated\n" );
	}
}


Resource::~Resource()
{
	if ( update_tid != -1 ) {
		if( daemonCore->Cancel_Timer(update_tid) < 0 ) {
			::dprintf( D_ALWAYS, "failed to cancel update timer (%d): "
					   "daemonCore error\n", update_tid );
		}
		update_tid = -1;
	}

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

		// Note on "&& !m_currently_fetching": A DYNAMIC slot will
		// defer its destruction while it is waiting on a fetch work
		// hook. The only time when a slot with a parent will be
		// destroyed while waiting on a hook is during
		// shutdown. During shutdown there is no need to give
		// resources back to the parent slot, and doing so may
		// actually be dangerous if our parent was deleted first.

		// If we have a parent, return our resources to it
	if( m_parent && !m_currently_fetching ) {
		*(m_parent->r_attr) += *(r_attr);
		m_parent->m_id_dispenser->insert( r_sub_id );
		m_parent->update();
		m_parent = NULL;
	}

	if( m_id_dispenser ) {
		delete m_id_dispenser;
		m_id_dispenser = NULL;
	}

	delete r_state; r_state = NULL;
	delete r_classad; r_classad = NULL;
	delete r_cur; r_cur = NULL;
	if( r_pre ) {
		delete r_pre; r_pre = NULL;
	}
	if( r_pre_pre ) {
		delete r_pre_pre; r_pre_pre = NULL;
	}
	delete r_cod_mgr; r_cod_mgr = NULL;
	delete r_reqexp; r_reqexp = NULL;
	delete r_attr; r_attr = NULL;
	delete r_load_queue; r_load_queue = NULL;
	free( r_name ); r_name = NULL;
	free( r_id_str ); r_id_str = NULL;
}


void
Resource::set_parent( Resource* rip )
{
	m_parent = rip;

		// If we have a parent, we consume its resources
	if( m_parent ) {
		*(m_parent->r_attr) -= *(r_attr);

			// If we have a parent, we are dynamic
		set_feature( DYNAMIC_SLOT );
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
	r_cur->publish( r_classad, A_PUBLIC );

	return TRUE;
}

int Resource::suspend_claim()
{

	switch( state() ) {
	case claimed_state:
		change_state( suspended_act );
		m_bUserSuspended = true;
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
	if ( suspended_act == r_state->activity() && m_bUserSuspended )
	{
		if (r_cur->resumeClaim())
		{
			change_state( busy_act );
			m_bUserSuspended = false;
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
		return (int)r_cur->starterKill( SIGHUP );
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
Resource::setBadputCausedByDraining()
{
	if( r_cur ) {
		r_cur->setBadputCausedByDraining();
	}
}

void
Resource::releaseAllClaims( void )
{
	shutdownAllClaims( true );
}

void
Resource::releaseAllClaimsReversibly( void )
{
	shutdownAllClaims( true, true );
}

void
Resource::killAllClaims( void )
{
	shutdownAllClaims( false );
}

void
Resource::shutdownAllClaims( bool graceful, bool reversible )
{
		// shutdown the COD claims
	r_cod_mgr->shutdownAllClaims( graceful );

	if( Resource::DYNAMIC_SLOT == get_feature() ) {
		if( graceful ) {
			void_retire_claim(reversible);
		} else {
			void_kill_claim();
		}

		// We have deleted ourself and can't send any updates.
	} else {
		if( graceful ) {
			void_retire_claim(reversible);
		} else {
			void_kill_claim();
		}

			// Tell the negotiator not to match any new jobs to this slot,
			// since they would just be rejected by the startd anyway.
		r_reqexp->unavail();
		update();
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
bool
Resource::hasOppClaim( void )
{
	State s = state();
	if( s == claimed_state || s == preempting_state ) {
		return true;
	}
	return false;
}


// This one checks if the Resource has *any* claims
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
		update();
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
		update();
	}
}


void
Resource::hackLoadForCOD( void )
{
	if( ! r_hack_load_for_cod ) {
		return;
	}

	MyString load;
	load.formatstr( "%s=%.2f", ATTR_LOAD_AVG, r_pre_cod_total_load );

	MyString c_load;
	c_load.formatstr( "%s=%.2f", ATTR_CONDOR_LOAD_AVG, r_pre_cod_condor_load );

	if( IsDebugVerbose( D_LOAD ) ) {
		if( r_cod_mgr->isRunning() ) {
			dprintf( D_LOAD, "COD job current running, using "
					 "'%s', '%s' for internal policy evaluation\n",
					 load.Value(), c_load.Value() );
		} else {
			dprintf( D_LOAD, "COD job recently ran, using '%s', '%s' "
					 "for internal policy evaluation\n",
					 load.Value(), c_load.Value() );
		}
	}
	r_classad->Insert( load.Value() );
	r_classad->Insert( c_load.Value() );

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
		r_cur->client()->setuser( r_cur->client()->owner() );
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
	update();
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
	int tmp;

	r_cur->vacate();	// Send a vacate to the client of the claim
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
			acceptClaimRequest();
			return;
		}
			// Else, fall through, no break.
		set_destination_state( owner_state );
		dest = owner_state;	// So change_state() below will be correct.
	case owner_state:
	case delete_state:
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
		if( (r_classad->EvalBool("START", r_pre->ad(), tmp))
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
		acceptClaimRequest();
	} else {
			// STATE TRANSITION preempting -> owner
		remove_pre();
		change_state( owner_state );
	}
}


void
Resource::init_classad( void )
{
	ASSERT( resmgr->config_classad );
	if( r_classad ) delete(r_classad);
	r_classad = new ClassAd( *resmgr->config_classad );

	// Publish everything we know about.
	this->publish( r_classad, A_PUBLIC | A_ALL | A_EVALUATED );
		// NOTE: we don't use A_SHARED_SLOT here, since when
		// init_classad is being called, we don't necessarily have
		// classads for the other slots, yet we'll publish the SHARED_SLOT
		// attrs after this...
}


void
Resource::refresh_classad( amask_t mask )
{
	if( ! r_classad ) {
			// Nothing to do (except prevent a segfault *grin*)
		return;
	}

	this->publish( r_classad, (A_PUBLIC | mask) );
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
	time_t last_benchmark = time(NULL);
	r_classad->Assign( ATTR_LAST_BENCHMARK, (unsigned)last_benchmark );
	return 0;
}

void
Resource::reconfig( void )
{
#if HAVE_JOB_HOOKS
	if (m_hook_keyword) {
		free(m_hook_keyword);
		m_hook_keyword = NULL;
	}
	m_hook_keyword_initialized = false;
#endif /* HAVE_JOB_HOOKS */
}


void
Resource::update( void )
{
	int timeout = 3;

	if ( update_tid == -1 ) {
			// Send no more than 16 ClassAds per second to help
			// minimize the odds of overwhelming the collector
			// on very large SMP machines.  So, we mod our resource num
			// by 8 and add that to the timeout
			// (why 8? since each update sends 2 ads).
		if ( r_id > 0 ) {
			timeout += r_id % 8;
		}

		// set a timer for the update
		update_tid = daemonCore->Register_Timer(
						timeout,
						(TimerHandlercpp)&Resource::do_update,
						"do_update",
						this );
	}

	if ( update_tid < 0 ) {
		// Somehow, the timer could not be set.  Ick!
		update_tid = -1;
	}
}

void
Resource::do_update( void )
{
	int rval;
	ClassAd private_ad;
	ClassAd public_ad;

        // Get the public and private ads
    publish_for_update( &public_ad, &private_ad );

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	StartdPluginManager::Update(&public_ad, &private_ad);
#endif
#endif

		// Send class ads to collector(s)
	rval = resmgr->send_update( UPDATE_STARTD_AD, &public_ad,
								&private_ad, true );
	if( rval ) {
		dprintf( D_FULLDEBUG, "Sent update to %d collector(s)\n", rval );
	} else {
		dprintf( D_ALWAYS, "Error sending update to collector(s)\n" );
	}

	// We _must_ reset update_tid to -1 before we return so
	// the class knows there is no pending update.
	update_tid = -1;
}

void
Resource::publish_for_update ( ClassAd *public_ad ,ClassAd *private_ad )
{
    this->publish( public_ad, A_ALL_PUB );
    if( vmapi_is_usable_for_condor() == FALSE ) {
        public_ad->InsertOrUpdate( "Start = False" );
    }

    if( vmapi_is_virtual_machine() == TRUE ) {
        ClassAd* host_classad;
        host_classad = vmapi_get_host_classAd();
        MergeClassAds( public_ad, host_classad, true);
    }

    this->publish_private( private_ad );

    // log classad into sql log so that it can be updated to DB
    if (FILEObj) {
        FILESQL::daemonAdInsert(public_ad, "Machines", FILEObj, prevLHF);
	}
}


void
Resource::final_update( void )
{
	ClassAd invalidate_ad;
	MyString line;
	MyString escaped_name;

		// Set the correct types
	invalidate_ad.SetMyTypeName( QUERY_ADTYPE );
	invalidate_ad.SetTargetTypeName( STARTD_ADTYPE );

	/*
	 * NOTE: the collector depends on the data below for performance reasons
	 * if you change here you will need to CollectorEngine::remove (AdTypes t_AddType, const ClassAd & c_query)
	 * the IP was added to allow the collector to create a hash key to delete in O(1).
     */
	 ClassAd::EscapeStringValue( r_name, escaped_name );
     line.formatstr( "( TARGET.%s == \"%s\" )", ATTR_NAME, escaped_name.Value() );
     invalidate_ad.AssignExpr( ATTR_REQUIREMENTS, line.Value() );
     invalidate_ad.Assign( ATTR_NAME, r_name );
     invalidate_ad.Assign( ATTR_MY_ADDRESS, daemonCore->publicNetworkIpAddr());

#if defined(WANT_CONTRIB) && defined(WITH_MANAGEMENT)
#if defined(HAVE_DLOPEN) || defined(WIN32)
	StartdPluginManager::Invalidate(&invalidate_ad);
#endif
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

    char     *address = collector.addr ();
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

    /* get the public and private ads */
    publish_for_update( &public_ad, &private_ad );

    if ( !public_ad.put ( *socket ) ) {

        dprintf (
            D_FULLDEBUG,
            "update_with_ack: "
            "Failed to send public ad to collector host %s.\n",
            address );

        return FALSE;

    }

    if ( !private_ad.put ( *socket ) ) {

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
	MyString hold_reason;
	int hold_subcode = 0;

	r_classad->EvalString("WANT_HOLD_REASON",r_cur->ad(),hold_reason);
	if( hold_reason.IsEmpty() ) {
		ExprTree *want_hold_expr;
		MyString want_hold_str;

		want_hold_expr = r_classad->LookupExpr("WANT_HOLD");
		if( want_hold_expr ) {
			want_hold_str.formatstr( "%s = %s", "WANT_HOLD",
								   ExprTreeToString( want_hold_expr ) );
		}

		hold_reason = "The startd put this job on hold.  (At preemption time, WANT_HOLD evaluated to true: ";
		hold_reason += want_hold_str;
		hold_reason += ")";
	}

	r_classad->EvalInteger("WANT_HOLD_SUBCODE",r_cur->ad(),hold_subcode);

	r_cur->starterHoldJob(hold_reason.Value(),CONDOR_HOLD_CODE_StartdHeldJob,hold_subcode,soft);
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
	int want_vacate = 0;
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
		if( r_classad->EvalBool("WANT_VACATE_VANILLA",
								r_cur->ad(),
								want_vacate ) ) {
			dprintf( D_ALWAYS, "State change: WANT_VACATE_VANILLA is %s\n",
					 want_vacate ? "TRUE" : "FALSE" );
			unknown = false;
		}
	}
	if( r_cur->universe() == CONDOR_UNIVERSE_VM ) {
		if( r_classad->EvalBool("WANT_VACATE_VM",
								r_cur->ad(),
								want_vacate ) ) {
			dprintf( D_ALWAYS, "State change: WANT_VACATE_VM is %s\n",
					 want_vacate ? "TRUE" : "FALSE" );
			unknown = false;
		}
	}
	if( unknown ) {
		if( r_classad->EvalBool( "WANT_VACATE",
								 r_cur->ad(),
								 want_vacate ) == 0) {

			dprintf( D_ALWAYS,
					 "In Resource::wants_vacate() with undefined WANT_VACATE\n" );
			dprintf( D_ALWAYS, "INTERNAL AD:\n" );
			r_classad->dPrint( D_ALWAYS );
			if( r_cur->ad() ) {
				dprintf( D_ALWAYS, "JOB AD:\n" );
				(r_cur->ad())->dPrint( D_ALWAYS );
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
	return want_vacate;
}


int
Resource::wants_suspend( void )
{
	int want_suspend;
	bool unknown = true;
	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( r_classad->EvalBool("WANT_SUSPEND_VANILLA",
								r_cur->ad(),
								want_suspend) ) {
			unknown = false;
		}
	}
	if( r_cur->universe() == CONDOR_UNIVERSE_VM ) {
		if( r_classad->EvalBool("WANT_SUSPEND_VM",
								r_cur->ad(),
								want_suspend) ) {
			unknown = false;
		}
	}
	if( unknown ) {
		if( r_classad->EvalBool( "WANT_SUSPEND",
								   r_cur->ad(),
								   want_suspend ) == 0) {
				// UNDEFINED means FALSE for WANT_SUSPEND
			want_suspend = false;
		}
	}
	return want_suspend;
}


int
Resource::wants_pckpt( void )
{
	int want_pckpt; 

	if( (r_cur->universe() != CONDOR_UNIVERSE_STANDARD) &&
			(r_cur->universe() != CONDOR_UNIVERSE_VM)) {
		return FALSE;
	}

	if( r_classad->EvalBool( "PERIODIC_CHECKPOINT",
				r_cur->ad(),
				want_pckpt ) == 0) { 
		// Default to no, if not defined.
		want_pckpt = 0;
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
		if(!r_classad->LookupExpr("CLAIM_WORKLIFE") ||
		   !r_classad->EvalInteger(
				  "CLAIM_WORKLIFE",
		          NULL,
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
		   !r_classad->EvalInteger(
		          ATTR_MAX_JOB_RETIREMENT_TIME,
		          r_cur->ad(),
		          MaxJobRetirementTime)) {
			MaxJobRetirementTime = 0;
		}
		if(r_cur->getRetirePeacefully()) {
			// Override startd's MaxJobRetirementTime setting.
			// Make it infinite.
			MaxJobRetirementTime = INT_MAX;
		}
		//look up the maximum retirement time specified by the job
		if(r_cur->ad()->LookupExpr(ATTR_MAX_JOB_RETIREMENT_TIME) &&
		   r_cur->ad()->EvalInteger(
		          ATTR_MAX_JOB_RETIREMENT_TIME,
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
		//There is no job running, so there is no point in waiting any longer
		MaxJobRetirementTime = 0;
	}

	return MaxJobRetirementTime - JobAge;
}

bool
Resource::retirementExpired()
{
	//This function evaulates to true if the job has run longer than
	//its maximum alloted graceful retirement time.

	int retirement_remaining;

		// if we are draining, coordinate the eviction of all the
		// slots to try to reduce idle time
	if( isDraining() ) {
		retirement_remaining = resmgr->gracefulDrainingTimeRemaining(this);
	}
	else {
		retirement_remaining = evalRetirementRemaining();
	}

	if ( retirement_remaining <= 0 ) {
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
			if( !r_cur->ad()->EvalInteger(
					ATTR_JOB_MAX_VACATE_TIME,
					r_classad,
					JobMaxVacateTime) )
			{
				JobMaxVacateTime = 0;
			}
		}
		else if( r_cur->ad()->LookupExpr(ATTR_KILL_SIG_TIMEOUT) ) {
				// the old way of doing things prior to JobMaxVacateTime
			if( !r_cur->ad()->EvalInteger(
					ATTR_KILL_SIG_TIMEOUT,
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
		MyString tmp_expr_name = expr_name;
		tmp_expr_name += "_VANILLA";
		tmp = eval_expr( tmp_expr_name.Value(), false, false );
		if( tmp >= 0 ) {
				// found it, just return the value;
			return tmp;
		}
			// otherwise, fall through and try the non-vanilla version
	}
	if( check_vanilla && r_cur && r_cur->universe() == CONDOR_UNIVERSE_VM ) {
		MyString tmp_expr_name = expr_name;
		tmp_expr_name += "_VM";
		tmp = eval_expr( tmp_expr_name.Value(), false, false );
		if( tmp >= 0 ) {
				// found it, just return the value;
			return tmp;
		}
			// otherwise, fall through and try the non-vm version
	}
	if( (r_classad->EvalBool(expr_name, r_cur ? r_cur->ad() : NULL , tmp) ) == 0 ) {
		if( fatal ) {
			dprintf(D_ALWAYS, "Can't evaluate %s in the context of following ads\n", expr_name );
			r_classad->dPrint(D_ALWAYS);
			dprintf(D_ALWAYS, "=============================\n");
			if ( r_cur && r_cur->ad() ) {
				r_cur->ad()->dPrint(D_ALWAYS);
			} else {
				dprintf( D_ALWAYS, "<no job ad>\n" );
			}
			EXCEPT( "Can't evaluate %s", expr_name );
		} else {
				// anything else for here?
			return -1;
		}
	}
		// EvalBool returned success, we can just return the value
	return tmp;
}


#if HAVE_HIBERNATION

bool
Resource::evaluateHibernate( MyString &state_str ) const
{
	ClassAd *ad = NULL;
	if ( NULL != r_cur ) {
		ad = r_cur->ad();
	}

	if ( r_classad->EvalString( "HIBERNATE", ad, state_str ) ) {
		return true;
	}
	return false;
}

#endif /* HAVE_HIBERNATION */


int
Resource::eval_kill()
{
		// fatal if undefined, check vanilla
	return eval_expr( "KILL", true, true );
}


int
Resource::eval_preempt( void )
{
		// fatal if undefined, check vanilla
	return eval_expr( "PREEMPT", true, true );
}


int
Resource::eval_suspend( void )
{
		// fatal if undefined, check vanilla
	return eval_expr( "SUSPEND", true, true );
}


int
Resource::eval_continue( void )
{
	return (m_bUserSuspended)?false:eval_expr( "CONTINUE", true, true );
}


int
Resource::eval_is_owner( void )
{
	if( vmapi_is_usable_for_condor() == FALSE )
		return 1;

	// fatal if undefined, don't check vanilla
	return eval_expr( ATTR_IS_OWNER, true, false );
}


int
Resource::eval_start( void )
{
	if( vmapi_is_usable_for_condor() == FALSE )
		return 0;

	// -1 if undefined, don't check vanilla
	return eval_expr( "START", false, false );
}


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


void
Resource::publish( ClassAd* cap, amask_t mask )
{
	State s;
	char* ptr;

		// Set the correct types on the ClassAd
	cap->SetMyTypeName( STARTD_ADTYPE );
	cap->SetTargetTypeName( JOB_ADTYPE );

		// Insert attributes directly in the Resource object, or not
		// handled by other objects.

	if( IS_STATIC(mask) ) {
			// We need these for both public and private ads
		cap->Assign( ATTR_STARTD_IP_ADDR,
				 daemonCore->InfoCommandSinfulString() );

		cap->Assign( ATTR_NAME, r_name );
	}

	if( IS_PUBLIC(mask) && IS_STATIC(mask) ) {
			// Since the Rank expression itself only lives in the
			// config file and the r_classad (not any obejects), we
			// have to insert it here from r_classad.  If Rank is
			// undefined in r_classad, we need to insert a default
			// value, since we don't want to use the job ClassAd's
			// Rank expression when we evaluate our Rank value.
		if( !caInsert(cap, r_classad, ATTR_RANK) ) {
			cap->Assign( ATTR_RANK, 0.0 );
		}

			// Similarly, the CpuBusy expression only lives in the
			// config file and in the r_classad.  So, we have to
			// insert it here, too.  This is just the expression that
			// defines what "CpuBusy" means, not the current value of
			// it and how long it's been true.  Those aren't static,
			// and need to be re-published after they're evaluated.
		if( !caInsert(cap, r_classad, ATTR_CPU_BUSY) ) {
			EXCEPT( "%s not in internal resource classad, but default "
					"should be added by ResMgr!", ATTR_CPU_BUSY );
		}

		caInsert(cap, r_classad, ATTR_SLOT_WEIGHT);

#if HAVE_HIBERNATION
		caInsert(cap, r_classad, ATTR_UNHIBERNATE);
#endif

			// Include everything from STARTD_EXPRS.
			// And then include everything from SLOTx_STARTD_EXPRS
		daemonCore->publish(cap);

		// config_fill_ad can not take strings with "." in it's prefix
		// e.g. slot1.1, instead needs to be slot1
		MyString szTmp(r_id_str);
		int iPeriodPos = szTmp.find(".");

		if ( iPeriodPos >=0 ) {
			szTmp.setChar ( iPeriodPos,  '\0' );
		}
		
		config_fill_ad( cap, szTmp.Value() );

			// Also, include a slot ID attribute, since it's handy for
			// defining expressions, and other things.
		cap->Assign(ATTR_SLOT_ID, r_id);
		if (param_boolean("ALLOW_VM_CRUFT", false)) {
			cap->Assign(ATTR_VIRTUAL_MACHINE_ID, r_id);
		}

        // include any attributes set via local resource inventory
        cap->Update(r_attr->get_mach_attr()->machattr());

        // advertise the slot type id number, as in SLOT_TYPE_<N>
        cap->Assign(ATTR_SLOT_TYPE_ID, int(r_attr->type()));

		switch (get_feature()) {
		case PARTITIONABLE_SLOT:
			cap->AssignExpr(ATTR_SLOT_PARTITIONABLE, "TRUE");
            cap->Assign(ATTR_SLOT_TYPE, "Partitionable");
			break;
		case DYNAMIC_SLOT:
			cap->AssignExpr(ATTR_SLOT_DYNAMIC, "TRUE");
            cap->Assign(ATTR_SLOT_TYPE, "Dynamic");
			break;
		default:
            cap->Assign(ATTR_SLOT_TYPE, "Static");
			break; // Do nothing
		}
	}		

	if( IS_PUBLIC(mask) && IS_UPDATE(mask) ) {
			// If we're claimed or preempting, handle anything listed
			// in STARTD_JOB_EXPRS.
			// Our current claim object might be gone though, so make
			// sure we have the object before we try to use it.
			// Also, our current claim object might not have a
			// ClassAd, so be careful about that, too.
		s = this->state();
		if( s == claimed_state || s == preempting_state ) {
			if( startd_job_exprs && r_cur && r_cur->ad() ) {
				startd_job_exprs->rewind();
				while( (ptr = startd_job_exprs->next()) ) {
					caInsert( cap, r_cur->ad(), ptr );
				}
			}
		}
	}

		// Put in cpu-specific attributes
	r_attr->publish( cap, mask );

		// Put in machine-wide attributes
	resmgr->m_attr->publish( cap, mask );

		// Put in ResMgr-specific attributes
	resmgr->publish( cap, mask );

	resmgr->publish_draining_attrs( this, cap, mask );

		// If this is a public ad, publish anything we had to evaluate
		// to "compute"
	if( IS_PUBLIC(mask) && IS_EVALUATED(mask) ) {
		cap->Assign( ATTR_CPU_BUSY_TIME, (int)cpu_busy_time() );

		cap->Assign( ATTR_CPU_IS_BUSY, r_cpu_busy ? true : false );

        publishDeathTime( cap );
	}

		// Put in state info
	r_state->publish( cap, mask );

		// Put in requirement expression info
	r_reqexp->publish( cap, mask );

		// Put in max job retirement time expression
	ptr = param(ATTR_MAX_JOB_RETIREMENT_TIME);
	if( ptr && !*ptr ) {
		free(ptr);
		ptr = NULL;
	}
	cap->AssignExpr( ATTR_MAX_JOB_RETIREMENT_TIME, ptr ? ptr : "0" );

	free(ptr);

	    // Is this the local universe startd?
    cap->Assign(ATTR_IS_LOCAL_STARTD, param_boolean("IS_LOCAL_STARTD", false));

		// Put in max vacate time expression
	ptr = param(ATTR_MACHINE_MAX_VACATE_TIME);
	if( ptr && !*ptr ) {
		free(ptr);
		ptr = NULL;
	}
	cap->AssignExpr( ATTR_MACHINE_MAX_VACATE_TIME, ptr ? ptr : "0" );

	free(ptr);

#if HAVE_JOB_HOOKS
	if (IS_PUBLIC(mask)) {
		cap->Assign( ATTR_LAST_FETCH_WORK_SPAWNED, 
					 (int)m_last_fetch_work_spawned );
		cap->Assign( ATTR_LAST_FETCH_WORK_COMPLETED,
					 (int)m_last_fetch_work_completed );
		cap->Assign( ATTR_NEXT_FETCH_WORK_DELAY,
					 m_next_fetch_work_delay );
	}
#endif /* HAVE_JOB_HOOKS */

		// Update info from the current Claim object, if it exists.
	if( r_cur ) {
		r_cur->publish( cap, mask );
        if (state() == claimed_state)  cap->Assign(ATTR_PUBLIC_CLAIM_ID, r_cur->publicClaimId());
	}
	if( r_pre ) {
		r_pre->publishPreemptingClaim( cap, mask );
	}

		// Put in availability statistics
	r_avail_stats.publish( cap, mask );

	r_cod_mgr->publish( cap, mask );

	// Publish the supplemental Class Ads
	resmgr->adlist_publish( r_id, cap, mask );

    // Publish the monitoring information
    daemonCore->dc_stats.Publish(*cap);
    daemonCore->monitor_data.ExportData( cap );

	if( IS_PUBLIC(mask) && IS_SHARED_SLOT(mask) ) {
		resmgr->publishSlotAttrs( cap );
	}

#if !defined(WANT_OLD_CLASSADS)
	cap->AddTargetRefs( TargetJobAttrs, false );
#endif
}

void
Resource::publish_private( ClassAd *ad )
{
		// Needed by the collector to correctly respond to queries
		// for private ads.  As of 7.2.0, the 
	ad->SetMyTypeName( STARTD_ADTYPE );

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

		// Add ClaimId.  If r_pre exists, we need to advertise it's
		// ClaimId.  Otherwise, we should get the ClaimId from r_cur.
		// CRUFT: This shouldn't still be called ATTR_CAPABILITY in
		// the ClassAd, but for backwards compatibility it is.  As of
		// 7.1.3, the negotiator accepts ATTR_CLAIM_ID or
		// ATTR_CAPABILITY, so once we no longer care about
		// compatibility with negotiators older than that, we can
		// ditch ATTR_CAPABILITY and switch the following over to
		// ATTR_CLAIM_ID.  That will slightly simplify
		// claimid-specific logic elsewhere, such as the private
		// attributes in ClassAds.
	if( r_pre_pre ) {
		ad->Assign( ATTR_CAPABILITY, r_pre_pre->id() );
	}
	else if( r_pre ) {
		ad->Assign( ATTR_CAPABILITY, r_pre->id() );
	} else if( r_cur ) {
		ad->Assign( ATTR_CAPABILITY, r_cur->id() );
	}		
}

void
Resource::publishDeathTime( ClassAd* cap )
{
    const char *death_time_env_name;
    char       *death_time_string;
    bool        have_death_time;
    int         death_time;
    int         relative_death_time;
    MyString    classad_attribute;

	if( ! cap ) {
		return;
	}

    have_death_time     = false;
    death_time_env_name = EnvGetName(ENV_DAEMON_DEATHTIME);
    death_time_string   = getenv(death_time_env_name);

    // Lookup the death time that we have.
    if ( death_time_string ) {
        death_time = atoi( death_time_string );
        if ( death_time != 0 ) {
            have_death_time = true;
        }
    }

    if ( !have_death_time ) {
        // If we don't have a death time, we'll leave forever.
        // Well, we'll live until Unix time runs out.
        relative_death_time = INT_MAX;
    } else {
        // We're publishing how much time we have to live.
        // If we don't have any time left, then we should have died
        // already, but something is wrong. That's okay, we'll tell people
        // not to expect anything, by telling them 0.
        time_t current_time;

        current_time = time(NULL);
        if (current_time > death_time) {
            relative_death_time = 0;
        } else {
            relative_death_time = death_time - current_time;
        }
    }

    classad_attribute.formatstr( "%s=%d", ATTR_TIME_TO_LIVE, relative_death_time );
    cap->Insert( classad_attribute.Value() );
    return;
}

void
Resource::publishSlotAttrs( ClassAd* cap )
{
	if( ! startd_slot_attrs ) {
		return;
	}
	if( ! cap ) {
		return;
	}
	if( ! r_classad ) {
		return;
	}
	char* ptr;
	MyString prefix = r_id_str;
	prefix += '_';
	startd_slot_attrs->rewind();
	while( (ptr = startd_slot_attrs->next()) ) {
		caInsert( cap, r_classad, ptr, prefix.Value() );
	}
}


void
Resource::refreshSlotAttrs( void )
{
	resmgr->publishSlotAttrs( r_classad );
}


void
Resource::compute( amask_t mask )
{
	if( IS_EVALUATED(mask) ) {
			// We need to evaluate some classad expressions to
			// "compute" their values.  We don't want to propagate
			// this mask to any other objects, since this bit only
			// applies to the Resource class

			// If we don't have a classad, we can bail now, since none
			// of this is going to work.
		if( ! r_classad ) {
			return;
		}

			// Evaluate the CpuBusy expression and compute CpuBusyTime
			// and CpuIsBusy.
		compute_cpu_busy();

		return;
	}

		// Only resource-specific things that need to be computed are
		// in the CpuAttributes object.
	r_attr->compute( mask );

		// Actually, we'll have the Reqexp object compute too, so that
		// we get static stuff recomputed on reconfig, etc.
	r_reqexp->compute( mask );

		// Compute availability statistics
	r_avail_stats.compute( mask );

}


void
Resource::dprintf_va( int flags, const char* fmt, va_list args )
{
	if( resmgr->is_smp() ) {
		MyString fmt_str( r_id_str );
		fmt_str += ": ";
		fmt_str += fmt;
		::_condor_dprintf_va( flags, fmt_str.Value(), args );
	} else {
		::_condor_dprintf_va( flags, fmt, args );
	}
}


void
Resource::dprintf( int flags, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	this->dprintf_va( flags, fmt, args );
	va_end( args );
}


float
Resource::compute_condor_load( void )
{
	float cpu_usage, avg, max, load;
	int numcpus = resmgr->num_real_cpus();

	time_t now = resmgr->now();
	int num_since_last = now - r_last_compute_condor_load;
	if( num_since_last < 1 ) {
		num_since_last = 1;
	}
	if( num_since_last > polling_interval ) {
		num_since_last = polling_interval;
	}

		// we only consider the opportunistic Condor claim for
		// CondorLoadAvg, not any of the COD claims...
	if( r_cur && r_cur->isActive() ) {
		cpu_usage = r_cur->percentCpuUsage();
	} else {
		cpu_usage = 0.0;
	}

	if( IsDebugVerbose( D_LOAD ) ) {
		dprintf( D_FULLDEBUG, "LoadQueue: Adding %d entries of value %f\n",
				 num_since_last, cpu_usage );
	}
	r_load_queue->push( num_since_last, cpu_usage );

	avg = (r_load_queue->avg() / numcpus);

	if( IsDebugVerbose( D_LOAD ) ) {
		r_load_queue->display( this );
		dprintf( D_FULLDEBUG,
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


void
Resource::compute_cpu_busy( void )
{
	int old_cpu_busy;
	old_cpu_busy = r_cpu_busy;
	r_cpu_busy = eval_cpu_busy();

	if( ! old_cpu_busy && r_cpu_busy ) {
			// It's busy now and it wasn't before, so set the
			// start time to now
		r_cpu_busy_start_time = resmgr->now();
	}
	if( old_cpu_busy && ! r_cpu_busy ) {
			// It was busy before, but isn't now, so clear the
			// start time
		r_cpu_busy_start_time = 0;
	}
}


time_t
Resource::cpu_busy_time( void )
{
	time_t now;
	int val;

	if( r_cpu_busy ) {
		now = time(NULL);
		val = now - r_cpu_busy_start_time;
		if( val < 0 ) {
			dprintf( D_ALWAYS, "ERROR in CpuAttributes::cpu_busy_time() "
					 "- negative cpu busy time!, returning 0\n" );
			return 0;
		}
		return val;
	}
	return 0;
}


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
	if( r_pre_cod_total_load > 0.0 ) {
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
Resource::endCODLoadHack( void )
{
		// our timer went off, so we can clear our tid
	r_cod_load_hack_tid = -1;

		// now, reset all the COD-load hack state
	r_hack_load_for_cod = false;
	r_pre_cod_total_load = 0.0;
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
			accepted = accept_request_claim(this);
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


bool
Resource::willingToRun(ClassAd* request_ad)
{
	int slot_requirements = 1, req_requirements = 1;

		// First, verify that the slot and job meet each other's
		// requirements at all.
	if (request_ad) {
		r_reqexp->restore();
		if (r_classad->EvalBool(ATTR_REQUIREMENTS,
								request_ad, slot_requirements) == 0) {
				// Since we have the request ad, treat UNDEFINED as FALSE.
			slot_requirements = 0;
		}

			// Since we have a request ad, we can also check its requirements.
		Starter* tmp_starter;
		bool no_starter = false;
		tmp_starter = resmgr->starter_mgr.findStarter(request_ad, r_classad, no_starter );
		if (!tmp_starter) {
			req_requirements = 0;
		}
		else {
			delete(tmp_starter);
			req_requirements = 1;
		}

			// The following dprintfs are only done if request_ad !=
			// NULL, because this function is frequently called with
			// request_ad==NULL when the startd is evaluating its
			// state internally, and it is not unexpected for START to
			// locally evaluate to false in that case.

		if (!slot_requirements || !req_requirements) {
			if (!slot_requirements) {
				dprintf(D_FAILURE|D_ALWAYS, "Slot requirements not satisfied.\n");
			}
			if (no_starter) {
				dprintf(D_FAILURE|D_ALWAYS, "No starter found to run this job!  Is something wrong with your Condor installation?\n");
			}
			else if (!req_requirements) {
				dprintf(D_FAILURE|D_ALWAYS, "Job requirements not satisfied.\n");
			}
		}

			// Possibly print out the ads we just got to the logs.
		if (IsDebugLevel(D_JOB)) {
			dprintf(D_JOB, "REQ_CLASSAD:\n");
			request_ad->dPrint(D_JOB);
		}
		if (IsDebugLevel(D_MACHINE)) {
			dprintf(D_MACHINE, "MACHINE_CLASSAD:\n");
			r_classad->dPrint(D_MACHINE);
		}
	}
	else {
			// All we can do is locally evaluate START.  We don't want
			// the full-blown ATTR_REQUIREMENTS since that includes
			// the valid checkpoint platform clause, which will always
			// be undefined (and irrelevant for our decision here).
		if (r_classad->EvalBool(ATTR_START, NULL, slot_requirements) == 0) {
				// Without a request classad, treat UNDEFINED as TRUE.
			slot_requirements = 1;
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
Resource::createOrUpdateFetchClaim(ClassAd* job_ad, float rank)
{
	if (state() == claimed_state && activity() == idle_act
		&& r_cur && r_cur->type() == CLAIM_FETCH)
	{
			// We're currently claimed with a fetch claim, and we just
			// fetched another job. Instead of generating a new Claim,
			// we just need to update r_cur with the new job ad.
		r_cur->setad(job_ad);
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
Resource::createFetchClaim(ClassAd* job_ad, float rank)
{
	Claim* new_claim = new Claim(this, CLAIM_FETCH);
	new_claim->setad(job_ad);
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
        // First, we have to find a Starter that will work.
    Starter* tmp_starter;
	bool no_starter = false;
    tmp_starter = resmgr->starter_mgr.findStarter(r_cur->ad(), r_classad, no_starter);
	if( ! tmp_starter ) {
		dprintf(D_ALWAYS|D_FAILURE, "ERROR: Could not find a starter that can run fetched work request, aborting.\n");
		change_state(owner_state);
		return false;
	}

		// Update the claim object with info from this job ClassAd now
		// that we're actually activating it. By not passing any
		// argument here, we tell saveJobInfo() to keep the copy of
		// the ClassAd it already has instead of clobbering it.
	r_cur->saveJobInfo();

	r_cur->setStarter(tmp_starter);

	if (!r_cur->spawnStarter()) {
		dprintf(D_ALWAYS|D_FAILURE, "ERROR: Failed to spawn starter for fetched work request, aborting.\n");
		change_state(owner_state);
			// spawnStarter() deletes the Claim's starter object on
			// failure, so there's no worry about leaking tmp_starter here.
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
	if (r_classad->EvalInteger(ATTR_FETCH_WORK_DELAY, job_ad, value) == 0) {
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
Resource::tryFetchWork(void)
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
		MyString param_name;
		param_name.formatstr("%s_JOB_HOOK_KEYWORD", r_id_str);
		m_hook_keyword = param(param_name.Value());
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
	void_kill_claim ();

	/* let the negotiator know not to match any new jobs to
    this slot */
	r_reqexp->unavail ();

}


float
Resource::compute_rank( ClassAd* req_classad ) {

	float rank;

	if( r_classad->EvalFloat( ATTR_RANK, req_classad, rank ) == 0 ) {
		ExprTree *rank_expr = r_classad->LookupExpr("RANK");
		dprintf( D_ALWAYS, "Error evaluating machine rank expression: %s\n", ExprTreeToString(rank_expr));
		dprintf( D_ALWAYS, "Setting RANK to 0.0\n");
		rank = 0.0;
	}
	return rank;
}


#endif /* HAVE_JOB_HOOKS */


Resource * initialize_resource(Resource * rip, ClassAd * req_classad, Claim* &leftover_claim)
{
	ASSERT(rip);
	ASSERT(req_classad);
	if( Resource::PARTITIONABLE_SLOT == rip->get_feature() ) {
		ClassAd	*mach_classad = rip->r_classad;
		CpuAttributes *cpu_attrs;
		MyString type;
		StringList type_list;
		int cpus, memory, disk;
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
		const char* resources[] = {ATTR_REQUEST_CPUS, ATTR_REQUEST_DISK, ATTR_REQUEST_MEMORY, NULL};
		for (int i=0; resources[i]; i++) {
			MyString knob("MODIFY_REQUEST_EXPR_");
			knob += resources[i];
			char *tmp = param(knob.Value());
			if( tmp ) {
				ExprTree *tree = NULL;
				classad::Value result;
				int val;
				ParseClassAdRvalExpr(tmp, tree);
				if ( tree &&
					 EvalExprTree(tree,req_classad,mach_classad,result) &&
					 result.IsIntegerValue(val) )
				{
					req_classad->Assign(resources[i],val);

				}
				if (tree) delete tree;
				free(tmp);
			}
		}

			// Now make sure the partitionable slot itself is satisfied by
			// the job. If not there is no point in trying to
			// partition it. This check also prevents
			// over-partitioning. The acceptability of the dynamic
			// slot and job will be checked later, in the normal
			// course of accepting the claim.
		int mach_requirements = 1;
		do {
			rip->r_reqexp->restore();
			if (mach_classad->EvalBool( ATTR_REQUIREMENTS, req_classad, mach_requirements) == 0) {
				mach_requirements = 0;  // If we can't eval it as a bool, treat it as false
			}
				// If the pslot cannot support this request, ABORT iff there is not
				// an unmodified_req_classad backup copy we can try on the next iteration of
				// the while loop
			if (mach_requirements == 0) {
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
		} while (mach_requirements == 0);

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
		MyString schedd_requested_attr;

			// Look to see how many CPUs are being requested.
		schedd_requested_attr = "_condor_";
		schedd_requested_attr += ATTR_REQUEST_CPUS;
		if( !req_classad->EvalInteger( schedd_requested_attr.Value(), mach_classad, cpus ) ) {
			if( !req_classad->EvalInteger( ATTR_REQUEST_CPUS, mach_classad, cpus ) ) {
				cpus = 1; // reasonable default, for sure
			}
		}
		type.formatstr_cat( "cpus=%d ", cpus );

			// Look to see how much MEMORY is being requested.
		schedd_requested_attr = "_condor_";
		schedd_requested_attr += ATTR_REQUEST_MEMORY;
		if( !req_classad->EvalInteger( schedd_requested_attr.Value(), mach_classad, memory ) ) {
			if( !req_classad->EvalInteger( ATTR_REQUEST_MEMORY, mach_classad, memory ) ) {
					// some memory size must be available else we cannot
					// match, plus a job ad without ATTR_MEMORY is sketchy
				rip->dprintf( D_ALWAYS,
						  "No memory request in incoming ad, aborting...\n" );
				return NULL;
			}
		}
		type.formatstr_cat( "memory=%d ", memory );


			// Look to see how much DISK is being requested.
		schedd_requested_attr = "_condor_";
		schedd_requested_attr += ATTR_REQUEST_DISK;
		if( !req_classad->EvalInteger( schedd_requested_attr.Value(), mach_classad, disk ) ) {
			if( !req_classad->EvalInteger( ATTR_REQUEST_DISK, mach_classad, disk ) ) {
					// some disk size must be available else we cannot
					// match, plus a job ad without ATTR_DISK is sketchy
				rip->dprintf( D_FULLDEBUG,
						  "No disk request in incoming ad, aborting...\n" );
				return NULL;
			}
		}
		type.formatstr_cat( "disk=%d%%",
			max((int) ceil((disk / (double) rip->r_attr->get_total_disk()) * 100), 1) );


        for (CpuAttributes::slotres_map_t::const_iterator j(rip->r_attr->get_slotres_map().begin());  j != rip->r_attr->get_slotres_map().end();  ++j) {
            string reqname;
            formatstr(reqname, "%s%s", ATTR_REQUEST_PREFIX, j->first.c_str());
            int reqval = 0;
            if (!req_classad->EvalInteger(reqname.c_str(), mach_classad, reqval)) reqval = 0;
            string attr;
            formatstr(attr, " %s=%d", j->first.c_str(), reqval);
            type += attr;
        }

		rip->dprintf( D_FULLDEBUG,
					  "Match requesting resources: %s\n", type.Value() );

		type_list.initializeFromString( type.Value() );
		cpu_attrs = ::buildSlot( resmgr->m_attr, rip->r_id, &type_list, -rip->type(), false );
		if( ! cpu_attrs ) {
			rip->dprintf( D_ALWAYS,
						  "Failed to parse attributes for request, aborting\n" );
			return NULL;
		}

		Resource * new_rip = new Resource( cpu_attrs, rip->r_id, true, rip );
		if( ! new_rip ) {
			rip->dprintf( D_ALWAYS,
						  "Failed to build new resource for request, aborting\n" );
			return NULL;
		}

			// Initialize the rest of the Resource
		new_rip->compute( A_ALL );
		new_rip->compute( A_TIMEOUT | A_UPDATE ); // Compute disk space
		new_rip->init_classad();
		new_rip->refresh_classad( A_EVALUATED ); 
		new_rip->refresh_classad( A_SHARED_SLOT ); 

			// The new resource needs the claim from its
			// parititionable parent
		delete new_rip->r_cur;
		new_rip->r_cur = rip->r_cur;
		new_rip->r_cur->setResource( new_rip );

			// And the partitionable parent needs a new claim
		rip->r_cur = new Claim( rip );

			// Recompute the partitionable slot's resources
		rip->change_state( unclaimed_state );
			// Call update() in case we were never matched, i.e. no state change
			// Note: update() may create a new claim if pass thru Owner state
		rip->update();

		resmgr->addResource( new_rip );

			// XXX: This is overkill, but the best way, right now, to
			// get many of the new_rip's attributes calculated.
		resmgr->compute( A_ALL );
		resmgr->compute( A_TIMEOUT | A_UPDATE );

			// Stash pslot claim as the "leftover_claim", which
			// we will send back directly to the schedd iff it supports
			// receiving partitionable slot leftover info as part of the
			// new-style extended claiming protocol. 
		bool scheddWantsLeftovers = false;
			// presence of this attr in request ad tells us in a 
			// backwards/forwards compatible way if the schedd understands
			// the claim protocol enhancement to accept leftovers
		req_classad->LookupBool("_condor_SEND_LEFTOVERS",scheddWantsLeftovers);
		if ( scheddWantsLeftovers && 
			 param_boolean("CLAIM_PARTITIONABLE_LEFTOVERS",true) ) 
		{
			leftover_claim = rip->r_cur;
			ASSERT(leftover_claim);
		}

		return new_rip;
	} else {
		// Basic slot.
		return rip;
	}
}
