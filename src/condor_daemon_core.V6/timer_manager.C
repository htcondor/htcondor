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
#include "condor_debug.h"
#include "condor_daemon_core.h"

static char* DEFAULT_INDENT = "DaemonCore--> ";

static	TimerManager*	_t = NULL;

const time_t TIME_T_NEVER	= 0x7fffffff;


TimerManager::TimerManager()
{
	if(_t)
	{
		EXCEPT("TimerManager object exists!");
	}
	timer_list = NULL;
	timer_ids = 0;
	in_timeout = FALSE;
	_t = this; 
}

TimerManager::~TimerManager()
{
	CancelAllTimers();
}

int TimerManager::NewTimer(Service* s, unsigned deltawhen, Event event, char* event_descrip,
						   unsigned period, int id)
{
	return( NewTimer(s,deltawhen,event,(Eventcpp)NULL,event_descrip,period,id,0) );
}

int TimerManager::NewTimer(unsigned deltawhen, Event event, char* event_descrip,
						   unsigned period, int id)
{
	return( NewTimer((Service *)NULL,deltawhen,event,(Eventcpp)NULL,event_descrip,period,id,0) );
}

int TimerManager::NewTimer(Service* s, unsigned deltawhen, Eventcpp event, char* event_descrip,
						   unsigned period, int id)
{
	if ( !s ) {
		dprintf( D_DAEMONCORE,"DaemonCore NewTimer() called with c++ pointer & NULL Service*\n");
		return -1;
	}
	return( NewTimer(s,deltawhen,(Event)NULL,event,event_descrip,period,id,1) );
}

// Add a new event in the timer list. if period is 0, this event is a one time
// event instead of periodical
int TimerManager::NewTimer(Service* s, unsigned deltawhen, Event event, Eventcpp eventcpp,
		char *event_descrip, unsigned period, int id, int is_cpp)
{
	Timer*		new_timer;
	Timer*		timer_ptr;
	Timer*		trail_ptr;

	dprintf( D_DAEMONCORE, "in DaemonCore NewTimer()\n" );
	new_timer = new Timer;
	if ( new_timer == NULL ) {
		dprintf( D_ALWAYS, "DaemonCore: Unable to allocate new timer\n" );
		return -1;
	}

	new_timer->handler = event;
	new_timer->handlercpp = eventcpp;
	new_timer->period = period;
	new_timer->service = s; 
	new_timer->is_cpp = is_cpp;
	if ( TIMER_NEVER == deltawhen ) {
		new_timer->when = TIME_T_NEVER;
	} else {
		new_timer->when = deltawhen + time(NULL);
	}
	new_timer->data_ptr = NULL;
	if ( event_descrip ) 
		new_timer->event_descrip = strdup(event_descrip);
	else
		new_timer->event_descrip = EMPTY_DESCRIP;

	if(id >= 0)	{
		if ( CancelTimer(id) == -1 ) {
			dprintf( D_DAEMONCORE, "cannot find timer id %d\n",id);
			daemonCore->free_descrip( new_timer->event_descrip);
			delete new_timer;
			return -1;
		}
		new_timer->id = id;
		dprintf(D_DAEMONCORE,"Renewing timer id %d for %d secs\n",id,deltawhen);
	} else {
		new_timer->id = timer_ids++;		
	}


	if ( timer_list == NULL ) {
		// list is empty, place ours in front
		timer_list = new_timer;
		new_timer->next = NULL;
	} else {
		// list is not empty, so keep timer_list ordered from soonest to
		// farthest (i.e. sorted on "when").
		if ( new_timer->when <= timer_list->when ) {
			// make the this new timer first in line
			new_timer->next = timer_list;
			timer_list = new_timer;
		} else {
			for (timer_ptr = timer_list; timer_ptr != NULL; 
				 timer_ptr = timer_ptr->next ) {
				if (new_timer->when <= timer_ptr->when) {
					break;
				}
				trail_ptr = timer_ptr;
			}
			if (timer_ptr == NULL) {
				new_timer->next = NULL;
				trail_ptr->next = new_timer;
			} else {
				new_timer->next = timer_ptr;
				trail_ptr->next = new_timer;
			}
		}
	}

	DumpTimerList(D_DAEMONCORE | D_FULLDEBUG);

	// Update curr_regdataptr for SetDataPtr()
	daemonCore->curr_regdataptr = &(new_timer->data_ptr);

	dprintf(D_DAEMONCORE,"leaving DaemonCore NewTimer, id=%d\n",new_timer->id);

	return	new_timer->id;
}

