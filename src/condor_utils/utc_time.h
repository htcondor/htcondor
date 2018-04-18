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


#if !defined(_CONDOR_UTC_TIME_H)
#define _CONDOR_UTC_TIME_H

#if defined(WIN32)
# include <winsock2.h>
#else
# include <sys/time.h>
#endif


void condor_gettimestamp( struct timeval &tv );

inline
double condor_gettimestamp_double() {
	struct timeval tv;
	condor_gettimestamp( tv );
	return ( tv.tv_sec + ( tv.tv_usec * 0.000001 ) );
}

/* These functions work like the timersub() function.
 * They subtract the time value in b from the time value in a and return
 * the result.
 * For timersub_usec(), the result is expressed in microseconds.
 * For timersub_double(), the result is expressed in seconds represented
 * as a double.
 */
inline
long timersub_usec( const struct timeval &a, const struct timeval &b ) {
	long diff = a.tv_usec - b.tv_usec;
	long sec_diff = a.tv_sec - b.tv_sec;
	if( sec_diff ) {
		diff += sec_diff*1000000;
	}
	return diff;
}

inline
double timersub_double( const struct timeval &a, const struct timeval &b ) {
	double usec_diff = a.tv_usec - b.tv_usec;
	double sec_diff = a.tv_sec - b.tv_sec;
	return sec_diff + (usec_diff / 1000000.0);
}

#endif /* _CONDOR_UTC_TIME_H */

