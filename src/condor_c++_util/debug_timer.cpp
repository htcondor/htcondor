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

#include "debug_timer.h"

DebugTimerBase::DebugTimerBase( bool start )
 : on(false), t1(0), t2(0)
{
	if ( start ) {
		Start( );
	}
}

DebugTimerBase::~DebugTimerBase( void )
{
}

double
DebugTimerBase::dtime( void )
{
	struct timeval	tv;
	gettimeofday( &tv, NULL );
	return ( tv.tv_sec + ( tv.tv_usec / 1000000.0 ) );
}

void
DebugTimerBase::Start( void )
{
	t1 = dtime( );
	on = true;
}

void
DebugTimerBase::Stop( void )
{
	if ( on ) {
		t2 = dtime( );
		on = false;
	}
}

void
DebugTimerBase::Log( const char *s, int num, bool stop )
{
	char	buf[256];
	if ( stop ) {
		Stop( );
	}
	double	timediff = t2 - t1;
	if ( num >= 0 ) {
		double	per = 0.0, per_sec = 0.0;
		if ( num > 0 ) {
			per = timediff / num;
			per_sec = 1.0 / per;
		}
		snprintf( buf, sizeof( buf ),
				  "DebugTimer: %-25s %4d in %8.5fs => %9.7fsp %10.2f/s\n",
				  s, num, timediff, per, per_sec );
		Output( buf );
	} else {
		snprintf( buf, sizeof( buf ),
				  "DebugTimer: %-25s %8.5fs\n", s, timediff );
		Output( buf );
	}
}