int TimerManager::ResetTimer(int id, unsigned when, unsigned period)
{
	Timer*			timer_ptr;
	Event			handler;
	Eventcpp		handlercpp;
	Service*		s; 
	int				is_cpp;
	char*			event_descrip;
	void*			data_ptr;
	int				reset_dataptr = FALSE;

	dprintf( D_DAEMONCORE, "In reset_timer(), id=%d, time=%d, period=%d\n",id,when,period);
	if (timer_list == NULL) {
		dprintf( D_DAEMONCORE, "Reseting Timer from empty list!\n");
		return -1;
	}

	// now find the correct timer entry and set timer_ptr to point to it
	if (timer_list->id == id) {
		// timer to remove is the first on the list
		timer_ptr = timer_list;
	} else {
		// timer to remove is not first on the list, so find it
		for (timer_ptr = timer_list; timer_ptr != NULL && timer_ptr->id != id; 
			 timer_ptr = timer_ptr->next) ;

		if ( timer_ptr == NULL ) {
			dprintf( D_DAEMONCORE, "Timer %d not found\n",id );
			return -1;
		}
	}

	handler = timer_ptr->handler;
	handlercpp = timer_ptr->handlercpp;
	s = timer_ptr->service; 
	is_cpp = timer_ptr->is_cpp;
	event_descrip = timer_ptr->event_descrip;
	data_ptr = timer_ptr->data_ptr;
	if ( daemonCore->curr_dataptr == &(timer_ptr->data_ptr) )
		reset_dataptr = TRUE;

	// note that this call to NewTimer() will first call CancelTimer on the tid, 
	// then create a new timer with the same tid.
	if ( NewTimer(s, when, handler, handlercpp, event_descrip, period, 
			id, is_cpp) != -1 ) {
		// and dont forget to renew the users data_ptr
		daemonCore->Register_DataPtr(data_ptr);
		// and if a handler was resetting _its own_ tid entry, reset curr_dataptr to new value
		if ( reset_dataptr == TRUE )
			daemonCore->curr_dataptr = daemonCore->curr_regdataptr;
		// now clear curr_regdataptr; the above NewTimer should appear "transparent"
		// as far as the user code/API is concerned
		daemonCore->curr_regdataptr = NULL;
		// set flag letting Timeout() know if a timer handler reset itself.  note
		// this is probably redundant since our call to NewTimer above called
		// CancelTimer, and CancelTimer already set the did_reset flag.  But
		// what the hell.
		if ( did_reset == id )
			did_reset = -1;
		// DumpTimerList(D_DAEMONCORE | D_FULLDEBUG);
		return 0;
	} else {
		return -1;
	}
}

