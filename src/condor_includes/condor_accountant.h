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

#ifndef _Condor_Accountant_H_
#define _Condor_Accountant_H_


#include "condor_classad.h"
// #include "classad_log.h"

#include "MyString.h"
#include "HashTable.h"

#include "condor_state.h"

#include <vector>
#include <map>
#include <set>
#include <string>

using std::vector;
using std::map;
using std::set;
using std::string;

// this is the required minimum separation between two priorities for them
// to be considered distinct values
static const float PriorityDelta = 0.5;

class ClassAdLog;
struct GroupEntry;

class Accountant {

public:

  //--------------------------------------------------------
  // User Functions
  //--------------------------------------------------------

  Accountant();
  ~Accountant();

  void Initialize(GroupEntry* group);  // Configuration

  int GetResourcesUsed(const MyString& CustomerName); // get # of used resources (unweighted by SlotWeight)
  float GetWeightedResourcesUsed(const MyString& CustomerName);
  float GetPriority(const MyString& CustomerName); // get priority for a customer
  void SetPriority(const MyString& CustomerName, float Priority); // set priority for a customer

  void SetAccumUsage(const MyString& CustomerName, float AccumUsage); // set accumulated usage for a customer
  void SetBeginTime(const MyString& CustomerName, int BeginTime); // set begin usage time for a customer
  void SetLastTime(const MyString& CustomerName, int LastTime); // set Last usage time for a customer
  float GetPriorityFactor(const MyString& CustomerName); // get priority factor for a customer

  void SetPriorityFactor(const MyString& CustomerName, float PriorityFactor);
  void ResetAccumulatedUsage(const MyString& CustomerName);
  void DeleteRecord(const MyString& CustomerName); 
  void ResetAllUsage();

  void AddMatch(const MyString& CustomerName, ClassAd* ResourceAd); // Add new match
  void RemoveMatch(const MyString& ResourceName); // remove a match

  float GetSlotWeight(ClassAd *candidate);
  void UpdatePriorities(); // update all the priorities

  void CheckMatches(ClassAdListDoesNotDeleteAds& ResourceList);  // Remove matches that are not claimed

  double GetLimit(const MyString& limit);
  double GetLimitMax(const MyString& limit);
  void ReportLimits(AttrList *attrList);

  ClassAd* ReportState(bool rollup = false);
  AttrList* ReportState(const MyString& CustomerName);

  void CheckResources(const string& CustomerName, int& NumResources, float& NumResourcesRW);

  void DisplayLog();
  void DisplayMatches();

  ClassAd* GetClassAd(const MyString& Key);

  // This maps submitter names to their assigned accounting group.
  // When called with a defined group name, it maps that group name to itself.
  GroupEntry* GetAssignedGroup(const MyString& CustomerName);
  GroupEntry* GetAssignedGroup(const MyString& CustomerName, bool& IsGroup);

  bool UsingWeightedSlots();

  struct ci_less {
      bool operator()(const string& a, const string& b) const {
          return strcasecmp(a.c_str(), b.c_str()) < 0;
      }
  };

private:

  //--------------------------------------------------------
  // Private methods Methods
  //--------------------------------------------------------
  
  void RemoveMatch(const MyString& ResourceName, time_t T);

  void LoadLimits(ClassAdListDoesNotDeleteAds &resourceList);
  void ClearLimits();
  void DumpLimits();

  void IncrementLimit(const MyString& limit);
  void DecrementLimit(const MyString& limit);

  void IncrementLimits(const MyString& limits);
  void DecrementLimits(const MyString& limits);

  // Get group priority helper function.
  float getGroupPriorityFactor(const MyString& CustomerName); 

  //--------------------------------------------------------
  // Configuration variables
  //--------------------------------------------------------

  float MinPriority;        // Minimum priority (if no resources used)
  float NiceUserPriorityFactor;
  float RemoteUserPriorityFactor;
  float DefaultPriorityFactor;
  MyString AccountantLocalDomain;
  float HalfLifePeriod;     // The time in sec in which the priority is halved by aging
  MyString LogFileName;      // Name of Log file
  int	MaxAcctLogSize;		// Max size of log file
  bool  DiscountSuspendedResources;
  bool  UseSlotWeights; 

  //--------------------------------------------------------
  // Data members
  //--------------------------------------------------------

  ClassAdLog* AcctLog;
  int LastUpdateTime;

  HashTable<MyString, double> concurrencyLimits;

  GroupEntry* hgq_root_group;
  map<string, GroupEntry*, ci_less> hgq_submitter_group_map;

  //--------------------------------------------------------
  // Static values
  //--------------------------------------------------------

  static MyString AcctRecord;
  static MyString CustomerRecord;
  static MyString ResourceRecord;

  //--------------------------------------------------------
  // Utility functions
  //--------------------------------------------------------

  static MyString GetResourceName(ClassAd* Resource);
  bool GetResourceState(ClassAd* Resource, State& state);
  int IsClaimed(ClassAd* ResourceAd, MyString& CustomerName);
  int CheckClaimedOrMatched(ClassAd* ResourceAd, const MyString& CustomerName);
  static ClassAd* FindResourceAd(const MyString& ResourceName, ClassAdListDoesNotDeleteAds& ResourceList);
  static MyString GetDomain(const MyString& CustomerName);

  bool DeleteClassAd(const MyString& Key);

  void SetAttributeInt(const MyString& Key, const MyString& AttrName, int AttrValue);
  void SetAttributeFloat(const MyString& Key, const MyString& AttrName, float AttrValue);
  void SetAttributeString(const MyString& Key, const MyString& AttrName, const MyString& AttrValue);

  bool GetAttributeInt(const MyString& Key, const MyString& AttrName, int& AttrValue);
  bool GetAttributeFloat(const MyString& Key, const MyString& AttrName, float& AttrValue);
  bool GetAttributeString(const MyString& Key, const MyString& AttrName, MyString& AttrValue);

  bool LoadState(const MyString& OldLogFileName);

  void ReportGroups(GroupEntry* group, ClassAd* ad, bool rollup, map<string, int>& gnmap);
};


extern void parse_group_name(const string& gname, vector<string>& gpath);

#endif
