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


 

//-----------------------------------------------------------------

#include "condor_common.h"
#include "condor_commands.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "MyString.h"
#include "format_time.h"
#include "daemon.h"
#include "condor_distribution.h"
#include "condor_attributes.h"

//-----------------------------------------------------------------

struct LineRec {
  MyString Name;
  float Priority;
  int Res;
  float wtRes;
  float AccUsage;
  float Factor;
  int BeginUsage;
  int LastUsage;
};

//-----------------------------------------------------------------

static int CalcTime(int,int,int);
static void usage(char* name);
static void ProcessInfo(AttrList* ad);
static int CountElem(AttrList* ad);
static void CollectInfo(int numElem, AttrList* ad, LineRec* LR);
static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem);
static void PrintResList(AttrList* ad);

//-----------------------------------------------------------------

extern "C" {
int CompPrio(const void * a, const void * b);
}


//-----------------------------------------------------------------

int DetailFlag=0;
time_t MinLastUsageTime;


int
main(int argc, char* argv[])
{

  bool LongFlag=false;
  int ResetUsage=0;
  int DeleteUser=0;
  int SetFactor=0;
  int SetPrio=0;
  int SetAccum=0;
  int SetBegin=0;
  int SetLast=0;
  bool ResetAll=false;
  int GetResList=0;
  char* pool = NULL;

  myDistro->Init( argc, argv );
  config();

  MinLastUsageTime=time(0)-60*60*24;  // Default to show only users active in the last day

  for (int i=1; i<argc; i++) {
    if (strcmp(argv[i],"-setprio")==0) {
      if (i+2>=argc) usage(argv[0]);
      SetPrio=i;
      i+=2;
    }
    else if (strcmp(argv[i],"-setfactor")==0) {
      if (i+2>=argc) usage(argv[0]);
      SetFactor=i;
      i+=2;
    }
    else if (strcmp(argv[i],"-setbegin")==0) {
      if (i+2>=argc) usage(argv[0]);
      SetBegin=i;
      i+=2;
    }
    else if (strcmp(argv[i],"-setaccum")==0) {
      if (i+2>=argc) usage(argv[0]);
      SetAccum=i;
      i+=2;
    }
    else if (strcmp(argv[i],"-setlast")==0) {
      if (i+2>=argc) usage(argv[0]);
      SetLast=i;
      i+=2;
    }
    else if (strcmp(argv[i],"-resetusage")==0) {
      if (i+1>=argc) usage(argv[0]);
      ResetUsage=i;
      i+=1;
    }
    else if (strcmp(argv[i],"-delete")==0) {
      if (i+1>=argc) usage(argv[0]);
      DeleteUser=i;
      i+=1;
    }
    else if (strcmp(argv[i],"-resetall")==0) {
      ResetAll=true;
    }
    else if (strcmp(argv[i],"-l")==0) {
      LongFlag=true;
    }
    else if (strcmp(argv[i],"-all")==0) {
      DetailFlag=1;
    }
    else if (strcmp(argv[i],"-activefrom")==0) {
      if (argc-i<=3) usage(argv[0]);
      int month=atoi(argv[i+1]);
      int day=atoi(argv[i+2]);
      int year=atoi(argv[i+3]);
      MinLastUsageTime=CalcTime(month,day,year);
      // printf("Date translation: %d/%d/%d = %d\n",month,day,year,FromDate);
      i+=3;
    }
    else if (strcmp(argv[i],"-allusers")==0) {
      MinLastUsageTime=-1;
    }
    else if (strcmp(argv[i],"-usage")==0) {
      DetailFlag=2;
    }
    else if (strcmp(argv[i],"-getreslist")==0) {
      if (argc-i<=1) usage(argv[0]);
      GetResList=i;
      i+=1;
    }
    else if (strcmp(argv[i],"-pool")==0) {
      if (argc-i<=1) usage(argv[0]);
      pool = argv[i+1];
      i++;
	}
    else {
      usage(argv[0]);
    }
  }
      
  //----------------------------------------------------------

	  // Get info on our negotiator
  Daemon negotiator( DT_NEGOTIATOR, NULL, pool );
  if( ! negotiator.locate() ) {
	  fprintf( stderr, "%s: %s\n", argv[0], negotiator.error() );
	  exit(1);
  }

  if (SetPrio) { // set priority

    char* tmp;
	if( ! (tmp = strchr(argv[SetPrio+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the priority of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}
    float Priority=atof(argv[SetPrio+2]);

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(SET_PRIORITY,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[SetPrio+1]) ||
        !sock->put(Priority) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send SET_PRIORITY command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The priority of %s was set to %f\n",argv[SetPrio+1],Priority);

  }

  else if (SetFactor) { // set priority

    char* tmp;
	if( ! (tmp = strchr(argv[SetFactor+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the priority of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}
    float Factor=atof(argv[SetFactor+2]);
	if (Factor<1) {
		fprintf( stderr, "Priority factors must be greater than or equal to "
				 "1.\n");
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(SET_PRIORITYFACTOR,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[SetFactor+1]) ||
        !sock->put(Factor) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send SET_PRIORITYFACTOR command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The priority factor of %s was set to %f\n",argv[SetFactor+1],Factor);

  }

  else if (SetAccum) { // set accumulated usage

    char* tmp;
	if( ! (tmp = strchr(argv[SetAccum+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the Accumulated usage of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}
    float accumUsage=atof(argv[SetAccum+2]);
	if (accumUsage<0.0) {
		fprintf( stderr, "Usage must be greater than 0 seconds\n");
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(SET_ACCUMUSAGE,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[SetAccum+1]) ||
        !sock->put(accumUsage) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send SET_ACCUMUSAGE command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The Accumulated Usage of %s was set to %f\n",argv[SetAccum+1],accumUsage);

  }
  else if (SetBegin) { // set begin usage time

    char* tmp;
	if( ! (tmp = strchr(argv[SetBegin+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the begin usage time of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}
    int beginTime=atoi(argv[SetBegin+2]);
	if (beginTime<0) {
		fprintf( stderr, "Time must be greater than 0 seconds\n");
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(SET_BEGINTIME,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[SetBegin+1]) ||
        !sock->put(beginTime) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send SET_BEGINTIME command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The Begin Usage Time of %s was set to %d\n",
			argv[SetBegin+1],beginTime);

  }
  else if (SetLast) { // set last usage time

    char* tmp;
	if( ! (tmp = strchr(argv[SetLast+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the last usage time of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}
    int lastTime=atoi(argv[SetLast+2]);
	if (lastTime<0) {
		fprintf( stderr, "Time must be greater than 0 seconds\n");
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(SET_LASTTIME,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[SetLast+1]) ||
        !sock->put(lastTime) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send SET_LASTTIME command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The Last Usage Time of %s was set to %d\n",
			argv[SetLast+1],lastTime);

  }
  else if (ResetUsage) { // set priority

    char* tmp;
	if( ! (tmp = strchr(argv[ResetUsage+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the priority of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(RESET_USAGE,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[ResetUsage+1]) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send RESET_USAGE command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The accumulated usage of %s was reset\n",argv[ResetUsage+1]);

  }

  else if (DeleteUser) { // remove a user record from the accountant

    char* tmp;
	if( ! (tmp = strchr(argv[DeleteUser+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the record you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto delete (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(DELETE_USER,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[DeleteUser+1]) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send DELETE_USER command to negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    printf("The accountant record named %s was deleted\n",argv[DeleteUser+1]);

  }

  else if (ResetAll) {

    // send request
    if( ! negotiator.sendCommand(RESET_ALL_USAGE, Stream::reli_sock, 0) ) {
		fprintf( stderr, 
				 "failed to send RESET_ALL_USAGE command to negotiator\n" );
		exit(1);
    }

    printf("The accumulated usage was reset for all users\n");

  }

  else if (GetResList) { // get resource list

    char* tmp;
	if( ! (tmp = strchr(argv[GetResList+1], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the priority of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}

    // send request
    Sock* sock;
    if( !(sock = negotiator.startCommand(GET_RESLIST,
										 Stream::reli_sock, 0) ) ||
        !sock->put(argv[GetResList+1]) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to send GET_RESLIST command to negotiator\n" );
      exit(1);
    }

    // get reply
    sock->decode();
    AttrList* ad=new AttrList();
    if (!ad->initFromStream(*sock) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to get classad from negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    if (LongFlag) ad->fPrint(stdout);
    else PrintResList(ad);
  }

  else {  // list priorities

    Sock* sock;
    if( !(sock = negotiator.startCommand( GET_PRIORITY,
										  Stream::reli_sock, 0)) ) {
      fprintf( stderr, "failed to send GET_PRIORITY command to negotiator\n" );
      exit(1);
    }

	// ship it out
	sock->eom();

    // get reply
    sock->decode();
    AttrList* ad=new AttrList();
    if (!ad->initFromStream(*sock) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to get classad from negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    if (LongFlag) ad->fPrint(stdout);
    else ProcessInfo(ad);
  }

  exit(0);
  return 0;
}

//-----------------------------------------------------------------

static void ProcessInfo(AttrList* ad)
{
  int NumElem=CountElem(ad);
  if ( NumElem <= 0 ) {
	  return;
  }
  LineRec* LR=new LineRec[NumElem];
  CollectInfo(NumElem,ad,LR);
  qsort(LR,NumElem,sizeof(LineRec),CompPrio);  
  PrintInfo(ad,LR,NumElem);
  delete[] LR;
} 

//-----------------------------------------------------------------

static int CountElem(AttrList* ad)
{
	int numSubmittors;
	if( ad->LookupInteger( "NumSubmittors", numSubmittors ) ) 
		return numSubmittors;
	else
		return -1;
} 

//-----------------------------------------------------------------

extern "C" {
int CompPrio(const void * ina, const void * inb) 
{
  LineRec* a = (LineRec *)ina;
  LineRec* b = (LineRec *)inb;

  if (DetailFlag==2) {
    if (a->AccUsage>b->AccUsage) return 1;
  }
  else {
    if (a->Priority>b->Priority) return 1;
  }
  return -1;
}
}

//-----------------------------------------------------------------

static void CollectInfo(int numElem, AttrList* ad, LineRec* LR)
{
  char  attrName[32], attrPrio[32], attrResUsed[32], attrWtResUsed[32], attrFactor[32], attrBeginUsage[32], attrAccUsage[42];
  char  attrLastUsage[32];
  char  name[128];
  float priority, Factor, AccUsage;
  int   resUsed, BeginUsage;
  int   LastUsage;
  float wtResUsed;

  for( int i=1; i<=numElem; i++) {
    LR[i-1].Priority=0;
    LR[i-1].LastUsage=MinLastUsageTime;
    sprintf( attrName , "Name%d", i );
    sprintf( attrPrio , "Priority%d", i );
    sprintf( attrResUsed , "ResourcesUsed%d", i );
    sprintf( attrWtResUsed , "WeightedResourcesUsed%d", i );
    sprintf( attrFactor , "PriorityFactor%d", i );
    sprintf( attrBeginUsage , "BeginUsageTime%d", i );
    sprintf( attrLastUsage , "LastUsageTime%d", i );
    sprintf( attrAccUsage , "WeightedAccumulatedUsage%d", i );

    if( !ad->LookupString	( attrName, name ) 		|| 
		!ad->LookupFloat	( attrPrio, priority ) )
			continue;

	if(	!ad->LookupFloat	( attrFactor, Factor )	||
		!ad->LookupFloat	( attrAccUsage, AccUsage )	||
		!ad->LookupInteger	( attrBeginUsage, BeginUsage )	||
		!ad->LookupInteger	( attrLastUsage, LastUsage )	||
		!ad->LookupInteger	( attrResUsed, resUsed ) ) 
			DetailFlag=0;

	if( !ad->LookupFloat( attrWtResUsed, wtResUsed ) ) {
		wtResUsed = resUsed;
	}

	if (LastUsage==0) LastUsage=-1;

    LR[i-1].Name=name;
    LR[i-1].Priority=priority;
    LR[i-1].Res=resUsed;
    LR[i-1].wtRes=wtResUsed;
    LR[i-1].Factor=Factor;
    LR[i-1].BeginUsage=BeginUsage;
    LR[i-1].LastUsage=LastUsage;
    LR[i-1].AccUsage=AccUsage;

  }
 
  // ad->fPrint(stdout);

  return;
}

//-----------------------------------------------------------------

static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem)
{
  char LastUsageStr[17];
  int T = 0;
  ad->LookupInteger( ATTR_LAST_UPDATE, T );
  printf("Last Priority Update: %s\n",format_date(T));

  LineRec Totals;
  Totals.Res=0;
  Totals.wtRes=0.0;
  Totals.BeginUsage=0;
  Totals.AccUsage=0;
  
  char* Fmt1="%-30s %14.2f\n";  // Data line format
  char* Fmt2="%-30s %14s\n";    // Title and separator line format
  char* Fmt3="Number of users shown: %-13d %14s\n";    // Totals line format

  if (DetailFlag==1) {
    Fmt1="%-30s %14.2f %8.2f %12.2f %4.0f %12.2f %14s %14s\n"; 
    Fmt2="%-30s %14s %8s %12s %4s %12s %14s %14s\n"; 
    Fmt3="Number of users: %-13d %14s %8s %12s %4.0f %12.2f %14s %14s\n"; 
  }
  else if (DetailFlag==2) {
    Fmt1="%-30s %12.2f %14s %14s\n"; 
    Fmt2="%-30s %12s %14s %14s\n"; 
    Fmt3="Number of users: %-13d %12.2f %14s %14s\n"; 
  }

  if (DetailFlag==2) {
    printf(Fmt2,"         ","Total Usage","     Usage    ","     Last     ");
    printf(Fmt2,"User Name","(wghted-hrs)","  Start Time  ","  Usage Time  ");
    printf(Fmt2,"------------------------------","-----------","----------------","----------------");
  }
  else {
    printf(Fmt2,"         ","Effective","  Real  ","  Priority  ","Res ","Total Usage","      Usage     ","      Last      ");
    printf(Fmt2,"User Name","Priority ","Priority","   Factor   ","Used","(wghted-hrs)","   Start Time   ","   Usage Time   ");
    printf(Fmt2,"------------------------------","---------","--------","------------","----","-----------","----------------","----------------");
  }

  int UserCount=0;
  for (int i=0; i<NumElem; i++) {
	if (LR[i].LastUsage<MinLastUsageTime) continue;
    UserCount++;
    strcpy(LastUsageStr,format_date_year(LR[i].LastUsage));
    if (LR[i].Name.Length()>30) LR[i].Name=LR[i].Name.Substr(0,29);
    if (DetailFlag==2)
      printf(Fmt1,LR[i].Name.Value(),LR[i].AccUsage/3600.0,format_date_year(LR[i].BeginUsage),LastUsageStr);
    else 
      printf(Fmt1,LR[i].Name.Value(),LR[i].Priority,(LR[i].Priority/LR[i].Factor),LR[i].Factor,LR[i].wtRes,LR[i].AccUsage/3600.0,format_date_year(LR[i].BeginUsage),LastUsageStr);
    Totals.wtRes+=LR[i].wtRes;
    Totals.AccUsage+=LR[i].AccUsage;
    if (LR[i].BeginUsage<Totals.BeginUsage || Totals.BeginUsage==0) Totals.BeginUsage=LR[i].BeginUsage;
  }

  strcpy(LastUsageStr,format_date_year(MinLastUsageTime));
  if (DetailFlag==2) {
    printf(Fmt2,"------------------------------","-----------","----------------","----------------");
    printf(Fmt3,NumElem,Totals.AccUsage/3600.0,format_date_year(Totals.BeginUsage),LastUsageStr);
  }
  else {
    printf(Fmt2,"------------------------------","---------","--------","------------","----","-----------","----------------","----------------");
    printf(Fmt3,UserCount,"","","",Totals.wtRes,Totals.AccUsage/3600.0,format_date_year(Totals.BeginUsage),LastUsageStr);
  }

  return;
}

//-----------------------------------------------------------------

static void usage(char* name) {
  fprintf( stderr, "usage: %s [ -pool hostname ] [ -all | -usage | { -setprio | -setfactor | -setaccum | -setbegin | -setlast }  user value | "
			"-resetusage user | -resetall | -getreslist user | -delete user ] "
			"[-allusers | -activefrom month day year] [-l]\n", name );
  exit(1);
}

//-----------------------------------------------------------------

static void PrintResList(AttrList* ad)
{
  // ad->fPrint(stdout);

  char  attrName[32], attrStartTime[32];
  char  name[128];
  int   StartTime;

  char* Fmt="%-30s %12s %12s\n";

  printf(Fmt,"Resource Name"," Start Time"," Match Time");
  printf(Fmt,"-------------"," ----------"," ----------");

  int i;

  for (i=1;;i++) {
    sprintf( attrName , "Name%d", i );
    sprintf( attrStartTime , "StartTime%d", i );

    if( !ad->LookupString   ( attrName, name ) ||
		!ad->LookupInteger  ( attrStartTime, StartTime))
            break;

    char* p=strrchr(name,'@');
    *p='\0';
    time_t Now=time(0)-StartTime;
	printf(Fmt,name,format_date(StartTime),format_time(Now));
  }

  printf(Fmt,"-------------"," ------------"," ------------");
  printf("Number of Resources Used: %d\n",i-1);

  return;
} 

//-----------------------------------------------------------------

int CalcTime(int month, int day, int year) {
  struct tm time_str;
  if (year<50) year +=100; // If I ask for 1 1 00, I want 1 1 2000, not 1 1 1900
  if (year>1900) year-=1900;
  time_str.tm_year=year;  time_str.tm_mon=month-1;
  time_str.tm_mday=day;
  time_str.tm_hour=0;
  time_str.tm_min=0;
  time_str.tm_sec=0;
  time_str.tm_isdst=-1;
  return mktime(&time_str);
}