int TimerManager::CancelTimer(int id)
{
	Timer*			timer_ptr;
	Timer*			trail_ptr;

	dprintf( D_DAEMONCORE, "In cancel_timer(), id=%d\n",id);
	if (timer_list == NULL) {
		dprintf( D_DAEMONCORE, "Removing Timer from empty list!\n");
		return -1;
	}

	// now find the correct timer entry and set timer_ptr to point to it
	if (timer_list->id == id) {
		// timer to remove is the first on the list
		if (timer_list->next == NULL) {
			// timer to remove is first and is the only one on the list
			timer_ptr = timer_list;
			timer_list = NULL;			
		} else {
			// timer to remove is first but not the only one
			timer_ptr = timer_list;
			timer_list = timer_list->next;
		}
	} else {
		// timer to remove is not first on the list, so find it
		for (timer_ptr = timer_list; timer_ptr != NULL && timer_ptr->id != id; 
			 timer_ptr = timer_ptr->next) {
			trail_ptr = timer_ptr;
		}

		if ( timer_ptr == NULL ) {
			dprintf( D_ALWAYS, "Timer %d not found\n",id );
			return -1;
		}

		// remove from the linked list
		trail_ptr->next = timer_ptr->next;
	}

	// free event_descrip
	if ( timer_ptr->event_descrip ) {
		daemonCore->free_descrip( timer_ptr->event_descrip);
		timer_ptr->event_descrip = 0;
	}

	// set curr_dataptr to NULL if a handler is removing itself. 
	if ( daemonCore->curr_dataptr == &(timer_ptr->data_ptr) )
		daemonCore->curr_dataptr = NULL;
	if ( daemonCore->curr_regdataptr == &(timer_ptr->data_ptr) )
		daemonCore->curr_regdataptr = NULL;


	delete timer_ptr;
	
	// set flag letting Timeout() know if a timer handler reset itself
	if ( did_reset == id ) {
		did_reset = -1;
	}

	return 0;
}

void TimerManager::CancelAllTimers()
{
	Timer	*timer_ptr;

	while( timer_list != NULL ) {
		timer_ptr = timer_list;
		timer_list = timer_list->next;
		daemonCore->free_descrip( timer_ptr->event_descrip);
		delete timer_ptr;
	}
	timer_list = NULL;
}

// Timeout() is called when a select() time out.  Returns number of seconds
// until next timeout event, a 0 if there are no more events, or a -1 if
// called while a handler is active (i.e. handler calls Timeout;
// Timeout is not re-entrant).
int
TimerManager::Timeout()
{
	int				current_id;
	unsigned		period;						// for periodic events
	Event			handler;
	Eventcpp		handlercpp;
	Service*		s; 
	int				result;
	time_t			now;
	int				is_cpp;
	char*			event_descrip;
	void*			data_ptr;

	if ( in_timeout == TRUE ) {
		dprintf(D_DAEMONCORE,"DaemonCore Timeout() called and in_timeout is TRUE\n");
		if ( timer_list == NULL )
			result = 0;
		else
			result = (timer_list->when) - time(NULL);
		return(result);
	}
	in_timeout = TRUE;
		
	dprintf( D_DAEMONCORE, "In DaemonCore Timeout()\n");

	if (timer_list == NULL) {
		dprintf( D_DAEMONCORE, "Empty timer list, nothing to do\n" );
	}

	time(&now);

	DumpTimerList(D_DAEMONCORE | D_FULLDEBUG);

	// loop until all handlers that should have been called by now or before
	// are invoked and renewed if periodic.  Remember that NewTimer and CancelTimer
	// keep the timer_list happily sorted on "when" for us.  We use "now" as a 
	// variable so that if some of these handler functions run for a long time,
	// we do not sit in this loop forever.
	while( timer_list != NULL && (timer_list->when <= now ) ) {
		// DumpTimerList(D_DAEMONCORE | D_FULLDEBUG);
		current_id = timer_list->id;
		period = timer_list->period;
		handler = timer_list->handler;
		handlercpp = timer_list->handlercpp;
		s = timer_list->service; 
		is_cpp = timer_list->is_cpp;
		event_descrip = strdup(timer_list->event_descrip);
		data_ptr = timer_list->data_ptr;

		// Update curr_dataptr for GetDataPtr()
		daemonCore->curr_dataptr = &(timer_list->data_ptr);

		// Initialize our flag so we know if ResetTimer was called.
		did_reset = current_id;

		// Log a message before calling handler
		dprintf(D_DAEMONCORE,"DaemonCore: Calling handler for Timer %d (%s)\n",
			current_id, event_descrip);

		// Now we call the registered handler.  If we were told that the handler
		// is a c++ method, we call the handler from the c++ object referenced 
		// by service*.  If we were told the handler is a c function, we call
		// it and pass the service* as a parameter.
		if ( timer_list->is_cpp ) {
			(s->*handlercpp)();			// typedef int (*Eventcpp)(int)
		} else {
			(*handler)(s);				// typedef int (*Event)(Service*,int)
		}

		// Make sure we didn't leak our priv state
		daemonCore->CheckPrivState();

		// Clear curr_dataptr
		daemonCore->curr_dataptr = NULL;

		/* Watch out for cancel_timer() called in the users handler with only 
		 * one item in list which makes timer_list go to NULL */
		if (timer_list != NULL  && did_reset == current_id) {
			// here we remove the timer we just serviced, or renew it if it is 
			// periodic.  note calls to CancelTimer are safe even if the user's
			// handler called CancelTimer as well...
			if ( period > 0 ) {
				// timer is periodic, renew it.  note that this call to NewTimer() 
				// will first call CancelTimer on the expired timer, then renew it.
				if ( NewTimer(s, period, handler, handlercpp, event_descrip, period, 
						current_id, is_cpp) != -1 ) {
					// and dont forget to renew the users data_ptr
					daemonCore->Register_DataPtr(data_ptr);
					// now clear curr_dataptr; the above NewTimer should appear "transparent"
					// as far as the user code/API is concerned
					daemonCore->curr_dataptr = NULL;
				}
			} else {
				// timer is not perodic; it is just a one-time event.  we just called
				// the handler, so now remove the timer from out list.  
				CancelTimer(current_id);
			}
		}
		free( event_descrip );
	}  // end of while loop


	// set result to number of seconds until next event.  get an update on the
	// time from time() in case the handlers we called above took significant time.
	if ( timer_list == NULL ) {
		// we set result to be -1 so that we do not busy poll.
		// a -1 return value will tell the DaemonCore:Driver to use select with
		// no timeout.
		result = -1;
	} else {
		result = (timer_list->when) - time(NULL);
		if (result < 0)
			result = 0;
	}

	dprintf( D_DAEMONCORE, "DaemonCore Timeout() Complete, returning %d \n",result);
	in_timeout = FALSE;
	return(result);
}


