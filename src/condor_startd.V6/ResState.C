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
ResState::change( State new_state )
{
	int val;

	if( new_state == r_state ) {
		return TRUE;
	}

#if 0
	if( ! is_valid( new_state ) ) {
		print_error( new_state );
		return FALSE;
	} 
#endif
	dprintf( D_FULLDEBUG, "Changing state: %s -> %s\n",
			 state_to_string(r_state), 
			 state_to_string(new_state) );

	r_state = new_state;
	stime = (int)time( NULL );

	switch( new_state ) {
	case owner_state:
			// Create a new match object.
		delete rip->r_cur;
		rip->r_cur = new Match;
		if( rip->r_pre ) {
			delete rip->r_pre;
			rip->r_pre = NULL;
		}

			// See if we should be in owner or unclaimed state
		val = rip->r_reqexp->eval();
		if( val == 0 ) {
				// Want to be in owner
			rip->r_reqexp->unavail();		// Sets requirements to false
			this->change( idle_act );
			break;
		} else {
				// Really want to be in unclaimed.
			return this->change( unclaimed_state );
		}
	case unclaimed_state:
		rip->r_reqexp->pub();
		this->change( idle_act );
		break;
	case matched_state:
		rip->r_reqexp->unavail();		// Sets requirements to false
		this->change( idle_act );
		break;
	case claimed_state:
		rip->r_reqexp->pub();			
		this->change( idle_act );
			// Start a timer in case we don't get a keep alive for
			// this claim 
		rip->r_cur->start_claim_timer();	
		break;
	case preempting_state:
		rip->r_reqexp->unavail();		// Sets requirements to false
		if( wants_vacate(rip) ) {
			vacate_claim( rip );
		} else {
				// Don't vacate, go directly to kill
			kill_claim( rip );
		}
		break;
	default:
		EXCEPT( "unknown state in ResState::change" );
	}

		// Note our current state and activity in the classad
	this->update( rip->r_classad );

		// Since we did a state change, we need to update the Central
		// Manager for this resource.
	rip->update();

		// If Condor is using this resource, i.e. claimed, matched or
		// preempting states, we want to poll the resource and
		// evaluate it's state frequently (POLLING_INTERVAL from the
		// config file, defaults to 5 seconds).  Otherwise, we only
		// need to evaluate the state when we're going to do an
		// update. 
	if( rip->in_use() ) {
		rip->start_poll_timer();
	} else {
		rip->cancel_poll_timer();
	}

	return TRUE;
}


int
ResState::change( Activity new_act )
{
	if( r_act == new_act ) {
		return TRUE;
	}

#if 0
	if( ! is_valid( new_act ) ) {
		print_error( new_act );
		return FALSE;
	} 
#endif 0

	dprintf( D_FULLDEBUG, "Changing activity: %s -> %s\n",
			 activity_to_string(r_act), 
			 activity_to_string(new_act) );
	
	load_activity_change();

	r_act = new_act;
	atime = (int)time( NULL );

		// Update our current state and activity in the classad
	this->update( rip->r_classad );

	return TRUE;
}


