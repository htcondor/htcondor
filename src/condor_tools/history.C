#include "condor_common.h"
#include <iostream.h>
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "MyString.h"
#include "TimeClass.h"

//------------------------------------------------------------------------

static void displayJobShort(ClassAd *ad);
static void short_header(void);
static void short_print(int,int,const char*,int,int,int,int,int,const char *);
static void short_header (void);
static void displayJobShort (ClassAd *);
static void shorten (char *, int);
static char* format_date( time_t date );
static char* format_time( int tot_secs );
static char encode_status( int status );

//------------------------------------------------------------------------

main()
{
  config( 0 );
  MyString LogFileName=param("SPOOL");
  LogFileName+="/MatchStat.log";
  FILE* LogFile=fopen(LogFileName,"r");
  if (!LogFile) {
    cerr << "ERROR - failed to open log file " << LogFileName.Value() << endl;
    exit(1);
  }
  
  int EndFlag=0;
  while(!EndFlag) {
    ClassAd* ad=new ClassAd(LogFile,"***",EndFlag);
    displayJobShort(ad);
  }
 
  fclose(LogFile);
  exit(0);
}

//------------------------------------------------------------------------

static void
displayJobShort (ClassAd *ad)
{
        int cluster, proc, date, status, prio, image_size;
        float utime;
        char owner[64], cmd[2048], args[2048];

        if (!ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cluster)           ||
                !ad->EvalInteger (ATTR_PROC_ID, NULL, proc)                             ||
                !ad->EvalInteger (ATTR_Q_DATE, NULL, date)                              ||
                !ad->EvalFloat   (ATTR_JOB_REMOTE_USER_CPU, NULL, utime)||
                !ad->EvalInteger (ATTR_JOB_STATUS, NULL, status)                ||
                !ad->EvalInteger (ATTR_JOB_PRIO, NULL, prio)                    ||
                !ad->EvalInteger (ATTR_IMAGE_SIZE, NULL, image_size)    ||
                !ad->EvalString  (ATTR_OWNER, NULL, owner)                              ||
                !ad->EvalString  (ATTR_JOB_CMD, NULL, cmd) )
        {
                printf (" --- ???? --- \n");
                return;
        }
        
        shorten (owner, 14);
        if (ad->EvalString ("Args", NULL, args)) strcat (cmd, args);
        shorten (cmd, 18);
        //short_print (cluster, proc, owner, date, (int)utime, status, prio,
        //                                image_size, cmd); 

}

//------------------------------------------------------------------------

static void
short_header (void)
{
    printf( " %-7s %-14s %11s %12s %-2s %-3s %-4s %-18s\n",
        "ID",
        "OWNER",
        "SUBMITTED",
        "CPU_USAGE",
        "ST",
        "PRI",
        "SIZE",
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
        int time,
        int status,
        int prio,
        int image_size,
        const char *cmd
        ) {
        printf( "%4d.%-3d %-14s %-11s %-12s %-2c %-3d %-4.1f %-18s\n",
                cluster,
                proc,
                owner,
                format_date(date),
                format_time(time),
                encode_status(status),
                prio,
                image_size/1024.0,
                cmd
        );
}

//------------------------------------------------------------------------

/*
  Format a date expressed in "UNIX time" into "month/day hour:minute".
*/

static char* format_date( time_t date )
{
        static char     buf[ 12 ];
        struct tm       *tm;

        tm = localtime( &date );
        sprintf( buf, "%2d/%-2d %02d:%02d",
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

        days = tot_secs / DAY;
        tot_secs %= DAY;
        hours = tot_secs / HOUR;
        tot_secs %= HOUR;
        min = tot_secs / MINUTE;
        secs = tot_secs % MINUTE;

        (void)sprintf( answer, "%3d+%02d:%02d:%02d", days, hours, min, secs );
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
        switch( status ) {
          case UNEXPANDED:
                return 'U';
          case IDLE:
                return 'I';
          case RUNNING:
                return 'R';
          case COMPLETED:
                return 'C';
          case REMOVED:
                return 'X';
          default:
                return ' ';
        }
}

