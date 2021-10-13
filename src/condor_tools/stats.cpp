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
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "condor_commands.h"
#include "condor_io.h"
#include "daemon.h"
#include "condor_distribution.h"
#define NUM_ELEMENTS(_ary)   (sizeof(_ary) / sizeof((_ary)[0]))

//------------------------------------------------------------------------

static int CalcTime(int,int,int);
static int TimeLine(const std::string& Name,int FromDate, int ToDate, int Res);

//------------------------------------------------------------------------

static void Usage(char* name) 
{
  fprintf(stderr, "Usage: %s [-f filename] [-orgformat] [-pool hostname] [-lastday | -lastweek | -lastmonth | -lasthours h | -from m d y [-to m d y]] {-resourcequery <name> | -resourcelist | -resgroupquery <name> | -resgrouplist | -userquery <name> | -userlist | -usergroupquery <name> | -usergrouplist | -ckptquery | -ckptlist}\n",name);
  exit(1);
}

//------------------------------------------------------------------------

const int MinuteSec=60;
const int HourSec=MinuteSec*60;
const int DaySec=HourSec*24;
const int WeekSec=DaySec*7;
const int MonthSec=DaySec*30;
//const int YearSec=DaySec*365;

//------------------------------------------------------------------------

/*
int GrpConvStr(char* InpStr, char* OutStr)
{
  float T,Unclaimed, Matched, Claimed,Preempting,Owner;
  if (sscanf(InpStr," %f %f %f %f %f %f",&T,&Unclaimed,&Matched,&Claimed,&Preempting,&Owner)!=6) return -1;
  sprintf(OutStr,"%f\t%f\t%f\t%f\t%f\t%f\n",T,Unclaimed,Matched,Claimed,Preempting,Owner);
  return 0;
}
*/

//------------------------------------------------------------------------

int ResConvStr(char* InpStr, char* OutStr)
{
  float T,LoadAvg;
  int KbdIdle;
  unsigned int State;
  if (sscanf(InpStr," %f %d %f %d",&T,&KbdIdle,&LoadAvg,&State)!=4) return -1;
  const char *stateStr;
  // Note: This should be kept in sync with condor_state.h, and with
  // StartdScanFunc() of view_server.C, and with
  // typedef enum ViewStates of view_server.h
  const char* StateName[] = {
	  "UNCLAIMED",
	  "MATCHED",
	  "CLAIMED",
	  "PRE-EMPTING",
	  "OWNER",
	  "SHUTDOWN",
	  "DELETE",
	  "BACKFILL",
	  "DRAINED",
  };
  if (State <= NUM_ELEMENTS(StateName) ) {
	stateStr = StateName[State-1];
  } else {
	stateStr = "UNKNOWN";
  }
  sprintf(OutStr,"%f\t%d\t%f\t%s\n",T,KbdIdle,LoadAvg,stateStr);
  return 0;
}

//------------------------------------------------------------------------

