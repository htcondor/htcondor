/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_distribution.h"

#include "MyString.h"

//------------------------------------------------------------------------

static void displayJobShort(ClassAd* ad);
static void short_header(void);
static void short_print(int,int,const char*,int,int,int,int,int,int,const char *);
static void short_header (void);
static void shorten (char *, int);
static char* format_date( time_t date );
static char* format_time( int tot_secs );
static char encode_status( int status );
static bool EvalBool(ClassAd *ad, ExprTree *tree);

//------------------------------------------------------------------------

static void Usage(char* name) 
{
  printf("Usage: %s [-l] [-f history-filename] [-constraint expr | cluster_id | cluster_id.proc_id | owner]\n",name);
  exit(1);
}

//------------------------------------------------------------------------


int
main(int argc, char* argv[])
{
  char* JobHistoryFileName=NULL;
  int LongFormat=FALSE;
  char* constraint=NULL;
  ExprTree *constraintExpr=NULL;
  int cluster, proc;
  char tmp[512];
  int i;
  myDistro->Init( argc, argv );

  for(i=1; i<argc; i++) {
    if (strcmp(argv[i],"-l")==0) {
      LongFormat=TRUE;   
    }
    else if (strcmp(argv[i],"-f")==0) {
      if (i+1==argc || JobHistoryFileName) break;
      i++;
	  JobHistoryFileName=argv[i];
    }
    else if (strcmp(argv[i],"-help")==0) {
	  Usage(argv[0]);
    }
    else if (strcmp(argv[i],"-constraint")==0) {
      if (i+1==argc || constraint) break;
      sprintf(tmp,"(%s)",argv[i+1]);
      constraint=tmp;
      i++;
    }
    else if (sscanf (argv[i], "%d.%d", &cluster, &proc) == 2) {
      if (constraint) break;
      sprintf (tmp, "((%s == %d) && (%s == %d))", 
               ATTR_CLUSTER_ID, cluster,ATTR_PROC_ID, proc);
      constraint=tmp;
    }
    else if (sscanf (argv[i], "%d", &cluster) == 1) {
      if (constraint) break;
      sprintf (tmp, "(%s == %d)", ATTR_CLUSTER_ID, cluster);
      constraint=tmp;
    }
    else {
      if (constraint) break;
      sprintf(tmp, "(%s == \"%s\")", ATTR_OWNER, argv[i]);
      constraint=tmp;
    }
  }
  if (i<argc) Usage(argv[0]);

if (constraint) puts(constraint);

  config();
  if (!JobHistoryFileName) {
    JobHistoryFileName=param("HISTORY");
  }

  FILE* LogFile=fopen(JobHistoryFileName,"r");
  if (!LogFile) {
    fprintf(stderr,"History file not found or empty.\n");
    exit(1);
  }

  // printf("HistroyFile=%s\nLongFormat=%d\n",JobHistoryFileName,LongFormat);
  // if (constraint) printf("constraint=%s\n",constraint);

  ClassAdParser parser;	// NAC
  if( constraint && !parser.ParseExpression( constraint, 
											 constraintExpr ) ) {	//NAC
	  fprintf( stderr, "Error:  could not parse constraint %s\n", constraint );
	  exit( 1 );
  }

  int EndFlag=0;
  int ErrorFlag=0;
  int EmptyFlag=0;
  ClassAd *ad=0;
  if (!LongFormat) short_header();
  while(!EndFlag) {
    if( !( ad=new ClassAd(LogFile,"***", EndFlag, ErrorFlag, EmptyFlag) ) ){
      fprintf( stderr, "Error:  Out of memory\n" );
      exit( 1 );
    } 
    if( ErrorFlag ) {
      printf( "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
      ErrorFlag=0;
      delete ad;
      continue;
    } 
	if( EmptyFlag ) {
      EmptyFlag=0;
      delete ad;
      continue;
    }
    if (!constraint || EvalBool(ad, constraintExpr)) {
      if (LongFormat) { 
	    ad->fPrint(stdout); printf("\n"); 
	  } else {
        displayJobShort(ad);
	  }
    }
    delete ad;
  }
 
  fclose(LogFile);
  return 0;
}

//------------------------------------------------------------------------

static void
displayJobShort(ClassAd* ad)
{
        int cluster, proc, date, status, prio, image_size, CompDate;
        float utime;
        char owner[64], cmd[2048], args[2048];

	if(!ad->EvalFloat(ATTR_JOB_REMOTE_WALL_CLOCK,NULL,utime)) {
		if(!ad->EvalFloat(ATTR_JOB_REMOTE_USER_CPU,NULL,utime)) {
			utime = 0;
		}
	}

        if (!ad->EvalInteger (ATTR_CLUSTER_ID, NULL, cluster)           ||
                !ad->EvalInteger (ATTR_PROC_ID, NULL, proc)             ||
                !ad->EvalInteger (ATTR_Q_DATE, NULL, date)              ||
                !ad->EvalInteger (ATTR_COMPLETION_DATE, NULL, CompDate)	||
                !ad->EvalInteger (ATTR_JOB_STATUS, NULL, status)        ||
                !ad->EvalInteger (ATTR_JOB_PRIO, NULL, prio)            ||
                !ad->EvalInteger (ATTR_IMAGE_SIZE, NULL, image_size)    ||
                !ad->EvalString  (ATTR_OWNER, NULL, owner)              ||
                !ad->EvalString  (ATTR_JOB_CMD, NULL, cmd) )
        {
                printf (" --- ???? --- \n");
                return;
        }
        
        shorten (owner, 14);
        if (ad->EvalString ("Args", NULL, args)) strcat (cmd, args);
        shorten (cmd, 15);
        short_print (cluster, proc, owner, date, CompDate, (int)utime, status, 
               prio, image_size, cmd); 

}

//------------------------------------------------------------------------

static void
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
        int prio,
        int image_size,
        const char *cmd
        ) {
		MyString SubmitDateStr=format_date(date);
		MyString CompDateStr=format_date(CompDate);
        printf( "%4d.%-3d %-14s %-11s %-12s %-2c %-11s %-15s\n",
                cluster,
                proc,
                owner,
                SubmitDateStr.Value(),
                format_time(time),
                encode_status(status),
                CompDateStr.Value(),
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

		if (date==0) return " ??? ";

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

//------------------------------------------------------------------------

static bool EvalBool(ClassAd* ad, ExprTree *tree)	// NAC
{
	Value result;
	bool boolValue;
	tree->SetParentScope( ad );
	if( !ad->EvaluateExpr( tree, result ) ) {
		delete tree;
		return false;
	}
	if( result.IsBooleanValue( boolValue ) ) {
		return boolValue;
	}
	
	return false;
}
