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
#include "proto.h"
#include "alarm.h"


Alarm::Alarm() : saved( 0 ) { }

Alarm::~Alarm()
{
	alarm( 0 );
}


void
Alarm::set( int sec )
{
	alarm( sec );
	dprintf( D_FULLDEBUG, "Set alarm for %d seconds\n", sec );
}

void
Alarm::suspend( )
{
	saved = alarm( 0 );
	dprintf( D_FULLDEBUG, "Suspended alarm with %d seconds remaining\n", saved );
}

void
Alarm::resume()
{
	(void)alarm( saved );
	dprintf( D_FULLDEBUG, "Resumed alarm with %d seconds remaining\n", saved );
	saved = 0;
}

void
Alarm::cancel()
{
	alarm( 0 );
	dprintf( D_FULLDEBUG, "Canceled alarm\n" );
}

CkptTimer::CkptTimer( int min_val, int max_val )
{
	cur = min_val;
	max = max_val;
	active = FALSE;
	suspended = FALSE;
}

CkptTimer::CkptTimer()
{
	cur = 0;
	max = 0;
	active = FALSE;
	suspended = FALSE;
}
	
void
CkptTimer::update_interval()
{
	if( cur ) {
		cur = cur * 2;
		if( cur > max ) {
			cur = max;
		}
	}
}

void
CkptTimer::start()
{
	if( cur ) {
		alarm.set( cur );
		suspended = FALSE;
		active = TRUE;
	}
}

void
CkptTimer::suspend()
{
	if( active ) {
		alarm.suspend();
		suspended = TRUE;
	}
}

void
CkptTimer::resume()
{
	if( active ) {
		alarm.resume();
		suspended = FALSE;
	}
}

void
CkptTimer::clear()
{
	if( active ) {
		alarm.cancel();
		suspended = FALSE;
		active = FALSE;
	}
}
