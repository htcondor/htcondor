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


#include "condor_daemon_core.h"
#include "Queue.h"

/**
   This class implements a special kind of Queue: a Queue that can
   drain itself.  Once you instantiate it, all you have to do is give
   it a function pointer to call on each data element, and the
   (optional) period to drain data elements, and this class handles
   everything else for you.  So long as there's still data in the
   queue, this class will dequeue another element after the desired
   period and call the given handler function, passing in a pointer to
   the data element.

   To prevent this from itself being a template, and to potentially
   allow more fancy c++ method support, the SelfDrainingQueue only
   stores pointers to ServiceData* objects.  Its the callers
   responsibility to allocate and deallocate these objects at the
   appropriate momemnt (deallocating in your handler method is
   probably the best place).  

   @see ServiceData
*/



class SelfDrainingQueue : public Service
{
public:
	SelfDrainingQueue( const char* name = NULL, int period = 0,
					   ServiceDataCompare compare_fn = NULL );
	~SelfDrainingQueue();


		/// Functions to register handlers and tune behavior
	bool registerHandler( ServiceDataHandler handler_fn );
	bool registerHandlercpp( ServiceDataHandlercpp handlercpp_fn, 
							 Service* service_ptr );
	bool registerCompareFunc( ServiceDataCompare compare_fn );
	bool setPeriod( int new_period );


		/// Main public interface
	bool enqueue( ServiceData* data, bool allow_dups = true );
	bool isMember( ServiceData* data );


private:
	Queue<ServiceData*> queue;
	ServiceDataHandler handler_fn;
	ServiceDataHandlercpp handlercpp_fn;
	ServiceDataCompare compare_fn;

	Service* service_ptr;
	int tid;
	int period;
	char* name;
	char* timer_name;
	
	int timerHandler( void );

	void registerTimer( void );
	void cancelTimer( void );
	void resetTimer( void );
};
