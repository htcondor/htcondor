/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "startd.h"

Resource::Resource( CpuAttributes* cap, int rid )
{
	char tmp[256];
	char* tmpName;
	int size = (int)ceil(60 / polling_interval);
	r_classad = NULL;
	r_state = new ResState( this );
	r_starter = new Starter( this );
	r_cur = new Match( this );
	r_pre = NULL;
	r_reqexp = new Reqexp( this );
	r_load_queue = new LoadQueue( size );

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

	kill_tid = -1;
	update_tid = -1;
	r_is_deactivating = false;

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

	if( r_attr->type() ) {
		dprintf( D_ALWAYS, "New machine resource of type %d allocated\n",  
				 r_attr->type() );
	} else { 
		dprintf( D_ALWAYS, "New machine resource allocated\n" );
	}
}


Resource::~Resource()
{
	this->cancel_kill_timer();

	if ( update_tid != -1 ) {
		daemonCore->Cancel_Timer(update_tid);
		update_tid = -1;
	}

	delete r_state;
	delete r_classad;
	delete r_starter;
	delete r_cur;		
	if( r_pre ) {
		delete r_pre;		
	}
	delete r_reqexp;   
	delete r_attr;		
	delete r_load_queue;
	free( r_name );
	free( r_id_str );
}


int
Resource::release_claim( void )
{
	switch( state() ) {
	case claimed_state:
		return change_state( preempting_state, vacating_act );
	case matched_state:
		return change_state( owner_state );
	default:
			// For good measure, try directly killing the starter if
			// we're in any other state.  If there's no starter, this
			// will just return without doing anything.  If there is a
			// starter, it shouldn't be there.
		return r_starter->kill( DC_SIGSOFTKILL );
	}
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
		return hardkill_starter();
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
	if( r_starter->kill( DC_SIGPCKPT ) < 0 ) {
		return FALSE;
	}
	r_cur->setlastpckpt((int)time(NULL));

		// Now that we updated this time, be sure to insert those
		// attributes into the classad right away so we don't keep
		// periodically checkpointing with stale info.
	r_cur->publish( r_classad, A_PUBLIC );

	return TRUE;
}


int
Resource::request_new_proc( void )
{
	if( state() == claimed_state ) {
		return r_starter->kill( DC_SIGHUP );
	} else {
		return FALSE;
	}
}	


int
Resource::deactivate_claim( void )
{
	dprintf(D_ALWAYS, "Called deactivate_claim()\n");
	if( state() == claimed_state ) {
		if( r_starter->active() ) {
				// Set a flag to avoid a potential race in our
				// protocol.  
			r_is_deactivating = true;
				// Singal the starter.
			return r_starter->kill( DC_SIGSOFTKILL );
		} else {
			return TRUE;
		}
	} else {
		return FALSE;
	}
}


int
Resource::deactivate_claim_forcibly( void )
{
	dprintf(D_ALWAYS, "Called deactivate_claim_forcibly()\n");
	if( state() == claimed_state ) {
		if( r_starter->active() ) {
				// Set a flag to avoid a potential race in our
				// protocol.  
			r_is_deactivating = true;
				// Singal the starter.
			return hardkill_starter();
		} else {
			return TRUE;
		}
	} else {
		return FALSE;
	}
}


int
Resource::hardkill_starter( void )
{
	if( ! r_starter->active() ) {
		return TRUE;
	}
	if( r_starter->kill( DC_SIGHARDKILL ) < 0 ) {
		r_starter->killpg( DC_SIGKILL );
		return FALSE;
	} else {
		start_kill_timer();
		return TRUE;
	}
}


int
Resource::sigkill_starter( void )
{
		// Now that the timer has gone off, clear out the tid.
	kill_tid = -1;
	if( r_starter->active() ) {
			// Kill all of the starter's children.
		r_starter->killkids( DC_SIGKILL );
			// Kill the starter's entire process group.
		return r_starter->killpg( DC_SIGKILL );
	}
	return TRUE;
}


bool
Resource::in_use( void )
{
	State s = state();
	if( s == owner_state || s == unclaimed_state ) {
		return false;
	}
	return true;
}


