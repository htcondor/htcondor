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

using std::string;
using std::pair;


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

// convert escapes in-place
// the string can only shrink while converting escapes so we can safely convert in-place.
void convert_escapes(string &text, bool &validStr)
{
	validStr = true;
	if (text.empty())
		return;

	int length = (int)text.length();
	int dest = 0;

	for (int source = 0; source < length; ++source) {
		char ch = text[source];
		// scan for escapes, a terminating slash cannot be an escape
		if (ch == '\\' && source < length - 1) {
			++source; // skip the \ character
			ch = text[source];

			switch(ch) {
			case '\"':	ch = '\"'; break;
			case '\'':	ch = '\''; break;
			case '\?':	ch = '\?'; break;
			case 'a':	ch = '\a'; break;
			case 'b':	ch = '\b'; break;
			case 'f':	ch = '\f'; break;
			case 'n':	ch = '\n'; break;
			case 'r':	ch = '\r'; break;
			case 't':	ch = '\t'; break;
			case 'v':	ch = '\v'; break;
			case '\\':	ch = '\\'; break;
			default:
				if (isodigit(ch)) {
					unsigned int  number = ch - '0';
					// There can be up to 3 octal digits in an octal escape
					//  \[0..3]nn or \nn or \n. We quit at 3 characters or
					// at the first non-octal character.
					if (source+1 < length) {
						char digit = text[source+1]; // is the next digit also 
						if (isodigit(digit)) {
							++source;
							number = (number << 3) + digit - '0';
							if (number < 0x20 && source+1 < length) {
								digit =  text[source+1];
								if (isodigit(digit)) {
									++source;
									number = (number << 3) + digit - '0';
								}
							}
						}
					}
					ch = (char)number;
					if(ch == 0) { // "\\0" is an invalid substring within a string literal
						validStr = false;
					}
				} else {
					// pass char after \ unmodified.
				}
				break;
			}
		}

		if (dest == source) {
			// no need to assign ch to text when we haven't seen any escapes yet.
			// text[dest] = ch;
			++dest;
		} else {
			text[dest] = ch;
			++dest;
		}
	}

	if (dest < length) {
		text.erase(dest, std::string::npos);
		length = dest;
	}
}

void convert_escapes_json(string &text, bool &validStr, bool &quotedExpr)
{
	validStr = true;
	if (text.empty())
		return;

	int length = (int)text.length();
	int dest = 0;

	if ( length >= 4 && text[0] == '\\' && text[1] == '/' &&
		 text[length-2] == '\\' && text[length-1] == '/' ) {
		quotedExpr = true;
	} else {
		quotedExpr = false;
	}

	for (int source = 0; source < length; ++source) {
		char ch = text[source];
		// scan for escapes, a terminating slash cannot be an escape
		if (ch == '\\' && source < length - 1) {
			unsigned int number = 0;
			++source; // skip the \ character
			ch = text[source];

			switch(ch) {
			case '\"':	ch = '\"'; break;
			case 'b':	ch = '\b'; break;
			case 'f':	ch = '\f'; break;
			case 'n':	ch = '\n'; break;
			case 'r':	ch = '\r'; break;
			case 't':	ch = '\t'; break;
			case '\\':	ch = '\\'; break;
			case 'u':
				// This is an escaped Unicode character of the form
				//   \uXXXX. Convert to utf-8.
				// TODO This doesn't properly handle utf-16 surrogate
				//   pairs in the json string. They should be converted
				//   to a single utf-8 4-byte character.
				if ( source + 4 >= length ) {
					validStr = false;
					return;
				}
				number = 0;
				for ( int i = 1; i <= 4; i++ ) {
					char ch2 = text[source + i];
					number = number << 4;
					switch(ch2) {
					case '0': case '1': case '2': case '3': case '4':
					case '5': case '6': case '7': case '8': case '9':
						number += ch2 - '0';
						break;
					case 'a': case 'b': case 'c':
					case 'd': case 'e': case 'f':
						ch2 -= 32;
						[[fallthrough]];
					case 'A': case 'B': case 'C':
					case 'D': case 'E': case 'F':
						number += ch2 +10 - 'A';
						break;
					default:
						validStr = false;
						return;
					}
				}
				source += 4;
				if ( number == 0 ) {
					validStr = false;
					return;
				}
				if ( number >= 0x0800 ) {
					// convert to 3-byte utf-8 character
					text[dest++] = 0x11100000 | (number >> 12);
					text[dest++] = 0x10000000 | ((number >> 6) | 0x00111111);
					text[dest++] = 0x10000000 | (number | 0x00111111);
					continue;
				} else if ( number >= 0x0080 ) {
					// convert to 2-byte utf-8 character
					text[dest++] = 0x11000000 | (number >> 6);
					text[dest++] = 0x10000000 | (number & 0x00111111);
					continue;
				}
				ch = number;
				break;
			default:
				// pass char after \ unmodified.
				break;
			}
		}

		if (dest == source) {
			// no need to assign ch to text when we haven't seen any escapes yet.
			// text[dest] = ch;
			++dest;
		} else {
			text[dest] = ch;
			++dest;
		}
	}

	if (dest < length) {
		text.erase(dest, std::string::npos);
	}
}


// Append a formated 64 bit int to a string.
// much faster than sprintf because it ignores locale

void
append_long(std::string &s, long long l) {
	char buf[28]; // build up the string backwards here
	char *p = buf;

	if (l >= 0) {
		do {
			*p = '0' + l % 10;
			p++;
		} while (l /= 10);

		while (p != buf) {
			p--;
			s += *p;
		}
		return;

	} else {
		s += '-';
		do {
			// a negative number mod 10 is a negative
			*p = '0' - l % 10;
			p++;
		} while (l /= 10);

		while (p != buf) {
			p--;
			s += *p;
		}
		return;
	}
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
    snprintf(timebuf, sizeof(timebuf), "%c%02d:%02d", sign, tzsecs / 3600, (tzsecs / 60) % 60);
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
            snprintf(timebuf, sizeof(timebuf), "%d+%02d:%02d:%02d", days, hrs, mins, (int) secs);
        } else {
            snprintf(timebuf, sizeof(timebuf), "%d+%02d:%02d:%02g", days, hrs, mins, secs);
        }
        buffer += timebuf;
    } else if (hrs) {
        if (fractional_seconds == 0) {
            snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d", hrs, mins, (int) secs);
        } else {
            snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02g", hrs, mins, secs);
        }
        buffer += timebuf;
    } else if (mins) {
        if (fractional_seconds == 0) {
            snprintf(timebuf, sizeof(timebuf), "%02d:%02d", mins, (int) secs);
        } else {
            snprintf(timebuf, sizeof(timebuf), "%02d:%02g", mins, secs);
        }
        buffer += timebuf;
        return;
    } else {
        if (fractional_seconds == 0) {
            snprintf(timebuf, sizeof(timebuf), "%02d", (int) secs);
        } else {
            snprintf(timebuf, sizeof(timebuf), "%02g", secs);
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

