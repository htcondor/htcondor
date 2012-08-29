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
// for std::sort
#include <algorithm> 

//-----------------------------------------------------------------

// this enum has flags used to keep track of which fields in the LineRec have valid data
// and also to specify which columns of data to print out.
//
enum {
   DetailName      = 0x0001,
   DetailPriority  = 0x0002,
   DetailFactor    = 0x0004,
   DetailGroup     = 0x0008,
   DetailResUsed   = 0x0010,
   DetailWtResUsed = 0x0020,
   DetailUseTime1  = 0x0040, // usage start time
   DetailUseTime2  = 0x0080, // usage end time
   DetailEffQuota  = 0x0100,
   DetailCfgQuota  = 0x0200,
   DetailSurplus   = 0x0400,
   DetailTreeQuota = 0x0800,
   DetailRealPrio  = 0x1000,
   DetailSortKey   = 0x2000,
   DetailUseDeltaT = 0x4000,
   DetailOrder     = 0x8000,
   DetailPrios     = DetailPriority | DetailFactor | DetailRealPrio,
   DetailUsage     = DetailResUsed | DetailWtResUsed,
   DetailQuota2    = DetailEffQuota | DetailCfgQuota,
   DetailQuotas    = DetailEffQuota | DetailCfgQuota | DetailTreeQuota | DetailSurplus,
   DetailMost      = DetailCfgQuota | DetailSurplus | DetailPriority | DetailFactor | DetailUsage | DetailUseDeltaT,
   DetailAll       = DetailMost | DetailQuotas | DetailPrios | DetailUseTime1 | DetailUseTime2,
   DetailDefault   = DetailMost // show this if none of the flags controlling details is set.
};

enum {
   SurplusUnset = -1, // 
   SurplusNone = 0,
   SurplusRegroup, // autoregroup
   SurplusByQuota, // accept_surplus
   SurplusUnknown,
};

struct LineRec {
  MyString Name;
  float Priority;
  int Res;
  float wtRes;
  float AccUsage;
  float Factor;
  int BeginUsage;
  int LastUsage;
  MyString AcctGroup;
  bool IsAcctGroup;
  int   HasDetail;      // one or more of Detailxxx flags indicating the that data exists.
  float EffectiveQuota;
  float ConfigQuota;
  float SubtreeQuota;
  float SortKey;
  int   Surplus;       // 0 is no, 1 = regroup (by prio), 2 = accept_surplus (by quota)
  int   index;
  int   GroupId;
  float DisplayOrder;  // used to flexibly control sort order in Hier sort mode.
};

//-----------------------------------------------------------------

static int CalcTime(int,int,int);
static void usage(char* name);
static void ProcessInfo(AttrList* ad,bool GroupRollup,bool HierFlag);
static int CountElem(AttrList* ad);
static void CollectInfo(int numElem, AttrList* ad, LineRec* LR, bool GroupRollup);
static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem, bool HierFlag);
static void PrintResList(AttrList* ad);

//-----------------------------------------------------------------

int DashAll = false; // set when -all is specified on the command line.
int DashHier = false; // set when -h is specified
int DashQuota = false; // set when -quota is specified on the command line.
int DashSortKey = false; // set when -sortkey is specified on the command line.
int DetailFlag=0;        // what fields to display
int DetailAvailFlag=0;   // what fields the negotiator returned data for.
int GlobalSurplusPolicy = SurplusUnset;
bool GroupOrder = false;
bool GroupPrioIsMeaningless = true; // don't show priority for groups since it doesn't mean anything
bool HideNoneGroupIfPossible = true;// don't show the <none> group if it is the only group.
bool UsersHaveQuotas = false;       // set to true if we got quota info back for user items.
bool HideGroups = false;            // set to true when it doesn't make sense to show groups (i.e. just displaying prio)
bool HideUsers  = false;            // set to true when it doesn't make sense to show users (i.e. just showing quotas)
time_t MinLastUsageTime;

enum {
   SortByColumn1 = 0,  // sort by prio or by useage depending on which is in column 1 
   SortGroupsFirstByIndex,
   SortHierByGroupId,
   SortHierBySortKey,
   SortHierStrictlyBySortKey, // by sort key, none is not forced last
   SortHierByDisplayOrder,
   SortGroupsByName,
   SortGroupsFirstByName,
};

struct LineRecLT {
    int detail_flag;
    int group_order;
    bool none_last;

    LineRecLT() : detail_flag(0), group_order(false), none_last(true) {}
    LineRecLT(int df, int go, bool nl) : detail_flag(df), group_order(go), none_last(nl) {}

