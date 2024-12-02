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
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include <unordered_set>

static	TimerManager*	_t = NULL;

extern void **curr_dataptr;
extern void **curr_regdataptr;

// disable warning about memory leaks due to exception. all memory freed on exit anyway
MSC_DISABLE_WARNING(6211)

TimerManager &
TimerManager::GetTimerManager()
{
	if (!_t)
	{
		_t = new TimerManager();
	}
	return *_t;
}

TimerManager::TimerManager()
{
	if(_t)
	{
		EXCEPT("TimerManager object exists!");
	}
	timer_list = NULL;
	list_tail = NULL;
	timer_ids = 0;
	in_timeout = NULL;
	_t = this; 
	did_reset = false;
	did_cancel = false;
    max_timer_events_per_cycle = INT_MAX;
}

TimerManager::~TimerManager()
{
	CancelAllTimers();
}

void TimerManager::reconfig()
{
    max_timer_events_per_cycle = param_integer("MAX_TIMER_EVENTS_PER_CYCLE");
    if (max_timer_events_per_cycle < 1) {
        max_timer_events_per_cycle = INT_MAX;
    }
}

int TimerManager::NewTimer (const Timeslice &timeslice,StdTimerHandler handler,const char * event_descrip)
{
	return NewTimer(nullptr,0,event_descrip,0,&timeslice, &handler);
}

int TimerManager::NewTimer( time_t deltawhen, time_t period, StdTimerHandler f, const char * event_description )
{
	return NewTimer( nullptr, deltawhen,
		event_description, period,
		nullptr, & f
	);
}

// Add a new event in the timer list. if period is 0, this event is a one time
// event instead of periodical
int TimerManager::NewTimer(Service* s, time_t deltawhen,
						   const char *event_descrip, time_t period,
						   const Timeslice *timeslice,
						   StdTimerHandler * f)
{
	Timer*		new_timer;

	new_timer = new Timer;
	if ( new_timer == NULL ) {
		dprintf( D_ALWAYS, "DaemonCore: Unable to allocate new timer\n" );
		return -1;
	}

	if (daemonCore && event_descrip) {
		daemonCore->dc_stats.NewProbe("Timer", event_descrip, AS_COUNT | IS_RCT | IF_NONZERO | IF_VERBOSEPUB);
	}

	if( f != NULL ) {
		new_timer->std_handler = * f;
	}
	new_timer->period = period;
	new_timer->service = s;

	if( timeslice ) {
		new_timer->timeslice = new Timeslice( *timeslice );
		deltawhen = new_timer->timeslice->getTimeToNextRun();
	}
	else {
		new_timer->timeslice = NULL;
	}

	new_timer->period_started = time(NULL);
	if ( TIMER_NEVER == deltawhen ) {
		new_timer->when = TIMER_NEVER;
	} else {
		new_timer->when = deltawhen + new_timer->period_started;
	}
	new_timer->data_ptr = NULL;
	if ( event_descrip )
		new_timer->event_descrip = strdup(event_descrip);
	else
		new_timer->event_descrip = strdup("<NULL>");


	new_timer->id = timer_ids++;


	InsertTimer( new_timer );

	DumpTimerList(D_DAEMONCORE | D_FULLDEBUG);

	// Update curr_regdataptr for SetDataPtr()
	curr_regdataptr = &(new_timer->data_ptr);

	dprintf(D_DAEMONCORE,"leaving DaemonCore NewTimer, id=%d\n",new_timer->id);

	return	new_timer->id;
}

int TimerManager::ResetTimerPeriod(int id,time_t   period)
{
	return ResetTimer(id,0,period,true);
}

bool TimerManager::ResetTimerTimeslice(int id, Timeslice const &new_timeslice)
{
	return ResetTimer(id,0,0,false,&new_timeslice)==0;
}

bool TimerManager::GetTimerTimeslice(int id, Timeslice &timeslice)
{
	Timer *timer_ptr = GetTimer( id, NULL );
	if( !timer_ptr || !timer_ptr->timeslice ) {
		return false;
	}
	timeslice = *timer_ptr->timeslice;
	return true;
}

