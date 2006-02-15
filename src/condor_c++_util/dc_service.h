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

#ifndef _CONDOR_DC_SERVICE_H_
#define _CONDOR_DC_SERVICE_H_

/* **********************************************************************
  This file declares two abstract base classes used by DaemonCore.
  However, they can also be refered to and used by objects that don't
  necessarily require DaemonCore themselves, so i'm moving them into a
  seperate header file in the C++ utility library so they can be
  accessed without pulling in all of DaemonCore.
********************************************************************** */
 

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


/**
   This class is an abstract base class used by some generic data
   structures in DaemonCore.  It's sort of like the "Service" class
   above, which is why it's got "Service" in the name (even though i
   don't like that name).  Any object that you want to store in one of
   the DC-specific data structures (for now, just the
   SelfDrainingQueue, but someday there might be more) should be
   derived from a ServiceData.  This allows SelfDrainingQueue to work
   properly without itself being a template, and will enable us to let
   users give us c++ member functions as call-backs for these data
   structures, if we really wanted...
*/
class ServiceData
{
public:
	virtual ~ServiceData() {};

protected:
    ServiceData() {}
};


/**
   These are some typedefs for function pointers that deal with
   ServiceData pointers. 
*/ 
typedef int (*ServiceDataHandler)(ServiceData*);

typedef int (*ServiceDataCompare)(ServiceData*, ServiceData*);

typedef int (Service::*ServiceDataHandlercpp)(ServiceData*);


#endif /* _CONDOR_DC_SERVICE_H_ */