void TimerManager::DumpTimerList(int flag, char* indent)
{
	Timer	*timer_ptr;
	char	*ptmp;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just 
	// flag & DebugFlags > 0 ), so our own check here:
	if ( (flag & DebugFlags) != flag )
		return;

	if ( indent == NULL) 
		indent = DEFAULT_INDENT;

	dprintf(flag, "\n");
	dprintf(flag, "%sTimers\n", indent);
	dprintf(flag, "%s~~~~~~\n", indent);
	for(timer_ptr = timer_list; timer_ptr != NULL; timer_ptr = timer_ptr->next)
	{
		if ( timer_ptr->event_descrip )
			ptmp = timer_ptr->event_descrip;
		else
			ptmp = "NULL";

		dprintf(flag, "%sid = %d, when = %d, period = %d, handler_descrip=<%s>\n", indent,
			 	timer_ptr->id, timer_ptr->when, timer_ptr->period,ptmp);
	}
	dprintf(flag, "\n");
}

void TimerManager::Start()
{
	struct timeval		timer;
	int					rv;

	for(;;)
	{
		// NOTE: on Linux we need to re-initialize timer
		// everytime we call select().  We might need it on
		// other platforms as well, so we leave it here for
		// everyone.

		// Timeout() also returns how many seconds until next 
		// event, as well as calls any event handlers whose time has come
		timer.tv_sec = Timeout();  
		timer.tv_usec = 0;
		if ( timer.tv_sec == 0 ) {
			// no timer events registered...  only a signal
			// can save us now!!
			dprintf(D_DAEMONCORE,"TimerManager::Start() about to block with no events!\n");
			rv = select(0,0,0,0,NULL);
		} else {
			dprintf(D_DAEMONCORE,"TimerManager::Start() about to block, timeout=%d\n",
				timer.tv_sec);
			rv = select(0,0,0,0, &timer);
		}		
	}
}
