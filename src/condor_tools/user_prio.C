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
};

//-----------------------------------------------------------------

static void usage(char* name);
static void ProcessInfo(AttrList* ad);
static int CountElem(AttrList* ad);
static void CollectInfo(int numElem, AttrList* ad, LineRec* LR);
static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem);

//-----------------------------------------------------------------

extern "C" {
int CompPrio(LineRec* a, LineRec* b);
}


//-----------------------------------------------------------------

main(int argc, char* argv[])
{
  int SetMode=0;

  if (argc>1) { // set priority
    if (argc!=4 || strcmp(argv[1],"-set")!=0 ) {
      usage( argv[0] );
      exit(1);
    }
    SetMode=1;
  }

  config( 0 );
  char* NegotiatorHost = param( "NEGOTIATOR_HOST" );
  if (!NegotiatorHost) {
    fprintf( stderr, "No NegotiatorHost host specified in config file\n" );
	exit(1);
  }


  if (SetMode) { // set priority

    char* tmp;
	if( ! (tmp = strchr(argv[2], '@')) ) {
		fprintf( stderr, 
				 "%s: You must specify the full name of the submittor you wish\n",
				 argv[0] );
		fprintf( stderr, "\tto update the priority of (%s or %s)\n", 
				 "user@uid.domain", "user@full.host.name" );
		exit(1);
	}
    double Priority=atof(argv[3]);

    // send request
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(SET_PRIORITY) ||
        !sock.put(argv[2]) ||
        !sock.put(Priority) ||
        !sock.end_of_message()) {
      fprintf( stderr, "failed to send SET_PRIORITY command to negotiator\n" );
      exit(1);
    }

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
  char  attrName[32], attrPrio[32], attrResUsed[32];
  char  name[128];
  float priority;
  int   resUsed;

  for( int i=1; i<=numElem; i++) {
    sprintf( attrName , "Name%d", i );
    sprintf( attrPrio , "Priority%d", i );
    sprintf( attrResUsed , "ResourcesUsed%d", i );

    if( !ad->LookupString	( attrName, name ) 		|| 
		!ad->LookupFloat	( attrPrio, priority )	||
		!ad->LookupInteger	( attrResUsed, resUsed ) ) 
			continue;

    LR[i-1].Name=name;
    LR[i-1].Priority=priority;
    LR[i-1].Res=resUsed;
  }

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

  char* Fmt1="%-30s   %12.3f   %4d\n"; 
  char* Fmt2="%-30s   %12s   %4s\n"; 
  char* Fmt3="%-30s   %12s   %4d\n"; 

  printf(Fmt2,"Name","Priority","Resources Used");
  printf(Fmt2,"----","--------","--------------");

  int Res=0;
  for (int i=0; i<NumElem; i++) {
    printf(Fmt1,(const char*)LR[i].Name,LR[i].Priority,LR[i].Res);
	Res+=LR[i].Res;
  }

  // printf(Fmt2,"----","--------","--------------");
  // printf(Fmt3,"Total","",Res);

  return;
}

//-----------------------------------------------------------------

static void usage(char* name) {
  fprintf( stderr, "usage: %s [ -set user priority_value ]\n", name );
}

