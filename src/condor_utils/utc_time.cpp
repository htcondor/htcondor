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
#include "utc_time.h"

#if defined(HAVE__FTIME)
# include <sys/timeb.h>
#endif


UtcTime::UtcTime( bool get_time )
{
	sec = 0;
	usec = 0;
	if ( get_time ) {
		getTime( );
	}
}

double
UtcTime::getTimeDouble( void )
{
#if defined(HAVE__FTIME)
	struct _timeb timebuffer;
	_ftime( &timebuffer );
	return ( timebuffer.time + (timebuffer.millitm * 0.001) );
#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval	tv;
	gettimeofday( &tv, NULL );
	return ( tv.tv_sec + ( tv.tv_usec * 0.000001 ) );
#else
#error Neither _ftime() nor gettimeofday() are available!
#endif
}

void
UtcTime::getTime()
{
#if defined(HAVE__FTIME)
		// call _ftime()
	struct _timeb timebuffer;
	_ftime( &timebuffer );

	sec = timebuffer.time;
	usec = (long) timebuffer.millitm * 1000; // convert milli to micro 
#elif defined(HAVE_GETTIMEOFDAY)
		// UNIX
	struct timeval now;
	gettimeofday( &now, NULL );
	sec = now.tv_sec;
	usec = (long)now.tv_usec;
#else
#error Neither _ftime() nor gettimeofday() are available!
#endif
}

double
UtcTime::difference( const UtcTime* other_time ) const
{
	if( ! other_time ) {
		return 0.0;
	}
	return difference( *other_time );
}

double
UtcTime::difference( const UtcTime &other_time ) const
{
	double other = other_time.combined();
	double me = combined();

	return me - other;
}

bool
operator==(const UtcTime &lhs, const UtcTime &rhs) 
{
	return ((lhs.seconds() == rhs.seconds()) &&
			(lhs.microseconds() == rhs.microseconds()) );
}
