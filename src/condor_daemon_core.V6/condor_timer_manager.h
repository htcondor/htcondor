/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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




// /////////////////////////////////////////////////////////////////////////
//
// Defines a timer driven by select timeouts that can handle 
// scheduled events as well as periodical events. 
//
// /////////////////////////////////////////////////////////////////////////

#ifndef _TIMERMANAGER_H_
#define _TIMERMANAGER_H_

#include "condor_constants.h"
#include "dc_service.h"
#include "condor_timeslice.h"

#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

const   int     STAR = -1;

//-----------------------------------------------------------------------------
/** @name Typedefs for Service
 */
//@{
/** Function
*/
typedef void     (*TimerHandler)();

/// Service Method
typedef void     (Service::*TimerHandlercpp)();

/** Function, which given a pointer to a void* releases its 
	memory (C Version).
	*/
typedef void    (*Release)(void*);

/** Service Method, which given a pointer to a void* memory
	buffer, releases its memory (C++ Version).
	*/
typedef void    (Service::*Releasecpp)(void*);
//@}

// to make the timer release stuff similar to the rest of Daemon Core...
#define TimerRelease Release
#define TimerReleasecpp Releasecpp

// This value, passed for "when", will cause the timer to never expire
const unsigned	TIMER_NEVER = 0xffffffff;

const time_t TIME_T_NEVER	= 0x7fffffff;


//-----------------------------------------------------------------------------
/// Not_Yet_Documented
struct tagTimer {
    /** Not_Yet_Documented */ time_t            when;
    /** Not_Yet_Documented */ time_t            period_started;
    /** Not_Yet_Documented */ unsigned          period;
    /** Not_Yet_Documented */ int               id;
    /** Not_Yet_Documented */ TimerHandler             handler;
    /** Not_Yet_Documented */ TimerHandlercpp          handlercpp;
    /** Not_Yet_Documented */ class Service*    service; 
    /** Not_Yet_Documented */ struct tagTimer*  next;
    /** Not_Yet_Documented */ char*             event_descrip;
    /** Not_Yet_Documented */ void*             data_ptr;
    /** Not_Yet_Documented */ Timeslice *       timeslice;
	/** Not_Yet_Documented */ Release           release;
	/** Not_Yet_Documented */ Releasecpp        releasecpp;
};

///
typedef struct tagTimer Timer;

//-----------------------------------------------------------------------------
/**
 */
class TimerManager
{
  public:

    static TimerManager &GetTimerManager();

    ///
    ~TimerManager();

    // Invoked by daemonCore on a reconfig and also on startup
    void reconfig();

    /** Not_Yet_Documented.
        @param deltawhen      Not_Yet_Documented.
        @param handler        Not_Yet_Documented.
		@param release        Not_Yet_Documented.
        @param event_descrip  Not_Yet_Documented.
        @param period         Not_Yet_Documented.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer(unsigned     deltawhen,
                 TimerHandler handler,
                 Release      release,
                 const char * event_descrip, 
                 unsigned     period          =  0);

	/** Not_Yet_Documented.
        @param deltawhen      Not_Yet_Documented.
        @param handler        Not_Yet_Documented.
        @param event_descrip  Not_Yet_Documented.
        @param period         Not_Yet_Documented.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer(unsigned     deltawhen,
                 TimerHandler handler,
                 const char * event_descrip, 
                 unsigned     period          =  0);

    /** Not_Yet_Documented.
        @param s              Not_Yet_Documented.
        @param deltawhen      Not_Yet_Documented.
        @param handler        Not_Yet_Documented.
        @param event_descrip  Not_Yet_Documented.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer (Service*     s,
                  unsigned     deltawhen,
                  TimerHandlercpp handler,
                  const char * event_descrip,
                  unsigned     period          =  0);

    /** Create a timer using a timeslice object to control interval.
        @param timeslice      Timeslice object specifying interval parameters
        @param handler        Function to call when timer fires.
        @param event_descrip  String describing the function.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer (const Timeslice &timeslice,
                  TimerHandler handler,
                  const char * event_descrip);

    /** Create a timer using a timeslice object to control interval.
        @param s              Service object of which function is a member.
        @param timeslice      Timeslice object specifying interval parameters
        @param handler        Function to call when timer fires.
        @param event_descrip  String describing the function.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer (Service*     s,
                  const Timeslice &timeslice,
                  TimerHandlercpp handler,
                  const char * event_descrip);

    /** Not_Yet_Documented.
        @param id The ID of the timer
		@param release_data_ptr True if the timer's data_ptr should be freed
        @return 0 if successful, -1 on failure (timer not found)
    */
    int CancelTimer(int id);