time_t TimerManager::GetNextRuntime(int id)
{
	Timer *timer_ptr = GetTimer( id, NULL );
	if (!timer_ptr) { return false; }

	return timer_ptr->when;
}
int TimerManager::ResetTimer(int id, time_t deltawhen, time_t   period,
							 bool recompute_when,
							 Timeslice const *new_timeslice)
{
	Timer*			timer_ptr;
	Timer*			trail_ptr;

	dprintf( D_DAEMONCORE,
			 "In reset_timer(), id=%d, delay=%lld, period=%lld\n",id,(long long)deltawhen,(long long)period);
	if (timer_list == nullptr) {
		dprintf( D_DAEMONCORE, "Reseting Timer from empty list!\n");
		return -1;
	}

	timer_ptr = timer_list;
	trail_ptr = nullptr;
	while ( timer_ptr && timer_ptr->id != id ) {
		trail_ptr = timer_ptr;
		timer_ptr = timer_ptr->next;
	}

	if ( timer_ptr == nullptr ) {
		dprintf( D_ALWAYS, "Timer %d not found\n",id );
		return -1;
	}
	if ( new_timeslice ) {
		if( timer_ptr->timeslice == NULL ) {
			timer_ptr->timeslice = new Timeslice( *new_timeslice );
		}
		else {
			*timer_ptr->timeslice = *new_timeslice;
		}

		timer_ptr->when = timer_ptr->timeslice->getNextStartTime();
	}
	else if ( timer_ptr->timeslice ) {
		dprintf( D_DAEMONCORE, "Timer %d with timeslice can't be reset\n",
				 id );
		return 0;
	} else if( recompute_when ) {
		time_t old_when = timer_ptr->when;

		if( period == TIMER_NEVER ) {
			timer_ptr->when = TIMER_NEVER;
		} else {
			timer_ptr->when = timer_ptr->period_started + period;
		}

			// sanity check
		int64_t wait_time = timer_ptr->when - time(nullptr);
		if( wait_time > period) {
			dprintf(D_ALWAYS,
					"ResetTimer() tried to set next call to %d (%s) %llds into"
					" the future, which is larger than the new period %lld.\n",
					id,
					timer_ptr->event_descrip ? timer_ptr->event_descrip : "",
					(long long) wait_time,
					(long long) period);

				// start a new period now to restore sanity
			timer_ptr->period_started = time(NULL);
			timer_ptr->when = timer_ptr->period_started + period;
		}

		dprintf(D_FULLDEBUG,
				"Changing period of timer %d (%s) from %lld to %lld "
				"(added %llds to time of next scheduled call)\n",
				id, 
				timer_ptr->event_descrip ? timer_ptr->event_descrip : "",
				(long long)timer_ptr->period,
				(long long)period,
				(long long)(timer_ptr->when - old_when));
	} else {
		timer_ptr->period_started = time(nullptr);
		if ( deltawhen == TIMER_NEVER ) {
			timer_ptr->when = TIMER_NEVER;
		} else {
			timer_ptr->when = deltawhen + timer_ptr->period_started;
		}
	}
	timer_ptr->period = period;

	RemoveTimer( timer_ptr, trail_ptr );
	InsertTimer( timer_ptr );

	if ( in_timeout == timer_ptr ) {
		// We're inside the handler for this timer. Let Timeout() know
		// the timer has already been reset for its next call.
		did_reset = true;
	}

	return 0;
}

int TimerManager::CancelTimer(int id)
{
	Timer*		timer_ptr;
	Timer*		trail_ptr;

	dprintf( D_DAEMONCORE, "In cancel_timer(), id=%d\n",id);
	if (timer_list == NULL) {
		dprintf( D_DAEMONCORE, "Removing Timer from empty list!\n");
		return -1;
	}

	timer_ptr = timer_list;
	trail_ptr = NULL;
	while ( timer_ptr && timer_ptr->id != id ) {
		trail_ptr = timer_ptr;
		timer_ptr = timer_ptr->next;
	}

	if ( timer_ptr == NULL ) {
		dprintf( D_ALWAYS, "Timer %d not found\n",id );
		return -1;
	}

	RemoveTimer( timer_ptr, trail_ptr );

	if ( in_timeout == timer_ptr ) {
		// We're inside the handler for this timer. Don't delete it,
		// since Timeout() still needs it. Timeout() will delete it once
		// it's done with it.
		did_cancel = true;
	} else {
		DeleteTimer( timer_ptr );
	}

	return 0;
}

void TimerManager::CancelAllTimers()
{
	Timer		*timer_ptr;

	while( timer_list != NULL ) {
		timer_ptr = timer_list;
		timer_list = timer_list->next;
		if( in_timeout == timer_ptr ) {
				// We get here if somebody calls exit from inside a timer.
			did_cancel = true;
		}
		else {
			DeleteTimer( timer_ptr );
		}
	}
	timer_list = NULL;
	list_tail = NULL;
}

