#include "startd.h"
static char *_FileName_ = __FILE__;

ResState::ResState( Resource* rip )
{
	r_load_q = new LoadQueue(60);
	r_state = owner_state;
	r_act = idle_act;
	atime = (int)time(NULL);
	stime = atime;
	this->rip = rip;
}


ResState::~ResState()
{
	delete r_load_q;
}


void
ResState::update( ClassAd* cp ) 
{
	char tmp[80];

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
	int statechange = FALSE, actchange = FALSE, now;

	if( new_state != r_state ) {
		statechange = TRUE;
	}
	if( new_act != r_act ) {
		actchange = TRUE;
	}
	if( ! (actchange || statechange) ) {
		return TRUE;   // If we're not changing anything, return
	}

		// leave_action and enter_action return TRUE if they result in
		// a state or activity change.  In these cases, we want to
		// abort the current state change.
	if( leave_action( r_state, r_act, statechange, actchange ) ) {
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
		r_state = new_state;
	}
	if( actchange ) {
		load_activity_change();
		r_act = new_act;
		atime = now;
	}

	if( enter_action( r_state, r_act, statechange, actchange ) ) {
		return TRUE;
	}
	
		// Note our current state and activity in the classad
	this->update( rip->r_classad );

		// We want to update the CM on every state or activity change
	rip->update();   

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
		if( rip->wants_vacate() ) {
			return change( new_state, vacating_act );
		} else {
			return change( new_state, killing_act );
		}
	} else {
		return change( new_state, idle_act );
	}
}


int
ResState::eval()
{
		// Recompute attributes needed at every timeout and refresh classad 
	rip->timeout_classad();

	int want_suspend, want_vacate;

	switch( r_state ) {

	case claimed_state:
		want_suspend = rip->wants_suspend();
		want_vacate = rip->wants_vacate();
		if( ((r_act == busy_act) && (!want_suspend)) ||
			(r_act == suspended_act) ) {
					// STATE TRANSITION #15 or #16
			if( want_vacate && rip->eval_vacate() ) {
				return change( preempting_state, vacating_act ); 
			}
			if( !want_vacate && rip->eval_kill() ) {
				return change( preempting_state, killing_act );  
			}
		}
		if( (r_act == busy_act) && want_suspend ) {
			if( rip->eval_suspend() ) {
				// STATE TRANSITION #12
				return change( suspended_act );
			}
		}
		if( r_act == suspended_act ) {
			if( rip->eval_continue() ) {
				// STATE TRANSITION #13
				return change( busy_act );
			}
		}
		if( (r_act == idle_act) && (rip->r_reqexp->eval() == 0) ) {
				// STATE TRANSITION #14
			return change( preempting_state ); 
		}
		if( (r_act == busy_act) && (rip->wants_pckpt()) ) {
			rip->periodic_checkpoint();
		}
		break;   // case claimed_state:

	case preempting_state:
		if( r_act == vacating_act ) {
			if( rip->eval_kill() ) {
					// STATE TRANSITION #18
				return change( killing_act );
			}
		}
		break;	// case preempting_state:

	case unclaimed_state:
		// See if we should be owner or unclaimed
		if( rip->r_reqexp->eval() == 0 ) {
			return change( owner_state );
		}
			// Check to see if we should run benchmarks
		deal_with_benchmarks( rip );
		break;	

	case owner_state:
		if( rip->r_reqexp->eval() != 0 ) {
			change( unclaimed_state );
		}
		break;	
		
	case matched_state:
		if( rip->r_reqexp->eval() == 0 ) {
				// STATE TRANSITION #8
			change( owner_state );
		}
		break;

	default:
		EXCEPT( "eval_state: ERROR: unknown state (%d)",
				(int)rip->r_state );
	}
	dprintf( D_FULLDEBUG, "State: %s\tActivity: %s\n", 
			 state_to_string(r_state),
			 activity_to_string(r_act) );
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
	case vacating_act:
	case killing_act:
		return 1;
		break;
	default:
		EXCEPT( "Unknown activity in act_to_load" );
	}
	return -1;
}


// This function is called on every activity change.  It's purpose is
// to keep the load_q array up to date by pushing a 0 or 1 onto the
// queue for every second we've been in the previous activity.  
void
ResState::load_activity_change() 
{
	int now		=	(int) time(NULL);
	int delta	= 	now - atime;
	int load	=	act_to_load( r_act );

	if( delta < 1 ) {
		delta = 1;
	}
	if( delta >= 60 ) {
		r_load_q->setval( (char)load );
	} else {
		r_load_q->push( delta, (char)load );
	}
}


float
ResState::condor_load()
{
	int now		=	(int) time(NULL);
	int delta	= 	now - atime;
	int load	=	act_to_load( r_act );
	int val;

	if( delta >= 60 ) {
			// Easy: Condor load is just 1 or 0 depending on previous
			// activity.
		return (float)load;
	} 

	if( delta < 1 ) {
		delta = 1;
	}
		// Hard: Need to use the load queue to determine average
		// over last minute.
	val = r_load_q->val( 60 - delta );
	val += ( load * delta );
	return ( (float)val / 60 );
}


