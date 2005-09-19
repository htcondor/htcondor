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

ResState::ResState( Resource* rip )
{
	r_state = owner_state;
	r_destination = no_state;
	r_act = idle_act;
	atime = (int)time(NULL);
	stime = atime;
	this->rip = rip;
}


void
ResState::publish( ClassAd* cp, amask_t how_much ) 
{
	char tmp[80];

	if( IS_PRIVATE(how_much) ) {
			// Nothing to publish for private ads
		return;
	}

	sprintf( tmp, "%s=\"%s\"", ATTR_STATE, state_to_string(r_state) );
	cp->InsertOrUpdate( tmp );

	sprintf( tmp, "%s=%d", ATTR_ENTERED_CURRENT_STATE, stime );
	cp->InsertOrUpdate( tmp );

	sprintf( tmp, "%s=\"%s\"", ATTR_ACTIVITY, activity_to_string(r_act) );
	cp->InsertOrUpdate( tmp );

	sprintf( tmp, "%s=%d", ATTR_ENTERED_CURRENT_ACTIVITY, atime );
	cp->InsertOrUpdate(tmp);
}


int
ResState::change( State new_state, Activity new_act )
{
	bool statechange = false, actchange = false;
	int now;

	if( new_state != r_state ) {
		statechange = true;
	}
	if( new_act != r_act ) {
		actchange = true;
	}
	if( ! (actchange || statechange) ) {
		return TRUE;   // If we're not changing anything, return
	}

		// leave_action and enter_action return TRUE if they result in
		// a state or activity change.  In these cases, we want to
		// abort the current state change.
	if( leave_action( r_state, r_act, new_state, new_act, statechange ) ) {
		return TRUE;
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

 	now = (int)time( NULL );
	if( statechange ) {
		stime = now;
			// Also reset activity time
		atime = now;
		r_state = new_state;
		if( r_state == r_destination ) {
				// We've reached our destination, so we can reset it.
			r_destination = no_state;
		}
	}
	if( actchange ) {
		r_act = new_act;
		atime = now;
	}

	if( enter_action( r_state, r_act, statechange, actchange ) ) {
		return TRUE;
	}

		// Update resource availability statistics on state changes
	rip->r_avail_stats.update( r_state, r_act );
	
		// Note our current state and activity in the classad
	this->publish( rip->r_classad, A_ALL );

		// We want to update the CM on every state or activity change
	rip->update();   

	if( r_act == retiring_act ) {
		// When we enter retirement, check right away to see if we should
		// be preempting instead.
		this->eval();
	}

	return TRUE;
}


int
ResState::change( Activity new_act )
{
	return change( r_state, new_act );
}


int
ResState::change( State new_state )
{
	if( new_state == preempting_state ) {
			// wants_vacate dprintf's about what it decides and the
			// implications on state changes...
		if( !rip->preemptWasTrue() || rip->wants_vacate() ) {
			return change( new_state, vacating_act );
		} else {
			return change( new_state, killing_act );
		}
	} else {
		return change( new_state, idle_act );
	}
}


int
ResState::eval( void )
{
	int want_suspend;

		// we may need to modify the load average in our internal
		// policy classad if we're currently running a COD job or have
		// been running 1 in the last minute.  so, give our rip a
		// chance to modify the load, if necessary, before we evaluate
		// anything.  
	rip->hackLoadForCOD();

		// also, since we might be an SMP where other VMs just changed
		// their state, we also want to re-publish the shared VM
		// attributes so that other VMs can see those results.
	rip->refreshVmAttrs();

	switch( r_state ) {

	case claimed_state:
		if( r_act == suspended_act && rip->isSuspendedForCOD() ) { 
				// this is the special case where we do *NOT* want to
				// evaluate any policy expressions.  so long as
				// there's an active COD job, we want to leave the
				// opportunistic claim "checkpointed to swap"
			return 0;
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

			if( rip->eval_preempt() ) {
				dprintf( D_ALWAYS, "State change: PREEMPT is TRUE\n" );
				// irreversible retirement
				// STATE TRANSITION #12 or #16
				rip->preemptIsTrue();
				return rip->retire_claim();
			}
		}
		if( r_act == retiring_act ) {
			if( rip->mayUnretire() ) {
				dprintf( D_ALWAYS, "State change: unretiring because no preempting claim exists\n" );
				// STATE TRANSITION #13
				return change( busy_act );
			}
			if( rip->retirementExpired() ) {
				dprintf( D_ALWAYS, "State change: retirement ended/expired\n" );
				// STATE TRANSITION #18
				return change( preempting_state );
			}
		}
		if( (r_act == busy_act || r_act == retiring_act) && want_suspend ) {
			if( rip->eval_suspend() ) {
				// STATE TRANSITION #14 or #17
				dprintf( D_ALWAYS, "State change: SUSPEND is TRUE\n" );
				return change( suspended_act );
			}
		}
		if( r_act == suspended_act ) {
			if( rip->eval_continue() ) {
				// STATE TRANSITION #15
				dprintf( D_ALWAYS, "State change: CONTINUE is TRUE\n" );
				if( rip->mayUnretire() ) {
					return change( busy_act );
				}
				else {
					// STATE TRANSITION #16
					return change( retiring_act );
				}
			}
			else if( !rip->mayUnretire() ) {
				// We are in suspended retirement.  It is possible
				// for the retirement time to expire in this case,
				// but only if MaxJobRetirementTime decreases
				// for some reason.
				if( rip->retirementExpired() ) {
					dprintf( D_ALWAYS, "State change: retirement ended/expired during suspension\n" );
					// STATE TRANSITION #18b
					return change( preempting_state );
				}
			}
		}
		if( (r_act == busy_act) && rip->hasPreemptingClaim() ) {
			dprintf( D_ALWAYS, "State change: retiring due to preempting claim\n" );
			// reversible retirement (e.g. if preempting claim goes away)
			// STATE TRANSITION #12
			return change( retiring_act );
		}
		if( (r_act == idle_act) && (rip->eval_start() == 0) ) {
				// START evaluates to False, so return to the owner
				// state.  In this case, we don't need to worry about
				// START locally evaluating to FALSE due to undefined
				// job attributes and well-placed meta-operators, b/c
				// we're in the claimed state, so we'll have a job ad
				// to evaluate against.
			dprintf( D_ALWAYS, "State change: START is false\n" );
			return change( preempting_state ); 
		}
		if( (r_act == busy_act || r_act == retiring_act) && (rip->wants_pckpt()) ) {
			rip->periodic_checkpoint();
		}
		if( rip->r_reqexp->restore() ) {
				// Our reqexp changed states, send an update
			rip->update();
		}
		break;   // case claimed_state:

	case preempting_state:
		if( r_act == vacating_act ) {
			if( rip->eval_kill() ) {
				dprintf( D_ALWAYS, "State change: KILL is TRUE\n" );
					// STATE TRANSITION #19
				return change( killing_act );
			}
		}
		break;	// case preempting_state:

	case unclaimed_state:
		// See if we should be owner or unclaimed
		if( rip->eval_is_owner() ) {
			dprintf( D_ALWAYS, "State change: IS_OWNER is TRUE\n" );
			return change( owner_state );
		}
			// Check to see if we should run benchmarks
		deal_with_benchmarks( rip );

		if( rip->r_reqexp->restore() ) {
				// Our reqexp changed states, send an update
			rip->update();
		}

		break;	

	case owner_state:
		if( ! rip->eval_is_owner() ) {
			dprintf( D_ALWAYS, "State change: IS_OWNER is false\n" );
			change( unclaimed_state );
		}
		break;	
		
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
						Activity, bool statechange )
{
	switch( cur_s ) {
	case preempting_state:
		if( statechange ) {
				// If we're leaving the preepting state, we should
				// delete all the attributes from our classad that are
				// only valid when we're claimed.

				// In fact, we should just delete the whole ClassAd
				// and rebuild it, since we might be leaving around
				// attributes from STARTD_JOB_EXPRS, etc.
			rip->init_classad();
		}
		break;
	case matched_state:
	case owner_state:
	case unclaimed_state:
		break;
	case claimed_state:
		if( cur_a == suspended_act ) {
			if( ! rip->r_cur->resumeClaim() ) {
					// If there's an error sending kill, it could only
					// mean the starter has blown up and we didn't
					// know about it.  Send SIGKILL to the process
					// group and go to the owner state.
				rip->r_cur->starterKillPg( SIGKILL );
				dprintf( D_ALWAYS,
						 "State change: Error sending signals to starter\n" );
				if( new_s != owner_state ) {
						/*
						  if we're already trying to get into the
						  owner state (because of an error like this,
						  either suspending or resuming), we do NOT
						  want to officially call ResState::change()
						  again, or we just get into an infinite loop.
						  instead, just return FALSE (that we didn't
						  already change the state), and allow the
						  previous attempt to change into owner_state
						  to continue...
						*/
					return change( owner_state );
				} else {
					return FALSE;
				}
			}
		}
		if( statechange ) {
			rip->r_cur->cancelLeaseTimer();	
		}
		break;
	default:
		EXCEPT("Unknown state in ResState::leave_action");
	}

	return FALSE;
}


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
			delete rip->r_pre;
			rip->r_pre = NULL;
		}
			// See if we should be in owner or unclaimed state
		if( ! rip->eval_is_owner() ) {
				// Really want to be in unclaimed.
			dprintf( D_ALWAYS, "State change: IS_OWNER is false\n" );
			return change( unclaimed_state );
		}
		rip->r_reqexp->restore();		
		break;

	case claimed_state:
		rip->r_reqexp->restore();			
		if( statechange ) {
			rip->r_cur->beginClaim();	
				// Update important attributes into the classad.
			rip->r_cur->publish( rip->r_classad, A_PUBLIC );
				// Generate a preempting claim object
			rip->r_pre = new Claim( rip );
		}
		if( a == suspended_act ) {
			if( ! rip->r_cur->suspendClaim() ) {
				rip->r_cur->starterKillPg( SIGKILL );
				dprintf( D_ALWAYS,
						 "State change: Error sending signals to starter\n" );
				return change( owner_state );
			}
		}
		if( a == busy_act ) {
			resmgr->start_poll_timer();

			if( rip->hasPreemptingClaim() || !rip->mayUnretire() ) {

				// We have returned to a busy state (e.g. from
				// suspension) and there is a preempting claim or we
				// are in irreversible retirement, so retire.

				return change( retiring_act );
			}
		}
		if( a == retiring_act ) {
			if( ! rip->claimIsActive() ) {
				// The starter exited by the time we got here.
				// No need to wait around in retirement.
				return change( preempting_state );
			}
		}
		break;

	case unclaimed_state:
		rip->r_reqexp->restore();
		break;

	case matched_state:
		rip->r_reqexp->unavail();
		break;

	case preempting_state:
		rip->r_reqexp->unavail();
		switch( a ) {
		case killing_act:
			if( rip->claimIsActive() ) {
				if( ! rip->r_cur->starterKillHard() ) {
						// starterKillHard returns FALSE if there was
						// an error in kill and we had to send SIGKILL
						// to the starter's process group.
					dprintf( D_ALWAYS,
							 "State change: Error sending signals to starter\n" );
					return change( owner_state );
				}
			} else {
				rip->leave_preempting_state();
				return TRUE;
			}
			break;

		case vacating_act:
			if( rip->claimIsActive() ) {
				if( ! rip->r_cur->starterKillSoft() ) {
					rip->r_cur->starterKillPg( SIGKILL );
					dprintf( D_ALWAYS,
							 "State change: Error sending signals to starter\n" );
					return change( owner_state );
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
		resmgr->deleteResource( rip );
		return TRUE;
		break;

	default: 
		EXCEPT("Unknown state in ResState::enter_action");
	}
	return FALSE;
}


void
ResState::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


void
ResState::set_destination( State new_state )
{
	r_destination = new_state;

	if( r_destination != delete_state ) {
		EXCEPT( "set_destination() doesn't work with %s state yet", 
				state_to_string(r_destination) );
	}

	dprintf( D_FULLDEBUG, "Set destination state to %s\n", 
			 state_to_string(new_state) );

		// Now, depending on what state we're in, do what we have to
		// do to start moving towards our destination.
	switch( r_state ) {
	case owner_state:
	case matched_state:
	case unclaimed_state:
		change( r_destination );
		break;
	case preempting_state:
			// Can't do anything else until the starter exits.
		break;
	case claimed_state:
		change( preempting_state );
		break;
	default:
		EXCEPT( "Unexpected state %s in ResState::set_destination",
				state_to_string(r_state) );
	}

}