// Timeout() is called when a select() time out.  Returns number of seconds
// until next timeout event, a 0 if there are no more events, or a -1 if
// called while a handler is active (i.e. handler calls Timeout;
// Timeout is not re-entrant).
time_t
TimerManager::Timeout(int * pNumFired /*= NULL*/, double * pruntime /*=NULL*/)
{
	time_t			result;
	time_t			now;
	int				num_fires = 0;	// num of handlers called in this timeout

    if (pNumFired) *pNumFired = 0;

	if ( in_timeout != NULL ) {
		dprintf(D_DAEMONCORE,"DaemonCore Timeout() called and in_timeout is non-NULL\n");
		if ( timer_list == NULL ) {
			result = 0;
		} else {
			result = (timer_list->when) - time(NULL);
		}
		if ( result < 0 ) {
			result = 0;
		}
		return(result);
	}
		
	if (timer_list == NULL) {
		dprintf( D_DAEMONCORE, "Empty timer list, nothing to do\n" );
	}

	time(&now);

	DumpTimerList(D_DAEMONCORE | D_FULLDEBUG);

    // if we are going to not limit the number of timer handlers we invoke,
    // make a set now of all timers that are ready to go... below we will
    // use this set in order to NOT invoke new timers that are inserted by
    // timer handlers themselves.
    std::unordered_set<int> readyTimerIds;
    if (max_timer_events_per_cycle == INT_MAX) {
        in_timeout = timer_list;
        while ((in_timeout != NULL) && (in_timeout->when <= now)) {
            readyTimerIds.insert(in_timeout->id);
            in_timeout = in_timeout->next;
        }
        in_timeout = NULL;
    }

	// loop until all handlers that should have been called by now or before
	// are invoked and renewed if periodic.  Remember that NewTimer and CancelTimer
	// keep the timer_list happily sorted on "when" for us.  We use "now" as a 
	// variable so that if some of these handler functions run for a long time,
	// we do not sit in this loop forever.
	// we make certain we do not call more than "max_fires" handlers in a 
	// single timeout --- this ensures that timers don't starve out the rest
	// of daemonCore if a timer handler resets itself to 0.
	while( (timer_list != NULL) && (timer_list->when <= now ) &&
		   (num_fires < max_timer_events_per_cycle))
	{
        in_timeout = timer_list;

        // In this code block, if there is no limit on how many timer handlers we will invoke,
        // we want to skip over timers that got  added or reset by other timer handlers to make
        // certain we aren't stuck here forever. So we will only call timer handlers that
        // were ready to fire when we first entered Timeout().
        if (max_timer_events_per_cycle == INT_MAX) {
            std::unordered_set<int>::iterator it;
            bool call_handler = false;
            // Skip handlers already invoked or added
            do {
                it = readyTimerIds.find(in_timeout->id);
                if (it == readyTimerIds.end()) {
                    // this timer was not ready when we first looked, so it must have been
                    // added or reset by another timer callback.  in this case, skip this timer
                    // callback (we will deal with it next time through the daemoncore loop).
                    dprintf(D_DAEMONCORE, "Timer %d not fired (SKIPPED) cause added\n", in_timeout->id);
                    in_timeout = in_timeout->next;
                }
                else {
                    // this timer was ready to fire when we first looked at the timer list, so
                    // we are going to go ahead and call its handler.  Erase it from our readyTimerIds
                    // list, and then break out of loop to go ahead and call the handler.
                    call_handler = true;
                    readyTimerIds.erase(it);
                }
            } while ((!call_handler) && (in_timeout != NULL) && (in_timeout->when <= now));

            if (!call_handler) {
                // no timers left that we want to fire at this time, break out of outer while loop
                break;
            }
        }  // end of block if max_timer_events_per_cycle == INT_MAX

        num_fires++;

		// Update curr_dataptr for GetDataPtr()
		curr_dataptr = &(in_timeout->data_ptr);

		// Initialize our flag so we know if ResetTimer was called.
		did_reset = false;
		did_cancel = false;

		// Log a message before calling handler, but only if
		// D_FULLDEBUG is also enabled.
		if (IsDebugVerbose(D_COMMAND)) {
			dprintf(D_COMMAND, "Calling Timer handler %d (%s)\n",
					in_timeout->id, in_timeout->event_descrip);
		}

		if( in_timeout->timeslice ) {
			in_timeout->timeslice->setStartTimeNow();
		}

		// Now we call the registered handler.  If we were told that the handler
		// is a c++ method, we call the handler from the c++ object referenced 
		// by service*.  If we were told the handler is a c function, we call
		// it and pass the service* as a parameter.
		if( in_timeout->std_handler ) {
			in_timeout->std_handler(in_timeout->id);
		}

		if( in_timeout->timeslice ) {
			in_timeout->timeslice->setFinishTimeNow();
		}

		if (IsDebugVerbose(D_COMMAND)) {
			if( in_timeout->timeslice ) {
				dprintf(D_COMMAND, "Return from Timer handler %d (%s) - took %.3fs\n",
						in_timeout->id, in_timeout->event_descrip,
						in_timeout->timeslice->getLastDuration() );
			}
			else {
				dprintf(D_COMMAND, "Return from Timer handler %d (%s)\n",
						in_timeout->id, in_timeout->event_descrip);
			}
		}

		if (pruntime && in_timeout->event_descrip) {
			*pruntime = daemonCore->dc_stats.AddRuntime(in_timeout->event_descrip, *pruntime);
		}

        // Make sure we didn't leak our priv state
		daemonCore->CheckPrivState();

		// Clear curr_dataptr
		curr_dataptr = NULL;

		if ( did_cancel ) {
			// Timer was canceled inside its handler. All we need to do
			// is delete it.
			DeleteTimer( in_timeout );
		} else if ( !did_reset ) {
			// here we remove the timer we just serviced, or renew it if it is 
			// periodic.

			// If a new timer was added at a time in the past
			// (possible when resetting a timeslice timer), then
			// it may have landed before the timer we just processed,
			// meaning that we cannot assume prev==NULL in the call
			// to RemoveTimer() below.

			Timer *prev = NULL;
			ASSERT( GetTimer(in_timeout->id,&prev) == in_timeout );
			RemoveTimer( in_timeout, prev );

			if ( in_timeout->period > 0 || in_timeout->timeslice ) {
				in_timeout->period_started = time(NULL);
				in_timeout->when = in_timeout->period_started;
				if ( in_timeout->timeslice ) {
					in_timeout->when += in_timeout->timeslice->getTimeToNextRun();
				} else {
					if( in_timeout->period == TIMER_NEVER ) {
						in_timeout->when = TIMER_NEVER;
					} else {
						in_timeout->when += in_timeout->period;
					}
				}
				InsertTimer( in_timeout );
			} else {
				// timer is not perodic; it is just a one-time event.  we just called
				// the handler, so now just delete it. 
				DeleteTimer( in_timeout );
			}
		}
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

    if (pNumFired) *pNumFired = num_fires;
	in_timeout = NULL;
	return(result);
}

#define IS_ZERO(_value_) \
	(  ( (_value_) >= -0.000001 ) && ( (_value_) <= 0.000001 )  )

void TimerManager::DumpTimerList(int flag, const char* indent)
{
	Timer		*timer_ptr;
	const char	*ptmp;

	// we want to allow flag to be "D_FULLDEBUG | D_DAEMONCORE",
	// and only have output if _both_ are specified by the user
	// in the condor_config.  this is a little different than
	// what dprintf does by itself ( which is just 
	// flag & DebugFlags > 0 ), so our own check here:
	if ( ! IsDebugCatAndVerbosity(flag) )
		return;

	if ( indent == NULL) 
		indent = DaemonCore::DEFAULT_INDENT;

	dprintf(flag, "\n");
	dprintf(flag, "%sTimers\n", indent);
	dprintf(flag, "%s~~~~~~\n", indent);
	for(timer_ptr = timer_list; timer_ptr != NULL; timer_ptr = timer_ptr->next)
	{
		if ( timer_ptr->event_descrip )
			ptmp = timer_ptr->event_descrip;
		else
			ptmp = "NULL";

		std::string slice_desc;
		if( !timer_ptr->timeslice ) {
			formatstr(slice_desc, "period = %lld, ", (long long)timer_ptr->period);
		}
		else {
			formatstr_cat(slice_desc, "timeslice = %.3g, ",
								   timer_ptr->timeslice->getTimeslice());
			if( !IS_ZERO(timer_ptr->timeslice->getDefaultInterval()) ) {
				formatstr_cat(slice_desc, "period = %.1f, ",
								   timer_ptr->timeslice->getDefaultInterval());
			}
			if( !IS_ZERO(timer_ptr->timeslice->getInitialInterval()) ) {
				formatstr_cat(slice_desc, "initial period = %.1f, ",
								   timer_ptr->timeslice->getInitialInterval());
			}
			if( !IS_ZERO(timer_ptr->timeslice->getMinInterval()) ) {
				formatstr_cat(slice_desc, "min period = %.1f, ",
								   timer_ptr->timeslice->getMinInterval());
			}
			if( !IS_ZERO(timer_ptr->timeslice->getMaxInterval()) ) {
				formatstr_cat(slice_desc, "max period = %.1f, ",
								   timer_ptr->timeslice->getMaxInterval());
			}
		}
		dprintf(flag, 
				"%sid = %d, when = %ld, %shandler_descrip=<%s>\n", 
				indent, timer_ptr->id, (long)timer_ptr->when, 
				slice_desc.c_str(),ptmp);
	}
	dprintf(flag, "\n");
}

void TimerManager::Start()
{
	struct timeval		timer;

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
			select(0,0,0,0,NULL);
		} else {
			dprintf(D_DAEMONCORE,
				"TimerManager::Start() about to block, timeout=%ld\n",
				(long)timer.tv_sec);
			select(0,0,0,0, &timer);
		}		
	}
}

