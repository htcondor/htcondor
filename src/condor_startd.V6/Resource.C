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
#include "startd.h"
#include "mds.h"


Resource::Resource( CpuAttributes* cap, int rid )
{
	char tmp[256];
	char* tmpName;
	r_classad = NULL;
	r_state = new ResState( this );
	r_cur = new Claim( this );
	r_pre = NULL;
	r_cod_mgr = new CODMgr( this );
	r_reqexp = new Reqexp( this );
	r_load_queue = new LoadQueue( 60 );

	r_id = rid;
	sprintf( tmp, "vm%d", rid );
	r_id_str = strdup( tmp );
	
	if( Name ) {
		tmpName = Name;
	} else {
		tmpName = my_full_hostname();
	}
	if( resmgr->is_smp() ) {
		sprintf( tmp, "%s@%s", r_id_str, tmpName );
		r_name = strdup( tmp );
	} else {
		r_name = strdup( tmpName );
	}

	r_attr = cap;
	r_attr->attach( this );

	update_tid = -1;

		// Set ckpt filename for avail stats here, since this object
		// knows the resource id, and we need to use a different ckpt
		// file for each resource.
	if( compute_avail_stats ) {
		char *log = param("LOG");
		if (log) {
			MyString avail_stats_ckpt_file(log);
			free(log);
			sprintf(tmp, "%c.avail_stats.%d", DIR_DELIM_CHAR, rid);
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

	delete r_state;
	delete r_classad;
	delete r_cur;		
	if( r_pre ) {
		delete r_pre;		
	}
	delete r_cod_mgr;
	delete r_reqexp;   
	delete r_attr;		
	delete r_load_queue;
	free( r_name );
	free( r_id_str );
}


int
Resource::retire_claim( void )
{
	switch( state() ) {
	case claimed_state:
		// Do not allow backing out of retirement (e.g. if a
		// preempting claim goes away) because this function is called
		// for irreversible events such as condor_vacate or PREEMPT.
		if(r_cur) {
			r_cur->disallowUnretire();
			if(resmgr->isShuttingDown() && daemonCore->GetPeacefulShutdown()) {
				// Admin is shutting things down but does not want any killing,
				// regardless of MaxJobRetirementTime configuration.
				r_cur->setRetirePeacefully(true);
			}
		}
		return change_state( retiring_act );
	case matched_state:
		return change_state( owner_state );
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
		return change_state( preempting_state, vacating_act );
	case preempting_state:
		if( activity() != killing_act ) {
			return change_state( preempting_state, vacating_act );
		}
		break;
	case matched_state:
		return change_state( owner_state );
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
		return change_state( preempting_state, killing_act );
	case matched_state:
		return change_state( owner_state );
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
	r_cur->alive();
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
	if( c->isCOD() ) {
		r_cod_mgr->removeClaim( c );
		return;
	}
	if( c == r_pre ) {
		remove_pre();
		r_pre = new Claim( this );
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


int
Resource::releaseAllClaims( void )
{
	return shutdownAllClaims( true );
}


int
Resource::killAllClaims( void )
{
	return shutdownAllClaims( false );
}


int
Resource::shutdownAllClaims( bool graceful )
{
		// shutdown the COD claims
	r_cod_mgr->shutdownAllClaims( graceful );

	if( graceful ) {
		retire_claim();
	} else {
		kill_claim();
	}
	return TRUE;
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
		did_update = change_state( suspended_act );
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
		did_update = change_state( busy_act );
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

	char float_buf[64];

	MyString load = ATTR_LOAD_AVG;
	load += '=';
	sprintf( float_buf, "%.2f", r_pre_cod_total_load );
	load += float_buf;

	MyString c_load = ATTR_CONDOR_LOAD_AVG;
	c_load += '=';
	sprintf( float_buf, "%.2f", r_pre_cod_condor_load );
	c_load += float_buf;

	if( DebugFlags & D_FULLDEBUG && DebugFlags & D_LOAD ) {
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

	MyString line = ATTR_CPU_IS_BUSY;
	line += "=False";
	r_classad->Insert( line.Value() );

	line = ATTR_CPU_BUSY_TIME;
	line += "=0";
	r_classad->Insert( line.Value() );
}


void
Resource::starterExited( Claim* cur_claim )
{
	if( ! cur_claim ) {
		EXCEPT( "Resource::starterExited() called with no Claim!" );
	}

	if( cur_claim->isCOD() ) {
 		r_cod_mgr->starterExited( cur_claim );
		return;
	}

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
Resource::newCODClaim( void )
{
	Claim* claim;
	claim = r_cod_mgr->addClaim();
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

	State dest = destination_state();
	switch( dest ) {
	case claimed_state:
			// If the machine is still available....
		if( ! eval_is_owner() ) {
			r_cur = r_pre;
			r_pre = NULL;
				// STATE TRANSITION preempting -> claimed
			accept_request_claim( this );
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
			// STATE TRANSITION preempting -> claimed
		accept_request_claim( this );
	} else {
			// STATE TRANSITION preempting -> owner
		remove_pre();
		change_state( owner_state );
	}
}


int
Resource::init_classad( void )
{
	assert( resmgr->config_classad );
	if( r_classad ) delete(r_classad);
	r_classad = new ClassAd( *resmgr->config_classad );

	// Publish everything we know about.
	this->publish( r_classad, A_PUBLIC | A_ALL | A_EVALUATED );
		// NOTE: we don't use A_SHARED_VM here, since when
		// init_classad is being called, we don't necessarily have
		// classads for the other VMs, yet we'll publish the SHARED_VM
		// attrs after this...
	
	return TRUE;
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
Resource::force_benchmark( void )
{
		// Force this resource to run benchmarking.
	resmgr->m_attr->benchmark( this, 1 );
	return TRUE;
}

int
Resource::update( void )
{
	int timeout = 3;
	int ret_value = TRUE;

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
		ret_value = FALSE;
	}
		
	return ret_value;
}

int
Resource::do_update( void )
{
	int rval;
	ClassAd private_ad;
	ClassAd public_ad;

	this->publish( &public_ad, A_ALL_PUB );
	this->publish( &private_ad, A_PRIVATE | A_ALL );

		// Send class ads to collector(s)
	rval = resmgr->send_update( UPDATE_STARTD_AD, &public_ad,
								&private_ad ); 
	if( rval ) {
		dprintf( D_FULLDEBUG, "Sent update to %d collector(s)\n", rval ); 
	} else {
		dprintf( D_ALWAYS, "Error sending update to collector(s)\n" );
	}

	// We _must_ reset update_tid to -1 before we return so
	// the class knows there is no pending update.
	update_tid = -1;

	return rval;
}


void
Resource::final_update( void ) 
{
	ClassAd invalidate_ad;
	char line[256];

		// Set the correct types
	invalidate_ad.SetMyTypeName( QUERY_ADTYPE );
	invalidate_ad.SetTargetTypeName( STARTD_ADTYPE );

		// We only want to invalidate this VM.
	sprintf( line, "%s = %s == \"%s\"", ATTR_REQUIREMENTS, ATTR_NAME, 
			 r_name );
	invalidate_ad.Insert( line );

	resmgr->send_update( INVALIDATE_STARTD_ADS, &invalidate_ad, NULL );
}


int
Resource::eval_and_update( void )
{
		// Evaluate the state of this resource.
	eval_state();

		// If we didn't update b/c of the eval_state, we need to
		// actually do the update now.
	update();

	return TRUE;
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
	if( unknown ) { 
		if( r_classad->EvalBool( "WANT_SUSPEND",
								   r_cur->ad(),
								   want_suspend ) == 0) { 
				// This should never happen, since we already check
				// when we're constructing the internal config classad
				// if we've got this defined. -Derek Wright 4/12/00
			EXCEPT( "Can't find WANT_SUSPEND in internal ClassAd" );
		}
	}
	return want_suspend;
}


int 
Resource::wants_pckpt( void )
{
	int want_pckpt; 

	if( r_cur->universe() != CONDOR_UNIVERSE_STANDARD ) {
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
	if(r_cur && r_cur->mayUnretire()) {
		if(!hasPreemptingClaim()) {
			// preempting claim has gone away
			return 1;
		}
	}
	return 0;
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

int
Resource::retirementExpired()
{
	//This function evaulates to true if the job has run longer than
	//its maximum alloted graceful retirement time.

	int MaxJobRetirementTime = 0;
	int JobMaxJobRetirementTime = 0;
	int JobAge = 0;

	if (r_cur && r_cur->isActive() && r_cur->ad()) {
		//look up the maximum retirement time specified by the startd
		if(!r_classad->LookupElem(ATTR_MAX_JOB_RETIREMENT_TIME) ||
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
		if(r_cur->ad()->LookupElem(ATTR_MAX_JOB_RETIREMENT_TIME) &&
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

	return (JobAge >= MaxJobRetirementTime);
}

int
Resource::eval_kill()
{
	int tmp;
	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( (r_classad->EvalBool( "KILL_VANILLA",
									r_cur->ad(), tmp) ) == 0 ) {  
			if( (r_classad->EvalBool( "KILL",
										r_cur->ad(),
										tmp) ) == 0 ) { 
				EXCEPT("Can't evaluate KILL");
			}
		}
	} else {
		if( (r_classad->EvalBool( "KILL",
									r_cur->ad(), 
									tmp)) == 0 ) { 
			EXCEPT("Can't evaluate KILL");
		}	
	}
	return tmp;
}


int
Resource::eval_preempt( void )
{
	int tmp;
	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( (r_classad->EvalBool( "PREEMPT_VANILLA",
								   r_cur->ad(), 
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "PREEMPT",
									   r_cur->ad(), 
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate PREEMPT");
			}
		}
	} else {
		if( (r_classad->EvalBool( "PREEMPT",
								   r_cur->ad(), 
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate PREEMPT");
		}
	}
	return tmp;
}


int
Resource::eval_suspend( void )
{
	int tmp;
	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( (r_classad->EvalBool( "SUSPEND_VANILLA",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "SUSPEND",
									   r_cur->ad(),
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate SUSPEND");
			}
		}
	} else {
		if( (r_classad->EvalBool( "SUSPEND",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate SUSPEND");
		}
	}
	return tmp;
}


int
Resource::eval_continue( void )
{
	int tmp;
	if( r_cur->universe() == CONDOR_UNIVERSE_VANILLA ) {
		if( (r_classad->EvalBool( "CONTINUE_VANILLA",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			if( (r_classad->EvalBool( "CONTINUE",
									   r_cur->ad(),
									   tmp)) == 0 ) {
				EXCEPT("Can't evaluate CONTINUE");
			}
		}
	} else {	
		if( (r_classad->EvalBool( "CONTINUE",
								   r_cur->ad(),
								   tmp)) == 0 ) {
			EXCEPT("Can't evaluate CONTINUE");
		}
	}
	return tmp;
}


int
Resource::eval_is_owner( void )
{
	int tmp;
	if( (r_classad->EvalBool( ATTR_IS_OWNER, 
							  r_cur->ad(),
							  tmp)) == 0 ) {
		EXCEPT("Can't evaluate %s", ATTR_IS_OWNER);
	}
	return tmp;
}


int
Resource::eval_start( void )
{
	int tmp;
	if( (r_classad->EvalBool( "START", 
							  r_cur->ad(),
							  tmp)) == 0 ) {
			// Undefined
		return -1;
	}
	return tmp;
}


int
Resource::eval_cpu_busy( void )
{
	int tmp = 0;
	if( ! r_classad ) {
			// We don't have our classad yet, so just return that
			// we're not busy.
		return 0;
	}
	if( (r_classad->EvalBool( ATTR_CPU_BUSY, r_cur->ad(), tmp )) == 0 ) {
			// Undefined, try "cpu_busy"
		if( (r_classad->EvalBool( "CPU_BUSY", r_cur->ad(), 
								  tmp )) == 0 ) {   
				// Totally undefined, return false;
			return 0;
		}
	}
	return tmp;
}


void
Resource::publish( ClassAd* cap, amask_t mask ) 
{
	char line[256];
	MyString my_line;
	State s;
	char* ptr;

		// Set the correct types on the ClassAd
	cap->SetMyTypeName( STARTD_ADTYPE );
	cap->SetTargetTypeName( JOB_ADTYPE );

		// Insert attributes directly in the Resource object, or not
		// handled by other objects.

	if( IS_STATIC(mask) ) {
			// We need these for both public and private ads
		sprintf( line, "%s = \"%s\"", ATTR_STARTD_IP_ADDR, 
				 daemonCore->InfoCommandSinfulString() );
		cap->Insert( line );

		sprintf( line, "%s = \"%s\"", ATTR_NAME, r_name );
		cap->Insert( line );
	}

	if( IS_PUBLIC(mask) && IS_STATIC(mask) ) {
		sprintf( line, "%s = \"%s\"", ATTR_MACHINE, my_full_hostname() );
		cap->Insert( line );

			// Since the Rank expression itself only lives in the
			// config file and the r_classad (not any obejects), we
			// have to insert it here from r_classad.  If Rank is
			// undefined in r_classad, we need to insert a default
			// value, since we don't want to use the job ClassAd's
			// Rank expression when we evaluate our Rank value.
		if( !caInsert(cap, r_classad, ATTR_RANK) ) {
			sprintf( line, "%s = 0.0", ATTR_RANK );
			cap->Insert( line );
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

			// Include everything from STARTD_EXPRS.
		config_fill_ad( cap );

			// Also, include a VM ID attribute, since it's handy for
			// defining expressions, and other things.
		
		sprintf( line, "%s = %d", ATTR_VIRTUAL_MACHINE_ID, r_id );
		cap->Insert( line );
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
				// update ImageSize attribute from procInfo (this is
				// only for the opportunistic job, not COD)
			if( r_cur && r_cur->isActive() ) {
				unsigned long imgsize = r_cur->imageSize();
				sprintf( line, "%s = %lu", ATTR_IMAGE_SIZE, imgsize );
				cap->Insert( line );
			}
		}
	}

		// Things you only need in private ads
	if( IS_PRIVATE(mask) && IS_UPDATE(mask) ) {
			// Add currently useful ClaimId.  If r_pre exists, we  
			// need to advertise it's ClaimId.  Otherwise, we
			// should get the ClaimId from r_cur.
			// CRUFT: This shouldn't still be called ATTR_CAPABILITY
			// in the ClassAd, but for backwards compatibility it is.
			// When we finally remove all the evil startd private ad
			// junk this can go away, too.
		if( r_pre ) {
			sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_pre->id() );
			cap->Insert( line );
		} else if( r_cur ) {
			sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_cur->id() );
			cap->Insert( line );
		}		
	}

		// Put in cpu-specific attributes
	r_attr->publish( cap, mask );
	
		// Put in machine-wide attributes 
	resmgr->m_attr->publish( cap, mask );

		// Put in ResMgr-specific attributes 
	resmgr->publish( cap, mask );

		// If this is a public ad, publish anything we had to evaluate
		// to "compute"
	if( IS_PUBLIC(mask) && IS_EVALUATED(mask) ) {
		sprintf( line, "%s=%d", ATTR_CPU_BUSY_TIME,
				 (int)cpu_busy_time() ); 
		cap->Insert(line); 

		sprintf( line, "%s=%s", ATTR_CPU_IS_BUSY, 
				 r_cpu_busy ? "True" : "False" );
		cap->Insert(line); 
	}

		// Put in state info
	r_state->publish( cap, mask );

		// Put in requirement expression info
	r_reqexp->publish( cap, mask );

		// Put in max job retirement time expression
	ptr = param(ATTR_MAX_JOB_RETIREMENT_TIME);
	if(!ptr || !*ptr) ptr = "0";
	my_line.sprintf("%s=%s",ATTR_MAX_JOB_RETIREMENT_TIME,ptr);
	cap->Insert(my_line.Value());

		// Update info from the current Claim object, if it exists.
	if( r_cur ) {
		r_cur->publish( cap, mask );
	}

		// Put in availability statistics
	r_avail_stats.publish( cap, mask );

	r_cod_mgr->publish( cap, mask );

	// Publish the supplemental Class Ads
	resmgr->adlist_publish( cap, mask );

	// Build the MDS/LDIF file
	char	*tmp;
	if ( ( tmp = param( "STARTD_MDS_OUTPUT" ) ) != NULL ) {
		if (  ( mask & ( A_PUBLIC | A_UPDATE ) ) == 
			  ( A_PUBLIC | A_UPDATE )  ) {

			int		mlen = 20 + strlen( tmp );
			char	*fname = (char *) malloc( mlen );
			if ( NULL == fname ) {
				dprintf( D_ALWAYS, "Failed to malloc MDS fname (%d bytes)\n",
						 mlen );
			} else {
				sprintf( fname, "%s-%s.ldif", tmp, r_id_str );
				if (  ( MdsGenerate( cap, fname ) ) < 0 ) {
					dprintf( D_ALWAYS, "Failed to generate MDS file '%s'\n",
							 fname );
				}
			}
		}
	}

	if( IS_PUBLIC(mask) && IS_SHARED_VM(mask) ) {
		resmgr->publishVmAttrs( cap );
	}
}


void
Resource::publishVmAttrs( ClassAd* cap )
{
	if( ! startd_vm_exprs ) {
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
	startd_vm_exprs->rewind();
	while( (ptr = startd_vm_exprs->next()) ) {
		caInsert( cap, r_classad, ptr, prefix.Value() );
	}
}


void
Resource::refreshVmAttrs( void )
{
	resmgr->publishVmAttrs( r_classad );
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
Resource::dprintf_va( int flags, char* fmt, va_list args )
{
	if( resmgr->is_smp() ) {
		MyString fmt_str( r_id_str );
		fmt_str += ": ";
		fmt_str += fmt;
		::_condor_dprintf_va( flags, (char*)fmt_str.Value(), args );
	} else {
		::_condor_dprintf_va( flags, fmt, args );
	}
}


void
Resource::dprintf( int flags, char* fmt, ... )
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
	int numcpus = resmgr->num_cpus();

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

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
		dprintf( D_FULLDEBUG, "LoadQueue: Adding %d entries of value %f\n", 
				 num_since_last, cpu_usage );
	}
	r_load_queue->push( num_since_last, cpu_usage );

	avg = (r_load_queue->avg() / numcpus);

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
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
	if( ! r_pre_cod_total_load ) {
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