    /** Not_Yet_Documented.
        @param tid The ID of the timer
        @param when    Timestamp for next call (ignored if recompute_when=true)
        @param period  Not_Yet_Documented
		@param recompute_when If true, 'when' is recomputed from new period
		                      and how long this timer has been waiting.
        @return 0 if successful, -1 on failure (timer not found)
    */
    int ResetTimer(int tid, unsigned when, unsigned period = 0, bool recompute_when=false, Timeslice const *new_timeslice=NULL);

	/**
       This is equivalent to calling ResetTimer with recompute_when=true.
	   @param tid The ID of the timer
	   @param period The new period for the timer.
	   @param new_timeslice The new timeslice settings to use
	          (other args such as when and period ignored if this is non-NULL)
	   @return 0 if successful, -1 on failure (timer not found)
	 */
    int ResetTimerPeriod(int tid, unsigned period);

	/**
       This is equivalent to calling ResetTimer with new_timeslice != NULL.
	   @param tid The ID of the timer
	   @param new_timeslice The new timeslice settings to use.
	   @return true if successful, false on failure (timer not found)
	 */
	bool ResetTimerTimeslice(int id, Timeslice const &new_timeslice);

	/**
	   Get a copy of the timeslice settings associated with a timer.
	   @param tid The ID of the timer
	   @param timeslice Object to receive a copy of the timeslice settings.
	   @return false if no timeslice associated with this timer.
	 */
	bool GetTimerTimeslice(int id, Timeslice &timeslice);

	/**
	 * Get the time_t of when the given timer ID will fire.
	 * @param tid The ID of the timer
	 * @return Timestamp of when this will fire; 0 if there is no such timer.
	 */
	time_t GetNextRuntime(int id);

    /// Not_Yet_Documented.
    void CancelAllTimers();

    /// Not_Yet_Documented.
    void DumpTimerList(int, const char* = NULL );

    /** Not_Yet_Documented.
        @return Not_Yet_Documented
    */
    int Timeout(int * pNumFired = NULL, double * pruntime = NULL); 

    /// Not_Yet_Documented.
    void Start();
    
  private:

    ///
    TimerManager();
    
    int NewTimer (Service*   s,
                  unsigned   deltawhen,
                  TimerHandler handler,
                  TimerHandlercpp handlercpp,
				  Release	 release,
				  Releasecpp releasecpp,
                  const char *event_descrip,
                  unsigned   period          =  0,
				  const Timeslice *timeslice = NULL);

	void RemoveTimer( Timer *timer, Timer *prev );
	void InsertTimer( Timer *new_timer );
	void DeleteTimer( Timer *timer );

	/*
	  @param id The id of the timer to find
	  @param prev If not NULL, this will be set to previous timer in list
	  @return pointer to timer with specified id or NULL if not found
	 */
	Timer *GetTimer( int id, Timer **prev );

    Timer*  timer_list;
	Timer*  list_tail;
    int     timer_ids;
    Timer*  in_timeout;
    bool    did_reset;
	bool    did_cancel;

    /* max_timer_events_per_cycle sets the maximum number of timer handlers
    we will invoke per call to Timeout().  This limit prevents timers
    from starving other kinds other DC handlers (i.e. it make certain
    that we make it back to the Driver() loop occasionally.  The higher
    the number, the more "timely" timer callbacks will be.  The lower
    the number, the more responsive non-timer calls (like commands)
    will be in the face of many timers coming due.
    A value of zero means invoke every timer callback that is due at the
    time we first enter Timeout(), but do not invoke new timers registered/reset
    during the timeout.
    */
    int max_timer_events_per_cycle;

};

#endif
