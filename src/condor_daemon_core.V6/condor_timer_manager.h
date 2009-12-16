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
    
    ///
    TimerManager();
    ///
    ~TimerManager();

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
        @param when    Not_Yet_Documented
        @param period  Not_Yet_Documented
        @return 0 if successful, -1 on failure (timer not found)
    */
    int ResetTimer(int tid, unsigned when, unsigned period = 0);

    /// Not_Yet_Documented.
    void CancelAllTimers();

    /// Not_Yet_Documented.
    void DumpTimerList(int, const char* = NULL );

    /** Not_Yet_Documented.
        @return Not_Yet_Documented
    */
    int Timeout(); 

    /// Not_Yet_Documented.
    void Start();
    
  private:
    
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

    Timer*  timer_list;
	Timer*  list_tail;
    int     timer_ids;
    Timer*  in_timeout;
    bool    did_reset;
	bool    did_cancel;
};

#endif