void TimerManager::RemoveTimer( Timer *timer, Timer *prev )
{
	if ( timer == NULL || ( prev && prev->next != timer ) ||
		 ( !prev && timer != timer_list ) ) {
		EXCEPT( "Bad call to TimerManager::RemoveTimer()!" );
	}

	if ( timer == timer_list ) {
		timer_list = timer_list->next;
	}
	if ( timer == list_tail ) {
		list_tail = prev;
	}
	if ( prev ) {
		prev->next = timer->next;
	}
}

void TimerManager::InsertTimer( Timer *new_timer )
{
	if ( timer_list == NULL ) {
		// list is empty, place ours in front
		timer_list = new_timer;
		list_tail = new_timer;
		new_timer->next = NULL;
			// since we have a new first timer, we must wake up select
		daemonCore->Wake_up_select();
	} else {
		// list is not empty, so keep timer_list ordered from soonest to
		// farthest (i.e. sorted on "when").
		// Note: when doing the comparisons, we always use "<" instead
		// of "<=" -- this makes certain we "round-robin" across 
		// timers that constantly reset themselves to zero.
		if ( new_timer->when < timer_list->when ) {
			// make the this new timer first in line
			new_timer->next = timer_list;
			timer_list = new_timer;
			// since we have a new first timer, we must wake up select
			daemonCore->Wake_up_select();
		} else if ( new_timer->when == TIMER_NEVER ) {
			// Our new timer goes to the very back of the list.
			new_timer->next = NULL;
			list_tail->next = new_timer;
			list_tail = new_timer;
		} else {
			Timer* timer_ptr;
			Timer* trail_ptr = NULL;
			for (timer_ptr = timer_list; timer_ptr != NULL; 
				 timer_ptr = timer_ptr->next ) 
			{
				if (new_timer->when < timer_ptr->when) {
					break;
				}
				trail_ptr = timer_ptr;
			}
			ASSERT( trail_ptr );
			new_timer->next = timer_ptr;
			trail_ptr->next = new_timer;
			if ( trail_ptr == list_tail ) {
				list_tail = new_timer;
			}
		}
	}
}

void TimerManager::DeleteTimer( Timer *timer )
{
	// free event_descrip
	free( timer->event_descrip );

	// set curr_dataptr to NULL if a handler is removing itself. 
	if ( curr_dataptr == &(timer->data_ptr) )
		curr_dataptr = NULL;
	if ( curr_regdataptr == &(timer->data_ptr) )
		curr_regdataptr = NULL;

	delete timer->timeslice;
	delete timer;
}

Timer *TimerManager::GetTimer( int id, Timer **prev )
{
	Timer *timer_ptr = timer_list;
	if( prev ) {
		*prev = NULL;
	}
	while ( timer_ptr && timer_ptr->id != id ) {
		if( prev ) {
			*prev = timer_ptr;
		}
		timer_ptr = timer_ptr->next;
	}

	return timer_ptr;
}


int
TimerManager::countTimersByDescription( const char * description ) {
    if( description == NULL ) { return -1; }
    if( timer_list == NULL ) { return 0; }

    int counter = 0;
	Timer * i = timer_list;
	for( ; i; i = i->next ) {
    	if( 0 == strcmp(i->event_descrip, description) ) {
    	    ++counter;
    	}
	}
	return counter;
}