LoadQueue::LoadQueue( int q_size )
{
	size = q_size;
	head = 0;
	buf = new char[size];
	this->setval( (char)0 );
}


LoadQueue::~LoadQueue()
{
	delete [] buf;
}


// Return the average value of the queue
float
LoadQueue::avg()
{
	int i, val = 0;
	for( i=0; i<size; i++ ) {
		val += buf[i];
	}
	return( (float)val/size );
}


// Return the sum of the values of the first num elements.
int
LoadQueue::val( int num )
{
	int i, j, val = 0, delta = size - num, foo;
		// delta is how many elements we need to skip over to get to
		// the values we care about.  If we were asked for more
		// elements than the size of our array, we need to return the
		// sum of all values, i.e., don't skip anything.
	if( delta < 0 ) {
		delta = 0;
		num = size;	
	}
	foo = head + delta;
	for( i=0; i<num; i++ ) {
		j = (foo + i) % size;
		val += buf[j];
	}
	return val;
}


// Push num elements onto the array with the given value.
void
LoadQueue::push( int num, char val ) 
{
	int i, j;
	if( num > size ) {
		num = size;
	}
	for( i=0; i<num; i++ ) {
		j = (head + i) % size;
		buf[j] = val;
	}
	head = (head + num) % size;
}


// Set all elements of the array to have the given value.
void
LoadQueue::setval( char val ) 
{
	memset( (void*)buf, (int)val, (size*sizeof(char)) );
		// Reset the head, too.
	head = 0;
}


int
ResState::leave_action( State s, Activity a, 
						int statechange, int ) 
{
	ClassAd* cp;
	switch( s ) {

	case preempting_state:
		if( statechange ) {
			cp = rip->r_classad;
			cp->Delete( ATTR_CLIENT_MACHINE );
			cp->Delete( ATTR_REMOTE_USER );
			cp->Delete( ATTR_JOB_START );
			cp->Delete( ATTR_JOB_ID );
			cp->Delete( ATTR_JOB_UNIVERSE );
			cp->Delete( ATTR_LAST_PERIODIC_CHECKPOINT );
		}
		break;
	case matched_state:
	case owner_state:
	case unclaimed_state:
		break;
	case claimed_state:
		if( a == suspended_act ) {
			if( rip->r_starter->kill( DC_SIGCONTINUE ) < 0 ) {
					// If there's an error sending kill, it could only
					// mean the starter has blown up and we didn't
					// know about it.  Send SIGKILL to the process
					// group and go to the owner state.
				rip->r_starter->killpg( DC_SIGKILL );
				return change( owner_state );
			}
		}
		if( statechange ) {
			rip->r_cur->cancel_claim_timer();	
		}
		break;
	default:
		EXCEPT("Unknown state in ResState::leave_action");
	}

	return FALSE;
}


int
ResState::enter_action( State s, Activity a,
						int statechange, int ) 
{
	switch( s ) {
	case owner_state:
		rip->cancel_poll_timer();
			// Always want to create new match objects
		if( rip->r_cur ) {
			delete( rip->r_cur );
		}
		rip->r_cur = new Match;
		if( rip->r_pre ) {
			delete rip->r_pre;
			rip->r_pre = NULL;
		}
			// See if we should be in owner or unclaimed state
		if( rip->r_reqexp->eval() != 0 ) {
				// Really want to be in unclaimed.
			return change( unclaimed_state );
		}
		rip->r_reqexp->unavail();		
		break;

	case claimed_state:
		rip->r_reqexp->pub();			
		if( statechange ) {
			rip->start_poll_timer();
			rip->r_cur->start_claim_timer();	
				// Update important attributes into the classad.
			rip->r_cur->update( rip->r_classad );
				// Generate a preempting match object
			rip->r_pre = new Match;
		}
		if( a == suspended_act ) {
			if( rip->r_starter->kill( DC_SIGSUSPEND ) < 0 ) {
				rip->r_starter->killpg( DC_SIGKILL );
				return change( owner_state );
			}
		}
		break;

	case unclaimed_state:
		rip->r_reqexp->pub();
		break;

	case matched_state:
		rip->r_reqexp->unavail();
		break;

	case preempting_state:
		rip->r_reqexp->unavail();
		switch( a ) {
		case killing_act:
			if( rip->r_starter->active() ) {
				if( ! rip->hardkill_starter() ) {
						// hardkill_starter returns FALSE if there was
						// an error in kill and we had to send SIGKILL
						// to the starter's process group.
					return change( owner_state );
				}
			} else {
				rip->leave_preempting_state();
				return TRUE;
			}
			break;

		case vacating_act:
			if( rip->r_starter->active() ) {
				if( rip->r_starter->kill( DC_SIGSOFTKILL ) < 0 ) {
					rip->r_starter->killpg( DC_SIGKILL );
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

	default: 
		EXCEPT("Unknown state in ResState::enter_action");
	}
	return FALSE;
}


