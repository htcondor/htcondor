#include <stdio.h>
#include <time.h>
#include "types.h"

/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/
char *
format_date( time_t date )
{
    static char buf[ 12 ];
    struct tm   *tm;

    tm = localtime( &date );
    sprintf( buf, "%2d/%-2d %02d:%02d",
        (tm->tm_mon)+1, tm->tm_mday, tm->tm_hour, tm->tm_min
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

    days = tot_secs / DAY;
    tot_secs %= DAY;
    hours = tot_secs / HOUR;
    tot_secs %= HOUR;
    min = tot_secs / MINUTE;
    secs = tot_secs % MINUTE;

    (void)sprintf( answer, "%3d+%02d:%02d:%02d", days, hours, min, secs );
    return answer;
}


