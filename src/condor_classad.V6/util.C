/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#include "common.h"
#include "util.h"
#include "limits.h"

using namespace std;

#ifndef __APPLE_CC__
extern DLL_IMPORT_MAGIC long timezone;
#endif

BEGIN_NAMESPACE( classad )

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

long timezone_offset(void)
{
#ifdef __APPLE_CC__
    static long tz_offset = 0;
    static bool have_tz_offset = false;

    if (!have_tz_offset) {
        struct tm  *tms;
        time_t     clock;

        time(&clock);
        tms = localtime(&clock);
        tz_offset = -tms->tm_gmtoff;
        if (0 != tms->tm_isdst) {
            tz_offset += 3600;
        }
    }
    return tz_offset;
#else
    return ::timezone;
#endif
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
					int  number;
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
						new_char = text[source];
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
    epoch_time = atime.secs;
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
#elif (defined (__SVR4) && defined (__sun)) || defined(__APPLE_CC__)
#ifndef __APPLE_CC__
#include <ieeefp.h>
#endif
int classad_isinf(double x) 
{ 
    if (finite(x) || x != x) {
        return 0;
    } else if (x > 0) {
        return 1;
    } else {
        return -1;
    }
}
#else
int classad_isinf(double x) 
{
    return isinf(x);
}
#endif 

#ifdef  __APPLE_CC__
int classad_isnan(double x)
{
    return __isnan(x);
}
#else
int classad_isnan(double x)
{
    return isnan(x);
}
#endif

#ifdef WIN32
bool classad_isneg(double x)
{
    int numclass = _fpclass(real);
    if (   numclass == _FPCLASS_NZ 
        || numclass == _FPCLASS_NINF
        || numcalss == _FPCLASS_NN
        || numclass == _FPCLASS_ND){
        return true;
    } else {
        return false;
    }
}
#elif defined (__SVR4) && defined (__sun)
#include <ieeefp.h>
bool classad_isneg(double x)
{
    int numclass = fpclass(x);

    if (   numclass == FP_NZERO 
        || numclass == FP_NINF
        || numclass ==  FP_NDENORM
        || numclass == FP_NNORM)
        return true;
    } else {
        return false;
    }
}
#elif defined signbit
bool classad_isneg(double x)
{
    if (signbit(x) != 0) {
        return true;
    } else {
        return false;
    }
}
#else
bool classad_isneg(double x)
{
    if (__signbit(x) != 0) {
        return true;
    } else {
        return false;
    }
}
#endif
END_NAMESPACE // classad

