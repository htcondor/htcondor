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

 

//-----------------------------------------------------------------

#include "condor_common.h"
#include <iostream.h>
#include "condor_commands.h"
#include "condor_config.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_io.h"
#include "TimeClass.h"
#include "MyString.h"

//-----------------------------------------------------------------

struct LineRec {
  MyString Name;
  float Priority;
  int Res;
  float AccUsage;
  float Factor;
  int BeginUsage;
};

//-----------------------------------------------------------------

static void usage(char* name);
static void ProcessInfo(AttrList* ad);
static int CountElem(AttrList* ad);
static void CollectInfo(int numElem, AttrList* ad, LineRec* LR);
static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem);
static char* format_date( time_t);

//-----------------------------------------------------------------

extern "C" {
int CompPrio(LineRec* a, LineRec* b);
}


//-----------------------------------------------------------------

bool DetailFlag=false;

main(int argc, char* argv[])
{
  int ResetUsage=0;
  int SetFactor=0;
  int SetPrio=0;
  bool ResetAll=false;

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
    else if (strcmp(argv[i],"-resetusage")==0) {
      if (i+1>=argc) usage(argv[0]);
      ResetUsage=i;
      i+=1;
    }
    else if (strcmp(argv[i],"-resetall")==0) {
      ResetAll=true;
    }
    else if (strcmp(argv[i],"-all")==0) {
      DetailFlag=true;
    }
    else {
      usage(argv[0]);
    }
  }
      
  //----------------------------------------------------------

  config( 0 );
  char* NegotiatorHost = param( "NEGOTIATOR_HOST" );
  if (!NegotiatorHost) {
    fprintf( stderr, "No NegotiatorHost host specified in config file\n" );
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
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(SET_PRIORITY) ||
        !sock.put(argv[SetPrio+1]) ||
        !sock.put(Priority) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to send SET_PRIORITY command to negotiator\n" );
      exit(1);
    }

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

    // send request
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(SET_PRIORITYFACTOR) ||
        !sock.put(argv[SetFactor+1]) ||
        !sock.put(Factor) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to send SET_PRIORITYFACTOR command to negotiator\n" );
      exit(1);
    }

    printf("The priority factor of %s was set to %f\n",argv[SetFactor+1],Factor);

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
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(RESET_USAGE) ||
        !sock.put(argv[ResetUsage+1]) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to send RESET_USAGE command to negotiator\n" );
      exit(1);
    }

    printf("The accumulated usage of %s was reset\n",argv[ResetUsage+1]);

  }

  else if (ResetAll) {

    // send request
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(RESET_ALL_USAGE) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to send RESET_ALL_USAGE command to negotiator\n" );
      exit(1);
    }

    printf("The accumulated usage was reset for all users\n");

  }

  else {  // list priorities

    // send request
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(GET_PRIORITY) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to send GET_PRIORITY command to negotiator\n" );
      exit(1);
    }

    // get reply
    sock.decode();
    AttrList* ad=new AttrList();
    if (!ad->get(sock) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to get classad from negotiator\n" );
      exit(1);
    }

    ProcessInfo(ad);
  }

  exit(0);
}

//-----------------------------------------------------------------

static void ProcessInfo(AttrList* ad)
{
  int NumElem=CountElem(ad);
  LineRec* LR=new LineRec[NumElem];
  CollectInfo(NumElem,ad,LR);
  qsort(LR,NumElem,sizeof(LineRec),CompPrio);  
  PrintInfo(ad,LR,NumElem);
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
int CompPrio(LineRec* a, LineRec* b) 
{
  if (a->Priority>b->Priority) return 1;
  return -1;
}
}

//-----------------------------------------------------------------

static void CollectInfo(int numElem, AttrList* ad, LineRec* LR)
{
  char  attrName[32], attrPrio[32], attrResUsed[32], attrFactor[32], attrBeginUsage[32], attrAccUsage[32];
  char  name[128];
  float priority, Factor, AccUsage;
  int   resUsed, BeginUsage;

  for( int i=1; i<=numElem; i++) {
    LR[i-1].Priority=0;
    sprintf( attrName , "Name%d", i );
    sprintf( attrPrio , "Priority%d", i );
    sprintf( attrResUsed , "ResourcesUsed%d", i );
    sprintf( attrFactor , "PriorityFactor%d", i );
    sprintf( attrBeginUsage , "BeginUsageTime%d", i );
    sprintf( attrAccUsage , "AccumulatedUsage%d", i );

    if( !ad->LookupString	( attrName, name ) 		|| 
		!ad->LookupFloat	( attrPrio, priority ) )
			continue;

	if(	!ad->LookupFloat	( attrFactor, Factor )	||
		!ad->LookupFloat	( attrAccUsage, AccUsage )	||
		!ad->LookupInteger	( attrBeginUsage, BeginUsage )	||
		!ad->LookupInteger	( attrResUsed, resUsed ) ) 
			DetailFlag=false;

    LR[i-1].Name=name;
    LR[i-1].Priority=priority;
    LR[i-1].Res=resUsed;
    LR[i-1].Factor=Factor;
    LR[i-1].BeginUsage=BeginUsage;
    LR[i-1].AccUsage=AccUsage;
  }
 
  // ad->fPrint(stdout);

  return;
}

//-----------------------------------------------------------------

static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem)
{
  ExprTree* exp;
  ad->ResetExpr();
  exp=ad->NextExpr();
  Time T=((Integer*) exp->RArg())->Value();
  printf("Last Priority Update: %s\n",T.Asc());

  char* Fmt1="%-30s %12.2f\n";
  char* Fmt2="%-30s %12s\n";

  if (DetailFlag) {
    Fmt1="%-30s %12.2f %8.2f %12.2f %4d %12.2f %11s\n"; 
    Fmt2="%-30s %12s %8s %12s %4s %12s %11s\n"; 
  }

  printf(Fmt2,"         ","Effective","  Real  ","  Priority  ","Res ","Accumulated","   Usage  ");
  printf(Fmt2,"User Name","Priority ","Priority","   Factor   ","Used","Usage (hrs)","Start Time");
  printf(Fmt2,"---------","---------","--------","------------","----","-----------","----------");

  for (int i=0; i<NumElem; i++) {
    printf(Fmt1,LR[i].Name.Value(),LR[i].Priority,(LR[i].Priority/LR[i].Factor),LR[i].Factor,LR[i].Res,LR[i].AccUsage/3600.0,format_date(LR[i].BeginUsage));
  }

  return;
}

//-----------------------------------------------------------------

static void usage(char* name) {
  fprintf( stderr, "usage: %s [ -all | { -setprio | -setfactor }  user value | -resetusage user | -resetall ]\n", name );
  exit(1);
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
