/* 
** Copyright 1997 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Adiel Yoaz
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

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

static void Usage(char* argv[]);
static void ProcessInfo(AttrList* ad);
static int CountElem(AttrList* ad);
static void CollectInfo(AttrList* ad, LineRec* LR);
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
      Usage(argv);
      exit(1);
    }
    SetMode=1;
  }

  config( 0 );
  char* NegotiatorHost = param( "NEGOTIATOR_HOST" );
  if (!NegotiatorHost) {
    printf( "No NegotiatorHost host specified in config file\n" );
  }


  if (SetMode) { // set priority

    char* UidDomain=param("UID_DOMAIN");
    if (!UidDomain) {
      printf("No UID_DOMAIN specified in config file\n");
      exit(1);
    }

    char tmp[512];
    sprintf(tmp, "%s@%s", argv[2], UidDomain);
    double Priority=atof(argv[3]);

    // send request
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(SET_PRIORITY) ||
        !sock.put(tmp) ||
        !sock.put(Priority) ||
        !sock.end_of_message()) {
      printf("failed to send SET_PRIORITY command to negotiator\n");
      exit(1);
    }

  }

  else {  // list priorities

    // send request
    ReliSock sock(NegotiatorHost, NEGOTIATOR_PORT);
    sock.encode();
    if (!sock.put(GET_PRIORITY) ||
        !sock.end_of_message()) {
      printf("failed to send GET_PRIORITY command to negotiator\n");
      exit(1);
    }

    // get reply
    sock.decode();
    AttrList* ad=new AttrList();
    if (!ad->get(sock) ||
        !sock.end_of_message()) {
      printf("failed to get classad from negotiator\n");
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
  CollectInfo(ad,LR);
  qsort(LR,NumElem,sizeof(LineRec),CompPrio);  
  PrintInfo(ad,LR,NumElem);
} 

//-----------------------------------------------------------------

static int CountElem(AttrList* ad)
{
  int count=0;
  ad->ResetExpr();
  while(ad->NextExpr()) count++;
  return (count-1)/3;
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

static void CollectInfo(AttrList* ad, LineRec* LR)
{
  ExprTree* exp;
  ad->ResetExpr();
  exp=ad->NextExpr();
  int i=0;
  while(exp=ad->NextExpr()) {
    LR[i].Name=((String*) exp->RArg())->Value();
    LR[i].Priority=((Float*) ad->NextExpr()->RArg())->Value();
    LR[i].Res=((Integer*) ad->NextExpr()->RArg())->Value();
    i++;
  }

  return;
}

//-----------------------------------------------------------------

static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem)
{
  // ad->AttrList::fPrint(stdout);
  ExprTree* exp;
  ad->ResetExpr();
  exp=ad->NextExpr();
  Time T=((Integer*) exp->RArg())->Value();
  // printf("%f\n",d);
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

static void Usage(char* argv[]) {
  printf("usage: %s [ -set user priority ]\n", argv[0]);
}

