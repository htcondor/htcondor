/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"


/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/
char *
format_date( time_t date )
{
    static char buf[ 12 ];
    struct tm   *tm;

	if (date<0) {
		strcpy(buf,"    ???    ");
		return buf;
	}

    tm = localtime( &date );
    sprintf( buf, "%2d/%-2d %02d:%02d",
        (tm->tm_mon)+1, tm->tm_mday, tm->tm_hour, tm->tm_min
    );
    return buf;
}

/*
  Format a date expressed in "UNIX time" into "month/day/year hour:minute".
*/
char *
format_date_year( time_t date )
{
    static char buf[ 18 ];
    struct tm   *tm;

	if (date<0) {
		strcpy(buf,"    ???    ");
		return buf;
	}

    tm = localtime( &date );
    sprintf( buf, "%2d/%02d/%-4d %02d:%02d",
        (tm->tm_mon)+1, tm->tm_mday, (tm->tm_year + 1900), tm->tm_hour, tm->tm_min
    );
    return buf;
}

/*
  Format a time value which is encoded as seconds since the UNIX
  "epoch".  We return a string in the format dd+hh:mm:ss, indicating
  days, hours, minutes, and seconds.  The string is in static data
  space, and will be overwritten by the next call to this function.
*/
char    *
format_time( int tot_secs )
{
    int     days;
    int     hours;
    int     min;
    int     secs;
    static char answer[25];

	if ( tot_secs < 0 ) {
		sprintf(answer,"[?????]");
		return answer;
	}

    days = tot_secs / DAY;
    tot_secs %= DAY;
    hours = tot_secs / HOUR;
    tot_secs %= HOUR;
    min = tot_secs / MINUTE;
    secs = tot_secs % MINUTE;

    (void)sprintf( answer, "%3d+%02d:%02d:%02d", days, hours, min, secs );
    return answer;
}

/*
  Same as format_time above but don't print seconds field.
*/
char    *
format_time_nosecs( int tot_secs )
{
    int     days;
    int     hours;
    int     min;
    static char answer[25];

	if ( tot_secs < 0 ) {
		sprintf(answer,"[?????]");
		return answer;
	}

    days = tot_secs / DAY;
    tot_secs %= DAY;
    hours = tot_secs / HOUR;
    tot_secs %= HOUR;
    min = tot_secs / MINUTE;

    (void)sprintf( answer, "%3d+%02d:%02d", days, hours, min );
    return answer;
}