void
Resource::starter_exited( void )
{
	dprintf( D_ALWAYS, "Starter pid %d has exited.\n",
			 r_starter->pid() );

		// Now that the starter is gone, we can clear this flag.
	r_is_deactivating = false;

		// Let our starter object know it's starter has exited.
	r_starter->exited();

		// Now that this starter has exited, cancel the timer that
		// would send it SIGKILL.
	cancel_kill_timer();

		// All of the potential paths from here result in a state
		// change, and all of them are triggered by the starter
		// exiting, so let folks know that happened.  The logic in
		// leave_preempting_state() is more complicated, and we'll
		// describe why we make the change we do in there.
	dprintf( D_ALWAYS, "State change: starter exited\n" );

	State s = state();
	switch( s ) {
	case claimed_state:
		r_cur->client()->setuser( r_cur->client()->owner() );
		change_state( idle_act );
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


int
Resource::spawn_starter( start_info_t* info, time_t now )
{
	int rval;
	if( ! r_starter ) {
			// Big error!
		dprintf( D_ALWAYS, "ERROR! Resource::spawn_starter() called "
				 "w/ no Starter object! Returning failure\n" );
		return 0;
	}

	rval = r_starter->spawn( info, now );

		// Fake ourselves out so we take another snapshot in 15
		// seconds, once the starter has had a chance to spawn the
		// user job and the job as (hopefully) done any initial
		// forking it's going to do.  If we're planning to check more
		// often that 15 seconds, anyway, don't bother with this.
	if( pid_snapshot_interval > 15 ) {
		r_starter->set_last_snapshot( (now + 15) -
									  pid_snapshot_interval );
	} 
	return rval;
}


/* 
   This function is called whenever we're in the preempting state
   without a starter.  This situation occurs b/c either the starter
   has finally exited after being told to go away, or we preempted a
   match that wasn't active with a starter in the first place.  In any
   event, leave_preempting_state is the one place that does what needs
   to be done to all the current and preempting matches we've got, and
   decides which state we should enter.
*/
void
Resource::leave_preempting_state( void )
{
	int tmp;

	r_cur->vacate();	// Send a vacate to the client of the match
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
	if( r_pre && r_pre->agentstream() ) {
		allow_it = true;
		if( (r_classad->EvalBool("START", r_pre->ad(), tmp)) 
			&& !tmp ) {
				// Only if it's defined and false do we consider the
				// machine busy.  We have a job ad, so local
				// evaluation gotchas don't apply here.
			dprintf( D_ALWAYS, 
					 "State change: preempting match refused - START is false\n" );
			allow_it = false;
		} else {
			dprintf( D_ALWAYS, 
					 "State change: preempting match exists - "
					 "START is true or undefined\n" );
		}
	} else {
		dprintf( D_ALWAYS, 
				 "State change: No preempting match, returning to owner\n" );
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

	this->publish( &public_ad, A_PUBLIC | A_ALL | A_EVALUATED );
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
Resource::start_kill_timer( void )
{
	if( kill_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	kill_tid = 
		daemonCore->Register_Timer( killing_timeout,
						0, 
						(TimerHandlercpp)&Resource::sigkill_starter,
						"sigkill_starter", this );
	if( kill_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


void
Resource::cancel_kill_timer( void )
{
	if( kill_tid != -1 ) {
		daemonCore->Cancel_Timer( kill_tid );
		kill_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled kill timer.\n" );
	}
}


int
Resource::wants_vacate( void )
{
	int want_vacate = 0;
	bool unknown = true;

	if( ! r_starter->active() ) {
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

	if( r_cur->universe() == VANILLA ) {
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
	if( r_cur->universe() == VANILLA ) {
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

	if( r_cur->universe() != STANDARD ) {
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
Resource::eval_kill()
{
	int tmp;
	if( r_cur->universe() == VANILLA ) {
		if( (r_classad->EvalBool( "KILL_VANILLA",
									r_cur->ad(), tmp) ) == 0 ) {  
			if( (r_classad->EvalBool( "KILL",
										r_classad,
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
	if( r_cur->universe() == VANILLA ) {
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
	if( r_cur->universe() == VANILLA ) {
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
	if( r_cur->universe() == VANILLA ) {
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
			// Our current match object might be gone though, so make
			// sure we have the object before we try to use it.
		s = this->state();
		if( s == claimed_state || s == preempting_state ) {
			if( startd_job_exprs && r_cur ) {
				startd_job_exprs->rewind();
				while( (ptr = startd_job_exprs->next()) ) {
					caInsert( cap, r_cur->ad(), ptr );
				}
			}
				// update ImageSize attribute from procInfo
			if (r_pinfo.imgsize) {
				sprintf( line, "%s = %lu", ATTR_IMAGE_SIZE, r_pinfo.imgsize );
				cap->Insert( line );
			}
		}
	}

		// Things you only need in private ads
	if( IS_PRIVATE(mask) && IS_UPDATE(mask) ) {
			// Add currently useful capability.  If r_pre exists, we  
			// need to advertise it's capability.  Otherwise, we
			// should  get the capability from r_cur.
		if( r_pre ) {
			sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_pre->capab() );
			cap->Insert( line );
		} else if( r_cur ) {
			sprintf( line, "%s = \"%s\"", ATTR_CAPABILITY, r_cur->capab() );
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

		// Update info from the current Match object, if it exists.
	if( r_cur ) {
		r_cur->publish( cap, mask );
	}

		// Put in availability statistics
	r_avail_stats.publish( cap, mask );
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
		::dprintf( flags, "%s: ", r_id_str );
		::_condor_dprintf_va( flags | D_NOHEADER, fmt, args );
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
	float avg;
	float max;
	float load;
	int numcpus = resmgr->num_cpus();
	int i;

	if( r_starter->active() ) { 
		time_t now = time(NULL);
		if( now - r_starter->last_snapshot() >= pid_snapshot_interval ) { 
			r_starter->recompute_pidfamily( now );
		}

		if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
			dprintf( D_FULLDEBUG, "Computing CondorLoad w/ pids: " );
			for( i=0; i<r_starter->pidfamily_size(); i++ ) {
				::dprintf( D_FULLDEBUG | D_NOHEADER, "%d ", (r_starter->pidfamily())[i] );	
			}
			::dprintf( D_FULLDEBUG | D_NOHEADER, "\n" );
		}

			// ProcAPI wants a non-const pointer reference, so we need
			// a temporary.
		procInfo *pinfoPTR = &r_pinfo;
		if( (ProcAPI::getProcSetInfo( r_starter->pidfamily(), 
									  r_starter->pidfamily_size(),  
									  pinfoPTR) < -1) ) {
				// If we failed, it might be b/c our pid family has
				// stale info, so before we give up for real,
				// recompute and try once more.
			r_starter->recompute_pidfamily();

			if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
				dprintf( D_FULLDEBUG, "Failed once, now using pids: " );
				for( i=0; i<r_starter->pidfamily_size(); i++ ) {
					::dprintf( D_FULLDEBUG | D_NOHEADER, "%d ", 
							   (r_starter->pidfamily())[i] );	
				}
				::dprintf( D_FULLDEBUG | D_NOHEADER, "\n" );
			}

			if( (ProcAPI::getProcSetInfo( r_starter->pidfamily(), 
										  r_starter->pidfamily_size(),  
										  pinfoPTR) < -1) ) {
				EXCEPT( "Fatal error getting process info for the starter and decendents" ); 
			}
		}
		if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
			dprintf( D_FULLDEBUG, "Percent CPU usage for those pids is: %f\n", 
					 r_pinfo.cpuusage );
		}
		r_load_queue->push( 1, r_pinfo.cpuusage );
	} else {
		r_load_queue->push( 1, 0.0 );
	}
	avg = (r_load_queue->avg() / numcpus);

	if( (DebugFlags & D_FULLDEBUG) && (DebugFlags & D_LOAD) ) {
		r_load_queue->display();
		dprintf( D_FULLDEBUG, 
				 "LoadQueue: Size: %d  Avg value: %f  Share of system load: %f\n", 
				 r_load_queue->size(), r_load_queue->avg(), avg );
	}

	max = MAX( numcpus, resmgr->m_attr->load() );
	load = (avg * max) / 100;
		// Make sure we don't think the CondorLoad on 1 node is higher
		// than the total system load.
	return MIN( load, resmgr->m_attr->load() );
}


void
Resource::resize_load_queue( void )
{
	int size = (int)ceil(60 / polling_interval);
	dprintf( D_FULLDEBUG, "Resizing load queue.  Old: %d, New: %d\n",
			 r_load_queue->size(), size );
	float val = r_load_queue->avg();
	delete r_load_queue;
	r_load_queue = new LoadQueue( size );
	r_load_queue->setval( val );
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
		r_cpu_busy_start_time = time( NULL );
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
			 command_to_string(cmd), state_to_string(s) );
}


void
Resource::log_ignore( int cmd, State s, Activity a ) 
{
	dprintf( D_ALWAYS, "Got %s while in %s/%s state, ignoring.\n", 
			 command_to_string(cmd), state_to_string(s),
			 activity_to_string(a) );
}


void
Resource::log_shutdown_ignore( int cmd ) 
{
	dprintf( D_ALWAYS, "Got %s while shutting down, ignoring.\n", 
			 command_to_string(cmd) );
}


void
Resource::remove_pre( void )
{
	if( r_pre ) {
		if( r_pre->agentstream() ) {
			r_pre->refuse_agent();
		}
		delete r_pre;
		r_pre = NULL;
	}	
}
