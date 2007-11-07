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
