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

ResState::ResState( Resource* res_ip )
{
	r_state = owner_state;
	r_destination = no_state;
	r_act = idle_act;
	r_act_was_benchmark = false;
	m_atime = resmgr->now();
	m_stime = m_atime;
	this->rip = res_ip;
	m_time_owner_idle = 0;
	m_time_unclaimed_idle = 0;
	m_time_unclaimed_benchmarking = 0;
	m_time_matched_idle = 0;
	m_time_backfill_idle = 0;
	m_time_backfill_busy = 0;
	m_time_backfill_killing = 0;
	m_time_claimed_idle = 0;
	m_time_claimed_busy = 0;
	m_time_claimed_suspended = 0;
	m_time_claimed_retiring = 0;
	m_time_preempting_vacating = 0;
	m_time_preempting_killing = 0;
	m_time_drained_retiring = 0;
	m_time_drained_idle = 0;
	m_time_draining_unclaimed = 0;
	m_num_cpus_avg = 0;
	m_draining_avg = 0;
	m_activity_avg_last_timestamp = 0;
	m_activity_avg_time_sum = 0;
}


void
ResState::publish( ClassAd* cp ) 
{
	cp->Assign( ATTR_STATE, state_to_string(r_state) );

	cp->Assign( ATTR_ENTERED_CURRENT_STATE, m_stime );

	cp->Assign( ATTR_ACTIVITY, activity_to_string(r_act) );

	cp->Assign( ATTR_ENTERED_CURRENT_ACTIVITY, m_atime );

		// Conditionally publish any attributes about time spent in
		// each of the following state/activity combinations.
	publishHistoryInfo(cp, owner_state, idle_act);
	publishHistoryInfo(cp, unclaimed_state, idle_act);
	publishHistoryInfo(cp, unclaimed_state, benchmarking_act);
	publishHistoryInfo(cp, matched_state, idle_act);
	publishHistoryInfo(cp, claimed_state, idle_act);
	publishHistoryInfo(cp, claimed_state, busy_act);
	publishHistoryInfo(cp, claimed_state, suspended_act);
	publishHistoryInfo(cp, claimed_state, retiring_act);
	publishHistoryInfo(cp, preempting_state, vacating_act);
	publishHistoryInfo(cp, preempting_state, killing_act);
	publishHistoryInfo(cp, backfill_state, idle_act);
	publishHistoryInfo(cp, backfill_state, busy_act);
	publishHistoryInfo(cp, backfill_state, killing_act);
	publishHistoryInfo(cp, drained_state, idle_act);
	publishHistoryInfo(cp, drained_state, retiring_act);
}

// private function to determine what to set the activity to when the state changes to preempting
Activity ResState::_preempting_activity()
{
	// wants_vacate dprintf's about what it decides and the
	// implications on state changes...
	if ( ! rip->preemptWasTrue() || rip->wants_vacate())
		return vacating_act;
	return killing_act;
}

void
ResState::change( State new_state, Activity new_act )
{
	bool statechange = false, actchange = false;

	if( new_state != r_state ) {
		statechange = true;
	}
	if( new_act != r_act ) {
		actchange = true;
	}
	if( ! (actchange || statechange) ) {
		return;   // If we're not changing anything, return
	}

		// leave_action and enter_action return TRUE if they result in
		// a state or activity change.  In these cases, we want to
		// abort the current state change.
	if( leave_action( r_state, r_act, new_state, new_act, statechange ) ) {
		return;
	}

	if( statechange && !actchange ) {
		dprintf( D_ALWAYS, "Changing state: %s -> %s\n",
				 state_to_string(r_state), 
				 state_to_string(new_state) );
	} else if (actchange && !statechange ) {
		dprintf( D_ALWAYS, "Changing activity: %s -> %s\n",
				 activity_to_string(r_act), 
				 activity_to_string(new_act) );
	} else {
		dprintf( D_ALWAYS, 
				 "Changing state and activity: %s/%s -> %s/%s\n", 
				 state_to_string(r_state), 
				 activity_to_string(r_act), 
				 state_to_string(new_state),
				 activity_to_string(new_act) );
	}

	// we should *try* and use a consistent value for current_time when updating a bunch of slots
	// but we don't want to use a time that's *too* late, so use the resmgr's current time if its
	// not too far in the past.
 	time_t now = resmgr->now();
	time_t actual_now = time(nullptr);
	if ((actual_now - now) > 1) {
		now = actual_now; 
		//dprintf(D_ERROR | D_BACKTRACE, "Warning : ResState::change() time lag %d (%d - %d)\n", actual_now - now, actual_now, now);
	}

		// Record the time we spent in the previous state
	updateHistoryTotals(now);

	if( statechange ) {
		m_stime = now;
			// Also reset activity time
		m_atime = now;
		r_state = new_state;
		if( r_state == r_destination ) {
				// We've reached our destination, so we can reset it.
			r_destination = no_state;
		}
	}
	if( actchange ) {
		r_act_was_benchmark = ( r_act == benchmarking_act );
		r_act = new_act;
		m_atime = now;
	}

	if( enter_action( r_state, r_act, statechange, actchange ) ) {
		return;
	}

		// update our current state and activity in the classad
		// and possibly trigger a collector update
	rip->state_or_activity_changed(now, statechange, actchange);

#if HAVE_BACKFILL
		/*
		  in the case of Backfill/Idle, we do *not* want to do the
		  following check for idleness or retirement, we just want to
		  let the usual polling interval cover our next eval().  so,
		  if we're in Backfill, we can immediately return now...
		*/
	if( r_state == backfill_state ) {
		return;
	}
#endif /* HAVE_BACKFILL */

	if( r_act == retiring_act || r_act == idle_act ) {
		// When we enter retirement or idleness, check right away to
		// see if we should be preempting instead.
		this->eval_policy();
	}

	return;
}

