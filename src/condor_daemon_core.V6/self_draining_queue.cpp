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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "self_draining_queue.h"


SelfDrainingQueue::SelfDrainingQueue( const char* queue_name, int per )
	: m_hash( SelfDrainingHashItem::HashFn ),
	  m_count_per_interval(1)
{
	if( queue_name ) {
		name = strdup( queue_name );
	} else {
		name = strdup( "(unnamed)" );
	}
	std::string t_name;
	formatstr( t_name, "SelfDrainingQueue::timerHandler[%s]", name );
	timer_name = strdup( t_name.c_str() );

	handler_fn = NULL;
	handlercpp_fn = NULL;
	service_ptr = NULL;

	this->period = per;
	tid = -1;
}


SelfDrainingQueue::~SelfDrainingQueue()
{
	cancelTimer();

	if( name ) {
		free( name );
		name = NULL;
	}
	if( timer_name ) {
		free( timer_name );
		timer_name = NULL;
	}
}


//--------------------------------------------------
// Registering callbacks and tuning behavior
//--------------------------------------------------


bool
SelfDrainingQueue::registerHandler( ServiceDataHandler handle_fn )
{
	if( handlercpp_fn ) {
		handlercpp_fn = NULL;
	}
	if( service_ptr ) {
		service_ptr = NULL;
	}
	this->handler_fn = handle_fn;
	return true;
}


bool
SelfDrainingQueue::registerHandlercpp( ServiceDataHandlercpp 
									   handlecpp_fn, 
									   Service* serve_ptr )
{
	if( handler_fn ) {
		handler_fn = NULL;
	}
	this->handlercpp_fn = handlecpp_fn;
	this->service_ptr = serve_ptr;
	return true;
}


bool
SelfDrainingQueue::setPeriod( int new_period )
{
	if( period == new_period ) {
		return false;
	}
	dprintf( D_FULLDEBUG, "Period for SelfDrainingQueue %s set to %d\n",
			 name, new_period );
	period = new_period; 
	if( tid != -1 ) { 
		resetTimer();
	}
	return true;
}

void
SelfDrainingQueue::setCountPerInterval( int count )
{
	m_count_per_interval = count;
	dprintf( D_FULLDEBUG, "Count per interval for SelfDrainingQueue "
			 "%s set to %d\n", name, count );
	ASSERT( count > 0 );
}


//--------------------------------------------------
// Public interface
//--------------------------------------------------


bool
SelfDrainingQueue::enqueue( ServiceData* data, bool allow_dups )
{
	if( ! allow_dups ) {
		SelfDrainingHashItem hash_item(data);
		if( m_hash.insert(hash_item,true) == -1 ) {
			dprintf( D_FULLDEBUG, "SelfDrainingQueue::enqueue() "
					 "refusing duplicate data\n" );
			return false;
		}
	}
	queue.push(data);
	dprintf( D_FULLDEBUG,
			 "Added data to SelfDrainingQueue %s, now has %d element(s)\n",
			 name, (int)queue.size() );
	registerTimer();
	return true;
}


//--------------------------------------------------
// Private methods 
//--------------------------------------------------


void
SelfDrainingQueue::timerHandler( void )
{
	dprintf( D_FULLDEBUG,
			 "Inside SelfDrainingQueue::timerHandler() for %s\n", name );

	if( queue.empty() ) {
		dprintf( D_FULLDEBUG, "SelfDrainingQueue %s is empty, "
				 "timerHandler() has nothing to do\n", name );
		cancelTimer();
		return;
	}
	int count;
	for( count=0; count<m_count_per_interval && !queue.empty(); count++ ) {
		ServiceData* d = queue.front();
		queue.pop();

		SelfDrainingHashItem hash_item(d);
		m_hash.remove(hash_item);

		if( handler_fn ) {
			handler_fn( d );
		} else if( handlercpp_fn && service_ptr ) {
			(service_ptr->*handlercpp_fn)( d );
		}
	}

	if( queue.empty() ) {
		dprintf( D_FULLDEBUG,
				 "SelfDrainingQueue %s is empty, not resetting timer\n",
				 name );
		cancelTimer();
	} else {
			// if there's anything left in the queue, reset our timer
		dprintf( D_FULLDEBUG,
				 "SelfDrainingQueue %s still has %d element(s), "
				 "resetting timer\n", name, (int)queue.size() );
		resetTimer();
	}
}


void
SelfDrainingQueue::registerTimer( void )
{
	if( !handler_fn && !(service_ptr && handlercpp_fn) ) {
		EXCEPT( "Programmer error: trying to register timer for "
				"SelfDrainingQueue %s without having a handler function", 
				name );
	}

		// if we've already got a timer id and we're trying to
		// re-register, we just want to return, since we know the
		// timer's going to go off on its own whenever it needs to 
	if( tid != -1 ) {
		dprintf( D_FULLDEBUG, "Timer for SelfDrainingQueue %s is already "
				 "registered (id: %d)\n", name, tid );
		return;
	}

	tid = daemonCore->
		Register_Timer( period, 
						(TimerHandlercpp)&SelfDrainingQueue::timerHandler,
						timer_name, this );
    if( tid == -1 ) {
            // Error registering timer!
        EXCEPT( "Can't register daemonCore timer for SelfDrainingQueue %s",
				name );
    }
	dprintf( D_FULLDEBUG, "Registered timer for SelfDrainingQueue %s, "
			 "period: %d (id: %d)\n", name, period, tid );
}


void
SelfDrainingQueue::cancelTimer( void )
{
	if( tid != -1 ) {
		dprintf( D_FULLDEBUG, "Canceling timer for SelfDrainingQueue %s "
				 "(timer id: %d)\n", name, tid );
		if (daemonCore) daemonCore->Cancel_Timer( tid );
		tid = -1;
	}
}


void
SelfDrainingQueue::resetTimer( void )
{
	if( tid == -1 ) {
		EXCEPT( "Programmer error: resetting a timer that doesn't exist" );
	}
	daemonCore->Reset_Timer( tid, period, 0 );
	dprintf( D_FULLDEBUG, "Reset timer for SelfDrainingQueue %s, "
			 "period: %d (id: %d)\n", name, period, tid );
}

size_t
SelfDrainingHashItem::HashFn(SelfDrainingHashItem const &item)
{
	return item.m_service->HashFn();
}
