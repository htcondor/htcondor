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

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include "classad/common.h"
#include "classad/util.h"
#include <limits.h>
#include <math.h>

using namespace std;

namespace classad {

#ifdef WIN32
#define BIGGEST_RANDOM_INT RAND_MAX
int get_random_integer(void)
{
    static char initialized = 0;

	if (!initialized) {
        int seed = time(NULL);
        srand(seed);
        initialized = 1;
	}

	return rand();
}
#else
#define BIGGEST_RANDOM_INT INT_MAX
int get_random_integer(void)
{
    static char initialized = 0;

	if (!initialized) {
        int seed = time(NULL);
        srand48(seed);
        initialized = 1;
	}
	return (int) (lrand48() & INT_MAX);
}
#endif

double get_random_real(void)
{
    return (get_random_integer() / (double) BIGGEST_RANDOM_INT);
}

long timezone_offset( time_t clock, bool no_dst )
{
    long tz_offset;
    struct tm  tms;

    getLocalTime(&clock, &tms);
#ifdef HAVE_TM_GMTOFF
    tz_offset = tms.tm_gmtoff;
#else
    struct tm gtms;
    getGMTime(&clock, &gtms);

    tz_offset = (tms.tm_hour - gtms.tm_hour)*3600 +
                (tms.tm_min - gtms.tm_min)*60 +
                (tms.tm_sec - gtms.tm_sec);

    // Correct above for 24-hour wrap-around.
    // Assume GM time and local time only differ by at most one day.
    if( tms.tm_year > gtms.tm_year ) {
        tz_offset += 24*3600;
    }
    else if( tms.tm_year < gtms.tm_year ) {
        tz_offset -= 24*3600;
    }
    else if( tms.tm_yday > gtms.tm_yday ) {
        tz_offset += 24*3600;
    }
    else if( tms.tm_yday < gtms.tm_yday ) {
        tz_offset -= 24*3600;
    }
#endif

	// Correct for daylignt savings time.
    if ( no_dst && 0 != tms.tm_isdst ) {
        tz_offset -= 3600;
    }

    return tz_offset;
}

void convert_escapes(string &text, bool &validStr)
{
	char *copy;
	int  length;
	int  source, dest;

	// We now it will be no longer than the original.
	length = text.length();
	copy = new char[length + 1];
	
	// We scan up to one less than the length, because we ignore
	// a terminating slash: it can't be an escape. 
	dest = 0;
	for (source = 0; source < length - 1; source++) {
		if (text[source] != '\\' || source == length - 1) {
			copy[dest++]= text[source]; 
		}
		else {
			source++;

			char new_char;
			switch(text[source]) {
			case 'a':	new_char = '\a'; break;
			case 'b':	new_char = '\b'; break;
			case 'f':	new_char = '\f'; break;
			case 'n':	new_char = '\n'; break;
			case 'r':	new_char = '\r'; break;
			case 't':	new_char = '\t'; break;
			case 'v':	new_char = '\v'; break;
			case '\\':	new_char = '\\'; break;
			case '\?':	new_char = '\?'; break;
			case '\'':	new_char = '\''; break;
			case '\"':	new_char = '\"'; break;
			default:   
				if (isodigit(text[source])) {
					unsigned int  number;
					// There are three allowed ways to have octal escape characters:
					//  \[0..3]nn or \nn or \n. We check for them in that order.
					if (   source <= length - 3
						&& text[source] >= '0' && text[source] <= '3'
						&& isodigit(text[source+1])
						&& isodigit(text[source+2])) {

						// We have the \[0..3]nn case
						char octal[4];
						octal[0] = text[source];
						octal[1] = text[source+1];
						octal[2] = text[source+2];
						octal[3] = 0;
						sscanf(octal, "%o", &number);
						new_char = number;
						source += 2; // to account for the two extra digits
					} else if (   source <= length -2
							   && isodigit(text[source+1])) {

						// We have the \nn case
						char octal[3];
						octal[0] = text[source];
						octal[1] = text[source+1];
						octal[2] = 0;
						sscanf(octal, "%o", &number);
						new_char = number;
						source += 1; // to account for the extra digit
					} else if (source <= length - 1) {
						char octal[2];
						octal[0] = text[source];
						octal[1] = 0;
						sscanf(octal, "%o", &number);
						new_char = number;
					} else {
						number = new_char = text[source];
					}
					if(number == 0) { // "\\0" is an invalid substring within a string literal
					  validStr = false;
					  delete [] copy;
					  return;
					}
				} else {
					new_char = text[source];
				}
				break;
			}
			copy[dest++] = new_char;
		}
	}
	copy[dest] = 0;
	text = copy;
	delete [] copy;
	return;
}

void 
getLocalTime(time_t *now, struct tm *localtm) 
{

#ifndef WIN32
	localtime_r( now, localtm );
#else
	// there is no localtime_r() on Windows, so for now
	// we just call localtime() and deep copy the result.

	struct tm *lt_ptr; 

	lt_ptr = localtime(now);

	if (localtm == NULL) { return; } 

	localtm->tm_sec   = lt_ptr->tm_sec;    /* seconds */
	localtm->tm_min   = lt_ptr->tm_min;    /* minutes */
	localtm->tm_hour  = lt_ptr->tm_hour;   /* hours */
	localtm->tm_mday  = lt_ptr->tm_mday;   /* day of the month */
	localtm->tm_mon   = lt_ptr->tm_mon;    /* month */
	localtm->tm_year  = lt_ptr->tm_year;   /* year */
	localtm->tm_wday  = lt_ptr->tm_wday;   /* day of the week */
	localtm->tm_yday  = lt_ptr->tm_yday;   /* day in the year */
	localtm->tm_isdst = lt_ptr->tm_isdst;  /* daylight saving time */
	
#endif
	return;
}

void 
getGMTime(time_t *now, struct tm *gtm) 
{

#ifndef WIN32
	gmtime_r( now, gtm );
#else
	// there is no localtime_r() on Windows, so for now
	// we just call localtime() and deep copy the result.

	struct tm *gt_ptr; 

	gt_ptr = gmtime(now);

	if (gtm == NULL) { return; } 

	gtm->tm_sec   = gt_ptr->tm_sec;    /* seconds */
	gtm->tm_min   = gt_ptr->tm_min;    /* minutes */
	gtm->tm_hour  = gt_ptr->tm_hour;   /* hours */
	gtm->tm_mday  = gt_ptr->tm_mday;   /* day of the month */
	gtm->tm_mon   = gt_ptr->tm_mon;    /* month */
	gtm->tm_year  = gt_ptr->tm_year;   /* year */
	gtm->tm_wday  = gt_ptr->tm_wday;   /* day of the week */
	gtm->tm_yday  = gt_ptr->tm_yday;   /* day in the year */
	gtm->tm_isdst = gt_ptr->tm_isdst;  /* daylight saving time */
	
#endif
	return;
}

void absTimeToString(const abstime_t &atime, string &buffer)
{
    int       tzsecs;
    time_t    epoch_time;
    char      timebuf[32], sign;
    struct tm tms;

    tzsecs = atime.offset;  
    epoch_time = atime.secs + atime.offset;
    if (tzsecs > 0) { 
        sign = '+';         // timezone offset's sign
    } else {
        sign = '-';
        tzsecs = -tzsecs;
    }
    getGMTime(&epoch_time, &tms);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tms);
    buffer += timebuf;
    sprintf(timebuf, "%c%02d%02d", sign, tzsecs / 3600, (tzsecs / 60) % 60);
    buffer += timebuf;
    return;
}