int main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
  int Now=time(0);
  int ToDate=Now;
  int FromDate=ToDate-DaySec;
  int Options=0;

  int QueryType=-1;
  std::string QueryArg;

  std::string FileName;
  std::string TimeFileName;
  char* pool = NULL;

  myDistro->Init( argc, argv );

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
    else if (strcmp(argv[i],"-lasthours")==0) {
      if (argc-i<=1) Usage(argv[0]);
      int hours=atoi(argv[i+1]);  
      ToDate=Now;
      FromDate=ToDate-HourSec*hours;
      i++;
    }
    else if (strcmp(argv[i],"-from")==0) {
      if (argc-i<=3) Usage(argv[0]);
      int month=atoi(argv[i+1]);
      int day=atoi(argv[i+2]);
      int year=atoi(argv[i+3]);
      FromDate=CalcTime(month,day,year);
      // printf("Date translation: %d/%d/%d = %d\n",month,day,year,FromDate);
      i+=3;
    }
    else if (strcmp(argv[i],"-to")==0) {
      if (argc-i<=3) Usage(argv[0]);
      int month=atoi(argv[i+1]);
      int day=atoi(argv[i+2]);
      int year=atoi(argv[i+3]);
      ToDate=CalcTime(month,day,year);
      // printf("Date translation: %d/%d/%d = %d\n",month,day,year,ToDate);
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
      QueryType=QUERY_HIST_SUBMITTOR_LIST;
    }
    else if (strcmp(argv[i],"-userquery")==0) {
      QueryType=QUERY_HIST_SUBMITTOR;
      if (argc-i<=1) Usage(argv[0]);
      QueryArg=argv[i+1];
      i++;
    }
    else if (strcmp(argv[i],"-usergrouplist")==0) {
      QueryType=QUERY_HIST_SUBMITTORGROUPS_LIST;
    }
    else if (strcmp(argv[i],"-usergroupquery")==0) {
      QueryType=QUERY_HIST_SUBMITTORGROUPS;
      if (argc-i<=1) Usage(argv[0]);
      QueryArg=argv[i+1];
      i++;
    }
    else if (strcmp(argv[i],"-ckptlist")==0) {
      QueryType=QUERY_HIST_CKPTSRVR_LIST;
    }
    else if (strcmp(argv[i],"-ckptquery")==0) {
      QueryType=QUERY_HIST_CKPTSRVR;
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
    else if (strcmp(argv[i],"-timedat")==0) {
      if (argc-i<=1) Usage(argv[0]);
      TimeFileName=argv[i+1];
      i++;
    }
    else if (strcmp(argv[i],"-orgformat")==0) {
      Options=1;
	}
    else if (strcmp(argv[i],"-pool")==0) {
      if (argc-i<=1) Usage(argv[0]);
	  pool=argv[i+1];
      i++;
    }
    else {
      Usage(argv[0]);
    }
  }

  // Check validity or arguments
  if (QueryType==-1 || FromDate<0 || FromDate>Now || ToDate<FromDate) Usage(argv[0]);
  // if (ToDate>Now) ToDate=Now;

  set_priv_initialize(); // allow uid switching if root
  config();

  Daemon view_host( DT_VIEW_COLLECTOR, 0, pool );
  if( ! view_host.locate() ) {
	  fprintf( stderr, "%s: %s\n", argv[0], view_host.error() );
	  exit(1);
  }

  const int MaxLen=200;
  char Line[MaxLen+1];
  char* LinePtr=Line;

  if (QueryArg.length()>MaxLen) {
    fprintf(stderr, "Command line argument is too long\n");
    exit(1);
  }
  strcpy(LinePtr, QueryArg.c_str());

  if (TimeFileName.length()>0) TimeLine(TimeFileName,FromDate,ToDate,10);

  // Open the output file
  FILE* outfile=stdout;
  if (FileName.length()>0) outfile=safe_fopen_wrapper_follow(FileName.c_str(),"w");

  if (outfile == NULL) {
	fprintf( stderr, "Failed to open file %s\n", FileName.c_str() );
	exit( 1 );
  }

  int LineCount=0;

  ReliSock sock;
  bool connect_error = true;
  do {
    if (sock.connect(view_host.addr(), 0)) {
      connect_error = false;
      break;
    }
  } while (view_host.nextValidCm() == true);

  if (connect_error == true) {
      fprintf( stderr, "failed to connect to the CondorView server (%s)\n", 
			 view_host.fullHostname() );
      fputs("No Data.\n",outfile);
      exit(1);
  }

  view_host.startCommand(QueryType, &sock);

  sock.encode();
  if (!sock.code(FromDate) ||
      !sock.code(ToDate) ||
      !sock.code(Options) ||
      !sock.code(LinePtr) ||
      !sock.end_of_message()) {
    fprintf( stderr, "failed to send query to the CondorView server %s\n",
			 view_host.fullHostname() );
    fputs("No Data.\n",outfile);
    exit(1);
  }

  char NewStr[200];

  sock.decode(); 
  while(1) {
    if (!sock.get(LinePtr,MaxLen)) {
      fprintf(stderr, "failed to receive data from the CondorView server %s\n",
			view_host.fullHostname());
      if (LineCount==0) fputs("No Data.\n",outfile);
      exit(1);
    }
    if (strlen(LinePtr)==0) break;
    LineCount++;
    if (Options==0 && QueryType==QUERY_HIST_STARTD) {
      ResConvStr(LinePtr,NewStr);
      fputs(NewStr,outfile);
    }
    else {
      fputs(LinePtr,outfile);
    }
  }
  if (!sock.end_of_message()) {
    fprintf(stderr, "failed to receive data from the CondorView server\n");
  }

  if (LineCount==0) fputs("No Data.\n",outfile);
  if (FileName.length()>0) fclose(outfile);

  return 0;
}

int CalcTime(int month, int day, int year) {
  struct tm time_str;
  if (year<50) year+= 100; // If I ask for 1 1 00, I want 1 1 2000
  if (year>1900) year-=1900;
  time_str.tm_year=year;
  time_str.tm_mon=month-1;
  time_str.tm_mday=day;
  time_str.tm_hour=0;
  time_str.tm_min=0;
  time_str.tm_sec=0;
  time_str.tm_isdst=-1;
  return mktime(&time_str);
}

int TimeLine(const std::string& TimeFileName,int FromDate, int ToDate, int Res)
{
  double Interval=double(ToDate-FromDate)/double(Res);
  float RelT;
  time_t T;
  FILE* TimeFile=safe_fopen_wrapper_follow(TimeFileName.c_str(),"w");
  if (!TimeFile) return -1;
  for (int i=0; i<=Res; i++) {
    T=(time_t)FromDate+((int)Interval*i);
    RelT=float(100.0/Res)*i;
	fprintf(TimeFile,"%.2f\t%s",RelT,asctime(localtime(&T)));
  }
  fclose(TimeFile);
  return 0;
}