int
ResState::eval_policy( void )
{
	int want_suspend;
#if HAVE_BACKFILL
	int kill_rval; 
#endif /* HAVE_BACKFILL */

	updateActivityAverages();

	switch( r_state ) {

	case claimed_state:
		if( r_act == suspended_act && rip->isSuspendedForCOD() ) { 
				// this is the special case where we do *NOT* want to
				// evaluate any policy expressions.  so long as
				// there's an active COD job, we want to leave the
				// opportunistic claim "checkpointed to swap"
			return 0;
		}
		if( rip->inRetirement() ) { // have we been preempted?
			if( rip->retirementExpired() ) {
					// Normally, when we are in retirement, we will
					// also be in the "retiring" activity.  However,
					// it is also possible to be in the suspended
					// activity.  Just to simplify things, we have one
					// catch-all state transition here.  We may also
					// get here in some other activity (e.g. idle or
					// busy) if we just got preempted and haven't had
					// time to transition into some other state.  No
					// matter.  Whatever activity we were in, the
					// retirement time has expired, so it is time to
					// change to the preempting state.
				if( rip->isDraining() ) {
					rip->setBadputCausedByDraining();
				} else {
					rip->setBadputCausedByPreemption();
				}

				// Before entering the preempting state, check to see if we
				// would have preempted during the preceding polling interval.
				// This allows startd policies to put jobs on hold during
				// draining.
				if( 1 == rip->eval_preempt() ) {
					rip->preemptIsTrue();
				}

				dprintf( D_ALWAYS, "State change: claim retirement ended/expired\n" );
				// TLM: STATE TRANSITION #17
				// STATE TRANSITION #18
				change( preempting_state );
				return TRUE; // XXX: change TRUE
			}
		}
		want_suspend = rip->wants_suspend();
		if( (r_act==busy_act && !want_suspend) ||
			(r_act==retiring_act && !rip->preemptWasTrue() && !want_suspend) ||
			(r_act==suspended_act && !rip->preemptWasTrue()) ) {

			//Explanation for the above conditions:
			//The want_suspend check is there because behavior is
			//potentially confusing without it.  Derek says:)
			//The preemptWasTrue check is there to see if we already
			//had PREEMPT turn TRUE, in which case, we don't need
			//to keep trying to retire over and over.

			if( 1 == rip->eval_preempt() ) {
				dprintf( D_ALWAYS, "State change: PREEMPT is TRUE\n" );
				// irreversible retirement
				// TLM: STATE TRANSITION #12
				// TLM: STATE TRANSITION #16
				rip->preemptIsTrue();
				rip->setBadputCausedByPreemption();
				return rip->retire_claim(false, "PREEMPT expression evaluated to True", CONDOR_HOLD_CODE::StartdPreemptExpression, 0);
			}
		}
		if( r_act == retiring_act ) {
			if( rip->mayUnretire() ) {
				dprintf( D_ALWAYS, "State change: unretiring because no preempting claim exists\n" );
				// TLM: STATE TRANSITION #19
				change( busy_act );
				return TRUE; // XXX: change TRUE
			}
			if( rip->retirementExpired() ) {
				dprintf( D_ALWAYS, "State change: retirement ended/expired\n" );
				rip->setBadputCausedByPreemption();
				// TLM: STATE TRANSITION #18
				change( preempting_state );
				return TRUE; // XXX: change TRUE
			}
		}
		if( (r_act == busy_act || r_act == retiring_act) && want_suspend ) {
			if( 1 == rip->eval_suspend() ) {
				// TLM: STATE TRANSITION #14
				// TLM: STATE TRANSITION #20
				dprintf( D_ALWAYS, "State change: SUSPEND is TRUE\n" );
				change( suspended_act );
				return TRUE; // XXX: change TRUE
			}
		}
		if( r_act == suspended_act ) {
			if( 1 == rip->eval_continue() ) {
				dprintf( D_ALWAYS, "State change: CONTINUE is TRUE\n" );
				if( !rip->inRetirement() ) {
					// STATE TRANSITION #15
					change( busy_act );
					return TRUE; // XXX: change TRUE
				}
				else {
					// STATE TRANSITION #16
					if (rip->hasPreemptingClaim()) {
						if (rip->r_pre->rank() > rip->r_cur->rank()) {
							rip->setVacateReason("Preempted for a higher Ranked job", CONDOR_HOLD_CODE::StartdPreemptingClaimRank, 0);
						} else {
							rip->setVacateReason("Preempted for a Priority user", CONDOR_HOLD_CODE::StartdPreemptingClaimUserPrio, 0);
						}
					}
					change( retiring_act );
					return TRUE; // XXX: change TRUE
				}
			}
		}
		if( (r_act == busy_act) && rip->hasPreemptingClaim() ) {
			dprintf( D_ALWAYS, "State change: retiring due to preempting claim\n" );
			// reversible retirement (e.g. if preempting claim goes away)
			// TLM: STATE TRANSITION #13
			if (rip->r_pre->rank() > rip->r_cur->rank()) {
				rip->setVacateReason("Preempted for a higher Ranked job", CONDOR_HOLD_CODE::StartdPreemptingClaimRank, 0);
			} else {
				rip->setVacateReason("Preempted for a Priority user", CONDOR_HOLD_CODE::StartdPreemptingClaimUserPrio, 0);
			}
			change( retiring_act );
			return TRUE; // XXX: change TRUE
		}
		if( (r_act == idle_act) && rip->hasPreemptingClaim() ) {
			dprintf( D_ALWAYS, "State change: preempting idle claim\n" );
			// TLM: STATE TRANSITION #10
			change( preempting_state );
			return TRUE; // XXX: change TRUE
		}
		if( (r_act == idle_act) && (rip->eval_start() == 0) ) {
				// START evaluates to False, so return to the owner
				// state.  In this case, we don't need to worry about
				// START locally evaluating to FALSE due to undefined
				// job attributes and well-placed meta-operators, b/c
				// we're in the claimed state, so we'll have a job ad
				// to evaluate against.
			dprintf( D_ALWAYS, "State change: START is false\n" );
			// TLM: STATE TRANSITION #10
			change( preempting_state );
			return TRUE; // XXX: change TRUE
		}
		if( (r_act == idle_act) && rip->claimWorklifeExpired() ) {
			dprintf( D_ALWAYS, "State change: idle claim shutting down due to CLAIM_WORKLIFE\n" );
			// TLM: STATE TRANSITION #10
			change( preempting_state );
			return TRUE; // XXX: change TRUE
		}
		if( (r_act == idle_act) && rip->isDraining() &&
				! rip->r_cur->waitingForActivation() ) {
			dprintf( D_ALWAYS, "State change: idle claim shutting down due to draining of this slot\n" );
			// TLM: STATE TRANSITION #10
			change( preempting_state );
			return TRUE;
		}
		if( (r_act == busy_act || r_act == retiring_act) && (rip->wants_pckpt()) ) {
			rip->periodic_checkpoint();
		}

#if HAVE_JOB_HOOKS
			// If we're compiled to support fetching work
			// automatically and configured to do so, check now if we
			// should try to fetch more work.
		if (r_act != suspended_act) {
			rip->tryFetchWork();
		}
#endif /* HAVE_JOB_HOOKS */

		if( rip->r_reqexp->restore() ) {
				// Our reqexp changed states, send an update
			rip->update_needed(Resource::WhyFor::wf_stateChange);
		}
		break;   // case claimed_state:


	case preempting_state:
		if( r_act == vacating_act ) {
			if( 1 == rip->eval_kill() ) {
				dprintf( D_ALWAYS, "State change: KILL is TRUE\n" );
				// TLM: STATE TRANSITION #21
				rip->setBadputCausedByPreemption();
				change( killing_act );
				return TRUE; // XXX: change TRUE
			}
		}
		break;	// case preempting_state:


	case unclaimed_state:
		if (rip->is_dynamic_slot() || rip->is_broken_slot()) {
#if HAVE_JOB_HOOKS
				// If we're currently fetching we can't delete
				// ourselves. If we do when the hook returns we won't
				// be around to handle the response.
			if( rip->isCurrentlyFetching() ) {
				dprintf(D_ALWAYS, "State change: Unclaimed -> Deleted delayed for outstanding work fetch\n");
				break;
			}
#endif
			// TLM: Undocumented, hopefully on purpose.
			change( delete_state );
			return TRUE; // XXX: change TRUE
		}

		if( rip->isDraining() ) {
			dprintf( D_ALWAYS, "State change: entering Drained state\n" );
			// TLM: STATE TRANSITION #37
			// TLM: Also unnumbered transition from Unclaimed/Benchmarking.
			change( drained_state, retiring_act );
			return TRUE;
		}

		// See if we should be owner or unclaimed
		if( rip->eval_is_owner() ) {
			dprintf( D_ALWAYS, "State change: IS_OWNER is TRUE\n" );
			// TLM: STATE TRANSITION #2
			change( owner_state );
			return TRUE; // XXX: change TRUE
		}

			// Check to see if we should run benchmarks
		if ( ! r_act_was_benchmark ) {
			int num_started;
			resmgr->m_attr->start_benchmarks( rip, num_started );
		}

#if HAVE_JOB_HOOKS
			// If we're compiled to support fetching work
			// automatically and configured to do so, check now if we
			// should try to fetch more work.
		rip->tryFetchWork();
#endif /* HAVE_JOB_HOOKS */

#if HAVE_BACKFILL
			// check if we should go into the Backfill state.  only do
			// so if a) we've got a BackfillMgr object configured and
			// instantiated, and b) START_BACKFILL evals to TRUE
		if( resmgr->m_backfill_mgr && rip->eval_start_backfill() > 0 ) {
			dprintf( D_ALWAYS, "State change: START_BACKFILL is TRUE\n" );
			// TLM: STATE TRANSITION #7
			// TLM: Also unnumbered transition from Unclaimed/Benchmarking.
			change( backfill_state, idle_act );
			return TRUE; // XXX: change TRUE
		}
#endif /* HAVE_BACKFILL */

		if( rip->r_reqexp->restore() ) {
				// Our reqexp changed states, send an update
			rip->update_needed(Resource::WhyFor::wf_stateChange);
		}

		break; // case unclaimed_state


	case owner_state:
			// If the dynamic slot is allocated in the owner state
			// (e.g. because of START expression contains attributes
			// of job ClassAd), it may never go back to Unclaimed 
			// state. So we need to delete the dynmaic slot in owner
			// state.
		if (rip->is_dynamic_slot() || rip->is_broken_slot()) {
#if HAVE_JOB_HOOKS
				// If we're currently fetching we can't delete
				// ourselves. If we do when the hook returns we won't
				// be around to handle the response.
			if( rip->isCurrentlyFetching() ) {
				dprintf(D_ALWAYS, "State change: Owner -> Deleted delayed for outstanding work fetch\n");
				break;
			}
#endif
			change( delete_state );
			return TRUE; // XXX: change TRUE
		}

		if( rip->isDraining() ) {
			dprintf( D_ALWAYS, "State change: entering Drained state\n" );
			// TLM: STATE TRANSITION #36
			change( drained_state, retiring_act );
			return TRUE;
		}

		if( ! rip->eval_is_owner() ) {
			dprintf( D_ALWAYS, "State change: IS_OWNER is false\n" );
			// TLM: STATE TRANSITION #1
			change( unclaimed_state );
			return TRUE; // change() can delete rip
		}
#if HAVE_JOB_HOOKS
			// If we're compiled to support fetching work
			// automatically and configured to do so, check now if we
			// should try to fetch more work.  Even if we're in the
			// owner state, we can still see if the expressions allow
			// any fetched work at this point.
		rip->tryFetchWork();
#endif /* HAVE_JOB_HOOKS */

		break; // case owner_state


	case matched_state:
			// Nothing to do here.  If we're matched, we only want to
			// leave if the match timer goes off, or if someone with
			// the right ClaimId tries to claim us.  We can't check
			// the START expression, since we don't have a job ad, and
			// we can't check IS_OWNER, since that isn't what you want
			// (IS_OWNER might be true, while the START expression
			// might allow some jobs in, and if you get matched with
			// one of those, you want to stay matched until they try
			// to claim us).
		break;


#if HAVE_BACKFILL
	case backfill_state:
		if( ! resmgr->m_backfill_mgr ) {
			EXCEPT( "in Backfill state but m_backfill_mgr is NULL!" );
		}
		if( r_act == killing_act ) {
				// maybe we should have a safety-valve timeout here to
				// prevent ourselves from staying in Backfill/Killing
				// for too long.  however, for now, there's nothing to
				// do here...
			return 0;
		}
			// see if we should leave the Backfill state
		kill_rval = rip->eval_evict_backfill();
		if( kill_rval > 0 ) {
			dprintf( D_ALWAYS, "State change: EVICT_BACKFILL is TRUE\n" );
			if( r_act == idle_act ) {
					// no sense going to killing if we're already
					// idle, go to owner immediately.
				// TLM: STATE TRANSITION #30
				change( owner_state );
				return TRUE; // XXX: change TRUE
			}
				// we can change into Backfill/Killing then set our
				// destination, since set_dest() won't take any
				// additional action if we're already in killing_act
			ASSERT( r_act == busy_act );
			// TLM: STATE TRANSITION #28
			change( backfill_state, killing_act );
			set_destination( owner_state );
			return TRUE;
		} else if( kill_rval < 0 ) {
			dprintf( D_ALWAYS, "WARNING: EVICT_BACKFILL is UNDEFINED, "
					 "staying in Backfill state\n" );
		}

#if HAVE_JOB_HOOKS
			// If we're compiled to support fetching work
			// automatically and configured to do so, check now if we
			// should try to fetch more work.
		rip->tryFetchWork();
#endif /* HAVE_JOB_HOOKS */

		if( r_act == idle_act ) {
				// if we're in Backfill/Idle, try to spawn a backfill job
			rip->start_backfill();
		}

		break; // case backfill_state
#endif /* HAVE_BACKFILL */


	case drained_state:
		if( !rip->isDraining() ) {
			dprintf(D_ALWAYS,"State change: slot is no longer draining.\n");
			// TLM: STATE TRANSITION #34
			change( owner_state );
			return TRUE;
		}
		if( r_act == retiring_act ) {
			if( resmgr->drainingIsComplete( rip ) ) {
				resmgr->resetMaxJobRetirementTime();
				// Only do this here if we want last drain stop time to be
				// when the draining finished and not when it was cancelled
				// (if it didn't auto-resume).
				// resmgr->setLastDrainStopTime();
				dprintf(D_ALWAYS,"State change: draining is complete.\n");
				// TLM: STATE TRANSITION #33
				change( drained_state, idle_act );
				return TRUE;
			}
		}
		else if( r_act == idle_act ) {
			if( resmgr->considerResumingAfterDraining() ) {
				// Only do this if we don't do it above, when the draining
				// actually completes.
				resmgr->setLastDrainStopTime();
				return TRUE;
			}
		}
		break;

	default:
		EXCEPT( "eval_state: ERROR: unknown state (%d)",
				(int)rip->state() );
	}
	return 0;
}