void relTimeToString(double rsecs, string &buffer)
{
    double  fractional_seconds;
    int     days, hrs, mins;
    double  secs;
    char    timebuf[128];
   
    if( rsecs < 0 ) {
        buffer += "-";
        rsecs = -rsecs;
    }
    fractional_seconds = rsecs - floor(rsecs);

    days = (int) rsecs;
    hrs  = days % 86400;
    mins = hrs  % 3600;
    secs = (mins % 60) + fractional_seconds;
    days = days / 86400;
    hrs  = hrs  / 3600;
    mins = mins / 60;
    
    if (days) {
        if (fractional_seconds == 0) {
            sprintf(timebuf, "%d+%02d:%02d:%02d", days, hrs, mins, (int) secs);
        } else {
            sprintf(timebuf, "%d+%02d:%02d:%02g", days, hrs, mins, secs);
        }
        buffer += timebuf;
    } else if (hrs) {
        if (fractional_seconds == 0) {
            sprintf(timebuf, "%02d:%02d:%02d", hrs, mins, (int) secs);
        } else {
            sprintf(timebuf, "%02d:%02d:%02g", hrs, mins, secs);
        }
        buffer += timebuf;
    } else if (mins) {
        if (fractional_seconds == 0) {
            sprintf(timebuf, "%02d:%02d", mins, (int) secs);
        } else {
            sprintf(timebuf, "%02d:%02g", mins, secs);
        }
        buffer += timebuf;
        return;
    } else {
        if (fractional_seconds == 0) {
            sprintf(timebuf, "%02d", (int) secs);
        } else {
            sprintf(timebuf, "%02g", secs);
        }
        buffer += timebuf;
    }
    return;
}

void day_numbers(int year, int month, int day, int &weekday, int &yearday)
{
    int fixed =       fixed_from_gregorian(year, month, day);
    int jan1_fixed =  fixed_from_gregorian(year, 1, 1);
    weekday = fixed % 7;
    yearday = fixed - jan1_fixed;
    return;
}

int fixed_from_gregorian(int year, int month, int day)
{
    int fixed;
    int month_adjustment;

    if (month <= 2) {
        month_adjustment = 0;
    } else if (is_leap_year(year)) {
        month_adjustment = -1;
    } else {
        month_adjustment = -2;
    }
    fixed = 365 * (year -1)
          + ((year - 1) / 4)
          - ((year - 1) / 100)
          + ((year - 1) / 400)
          + ((367 * month - 362) / 12)
          + month_adjustment
          + day;
    return fixed;
}

bool is_leap_year(int year)
{
    int  mod4;
    int  mod400;
    bool leap_year;

    mod4   = year % 4;
    mod400 = year % 400;

    if (mod4 == 0 && mod400 != 100 && mod400 != 200 && mod400 != 300) {
        leap_year = true;
    } else {
        leap_year = false;
    }
    return leap_year;
}

#ifdef WIN32
int classad_isinf(double x) 
{

	int result;
	result = _fpclass(x);

	if (result == _FPCLASS_NINF ) {
		/* negative infinity */
		return -1;
	} else if ( result == _FPCLASS_PINF ) {
		/* positive infinity */
		return 1;
	} else {
		/* otherwise */
		return 0;
	}
}
#else
int classad_isinf(double x)
{
    if (!isinf(x) || x != x) {
        return 0;
    } else if (x > 0) {
        return 1;
    } else {
        return -1;
    }
}
#endif 

int classad_isnan(double x)
{
    return isnan(x);
}

} // classad

