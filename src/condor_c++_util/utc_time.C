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

#include "condor_common.h"
#include "utc_time.h"

#ifdef WIN32
#include <sys/timeb.h>
#endif




UtcTime::UtcTime()
{
	sec = 0;
	usec = 0;
}


void
UtcTime::getTime()
{
#ifdef WIN32

		// call _ftime()
	struct _timeb timebuffer;
	_ftime( &timebuffer );

	sec = timebuffer.time;
	usec = (long) timebuffer.millitm * 1000; // convert milli to micro 

#else	// UNIX
	struct timeval now;
	gettimeofday( &now, NULL );
	sec = now.tv_sec;
	usec = (long)now.tv_usec;
#endif
}


double
UtcTime::difference( UtcTime* other_time )
{
	if( ! other_time ) {
		return 0.0;
	}
	double other = other_time->seconds() + 
		((double)other_time->microseconds() / 1000000);

	double me = sec + ((double)usec/1000000);

	return me - other;
}