int
ResState::eval()
{
		// Recompute attributes needed at every timeout and refresh classad 
	rip->timeout_classad();

	int want_suspend;

	switch( r_state ) {

	case claimed_state:
		want_suspend = wants_suspend( rip );
		if( ((r_act == busy_act) && (!want_suspend)) ||
			(r_act == suspended_act) ) {
			if( eval_vacate( rip ) || eval_kill( rip ) ) {
					// STATE TRANSITION #15 or #16
				return release_claim( rip );
			}
		}
		if( (r_act == busy_act) && want_suspend ) {
			if( eval_suspend( rip ) ) {
				// STATE TRANSITION #12
				return suspend_claim( rip );
			}
		}
		if( r_act == suspended_act ) {
			if( eval_continue( rip ) ) {
				// STATE TRANSITION #13
				return continue_claim( rip );
			}
		}
		if( (r_act == idle_act) && (rip->r_reqexp->eval() == 0) ) {
				// STATE TRANSITION #14
			return release_claim( rip );
		}
		break;   // case claimed_state:

	case preempting_state:
		if( r_act == vacating_act ) {
			if( eval_kill( rip ) ) {
					// STATE TRANSITION #18
				return kill_claim( rip );
			}
		}
		if( r_act == idle_act ) {
			ClassAd* cp = rip->r_classad;
			cp->Delete( ATTR_CLIENT_MACHINE );
			cp->Delete( ATTR_REMOTE_USER );
			cp->Delete( ATTR_JOB_START );
			cp->Delete( ATTR_JOB_ID );
			cp->Delete( ATTR_JOB_UNIVERSE );
			cp->Delete( ATTR_LAST_PERIODIC_CHECKPOINT );
			delete rip->r_cur;
			rip->r_cur = NULL;

				// In english:  "If the owner wants the machine
				// back, or there's no one waiting..."
			if( (rip->r_reqexp->eval() == 0) ||
				( ! (rip->r_pre && rip->r_pre->agentstream()) ) ) {

					// STATE TRANSITION #21
				rip->r_cur = new Match;
				if( rip->r_pre ) {
					rip->r_pre->vacate();
					delete rip->r_pre;
					rip->r_pre = NULL;
				}
				return rip->r_state->change( owner_state );
			}
			if( rip->r_pre && rip->r_pre->agentstream() ) {
				rip->r_cur = rip->r_pre;
				rip->r_pre = NULL;
					// STATE TRANSITION #20
				accept_request_claim( rip );
			} 
		}
		break;	// case preempting_state:

	case unclaimed_state:
		// See if we should be owner or unclaimed
		if( rip->r_reqexp->eval() == 0 ) {
			this->change( owner_state );
		}
			// Check to see if we should run benchmarks
		deal_with_benchmarks( rip );
		break;	

	case owner_state:
		if( rip->r_reqexp->eval() != 0 ) {
			this->change( unclaimed_state );
		}
		break;	
		
	case matched_state:
		if( rip->r_reqexp->eval() == 0 ) {
				// STATE TRANSITION #8
			this->change( owner_state );
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

//////////////////////////////////////////////////////////////////////
//  DEAD CODE
//////////////////////////////////////////////////////////////////////
#if 0
bool
ResState::is_valid( State new_state )
{
	if( r_state == new_state ) {
		return true;
	}

	switch( r_state ) {
	case owner_state:
		assert( r_act == idle_act );
		if( new_state == unclaimed_state ) {
			return true;
		} else {
			return false;
		}
	case unclaimed_state:
		if( r_act != idle_act ) {
			return false;
		}
		switch( new_state ) {
		case matched_state:
		case claimed_state:
			return true;
		default:
			return false;
		}
	case matched_state:
		assert( r_act == idle_act );
		switch( new_state ) {
		case owner_state:
		case claimed_state:
			return true;
		default:
			return false;
		}
	case claimed_state:
		if( new_state == preempting_state ) {
			return true;
		} else {
			return false;
		}
	case preempting_state:
		if(	r_act != idle_act ) {
			return false;
		}
		switch( new_state ) {
		case claimed_state:
		case owner_state:
			return true;
		default:
			return false;
		}
	}			
}


bool
ResState::is_valid( Activity new_act )
{
	if( r_act == new_act ) {
		return true;
	}

	switch( r_state ) {
	case owner_state:
		if( new_act == idle_act) {
			return true;
		} else {
			return false;
		}
	case unclaimed_state:
		if( (r_act == idle_act && new_act == benchmarking_act) ||
			(r_act == benchmarking_act && new_act == idle_act) ) {
			return true;
		} else {
			return false;
		}
	case matched_state:
		if( new_act == idle_act) {
			return true;
		} else {
			return false;
		}
	case claimed_state:
		switch( r_act ) {
		case idle_act:
		case suspended_act:
			if( new_act == busy_act ) {
				return true;
			} else {
				return false;
			}
		case busy_act:
			if( new_act == idle_act || new_act == suspended_act ) {
				return true;
			} else {
				EXCEPT( "unknown activity in claimed state" );
			}
		default: 
			EXCEPT( "unknown activity in claimed state" );
		}
	case preempting_state:
		switch( r_act ) {
		case killing_act:
			if( new_act == idle_act ) {
				return true;
			} else {
				return false;
			}
		case vacating_act:
			if( new_act == idle_act || new_act == killing_act ) {
				return true;
			} else {
				EXCEPT( "unknown activity in preempting state" );
			}
		case idle_act:
			return false;
		default: 
			EXCEPT( "unknown activity in preempting state" );
		}
	}
}


void
ResState::print_error( State new_state )
{
	dprintf( D_ALWAYS, "Illegal state/activity change.\n" );
	dprintf( D_ALWAYS, "current state/activity: %s / %s.\n",
			 state_to_string(r_state),
			 activity_to_string(r_act) );
	dprintf( D_ALWAYS, "requested state: %s.\n", 
			 state_to_string(new_state) );
}


void
ResState::print_error( Activity new_act )
{
	dprintf( D_ALWAYS, "Illegal state/activity change.\n" );
	dprintf( D_ALWAYS, "current state/activity: %s / %s.\n",
			 state_to_string(r_state),
			 activity_to_string(r_act) );
	dprintf( D_ALWAYS, "requested activity: %s.\n", 
			 activity_to_string(new_act) );
}

#endif

