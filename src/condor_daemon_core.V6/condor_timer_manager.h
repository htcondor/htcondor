/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
/** Function, which given a pointer to Service object,
    returns an int (C Version).
*/
typedef int     (*Event)(Service*);

/// Service Method that returns an int (C++ Version).
typedef int     (Service::*Eventcpp)();
//@}

// to make the timer handler stuff similar to the rest of Daemon Core...
#define TimerHandler Event
#define TimerHandlercpp Eventcpp

// This value, passed for "when", will cause the timer to never expire
const unsigned	TIMER_NEVER = 0xffffffff;


//-----------------------------------------------------------------------------
/// Not_Yet_Documented
struct tagTimer {
    /** Not_Yet_Documented */ time_t            when;
    /** Not_Yet_Documented */ unsigned          period;
    /** Not_Yet_Documented */ int               id;
    /** Not_Yet_Documented */ Event             handler;
    /** Not_Yet_Documented */ Eventcpp          handlercpp;
    /** Not_Yet_Documented */ class Service*    service; 
    /** Not_Yet_Documented */ struct tagTimer*  next;
    /** Not_Yet_Documented */ int               is_cpp;
    /** Not_Yet_Documented */ char*             event_descrip;
    /** Not_Yet_Documented */ void*             data_ptr;
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
        @param s               Not_Yet_Documented.
        @param deltawhen       Not_Yet_Documented.
        @param event           Not_Yet_Documented.
        @param event_descrip   Not_Yet_Documented.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer(Service*     s,
                 unsigned     deltawhen,
                 Event        event,
                 char *       event_descrip,
                 unsigned     period          =  0,
                 int          id              = -1);

    /** Not_Yet_Documented.
        @param deltawhen      Not_Yet_Documented.
        @param event          Not_Yet_Documented.
        @param event_descrip  Not_Yet_Documented.
        @param period         Not_Yet_Documented.
        @param id             Not_Yet_Documented.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer(unsigned    deltawhen,
                 Event       event,
                 char *      event_descrip, 
                 unsigned    period          =  0,
                 int         id              = -1);

    /** Not_Yet_Documented.
        @param s              Not_Yet_Documented.
        @param deltawhen      Not_Yet_Documented.
        @param event          Not_Yet_Documented.
        @param event_descrip  Not_Yet_Documented.
        @return The ID of the new timer, or -1 on failure
    */
    int NewTimer (Service*   s,
                  unsigned   deltawhen,
                  Eventcpp   event,
                  char *     event_descrip,
                  unsigned   period          =  0,
                  int        id              = -1);

    /** Not_Yet_Documented.
        @param id The ID of the timer
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
    void DumpTimerList(int, char* = NULL );

    /** Not_Yet_Documented.
        @return Not_Yet_Documented
    */
    int Timeout(); 

    /// Not_Yet_Documented.
    void Start();
    
  private:
    
    int NewTimer (Service*  s,
                  unsigned  deltawhen,
                  Event     event,
                  Eventcpp  eventcpp,
                  char *    event_descrip,
                  unsigned  period          =  0,
                  int       id              = -1, 
                  int       is_cpp          =  0);

    Timer*  timer_list;
    int     timer_ids;
    int     in_timeout;
    int     did_reset;
};

#endif