    bool operator()(const LineRec& a, const LineRec& b) const {
        if (group_order) {
            if (group_order == SortHierByGroupId) {  // sort by the order groups were returned from negotiator
               if (a.GroupId != b.GroupId) {
                  // force the <none> group (groupId == 0) to end up last)
                  if (none_last) {
                     if ( ! a.GroupId) return false;
                     if ( ! b.GroupId) return true;
                  }
                  return a.GroupId < b.GroupId;
               }
               if (a.IsAcctGroup != b.IsAcctGroup)
                  return a.IsAcctGroup > b.IsAcctGroup;

            } else if (group_order == SortHierBySortKey) {  // sort groups by the group sort_key value
               // force the <none> group (groupId == 0) to end up last)
               if (a.GroupId != b.GroupId) {
                  if (none_last) {
                     if ( ! a.GroupId) return false;
                     if ( ! b.GroupId) return true;
                  }
                  if (a.SortKey < b.SortKey || a.SortKey > b.SortKey) {
                     return a.SortKey < b.SortKey;
                  }
                  return a.GroupId < b.GroupId;
               }
               if (a.IsAcctGroup != b.IsAcctGroup)
                  return a.IsAcctGroup > b.IsAcctGroup;

            } else if (group_order == SortHierStrictlyBySortKey) {  // sort groups by the group sort_key value
               if (a.GroupId != b.GroupId) {
                  if (a.SortKey < b.SortKey || a.SortKey > b.SortKey) {
                     return a.SortKey < b.SortKey;
                  }
                  return a.GroupId < b.GroupId;
               }
               if (a.IsAcctGroup != b.IsAcctGroup)
                  return a.IsAcctGroup > b.IsAcctGroup;

            } else if (group_order == SortHierByDisplayOrder) {  // sort groups by the DisplayOrder value
               if (a.GroupId != b.GroupId) {
                  if (none_last) {
                     if ( ! a.GroupId) return false;
                     if ( ! b.GroupId) return true;
                  }
                  if (a.DisplayOrder < b.DisplayOrder || a.DisplayOrder > b.DisplayOrder) {
                     return a.DisplayOrder < b.DisplayOrder;
                  }
                  return a.GroupId < b.GroupId;
               }
               if (a.IsAcctGroup != b.IsAcctGroup)
                  return a.IsAcctGroup > b.IsAcctGroup;

            } else if (group_order == SortGroupsByName) { // sort by group name
               const MyString * pa = a.IsAcctGroup ? &a.Name : &a.AcctGroup;
               const MyString * pb = b.IsAcctGroup ? &b.Name : &b.AcctGroup;
               if (*pa != *pb)
                  return *pa < *pb;
               if (a.IsAcctGroup != b.IsAcctGroup)
                  return a.IsAcctGroup > b.IsAcctGroup;

            } else if (group_order == SortGroupsFirstByName) { // sort by group name
               if (a.IsAcctGroup != b.IsAcctGroup)
                  return a.IsAcctGroup > b.IsAcctGroup;
               if (a.IsAcctGroup) {
                  if (none_last) {
                     if ( ! a.GroupId) return false;
                     if ( ! b.GroupId) return true;
                  }
                  return a.Name < b.Name;
               }
            } else { //SortGroupsFirstByIndex
               // Acct groups come out of the acct classad ordered before the 
               // individual submitters, and in breadth-first order of hierarchy.
               // This preserves that ordering:
               if (a.IsAcctGroup || b.IsAcctGroup) {
                   return &a < &b;
               }
            }
        }

        // Order submitters by accumulated usage or priority or name depending on CL flags
        if (detail_flag & DetailPriority) {
            return a.Priority < b.Priority;
        } else if (detail_flag & DetailUsage) {
            return a.AccUsage < b.AccUsage;
        } else {
            return a.Name < b.Name;
        }

        return false;
    }
};


bool IsArg(const char * parg, const char * pval, int must_match_length = -1) {
   if (*parg != '-') return false;
   ++parg;
   if (*parg == '-') ++parg; // allow -- as well as - for an arg prefix.
   if (*parg != *pval) return false;

   // do argument matching based on a minimum prefix. when we run out
   // of characters in parg we must be at a \0 or no match and we
   // must have matched at least must_match_length characters or no match
   int match_length = 0;
   while (*parg == *pval) {
      ++match_length;
      ++parg; ++pval;
      if (!*pval) break;
   }
   if (*parg) return false;
   if (must_match_length < 0) return (*pval == 0);
   return match_length >= must_match_length;
}