int
act_to_load( Activity act )
{
	switch( act ) {
	case idle_act:
	case suspended_act:
		return 0;
		break;
	case busy_act:
	case benchmarking_act:
	case retiring_act:
	case vacating_act:
	case killing_act:
		return 1;
		break;
	default:
		EXCEPT( "Unknown activity in act_to_load" );
	}
	return -1;
}


int
ResState::leave_action( State cur_s, Activity cur_a, State new_s,
						Activity new_a, bool statechange )
{
	switch( cur_s ) {
	case preempting_state:
		if( statechange ) {
				// If we're leaving the preepting state, we should
				// delete all the attributes from our classad that are
				// only valid when we're claimed.

				// In fact, we should just delete the whole ClassAd
				// and rebuild it, since we might be leaving around
				// attributes from STARTD_JOB_ATTRS, etc.
			resmgr->update_cur_time();
			rip->init_classad();
		}
		break;

#if HAVE_BACKFILL
	case backfill_state:
			// at this point, nothing special to do... 
#endif /* HAVE_BACKFILL */
	case matched_state:
	case owner_state:
	case unclaimed_state:
	case drained_state:
		break;

	case claimed_state:
		if( cur_a == suspended_act ) {
			if( ! rip->r_cur->resumeClaim() ) {
					// If there's an error sending kill, it could only
					// mean the starter has blown up and we didn't
					// know about it.  Send SIGKILL to the process
					// group and go to the owner state.
				rip->r_cur->starterKillFamily();
				dprintf( D_ALWAYS,
						 "State change: Error sending signals to starter\n" );
				if( new_s != preempting_state ) {
					change( preempting_state );
					return TRUE; // XXX: change TRUE
				} else {
						/*
						  if we're already trying to get into the
						  preempting state (because of an error like this,
						  either suspending or resuming), we do NOT
						  want to officially call ResState::change()
						  again, or we just get into an infinite loop.
						  instead, just return FALSE (that we didn't
						  already change the state), and allow the
						  previous attempt to change into preempting_state
						  to continue...
						*/
					return FALSE;
				}
			}
		}

		//TODO: tstclair evaluate whether we should even be doing this in ResState
		if( statechange && new_a != vacating_act ) {
			rip->r_cur->cancelLeaseTimer();	
		}
		break;
	default:
		EXCEPT("Unknown state in ResState::leave_action (%d)", (int)cur_s);
	}

	return FALSE;
}

