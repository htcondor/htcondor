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
#include "condor_network.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "MyString.h"

#include "condor_commands.h"
#include "condor_io.h"

//------------------------------------------------------------------------

static int CalcTime(int,int,int);

//------------------------------------------------------------------------

static void Usage(char* name) 
{
  fprintf(stderr, "Usage: %s [-lastday | -lastweek | -lastmonth | -from m d y [-to m d y]] {-resourcequery <name> | -resourcelist | -resgroupquery <name> | -resgrouplist | -userquery <name> | -userlist}\n");
  exit(1);
}

//------------------------------------------------------------------------

static const int MinuteSec=60;
static const int HourSec=MinuteSec*60;
static const int DaySec=HourSec*24;
static const int WeekSec=DaySec*7;
static const int MonthSec=DaySec*30;
static const int YearSec=DaySec*365;

//------------------------------------------------------------------------

main(int argc, char* argv[])
{
  time_t Now=time(0);
  time_t ToDate=Now;
  time_t FromDate=ToDate-60*60*24;

  int QueryType=-1;
  MyString QueryArg;

  MyString FileName="outfile.dat";

  for(int i=1; i<argc; i++) {

    // Analyze date specifiers

    if (strcmp(argv[i],"-lastday")==0) {
      ToDate=Now;
      FromDate=ToDate-DaySec;
    }
    else if (strcmp(argv[i],"-lastweek")==0) {
      ToDate=Now;
      FromDate=ToDate-WeekSec;
    }
    else if (strcmp(argv[i],"-lastmonth")==0) {
      ToDate=Now;
      FromDate=ToDate-MonthSec;
    }
    else if (strcmp(argv[i],"-from")==0) {
      if (argc-i<=3) Usage(argv[0]);
      int month=atoi(argv[i+1]);
      int day=atoi(argv[i+2]);
      int year=atoi(argv[i+3]);
      FromDate=CalcTime(month,day,year);
      i+=3;
    }
    else if (strcmp(argv[i],"-to")==0) {
      if (argc-i<=3) Usage(argv[0]);
      int month=atoi(argv[i+1]);
      int day=atoi(argv[i+2]);
      int year=atoi(argv[i+3]);
      ToDate=CalcTime(month,day,year);
      i+=3;
    }

    // Query type

    else if (strcmp(argv[i],"-resourcelist")==0) {
      QueryType=QUERY_HIST_STARTD_LIST;
    }
    else if (strcmp(argv[i],"-resourcequery")==0) {
      QueryType=QUERY_HIST_STARTD;
      if (argc-i<=1) Usage(argv[0]);
      QueryArg=argv[i+1];
      i++;
    }
    else if (strcmp(argv[i],"-resgrouplist")==0) {
      QueryType=QUERY_HIST_GROUPS_LIST;
    }
    else if (strcmp(argv[i],"-resgroupquery")==0) {
      QueryType=QUERY_HIST_GROUPS;
      if (argc-i<=1) Usage(argv[0]);
      QueryArg=argv[i+1];
      i++;
    }
    else if (strcmp(argv[i],"-userlist")==0) {
      QueryType=QUERY_HIST_USER_LIST;
    }
    else if (strcmp(argv[i],"-userquery")==0) {
      QueryType=QUERY_HIST_USER;
      if (argc-i<=1) Usage(argv[0]);
      QueryArg=argv[i+1];
      i++;
    }

    // miscaleneous

    else if (strcmp(argv[i],"-f")==0) {
      if (argc-i<=1) Usage(argv[0]);
      FileName=argv[i+1];
      i++;
    }
    else {
      Usage(argv[0]);
    }
  }

  // Check validity or arguments
  if (QueryType==-1 || FromDate<0 || FromDate>Now || ToDate<FromDate) Usage(argv[0]);
  if (ToDate>Now) ToDate=Now;

  config( 0 );
  char* CondorViewHost = param("CONDOR_VIEW_HOST");
  if (!CondorViewHost) {
    fprintf(stderr, "No CONDOR_VIEW_HOST specified in config file\n");
    exit(1);
  }

  char Line[200], *LinePtr;
  strcpy(Line, (const char*) QueryArg);

  ReliSock sock(CondorViewHost, CONDOR_VIEW_PORT);
  sock.encode();
  if (!sock.put(QueryType) ||
      !sock.put(FromDate) ||
      !sock.put(ToDate) ||
      !sock.put(QueryType) ||
      !sock.put(Line) ||
      !sock.end_of_message()) {
    fprintf(stderr, "failed to send query to the CondorView server\n");
    exit(1);
  }

  sock.decode(); 
  FILE* outfile=fopen(FileName,"w");

  while(1) {
    if (!sock.get(LinePtr)) {
      fprintf(stderr, "failed to receive data from the CondorView server\n");
      exit(1);
    }
    if (strlen(LinePtr)==0) break;
    fputs(LinePtr, outfile);
  }
  if (!sock.end_of_message()) {
    fprintf(stderr, "failed to receive data from the CondorView server\n");
  }

  fclose(outfile);

  return 0;
}

int CalcTime(int month, int day, int year) {
  return 0;
}
