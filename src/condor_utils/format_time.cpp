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


/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/
char *
format_date( time_t date )
{
    static char buf[ 48 ];
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
    static char buf[ 60 ];
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
   Call format_time and returns the format_time char * return with an offset
   to remove un-needed characters: space, 0, '+', until first ':'
   Minum return: 00:00
*/
char *
format_time_short( int tot_secs )
{
     //Get time in ddd+hh:mm:ss fromat
     char *time = format_time(tot_secs);

     int offset = 0;
     while (time[offset]) { //Run till break
          //If character is not whitespace, 0, or the + symbol we stop
          if ( time[offset] != ' ' && time[offset] != '0' && time[offset] != '+' ){
               //Check to see if current offset character is ':'
               if ( time[offset] == ':' ) offset++; //If so then increment to only display mm:ss
               break;
          }
          offset++;
     }

     //return formatted time with offset
     return (time + offset);
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