int
main(int argc, char* argv[])
{

  bool LongFlag=false;
  bool HierFlag=true;
  int ResetUsage=0;
  int DeleteUser=0;
  int SetFactor=0;
  int SetPrio=0;
  int SetAccum=0;
  int SetBegin=0;
  int SetLast=0;
  bool ResetAll=false;
  int GetResList=0;
  std::string pool;
  bool GroupRollup = false;

  myDistro->Init( argc, argv );
  config();

  MinLastUsageTime=time(0)-60*60*24;  // Default to show only users active in the last day

  for (int i=1; i<argc; i++) {
    if (IsArg(argv[i],"setprio")) {
      if (i+2>=argc) usage(argv[0]);
      SetPrio=i;
      i+=2;
    }
    else if (IsArg(argv[i],"setfactor")) {
      if (i+2>=argc) usage(argv[0]);
      SetFactor=i;
      i+=2;
    }
    else if (IsArg(argv[i],"setbegin")) {
      if (i+2>=argc) usage(argv[0]);
      SetBegin=i;
      i+=2;
    }
    else if (IsArg(argv[i],"setaccum")) {
      if (i+2>=argc) usage(argv[0]);
      SetAccum=i;
      i+=2;
    }
    else if (IsArg(argv[i],"setlast")) {
      if (i+2>=argc) usage(argv[0]);
      SetLast=i;
      i+=2;
    }
    else if (IsArg(argv[i],"resetusage")) {
      if (i+1>=argc) usage(argv[0]);
      ResetUsage=i;
      i+=1;
    }
    else if (IsArg(argv[i],"delete",3)) {
      if (i+1>=argc) usage(argv[0]);
      DeleteUser=i;
      i+=1;
    }
    else if (IsArg(argv[i],"resetall")) {
      ResetAll=true;
    }
    else if (IsArg(argv[i],"long",1)) {
      LongFlag=true;
    }
    else if (IsArg(argv[i],"hierarchical",2) || IsArg(argv[i],"heir")) {
      HierFlag=true;
      DashHier=true;
    }
    else if (IsArg(argv[i],"flat",2)) {
      HierFlag=false;
      DashHier=true;
    }
    else if (IsArg(argv[i],"grouprollup")) {
      GroupRollup=true;
    }
    else if (IsArg(argv[i],"grouporder")) {
      GroupOrder=true;
      HierFlag = false;
      DashHier = true;
    }
    else if (IsArg(argv[i],"quotas",1)) {
      DetailFlag |= DetailQuotas;
      DashQuota = true;
    }
    else if (IsArg(argv[i],"surplus")) {
      DetailFlag |= DetailSurplus;
      DashQuota = true;
    }
    else if (IsArg(argv[i],"sortkey",2)) {
      DetailFlag |= DetailSortKey;
      DashSortKey = true;
    }
    else if (IsArg(argv[i],"all")) {
      DetailFlag |= DetailAll;
      DashAll = true;
    }
    else if (IsArg(argv[i],"most",2)) {
      DetailFlag |= DetailMost;
#ifdef DEBUG
      DetailFlag |= DetailGroup;
#endif
      DashAll = true;
    }
    else if (IsArg(argv[i],"groupid")) {
      DetailFlag |= DetailGroup;
    }
    else if (IsArg(argv[i],"order")) {
      DetailFlag |= DetailOrder;
    }
    else if (IsArg(argv[i],"activefrom")) {
      if (argc-i<=3) usage(argv[0]);
      int month=atoi(argv[i+1]);
      int day=atoi(argv[i+2]);
      int year=atoi(argv[i+3]);
      MinLastUsageTime=CalcTime(month,day,year);
      // printf("Date translation: %d/%d/%d = %d\n",month,day,year,FromDate);
      i+=3;
    }
    else if (IsArg(argv[i],"allusers")) {
      MinLastUsageTime=-1;
    }
    else if (IsArg(argv[i],"usage",2)) {
      DetailFlag |= DetailUsage | DetailUseTime1 | DetailUseTime2;
    }
    else if (IsArg(argv[i],"priority",4)) {
      DetailFlag |= DetailPrios;
    }
    else if (IsArg(argv[i],"getreslist",6)) {
      if (argc-i<=1) usage(argv[0]);
      GetResList=i;
      i+=1;
    }
    else if (IsArg(argv[i],"pool",1)) {
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
  Daemon negotiator(DT_NEGOTIATOR, NULL, (pool != "") ? pool.c_str() : NULL);
  if (!negotiator.locate()) {
	  fprintf(stderr, "%s: Can't locate negotiator in %s\n", 
              argv[0], (pool != "") ? pool.c_str() : "local pool");
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
    if (!ad->initAttrListFromStream(*sock) ||
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
    if (!(sock = negotiator.startCommand((GroupRollup) ? GET_PRIORITY_ROLLUP : GET_PRIORITY, Stream::reli_sock, 0)) ||
        !sock->end_of_message()) {
        fprintf(stderr, "failed to send %s command to negotiator\n", (GroupRollup) ? "GET_PRIORITY_ROLLUP" : "GET_PRIORITY");
        exit(1);
    }

    // get reply
    sock->decode();
    AttrList* ad=new AttrList();
    if (!ad->initAttrListFromStream(*sock) ||
        !sock->end_of_message()) {
      fprintf( stderr, "failed to get classad from negotiator\n" );
      exit(1);
    }

    sock->close();
    delete sock;

    // if no details specified, show priorities
    if ( ! DetailFlag) {
       DetailFlag = DetailDefault;
#ifdef DEBUG
       DetailFlag |= DetailGroup;
#endif
    }
    // if showing only prio, don't bother showing groups 
    if ( ! (DetailFlag & ~DetailPrios) && GroupPrioIsMeaningless) {
       if ( ! DashHier ) HierFlag = false;
       HideGroups = !HierFlag;
    }

    if (LongFlag) ad->fPrint(stdout);
    else ProcessInfo(ad,GroupRollup,HierFlag);
  }

  exit(0);
  return 0;
}

//-----------------------------------------------------------------

static void ProcessInfo(AttrList* ad,bool GroupRollup,bool HierFlag)
{
  int NumElem=CountElem(ad);
  if ( NumElem <= 0 ) {
	  return;
  }
  LineRec* LR=new LineRec[NumElem];
  CollectInfo(NumElem,ad,LR,GroupRollup);

  // check to see if all of the visible users are in the none group.
  bool all_users_in_none_group = true;
  for (int j = 0; j < NumElem; ++j) {
     if (LR[j].LastUsage<MinLastUsageTime) 
        continue;
     if (LR[j].GroupId > 0) {
        all_users_in_none_group = false;
        HideNoneGroupIfPossible = false;
        break;
     }
  }

  // if Group details are not available, or if all visible users 
  // are in the none group. then do not show Hierarchical
  if ( ! (DetailAvailFlag & DetailGroup) || (HideNoneGroupIfPossible && all_users_in_none_group)) {
     HierFlag = false;
  }

  // if not showing groups, don't show quota fields unless explicitly requested
  if (HideGroups || all_users_in_none_group) {
     if ( ! DashQuota && ! UsersHaveQuotas) DetailFlag &= ~DetailQuotas;
  }

  // don't show users if we are only showing group-only columns
  if ((DetailFlag & DetailQuotas) != 0 && (DetailFlag & ~DetailQuotas) == 0 && !UsersHaveQuotas) {
     HideUsers = true;
  }

  int order = SortByColumn1; // sort by prio or by useage depending on which is in column 1
  bool none_last = false;
  if (HierFlag) {
     none_last = (GlobalSurplusPolicy == SurplusRegroup);
     order = SortHierByGroupId;
     if (DetailFlag & (DetailPriority | DetailUsage))
        order = SortHierByDisplayOrder;
  } else if (GroupOrder) {
     order = SortGroupsFirstByIndex;
  }
  std::sort(LR, LR+NumElem, LineRecLT(DetailFlag, order, none_last));

  PrintInfo(ad,LR,NumElem,HierFlag);
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


//-----------------------------------------------------------------

static void CollectInfo(int numElem, AttrList* ad, LineRec* LR, bool GroupRollup)
{
  char  attrName[32], attrPrio[32], attrResUsed[32], attrWtResUsed[32], attrFactor[32], attrBeginUsage[32], attrAccUsage[42];
  char  attrLastUsage[32];
  MyString attrAcctGroup;
  MyString attrIsAcctGroup;
  char  name[128], policy[32];
  float priority, Factor, AccUsage = -1;
  int   resUsed = 0, BeginUsage = 0;
  int   LastUsage = 0;
  float wtResUsed;
  MyString AcctGroup;
  bool IsAcctGroup;
  float effective_quota = 0, config_quota = 0, subtree_quota = 0;
  bool fNeedGroupIdFixup = false;

  for( int i=1; i<=numElem; i++) {
    LR[i-1].Priority=0;
    LR[i-1].index = i;
    LR[i-1].GroupId=0;
    LR[i-1].SortKey = 0;
    LR[i-1].Surplus = SurplusUnset;
    LR[i-1].DisplayOrder = 0;
    LR[i-1].HasDetail = 0;
    LR[i-1].LastUsage=MinLastUsageTime;
    sprintf( attrName , "Name%d", i );
    sprintf( attrPrio , "Priority%d", i );
    sprintf( attrResUsed , "ResourcesUsed%d", i );
    sprintf( attrWtResUsed , "WeightedResourcesUsed%d", i );
    sprintf( attrFactor , "PriorityFactor%d", i );
    sprintf( attrBeginUsage , "BeginUsageTime%d", i );
    sprintf( attrLastUsage , "LastUsageTime%d", i );
    sprintf( attrAccUsage , "WeightedAccumulatedUsage%d", i );
    attrAcctGroup.formatstr("AccountingGroup%d", i);
    attrIsAcctGroup.formatstr("IsAccountingGroup%d", i);

    if( !ad->LookupString	( attrName, name, COUNTOF(name) ) 		|| 
		!ad->LookupFloat	( attrPrio, priority ) )
			continue;

    LR[i-1].HasDetail |= DetailPriority | DetailName;

    if( ad->LookupFloat( attrFactor, Factor ) ) {
       LR[i-1].HasDetail |= DetailFactor;
       if (Factor > 0)
          LR[i-1].HasDetail |= DetailRealPrio;
       }
	if( ad->LookupFloat( attrAccUsage, AccUsage ) ) LR[i-1].HasDetail |= DetailUsage;
	if( ad->LookupInteger( attrBeginUsage, BeginUsage ) ) LR[i-1].HasDetail |= DetailUseTime1;
	if( ad->LookupInteger( attrLastUsage, LastUsage ) ) LR[i-1].HasDetail |= DetailUseTime2 | DetailUseDeltaT;
	if( ad->LookupInteger( attrResUsed, resUsed ) ) LR[i-1].HasDetail |= DetailUsage;

	if( !ad->LookupFloat( attrWtResUsed, wtResUsed ) ) {
		wtResUsed = resUsed;
	} else {
		LR[i-1].HasDetail |= DetailWtResUsed;
	}

    if (!ad->LookupString(attrAcctGroup.Value(), AcctGroup)) {
        AcctGroup = "<none>";
    }
    if (!ad->LookupBool(attrIsAcctGroup.Value(), IsAcctGroup)) {
        IsAcctGroup = false;
    }

    char attr[32];
    sprintf( attr, "EffectiveQuota%d", i );
    if (ad->LookupFloat(attr, effective_quota)) LR[i-1].HasDetail |= DetailEffQuota;
    sprintf( attr, "ConfigQuota%d", i );
    if (ad->LookupFloat(attr, config_quota)) LR[i-1].HasDetail |= DetailCfgQuota;

    if (IsAcctGroup) {
        LR[i-1].GroupId = -i;
        if (!strcmp(name,"<none>") || !strcmp(name,"."))
           LR[i-1].GroupId = 0;

        sprintf( attr, "GroupAutoRegroup%d", i );
        bool regroup = false;
        if (ad->LookupBool(attr, regroup)) LR[i-1].HasDetail |= DetailSurplus;
        LR[i-1].Surplus = regroup ? SurplusRegroup : SurplusNone;
        if (regroup)
           GlobalSurplusPolicy = SurplusRegroup;

        sprintf( attr, "SurplusPolicy%d", i );
        if (ad->LookupString(attr, policy, COUNTOF(policy) )) {
           LR[i-1].HasDetail |= DetailSurplus;
           if (MATCH == strcasecmp(policy, "regroup")) {
              GlobalSurplusPolicy = SurplusRegroup;
              LR[i-1].Surplus = SurplusRegroup;
           } else if (MATCH == strcasecmp(policy, "byquota")) {
              GlobalSurplusPolicy = SurplusByQuota;
              LR[i-1].Surplus = SurplusByQuota;
           } else if (MATCH == strcasecmp(policy, "no")) {
              LR[i-1].Surplus = SurplusNone;
              } else {
              LR[i-1].Surplus = SurplusUnknown;
           }
        }

        float sort_key = LR[i-1].SortKey;
        sprintf( attr, "GroupSortKey%d", i );
        if (ad->LookupFloat(attr, sort_key)) LR[i-1].HasDetail |= DetailSortKey;
        LR[i-1].SortKey = sort_key;

        sprintf( attr, "SubtreeQuota%d", i );
        if (ad->LookupFloat(attr, subtree_quota)) LR[i-1].HasDetail |= DetailTreeQuota;
        if (GroupRollup && !(LR[i-1].HasDetail & DetailEffQuota)) {
           LR[i-1].HasDetail &= ~DetailPrios;
        }

        if (GroupPrioIsMeaningless) {
           LR[i-1].HasDetail &= ~DetailPriority;
        }

    } else {
       // if this is not a group, then it's in a group, if it's in the none
       // group we can set it's group id now, but if not we need to stuff 
       // in a bogus value and set a flag to turn on groupid fixup later.
       if (AcctGroup == "<none>" || AcctGroup == ".")
          LR[i-1].GroupId = 0;
       else {
          // set to large negative number so we know we have to fix it
          LR[i-1].GroupId = -numElem*2;
          fNeedGroupIdFixup = true;
       }
       if (LR[i-1].HasDetail & (DetailEffQuota | DetailCfgQuota))
          UsersHaveQuotas = true;
    }

    if (LR[i-1].GroupId != 0)
       LR[i-1].HasDetail |= DetailGroup;

	if (LastUsage==0) LastUsage=-1;

    DetailAvailFlag |= LR[i-1].HasDetail;

    LR[i-1].Name=name;
    LR[i-1].Priority=priority;
    LR[i-1].Res=resUsed;
    LR[i-1].wtRes=wtResUsed;
    LR[i-1].Factor=Factor;
    LR[i-1].BeginUsage=BeginUsage;
    LR[i-1].LastUsage=LastUsage;
    LR[i-1].AccUsage=AccUsage;
    LR[i-1].AcctGroup=AcctGroup;
    LR[i-1].IsAcctGroup=IsAcctGroup;
    LR[i-1].EffectiveQuota = effective_quota;
    LR[i-1].ConfigQuota = config_quota;
    LR[i-1].SubtreeQuota = subtree_quota;
  }
 
  // don't print colums if the negotiator didn't send us any data for them.
  if ( ! (DetailAvailFlag & DetailUsage))
    DetailFlag &= ~DetailUsage;
  if ( ! (DetailAvailFlag & DetailFactor))
    DetailFlag &= ~DetailFactor;
  if ( ! (DetailAvailFlag & DetailUseTime1))
    DetailFlag &= ~DetailUseTime1;
  if ( ! (DetailAvailFlag & DetailUseTime2))
    DetailFlag &= ~(DetailUseTime2 | DetailUseDeltaT);
  if ( ! DashQuota && !(DetailAvailFlag & DetailQuotas))
    DetailFlag &= ~DetailQuotas;

// ad->fPrint(stdout);

  if (fNeedGroupIdFixup) {

     // sort the records so that groups are first and in lex order with <none> last
     // we do this so that we can assign group id's in a reasonable way.
     std::sort(LR, LR+numElem, LineRecLT(DetailFlag, SortGroupsFirstByName, true));

     // assign group ids sequentially except for the <none> group which we
     // will leave as GroupId == 0. 
     int cGroups = 0;
     int ixNone = 0;
     for (int i = 0; i < numElem; ++i) {
        if ( ! LR[i].IsAcctGroup)
           break;

        ++cGroups;
        if (LR[i].GroupId < 0)
           LR[i].GroupId = cGroups;
        else
           ixNone = i;

        if (DetailFlag & DetailPriority) {
           // if sort key not supplied, use group priority as an approximation
           if (DetailAvailFlag & DetailSortKey)
              LR[i].DisplayOrder = LR[i].SortKey;
           else
              LR[i].DisplayOrder = LR[i].GroupId ? LR[i].Priority : FLT_MAX;
        } else if (DetailFlag & DetailUsage) {
           LR[i].DisplayOrder = LR[i].AccUsage / 3600.0;
        } else {
           LR[i].DisplayOrder = LR[i].GroupId ? (float)LR[i].GroupId : FLT_MAX;
        }
        LR[i].HasDetail |= DetailOrder;
        DetailAvailFlag |= DetailOrder;
     }

     // now fix up the user records to that they have the correct GroupId
     // for the group that they fit in. remember that only the first cGroups
     // entries of the LR array are groups, and entries with GroupId >= 0
     // are already correct.
     for (int i = cGroups; i < numElem; ++i) {
        if (LR[i].GroupId >= 0) {
           LR[i].DisplayOrder = LR[ixNone].DisplayOrder;
           LR[i].HasDetail |= (LR[ixNone].HasDetail & DetailOrder);
           continue;
        }

        // we already searched for a group name match before, now that
        // the LR array is complete, we can search after.
        for (int jj = 0; jj < cGroups; ++jj) {
           if (LR[i].AcctGroup == LR[jj].Name) {
              LR[i].GroupId = LR[jj].GroupId;
              LR[i].DisplayOrder = LR[jj].DisplayOrder;
              LR[i].HasDetail |= (LR[jj].HasDetail & DetailOrder);
              break;
           }
        }
     }
  }

  return;
}

// copy a string into a destination buffer, and fill the remainder of the buffer
// with the chFill character with a NUL character at the end.
enum {
   PAD_RIGHT = 0,
   PAD_LEFT,
   PAD_CENTER,
};

static char * CopyAndPadToWidth(char * pszDest, const char * pszSrc, int cch, int chFill, int pad = PAD_RIGHT)
{
   char * psz = pszDest;
   int cchRemain = cch;

   if (PAD_LEFT == pad) {
      int cchSrc = pszSrc ? strlen(pszSrc)+1 : 0;
      while (cchRemain > cchSrc) {
         *psz++ = chFill;
         --cchRemain;
         }
      }
   if (PAD_CENTER == pad) {
      int cchSrc = pszSrc ? strlen(pszSrc)+1 : 0;
      int cchLeft = (cchRemain - cchSrc)/2;
      while (cchLeft > 0) {
         *psz++ = chFill;
         --cchRemain;
         --cchLeft;
         }
      }

   if (pszSrc) {
      while (*pszSrc && cchRemain > 0) {
         *psz++ = *pszSrc++;
         --cchRemain;
      }
   }
   while (cchRemain > 0) {
      *psz++ = chFill;
      --cchRemain;
   }
   pszDest[cch-1] = 0;
   return pszDest;
}

static char * FormatDateTime(char * pszDest, int cchDest, int dtOne, const char * pszTimeZero)
{
   if (pszTimeZero && dtOne <= 0)
      CopyAndPadToWidth(pszDest, pszTimeZero, cchDest, ' ', PAD_LEFT);
   else
      CopyAndPadToWidth(pszDest, format_date_year (dtOne), cchDest, ' ', PAD_LEFT);
   return pszDest;
}

static char * FormatDeltaTime(char * pszDest, int cchDest, int tmDelta, const char * pszDeltaZero)
{
   if (pszDeltaZero && tmDelta <= 0) {
      CopyAndPadToWidth(pszDest, pszDeltaZero, cchDest, ' ', PAD_LEFT);
   } else {
      CopyAndPadToWidth(pszDest, format_time_nosecs (tmDelta), cchDest, ' ', PAD_LEFT);
   }
   return pszDest;
}

static char * FormatFloat(char * pszDest, int width, int decimal, float value)
{
   char sz[60];
   char fmt[10] = "%";
   sprintf(fmt+1, "%d.%df", width, decimal);
   sprintf(sz, fmt, value);
   int cch = strlen(sz);
   if (cch > width)
      {
      sprintf(fmt+1,".%dg", width-6);
      sprintf(sz, fmt, value);
      cch = strlen(sz);
      }
   CopyAndPadToWidth(pszDest, sz, width+1, ' ', PAD_LEFT);
   return pszDest;
}

static const char * SurplusName(int surplus)
{
   switch (surplus)
   {
      case SurplusUnset:   return "";        break;
      case SurplusNone:    return "no";      break;
      case SurplusRegroup: return "Regroup"; break;
      case SurplusByQuota: return "ByQuota"; break;
   }
   return "n/a";
}

static const struct {
   int DetailFlag;
   int width;
   const char * pHead;
   } aCols[] = {
   { DetailGroup,      5, "Group\0Id" },
   { DetailOrder,      9, "Group\0Order" },
   { DetailSortKey,    9, "Group\0Sort Key" },
   { DetailEffQuota,   9, "Effective\0Quota" },
   { DetailCfgQuota,   9, "Config\0Quota" },
   { DetailSurplus,    7, "Use\0Surplus" },
   { DetailTreeQuota,  9, "Subtree\0Quota" },
   { DetailPriority,  12, "Effective\0Priority" },
   { DetailRealPrio,   8, "Real\0Priority" },
   { DetailFactor,     9, "Priority\0Factor" },
   { DetailResUsed,    6, "Res\0In Use" },
   { DetailWtResUsed, 12, "Total Usage\0(wghted-hrs)" },
   { DetailUseTime1,  16, "Usage\0Start Time" },
   { DetailUseTime2,  16, "Last\0Usage Time" },
   { DetailUseDeltaT, 10, "Time Since\0Last Usage" },
};
const int MAX_NAME_COLUMN_WIDTH = 99;

static void PrintInfo(AttrList* ad, LineRec* LR, int NumElem, bool HierFlag)
{

   // figure out the width of the longest name column.
   //
   int max_name = 10;
   for (int j = 0; j < NumElem; ++j) {
      if (LR[j].LastUsage >= MinLastUsageTime) {
         int name_length = LR[j].Name.Length();
         if (HierFlag && ! LR[j].IsAcctGroup) {
            name_length += 2;
            if (LR[j].GroupId != 0)
               name_length -= LR[j].AcctGroup.Length();
         }
         if (name_length > max_name) max_name = name_length;
      }
   }

   int tmLast = 0;
   ad->LookupInteger( ATTR_LAST_UPDATE, tmLast );
   printf("Last Priority Update: %s\n",format_date(tmLast));

   LineRec Totals;
   Totals.Res=0;
   Totals.wtRes=0.0;
   Totals.BeginUsage=0;
   Totals.AccUsage=0;

   int cols_max_width = 0;
   for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
      cols_max_width += aCols[ii].width + 1;

   char UserCountStr[30];
   const char * UserCountFmt = "Number of users: %d";
   sprintf(UserCountStr, UserCountFmt, NumElem);
   const int min_name = strlen(UserCountStr);

   if (max_name > MAX_NAME_COLUMN_WIDTH) max_name = MAX_NAME_COLUMN_WIDTH;
   if (max_name < min_name) max_name = min_name;
   if (HierFlag) max_name += 2;
   char * Line  = (char*)malloc(max_name*2+cols_max_width+4);

   // print first row of headings
   CopyAndPadToWidth(Line,HierFlag ? "Group" : NULL,max_name+1,' ');
   int ix = max_name;
   for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
      {
      if (!(aCols[ii].DetailFlag & DetailFlag))
         continue;
      Line[ix++] = ' ';
      CopyAndPadToWidth(Line+ix, aCols[ii].pHead, aCols[ii].width+1, ' ', PAD_CENTER);
      ix += aCols[ii].width;
      }
   printf("%s\n", Line);

   // print second row of headings
   const char * pszNameTitle2 = HierFlag ? "  User Name" : "User Name";
   if (HideUsers) pszNameTitle2 = HierFlag ? "Name" : "Group Name";
   CopyAndPadToWidth(Line,pszNameTitle2,max_name+1,' ');
   ix = max_name;
   for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
      {
      if (!(aCols[ii].DetailFlag & DetailFlag))
         continue;
      Line[ix++] = ' ';
      CopyAndPadToWidth(Line+ix, aCols[ii].pHead+strlen(aCols[ii].pHead)+1, aCols[ii].width+1, ' ', PAD_CENTER);
      ix += aCols[ii].width;
      }
   printf("%s\n", Line);

   // print ---- under headings
   CopyAndPadToWidth(Line,NULL,max_name+1,'-');
   ix = max_name;
   for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
      {
      if (!(aCols[ii].DetailFlag & DetailFlag))
         continue;
      Line[ix++] = ' ';
      CopyAndPadToWidth(Line+ix, NULL, aCols[ii].width+1, '-');
      ix += aCols[ii].width;
      }
   printf("%s\n", Line);

   // print data lines
   int UserCount=0;
   for (int j=0; j < NumElem; ++j) {
      // We want to avoid counting totals twice for acct group records
      bool is_group = LR[j].IsAcctGroup;

      if (LR[j].LastUsage < MinLastUsageTime) 
         continue;

      if ( ! is_group) {
         ++UserCount;
         if (HideUsers)
            continue;
      } else {
         if (HideGroups || 
             ( ! HierFlag && HideNoneGroupIfPossible && ! LR[j].GroupId))
            continue;
         // if we show any groups, then show the none group.
         HideNoneGroupIfPossible = false;
      }

      // these assist in debugging.
      //printf("%d ", LR[j].index);
      //printf("%d %3d ", LR[j].IsAcctGroup, LR[j].GroupId);
      //printf("%s ", LR[j].AcctGroup);

      // write group/user name into Line
      if ( ! HierFlag || is_group) {
         CopyAndPadToWidth(Line,LR[j].Name.Value(),max_name+1,' ');
      } else {
         Line[0] = Line[1] = ' ';
         const char * pszName = LR[j].Name.Value();
         if (LR[j].GroupId > 0) 
            pszName +=  strlen(LR[j].AcctGroup.Value())+1;
         CopyAndPadToWidth(Line+2, pszName, max_name+1-2, ' ');
      }
      ix = max_name;

      // append columnar data into Line
      for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
         {
         if (!(aCols[ii].DetailFlag & DetailFlag))
            continue;

         Line[ix++] = ' ';

         const int item_NA  = 999999997;  // print n/a (not available)
         const int item_ND  = 999999998;  // print n/d (no data)
         const int item_Max = 999999999;  // print <max>

         int item = aCols[ii].DetailFlag;
         if ( ! (LR[j].HasDetail & aCols[ii].DetailFlag))
            {
            if (is_group && (aCols[ii].DetailFlag & (DetailQuotas | DetailSortKey)))
               item = item_ND;
#ifdef DEBUG
            else if ( ! is_group && (aCols[ii].DetailFlag & DetailSortKey) != 0)
               ;
#endif
            else
               item = 0; // just print spaces...
            }

         switch (item)
            {
            case DetailGroup:     FormatFloat(Line+ix, aCols[ii].width, 0, LR[j].GroupId * 1.0);
               break;
            case DetailOrder:     FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].DisplayOrder);
               break;
            case DetailSurplus:
               CopyAndPadToWidth(Line+ix, 
                  LR[j].GroupId ? SurplusName(LR[j].Surplus): "yes",
                  aCols[ii].width+1, ' ');
               break;
            case DetailEffQuota:  FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].EffectiveQuota);
               break;
            case DetailCfgQuota:  FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].ConfigQuota);
               break;
            case DetailTreeQuota: FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].SubtreeQuota);
               break;
            case DetailSortKey:   
               if (LR[j].SortKey < 3.40282001e38) // can't use MAX_FLT here because roundtripping through classads 
                  FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].SortKey);
               else
                  CopyAndPadToWidth(Line+ix, "<last>", aCols[ii].width+1, ' ', PAD_LEFT);
               break;
            case DetailPriority:  FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].Priority);
               break;
            case DetailRealPrio:  FormatFloat(Line+ix, aCols[ii].width, 2, (LR[j].Factor>0) ? (LR[j].Priority/LR[j].Factor) : 0);
               break;
            case DetailFactor:
               if (LR[j].Factor > 0)
                  FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].Factor);
               else
                  CopyAndPadToWidth(Line+ix, NULL, aCols[ii].width+1, ' ');
               break;
            case DetailResUsed:   FormatFloat(Line+ix, aCols[ii].width, 0, LR[j].wtRes);
               break;
            case DetailWtResUsed: FormatFloat(Line+ix, aCols[ii].width, 2, LR[j].AccUsage/3600.0);
               break;
            case DetailUseTime1:  FormatDateTime(Line+ix, aCols[ii].width+1, LR[j].BeginUsage, "");
               break;
            case DetailUseTime2:  FormatDateTime(Line+ix, aCols[ii].width+1, LR[j].LastUsage, "");
               break;
            case DetailUseDeltaT: FormatDeltaTime(Line+ix, aCols[ii].width+1, tmLast - LR[j].LastUsage, "<now>");
               break;
            case item_NA:
               CopyAndPadToWidth(Line+ix, "n/a", aCols[ii].width+1, ' ', PAD_LEFT);
               break;
            case item_ND:
               CopyAndPadToWidth(Line+ix, "n/d", aCols[ii].width+1, ' ', PAD_LEFT);
               break;
            case item_Max:
               CopyAndPadToWidth(Line+ix, "<max>", aCols[ii].width+1, ' ');
               break;
            default:
               CopyAndPadToWidth(Line+ix, NULL, aCols[ii].width+1, ' ');
               break;
            }

         ix += aCols[ii].width;
         }
      printf("%s\n", Line);

      // update totals
      if (!is_group) {
         Totals.wtRes+=LR[j].wtRes;
         Totals.AccUsage+=LR[j].AccUsage;
         if (LR[j].BeginUsage < Totals.BeginUsage) 
            Totals.BeginUsage = LR[j].BeginUsage;
      }
   }

   // print ---- under data
   CopyAndPadToWidth(Line,NULL,max_name+1,'-');
   ix = max_name;
   for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
      {
      if (!(aCols[ii].DetailFlag & DetailFlag))
         continue;
      Line[ix++] = ' ';
      CopyAndPadToWidth(Line+ix, NULL, aCols[ii].width+1, '-');
      ix += aCols[ii].width;
      }
   printf("%s\n", Line);

   // print summary/footer
   // 
   sprintf(UserCountStr, UserCountFmt, UserCount);
   CopyAndPadToWidth(Line,UserCountStr,max_name+1,' ');
   ix = max_name;
   for (int ii = 0; ii < (int)COUNTOF(aCols); ++ii)
      {
      if (!(aCols[ii].DetailFlag & DetailFlag))
         continue;

      Line[ix++] = ' ';

      switch (aCols[ii].DetailFlag)
         {
         case DetailSurplus:
            CopyAndPadToWidth(Line+ix, SurplusName(GlobalSurplusPolicy), aCols[ii].width+1, ' ');
            break;
         case DetailResUsed:   FormatFloat(Line+ix, aCols[ii].width, 0, Totals.wtRes);
            break;
         case DetailWtResUsed: FormatFloat(Line+ix, aCols[ii].width, 2, Totals.AccUsage/3600.0);
            break;
         case DetailUseTime1:  FormatDateTime(Line+ix, aCols[ii].width+1, Totals.BeginUsage, "");
            break;
         case DetailUseTime2:  FormatDateTime(Line+ix, aCols[ii].width+1, MinLastUsageTime, "");
            break;
         case DetailUseDeltaT:
            if (MinLastUsageTime <= 0)
               CopyAndPadToWidth(Line+ix, "", aCols[ii].width+1, ' ', PAD_LEFT);
            else
               FormatDeltaTime(Line+ix, aCols[ii].width+1, tmLast - MinLastUsageTime, "<now>");
            break;
         default:  
            CopyAndPadToWidth(Line+ix, NULL, aCols[ii].width+1, ' ');
         }
      ix += aCols[ii].width;
      }
   printf("%s\n", Line);
   free(Line);
}


