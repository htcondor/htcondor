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



// /////////////////////////////////////////////////////////////////////////
//
// Defines a timer driven by select timeouts that can handle 
// scheduled events as well as periodical events. 
//
// /////////////////////////////////////////////////////////////////////////

#ifndef _TIMERMANAGER_H_
#define _TIMERMANAGER_H_

#include "condor_constants.h"

#ifdef WIN32
#include <time.h>
#else
#include <sys/time.h>
#endif

const   int     STAR = -1;

//-----------------------------------------------------------------------------
/** The Service class is an abstract base class for all classes that have
    callback event handler methods.  Because C++ classes do not have a
    common base class, there is no way to refer to an generic object pointer.
    In Java, one would use the Object class, because all classes implicitly
    inherit from it.  The <tt>void*</tt> type of C is not adequate for this
    purpose, because a Daemon Core service register method would not know
    the class the <tt>void*</tt> object to cast to.<p>

    So whenever you wish to have a callback method (as opposed to a callback
    function), the class of that method must subclass Service, so that
    you can pass your object containing the callback method to Daemon Core
    register methods that expect a <tt>Service*</tt> parameter.
 */
class Service {
  public:
    /** The destructor is virtual because the Service class requires one
        virtual function so that a vtable is made for this class.  If it
        isn't, daemonCore is horribly broken when it attempts to call
        object methods whose class may or may not have a vtable.<p>

        A really neat idea would be to have this destructor remove any
        Daemon Core callbacks registered to methods in the object.  Thus,
        by deleting your service object, you are automatically unregistering
        callbacks.  Right now, if you forget to unregister a callback before
        you delete an object containing callback methods, nasty trouble
        will result when Daemon Core attempts to call methods that no longer
        exist in memory.
    */
	virtual ~Service() {}

    /** Constructor.  Since Service is an abstract class, we'll make
	  its constructor a protected method to prevent outsiders from
	  instantiating Service objects.
    */
  protected:
	Service() {}

};

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
