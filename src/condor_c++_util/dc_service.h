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
	virtual ~ServiceData();

		/** Comparison function for use with SelfDrainingQueue.
			@return -1 if this < other, 0 if this == other, and 1 if this > other
		*/
	virtual int ServiceDataCompare( ServiceData const* other ) const = 0;

		/** For use with SelfDrainingQueue. */
	virtual unsigned int HashFn( ) const = 0;

protected:
    ServiceData() {}
};


/**
   These are some typedefs for function pointers that deal with
   ServiceData pointers. 
*/ 
typedef int (*ServiceDataHandler)(ServiceData*);

typedef int (Service::*ServiceDataHandlercpp)(ServiceData*);


#endif /* _CONDOR_DC_SERVICE_H_ */
