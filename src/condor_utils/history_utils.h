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


#ifndef _HISTORY_UTILS_H_
#define _HISTORY_UTILS_H_

#include "condor_common.h"
#include "condor_classad.h"
#include "proc.h"

//------------------------------------------------------------------------

void displayJobShort(ClassAd* ad);
void short_header(void);
static void short_print(int,int,const char*,int,int,int,int,int,int,const char *);
static void shorten (char *, int);
static const char* format_date( time_t date );
static char* format_time( int tot_secs );
static char encode_status( int status );


//------------------------------------------------------------------------

//------------------------------------------------------------------------

void
displayJobShort(ClassAd* ad)
{
    int cluster, proc, date, status, prio, image_size, memory_usage, CompDate;
	// Initialization here not strictly necessary, but some compilers (gcc)
	// are unable to determine that the member will always be initialized in
	// LookupFloat below.
    double utime = 0;
    char *owner, *cmd, *args;

    owner = NULL;
    cmd   = NULL;
    args  = NULL;

	if(!ad->LookupFloat(ATTR_JOB_REMOTE_WALL_CLOCK,utime)) {
		if(!ad->LookupFloat(ATTR_JOB_REMOTE_USER_CPU,utime)) {
			utime = 0;
		}
	}

        if (!ad->LookupInteger (ATTR_CLUSTER_ID, cluster)           ||
                !ad->LookupInteger (ATTR_PROC_ID, proc)             ||
                !ad->LookupInteger (ATTR_Q_DATE, date)              ||
                !ad->LookupInteger (ATTR_COMPLETION_DATE, CompDate)	||
                !ad->LookupInteger (ATTR_JOB_STATUS, status)        ||
                !ad->LookupInteger (ATTR_JOB_PRIO, prio)            ||
                !ad->LookupInteger (ATTR_IMAGE_SIZE, image_size)    ||
                !ad->LookupString  (ATTR_OWNER, &owner)             ||
                !ad->LookupString  (ATTR_JOB_CMD, &cmd) )
        {
                printf (" --- ???? --- \n");
				free(owner);
				free(cmd);
                return;
        }
        
	// print memory usage unless it's unavailable, then print image size
	// note that MemoryUsage is megabytes, but image_size is kilobytes.
	if (!ad->LookupInteger(ATTR_MEMORY_USAGE, memory_usage)) {
		memory_usage = (image_size+1023)/1024;
	}

        shorten (owner, 14);
        if (ad->LookupString ("Args", &args)) {
            int cmd_len = (int)strlen(cmd);
            int extra_len = 14 - cmd_len;
            if (extra_len > 0) {
                void * pv = realloc(cmd, 16); ASSERT ( pv != NULL );
                cmd = (char *)pv;
                strcat(cmd, " ");
                strncat(cmd, args, extra_len);
            }
        }
        shorten (cmd, 15);
        short_print (cluster, proc, owner, date, CompDate, (int)utime, status, 
               prio, memory_usage, cmd); 


        free(owner);
        free(cmd);
        free(args);
        return;
}

//------------------------------------------------------------------------

void
short_header (void)
{
    printf( " %-7s %-14s %11s %12s %-2s %11s %-15s\n",
        "ID",
        "OWNER",
        "SUBMITTED",
        "RUN_TIME",
        "ST",
		"COMPLETED",
        "CMD"
    );
}

//------------------------------------------------------------------------

static void
shorten (char *buff, int len)
{
    if ((unsigned int)strlen (buff) > (unsigned int)len) buff[len] = '\0';
}

//------------------------------------------------------------------------

/*
  Print a line of data for the "short" display of a PROC structure.  The
  "short" display is the one used by "condor_q".  N.B. the columns used
  by this routine must match those defined by the short_header routine
  defined above.
*/

static void
short_print(
        int cluster,
        int proc,
        const char *owner,
        int date,
		int CompDate,
        int time,
        int status,
        int  /*prio*/,
        int  /*image_size*/,
        const char *cmd
        ) {
		std::string SubmitDateStr=format_date(date);
		std::string CompDateStr=format_date(CompDate);
        printf( "%4d.%-3d %-14s %-11s %-12s %-2c %-11s %-15s\n",
                cluster,
                proc,
                owner,
                SubmitDateStr.c_str(),
                format_time(time),
                encode_status(status),
                CompDateStr.c_str(),
                cmd
        );
}


//------------------------------------------------------------------------

/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/

static const char* format_date( time_t date )
{
        static char     buf[ 48 ];
        struct tm       *tm;

		if (date==0) return " ??? ";

        tm = localtime( &date );
        snprintf( buf, sizeof(buf), "%2d/%-2d %02d:%02d",
                (tm->tm_mon)+1, tm->tm_mday, tm->tm_hour, tm->tm_min
        );
        return buf;
}

//------------------------------------------------------------------------

/*
  Format a time value which is encoded as seconds since the UNIX
  "epoch".  We return a string in the format dd+hh:mm:ss, indicating
  days, hours, minutes, and seconds.  The string is in static data
  space, and will be overwritten by the next call to this function.
*/

static char     *
format_time( int tot_secs )
{
        int             days;
        int             hours;
        int             min;
        int             secs;
        static char     answer[25];

		if ( tot_secs < 0 ) {
			snprintf(answer,sizeof(answer),"[?????]");
			return answer;
		}

        days = tot_secs / DAY;
        tot_secs %= DAY;
        hours = tot_secs / HOUR;
        tot_secs %= HOUR;
        min = tot_secs / MINUTE;
        secs = tot_secs % MINUTE;

		std::ignore = snprintf( answer, sizeof(answer), "%3d+%02d:%02d:%02d", days, hours, min, secs );
        return answer;
}

//------------------------------------------------------------------------

/*
  Encode a status from a PROC structure as a single letter suited for
  printing.
*/

static char
encode_status( int status )
{
    static const char encode[JOB_STATUS_MAX+1] = {
        0,	 //zero
        'I',	 //IDLE					1
        'R',	 //RUNNING				2
        'X',	 //REMOVED				3
        'C',	 //COMPLETED			4
        'H',	 //HELD					5
        '>',	 //TRANSFERRING_OUTPUT	6
        'S',	 //SUSPENDED			7
        'F',	 //JOB_STATUS_FAILED	8
        'B',	 //JOB_STATUS_BLOCKED	9
    };

    if (status < JOB_STATUS_MIN || status > JOB_STATUS_MAX) {
        return ' ';
    }
    return encode[status];
}

#endif