extern ExprTree * globalDrainingStartExpr;

int
ResState::enter_action( State s, Activity a,
						bool statechange, bool )
{
#ifdef WIN32
	if (a == busy_act)
		systray_notifier.notifyCondorJobRunning(rip->r_id - 1);
	else if (s == unclaimed_state)
		systray_notifier.notifyCondorIdle(rip->r_id - 1);
	else if (s == preempting_state)
		systray_notifier.notifyCondorJobPreempting(rip->r_id - 1);
	else if (a == suspended_act)
		systray_notifier.notifyCondorJobSuspended(rip->r_id - 1);
	else
		systray_notifier.notifyCondorClaimed(rip->r_id - 1);
#endif

	
	switch( s ) {
	case owner_state:
			// Always want to create new claim objects
		if( rip->r_cur ) {
			delete( rip->r_cur );
		}
		rip->r_cur = new Claim( rip );
		if( rip->r_pre ) {
			rip->remove_pre();
		}
			// See if we should be in owner or unclaimed state
		if( ! rip->eval_is_owner() ) {
				// Really want to be in unclaimed.
			dprintf( D_ALWAYS, "State change: IS_OWNER is false\n" );
			change( unclaimed_state );
			return TRUE; // XXX: change TRUE
		}
		rip->r_reqexp->restore();		
		break;

	case claimed_state:
		rip->r_reqexp->restore();			
		if( statechange ) {
			rip->r_cur->beginClaim();	
				// Update important attributes into the classad.
			rip->r_cur->publish( rip->r_classad );
				// Generate a preempting claim object, but not for a pslot
			if (!rip->is_partitionable_slot()) {
				rip->r_pre = new Claim( rip );
			}
		}
		// If the slot is leaving retiring action but remaining in claimed
		// state, clear the vacate reason that caused it to enter retiring
		// action.
		if (a != retiring_act) {
			rip->r_cur->clearVacateReason();
		}
		if (a == suspended_act) {
			if( ! rip->r_cur->suspendClaim() ) {
				rip->r_cur->starterKillFamily();
				dprintf( D_ALWAYS,
						 "State change: Error sending signals to starter\n" );
				change( preempting_state );
				return TRUE; // XXX: change TRUE
			}
		}
		else if (a == busy_act) {
			resmgr->start_poll_timer();

			if( rip->inRetirement() && !rip->wasAcceptedWhileDraining() ) {

				// We have returned to a busy state (e.g. from
				// suspension) and there is a preempting claim or we
				// are in irreversible retirement, so retire.

				// inRetirement() is actually hasPreemptingClaim() || !mayUnretire(),
				// where we can't unretire if we're draining.  Otherwise,
				// the drain command would start us retiring and we'd
				// immediately unretire.  However, if this job was accepted
				// while we were draining, we don't want to start retiring
				// it until the last of the "original" jobs has finished.

				if (rip->hasPreemptingClaim()) {
					if (rip->r_pre->rank() > rip->r_cur->rank()) {
						rip->setVacateReason("Preempted for a higher Ranked job", CONDOR_HOLD_CODE::StartdPreemptingClaimRank, 0);
					} else {
						rip->setVacateReason("Preempted for a Priority user", CONDOR_HOLD_CODE::StartdPreemptingClaimUserPrio, 0);
					}
				}
				change( retiring_act );
				return TRUE; // XXX: change TRUE
			}
		}
		else if (a == retiring_act) {
			if( ! rip->claimIsActive() ) {
				// The starter exited by the time we got here.
				// No need to wait around in retirement.
				change( preempting_state );
				return TRUE; // XXX: change TRUE
			}
		}
#if HAVE_JOB_HOOKS
		else if (a == idle_act) {
			if (rip->r_cur->type() == CLAIM_FETCH) {
				if (statechange) {
						// We just entered Claimed/Idle on a state change,
						// and we've got a fetch claim, so try to activate it.
					ASSERT(rip->r_cur->hasJobAd());
					rip->spawnFetchedWork();
						// spawnFetchedWork() *always* causes a state change.
					return TRUE;
				}
				else {
						// We just entered Claimed/Idle, but not due
						// to a state change.  The starter must have
						// exited, so we should try to fetch more work.
					rip->tryFetchWork();

						// Starting the fetch doesn't cause a state
						// change, only the handler does, so we should
						// just return FALSE.
					return FALSE;
				}
			}
		}
#endif /* HAVE_JOB_HOOKS */

		break;

	case unclaimed_state:
		rip->r_reqexp->restore();
		break;

#if HAVE_BACKFILL
	case backfill_state:
			// whenever we're in Backill, we might be available
		rip->r_reqexp->restore();
		
		switch( a ) {

		case killing_act:
				// TODO notice and handle failure 
			rip->hardkill_backfill();
			break;

		case idle_act:
 				/*
				  we want to make sure the ResMgr will do frequent
				  evaluations now that we're in Backfill/Idle, so we
				  can spawn the backfill client quickly.  we do NOT
				  want to just immediately spawn it here, so that we
				  have a little bit of delay (to prevent pegging the
				  CPU in case of failure) and so that if there's a
				  temporary failure to spawn, we don't forget to keep
				  trying... 
				*/
			resmgr->start_poll_timer();
			break;

		case busy_act:
				// nothing special to do (yet)
			break;

		default:
			EXCEPT( "activity %s not yet supported in backfill state", 
					activity_to_string(a) ); 
		}
		break;
#endif /* HAVE_BACKFILL */

	case matched_state:
		rip->r_reqexp->unavail();
		break;

	case preempting_state:
		rip->r_reqexp->unavail();
		switch( a ) {
		case killing_act:
			if( rip->claimIsActive() ) {
				if( rip->preemptWasTrue() && rip->wants_hold() ) {
					rip->hold_job(false);
				} else {
					rip->r_cur->starterVacateJob(false);
				}
			} else {
				rip->leave_preempting_state();
				return TRUE;
			}
			break;

		case vacating_act:
			if( rip->claimIsActive() ) {
				if( rip->preemptWasTrue() && rip->wants_hold() ) {
					rip->hold_job(true);
				} else {
					rip->r_cur->starterVacateJob(true);
				}
			} else {
				rip->leave_preempting_state();
				return TRUE;
			}
			break;

		default:
			EXCEPT( "Unknown activity in ResState::enter_action" );
		}
		break; 	// preempting_state

	case delete_state:
		if (rip->is_dynamic_slot() || rip->is_broken_slot()) {
			resmgr->removeResource( rip );
		} else {
			resmgr->deleteResource( rip );
		}
		return TRUE;
		break;

	case drained_state:
		rip->r_reqexp->unavail( globalDrainingStartExpr );
		break;

	default:
		EXCEPT("Unknown state in ResState::enter_action");
	}
	return FALSE;
}