//-----------------------------------------------------------------

static void usage(char* name) {
  fprintf( stderr, "usage: %s [options] [edit-option | display-options]\n"
     "\twhere [options] are\n"
     "\t\t-pool <host>\t\tUse host as the central manager to query\n"
     "\t\t-help\t\t\tThis Screen\n"
     "\twhere [edit-option] is one of\n"
     "\t\t-resetusage <user>\tReset usage data for <user>\n"
     "\t\t-resetall\t\tReset all useage data\n"
     "\t\t-delete <user>\t\tRemove a user record from the accountant\n"
     "\t\t-setprio <user> <val>\tSet priority for <user>\n"
     "\t\t-setfactor <user> <val>\tSet priority factor for <user>\n"
     "\t\t-setaccum <user> <val>\tSet Accumulated usage for <user>\n"
     "\t\t-setbegin <user> <val>\tset last first date for <user>\n"
     "\t\t-setlast <user> <val>\tset last active date for <user>\n"
     "\twhere [display-options] are\n"
     "\t\t-getreslist <user>\tDisplay list of resources for <user>\n"
     "\tor one or more of\n"
     "\t\t-allusers\t\tDisplay data for all users\n"
     "\t\t-activefrom <month> <day> <year> Display data for users active since this date\n"
     "\t\t-priority\t\tDisplay user priority fields\n"
     "\t\t-usage\t\t\tDisplay user/group usage fields\n"
     "\t\t-quotas\t\t\tDisplay group quota fields\n"
     "\t\t-most\t\t\tDisplay most useful prio and usage fields\n"
     "\t\t-all\t\t\tDisplay all fields\n"
     "\t\t-flat\t\t\tDo not display users under their groups\n"
     "\t\t-hierarchical\t\tDisplay users under their groups\n"
     "\t\t-grouporder\t\tDisplay groups first, then users\n"
     "\t\t-grouprollup\t\tGroup value are the sum of user values\n"
     "\t\t-long\t\t\tVerbose output (entire classads)\n"
//     "\t\t-ads <file>\t\tFile of priority ads to display\n"
     , name );
  exit(1);
}

//-----------------------------------------------------------------

static void PrintResList(AttrList* ad)
{
  // ad->fPrint(stdout);

  char  attrName[32], attrStartTime[32];
  char  name[128];
  int   StartTime;

  const char* Fmt="%-30s %12s %12s\n";

  printf(Fmt,"Resource Name"," Start Time"," Match Time");
  printf(Fmt,"-------------"," ----------"," ----------");

  int i;

  for (i=1;;i++) {
    sprintf( attrName , "Name%d", i );
    sprintf( attrStartTime , "StartTime%d", i );

    if( !ad->LookupString   ( attrName, name, COUNTOF(name) ) ||
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

