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


#ifndef __CLASSAD_UTIL_H__
#define __CLASSAD_UTIL_H__

#include "classad/common.h"
#include <charconv>

namespace classad {

// macro for recognising octal digits
//#define isodigit(x) (((x) >= '0') && ((x) < '8'))
// using & instead of && to avoid a branch...
#define isodigit(x) (((x) >= '0') & ((x) < '8'))

// A structure which represents the Absolute Time Literal
struct abstime_t 
{
	time_t secs;   // seconds from the epoch (UTC) 
	int    offset; // seconds east of Greenwich 
};

// Get a random number between 0 and something large--usually an int
int get_random_integer(void);
// Get a random number between 0 and 1
double get_random_real(void);

/* This calculates the timezone offset of the given time for the current
 * locality. The returned value is the offset in seconds east of UTC.
 * If the optional no_dst parameter is set to true, the calculation is
 * made assuming no daylight saving time.
 */
long timezone_offset( time_t clock, bool no_dst = false );

/* This converts a string so that sequences like \t
 * (two-characters, slash and t) are converted into the 
 * correct characters like tab. It also converts octal sequences. 
 */
void convert_escapes(std::string &text, bool &validStr); 
void convert_escapes_json(std::string &text, bool &validStr, bool &quotedExpr);

// appends a formatted long int to a std:::string -- much faster than sprintf
void append_long(std::string &s, long long l);

void getLocalTime(time_t *now, struct tm *localtm);
void getGMTime(time_t *now, struct tm *localtm);

void absTimeToString(const abstime_t &atime, std::string &buffer);
void relTimeToString(double rtime, std::string &buffer);

void day_numbers(int year, int month, int day, int &weekday, int &yearday);
int fixed_from_gregorian(int year, int month, int day);
bool is_leap_year(int year);

int classad_isinf(double x);
int classad_isnan(double x);

} // classad

#endif//__CLASSAD_UTIL_H__