void
ResState::dprintf( int flags, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


void
ResState::set_destination( State new_state )
{

	switch( new_state ) {
	case delete_state:
	case owner_state:
			// these 2 are always valid, nothing special to check
		break;

	case matched_state:
			// we don't want to set our destination to matched if our
			// destination is already set to something
			// else... otherwise, we'll allow it.
		if( r_destination != no_state ) {
			dprintf( D_ALWAYS, "Not setting destination state to Matched "
					 "since destination already set to %s\n",
					 state_to_string(r_destination) ); 
			return;
		}
		break;

	case claimed_state:
			// this is only valid if we've got a pending request to
			// claim that's already been stashed in our Claim object 
#if HAVE_JOB_HOOKS
		if (rip->r_cur->type() == CLAIM_FETCH) {
			if (rip->r_cur->ad() == NULL) {
				EXCEPT( "set_destination(Claimed) called but there's no "
						"fetched job classad in our current Claim" );
			}
		}
		else
#endif /* HAVE_JOB_HOOKS */
		if( ! rip->r_cur->requestStream() ) {
			EXCEPT( "set_destination(Claimed) called but there's no "
					"pending request stream set in our current Claim" );
		}
		break;

	default:
		EXCEPT( "set_destination() doesn't work with %s state yet", 
				state_to_string(r_destination) );
	}

	r_destination = new_state;
	dprintf( D_FULLDEBUG, "Set destination state to %s\n", 
			 state_to_string(new_state) );

		// Now, depending on what state we're in, do what we have to
		// do to start moving towards our destination.
	switch( r_state ) {
	case owner_state:
	case matched_state:
	case unclaimed_state:
		if( r_destination == claimed_state ) {
				// this is a little weird, but we can't just enter
				// claimed directly, we have to see what kind of claim
				// it is and potentailly do all the gnarly claiming
				// protocol stuff, first...
			rip->acceptClaimRequest();
		} else {
			change( r_destination );
		}
		break;
	case preempting_state:
			// Can't do anything else until the starter exits.
		break;
	case claimed_state:
		change( preempting_state );
		break;
#if HAVE_BACKFILL
	case backfill_state:
		if( r_act == idle_act ) {
				// if we're idle, we can go immediately 
			if( r_destination == claimed_state ) {
				rip->acceptClaimRequest();
			} else {
				change( r_destination );
			}
		} else {
				// otherwise, start killing, and we'll finish our
				// journey when the backfill starter exits
			change( killing_act );
		}
		break;
#endif /* HAVE_BACKFILL */

	default:
		EXCEPT( "Unexpected state %s in ResState::set_destination",
				state_to_string(r_state) );
	}

}


int
ResState::starterExited( void )
{
		// in many cases, the starter exiting is what allows us to go
		// to our destination...
	switch( r_destination ) {

	case no_state:
			// destination not set, nothing to do here...
		break;

	case delete_state:
	case owner_state:
	case matched_state:
			// for all 3 of these, once the starter is gone, we can
			// enter the destination directly.
		dprintf( D_ALWAYS, "State change: starter exited\n" );
		change( r_destination );
		return TRUE; // XXX: change TRUE
		break;

	case claimed_state:
			// now that a starter is gone, if we've got a pending
			// request to claim, we can finally accept it.  if that
			// pending request is gone for some reason, go back to
			// the Owner state... 
		dprintf( D_ALWAYS, "State change: starter exited\n" );
		if (rip->acceptClaimRequest()) {
				// Successfully accepted the claim and changed state.
			return TRUE;
		} else {
			r_destination = no_state;
			change( owner_state );
			return TRUE; // XXX: change TRUE
		}
		break;

	default:
		EXCEPT( "Unexpected destination state (%s) in"
				"ResState::starterExited()\n",
				state_to_string(r_destination) );
		break;
	}

		// if we're here, we didn't have a destination, so do some
		// other state-related logic now that a starter exited

	switch( r_state ) {

	case claimed_state:
			// TODO: if we're just claimed when it exits, goto idle_act
		break;

	case preempting_state:
			// TODO: eventually, we should have a destination before
			// we enter preempting, but for now, just allow it...
		break;

#if HAVE_BACKFILL
	case backfill_state:
		if( r_act == idle_act ) {
			dprintf( D_ALWAYS, "ERROR: ResState::starterExited() called "
					 "while already in Backfill/Idle\n" );
			return FALSE;
		}
		dprintf( D_ALWAYS, "State change: Backfill starter exited\n" );
		change( idle_act );
		return TRUE; // XXX: change TRUE
		break;
#endif /* HAVE_BACKFILL */

	default:
		EXCEPT( "Unexpected current state (%s) in"
				"ResState::starterExited()\n",
				state_to_string(r_state) );
		break;
	}

		// if we got here, we didn't do any state changes at all,
		// which is bad
		// TODO: fail more violently if we get here (only once we're
		// really using destination for everything we can...)
	return FALSE;
}

void
ResState::resetActivityAverages()
{
	m_activity_avg_last_timestamp = time(NULL);
	m_activity_avg_time_sum = 0;
	m_num_cpus_avg = rip->r_attr->num_cpus();
	m_draining_avg = rip->isDraining();
}

void
ResState::updateActivityAverages()
{
	if ( m_activity_avg_last_timestamp == 0 ) {
		resetActivityAverages();
	}
	else {
		time_t now = time(NULL);
		int delta = now - m_activity_avg_last_timestamp;
		m_activity_avg_last_timestamp = now;
		if( delta > 0 ) {
			m_num_cpus_avg = (m_num_cpus_avg * m_activity_avg_time_sum + rip->r_attr->num_cpus() * delta)/(m_activity_avg_time_sum + delta);
			m_draining_avg = (m_draining_avg * m_activity_avg_time_sum + rip->isDraining() * delta)/(m_activity_avg_time_sum + delta);
			m_activity_avg_time_sum += delta;
		}
	}
}

void
ResState::updateHistoryTotals( time_t now )
{
		// We just have to see what state/activity we're leaving, and
		// increment the right counter variable.
	time_t* history_ptr = getHistoryTotalPtr(r_state, r_act);
	*history_ptr += (now - m_atime);

	if( r_state == unclaimed_state || r_state == drained_state )
	{
			// unclaimed draining time is in cpu seconds
		int delta = (int)((now - m_atime)*m_draining_avg*m_num_cpus_avg);
		m_time_draining_unclaimed += delta;
	}
	resetActivityAverages();
}

time_t
ResState::timeDrainingUnclaimed()
{
	time_t total = m_time_draining_unclaimed;

		// Add in the time spent in the current state/activity
	if( r_state == unclaimed_state || r_state == drained_state )
	{
		time_t now = time(nullptr);
			// unclaimed draining time is in cpu seconds
		total += (now - m_atime) * m_draining_avg*m_num_cpus_avg;
	}
	return total;
}

time_t*
ResState::getHistoryTotalPtr( State _state, Activity _act ) {
	ResState::HistoryInfo info = getHistoryInfo(_state, _act);
	return info.time_ptr;
}


const char*
ResState::getHistoryTotalAttr( State _state, Activity _act ) {
	ResState::HistoryInfo info = getHistoryInfo(_state, _act);
	return info.attr_name;
}


ResState::HistoryInfo
ResState::getHistoryInfo( State _state, Activity _act ) {
	ResState::HistoryInfo info;
	time_t* var_ptr = NULL;
	const char* attr_name = NULL;
	switch (_state) {
	case owner_state:
		var_ptr = &m_time_owner_idle;
		attr_name = ATTR_TOTAL_TIME_OWNER_IDLE;
		break;
	case unclaimed_state:
		switch (_act) {
		case idle_act:
			var_ptr = &m_time_unclaimed_idle;
			attr_name = ATTR_TOTAL_TIME_UNCLAIMED_IDLE;
			break;
		case benchmarking_act:
			var_ptr = &m_time_unclaimed_benchmarking;
			attr_name = ATTR_TOTAL_TIME_UNCLAIMED_BENCHMARKING;
			break;
		default:
			EXCEPT("Unexpected activity (%s: %d) in getHistoryInfo() for %s",
				   activity_to_string(_act), (int)_act,
				   state_to_string(_state));
		}
		break;
	case matched_state:
		var_ptr = &m_time_matched_idle;
		attr_name = ATTR_TOTAL_TIME_MATCHED_IDLE;
		break;
	case claimed_state:
		switch (_act) {
		case idle_act:
			var_ptr = &m_time_claimed_idle;
			attr_name = ATTR_TOTAL_TIME_CLAIMED_IDLE;
			break;
		case busy_act:
			var_ptr = &m_time_claimed_busy;
			attr_name = ATTR_TOTAL_TIME_CLAIMED_BUSY;
			break;
		case suspended_act:
			var_ptr = &m_time_claimed_suspended;
			attr_name = ATTR_TOTAL_TIME_CLAIMED_SUSPENDED;
			break;
		case retiring_act:
			var_ptr = &m_time_claimed_retiring;
			attr_name = ATTR_TOTAL_TIME_CLAIMED_RETIRING;
			break;
		default:
			EXCEPT("Unexpected activity (%s: %d) in getHistoryInfo() for %s",
				   activity_to_string(_act), (int)_act,
				   state_to_string(_state));
		}
		break;
	case preempting_state:
		switch (_act) {
		case vacating_act:
			var_ptr = &m_time_preempting_vacating;
			attr_name = ATTR_TOTAL_TIME_PREEMPTING_VACATING;
			break;
		case killing_act:
			var_ptr = &m_time_preempting_killing;
			attr_name = ATTR_TOTAL_TIME_PREEMPTING_KILLING;
			break;
		default:
			EXCEPT("Unexpected activity (%s: %d) in getHistoryInfo() for %s",
				   activity_to_string(_act), (int)_act,
				   state_to_string(_state));
		}
		break;
	case drained_state:
		switch (_act) {
		case idle_act:
			var_ptr = &m_time_drained_idle;
			attr_name = ATTR_TOTAL_TIME_DRAINED_IDLE;
			break;
		case retiring_act:
			var_ptr = &m_time_drained_retiring;
			attr_name = ATTR_TOTAL_TIME_DRAINED_RETIRING;
			break;
		default:
			EXCEPT("Unexpected activity (%s: %d) in getHistoryInfo() for %s",
				   activity_to_string(_act), (int)_act,
				   state_to_string(_state));
		}
		break;
	case backfill_state:
		switch (_act) {
		case idle_act:
			var_ptr = &m_time_backfill_idle;
			attr_name = ATTR_TOTAL_TIME_BACKFILL_IDLE;
			break;
		case busy_act:
			var_ptr = &m_time_backfill_busy;
			attr_name = ATTR_TOTAL_TIME_BACKFILL_BUSY;
			break;
		case killing_act:
			var_ptr = &m_time_backfill_killing;
			attr_name = ATTR_TOTAL_TIME_BACKFILL_KILLING;
			break;
		default:
			EXCEPT("Unexpected activity (%s: %d) in getHistoryInfo() for %s",
				   activity_to_string(_act), (int)_act,
				   state_to_string(_state));
		}
		break;
	default:
		EXCEPT("Unexpected state (%s: %d) in getHistoryInfo()",
			   state_to_string(_state), (int)_state);

	}
	info.time_ptr = var_ptr;
	info.attr_name = attr_name;
	return info;
}

bool
ResState::publishHistoryInfo( ClassAd* cap, State _state, Activity _act )
{
	ResState::HistoryInfo info = getHistoryInfo(_state, _act);
	time_t total = *info.time_ptr;

		// Add in the time spent in the current state/activity
	if (_state == r_state && _act == r_act) {
		total += (time(NULL) - m_atime);
	}
	if (total) {
		cap->Assign(info.attr_name, total);
		return true;
	}
	return false;
}

time_t
ResState::activityTimeElapsed() const
{
	return time(nullptr) - m_atime;
}
