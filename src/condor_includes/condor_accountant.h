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

// local working attribute to stash match-cost on resource ads that have a 
// request matched against them
#define CP_MATCH_COST "_cp_match_cost"

template <typename K, typename AD> class ClassAdLog;
struct GroupEntry;

class Accountant {

public:

  //--------------------------------------------------------
  // User Functions
  //--------------------------------------------------------

  Accountant();
  ~Accountant();

  void Initialize(GroupEntry* group);  // Configuration

  int   GetResourcesUsed(const string& CustomerName); // get # of used resources (unweighted by SlotWeight)
  float GetWeightedResourcesUsed(const string& CustomerName);
  float GetPriority(const string& CustomerName); // get priority for a customer
  int   GetCeiling(const string& CustomerName); // get Ceiling for a customer
  void  SetPriority(const string& CustomerName, float Priority); // set priority for a customer
  void  SetCeiling(const string& CustomerName, int Ceiling); // set Ceiling for a customer

  void SetAccumUsage(const string& CustomerName, float AccumUsage); // set accumulated usage for a customer
  void SetBeginTime(const string& CustomerName, int BeginTime); // set begin usage time for a customer
  void SetLastTime(const string& CustomerName, int LastTime); // set Last usage time for a customer
  float GetPriorityFactor(const string& CustomerName); // get priority factor for a customer

  void SetPriorityFactor(const string& CustomerName, float PriorityFactor);
  void ResetAccumulatedUsage(const string& CustomerName);
  void DeleteRecord(const string& CustomerName);
  void ResetAllUsage();

  void AddMatch(const string& CustomerName, ClassAd* ResourceAd); // Add new match
  void RemoveMatch(const string& ResourceName); // remove a match

  float GetSlotWeight(ClassAd *candidate) const;
  void UpdatePriorities(); // update all the priorities

  void CheckMatches(ClassAdListDoesNotDeleteAds& ResourceList);  // Remove matches that are not claimed

  int GetLastUpdateTime() const { return LastUpdateTime; }

  double GetLimit(const string& limit);
  double GetLimitMax(const string& limit);
  void ReportLimits(ClassAd *attrList);

  ClassAd* ReportState(bool rollup = false);
  ClassAd* ReportState(const string& CustomerName);

  void CheckResources(const string& CustomerName, int& NumResources, float& NumResourcesRW);

  void DisplayLog();
  void DisplayMatches();

  ClassAd* GetClassAd(const string& Key);

  // This maps submitter names to their assigned accounting group.
  // When called with a defined group name, it maps that group name to itself.
  GroupEntry* GetAssignedGroup(const string& CustomerName);
  GroupEntry* GetAssignedGroup(const string& CustomerName, bool& IsGroup);

  bool UsingWeightedSlots() const;

  struct ci_less {
      bool operator()(const string& a, const string& b) const {
          return strcasecmp(a.c_str(), b.c_str()) < 0;
      }
  };

private:

  //--------------------------------------------------------
  // Private methods Methods
  //--------------------------------------------------------
  
  void RemoveMatch(const string& ResourceName, time_t T);

  void LoadLimits(ClassAdListDoesNotDeleteAds &resourceList);
  void ClearLimits();
  void DumpLimits();

  void IncrementLimit(const string& limit);
  void DecrementLimit(const string& limit);

  void IncrementLimits(const string& limits);
  void DecrementLimits(const string& limits);

  // Get group priority helper function.
  float getGroupPriorityFactor(const string& CustomerName);

  //--------------------------------------------------------
  // Configuration variables
  //--------------------------------------------------------

  float MinPriority;        // Minimum priority (if no resources used)
  float NiceUserPriorityFactor;
  float RemoteUserPriorityFactor;
  float DefaultPriorityFactor;
  string AccountantLocalDomain;
  float HalfLifePeriod;     // The time in sec in which the priority is halved by aging
  string LogFileName;      // Name of Log file
  int	MaxAcctLogSize;		// Max size of log file
  bool  DiscountSuspendedResources;
  bool  UseSlotWeights; 

  //--------------------------------------------------------
  // Data members
  //--------------------------------------------------------

  ClassAdLog<std::string, ClassAd*> * AcctLog;
  int LastUpdateTime;

  HashTable<string, double> concurrencyLimits;

  GroupEntry* hgq_root_group;
  map<string, GroupEntry*, ci_less> hgq_submitter_group_map;

  //--------------------------------------------------------
  // Static values
  //--------------------------------------------------------

  static string AcctRecord;
  static string CustomerRecord;
  static string ResourceRecord;

  //--------------------------------------------------------
  // Utility functions
  //--------------------------------------------------------

  static string GetResourceName(ClassAd* Resource);
  bool GetResourceState(ClassAd* Resource, State& state);
  int IsClaimed(ClassAd* ResourceAd, string& CustomerName);
  int CheckClaimedOrMatched(ClassAd* ResourceAd, const string& CustomerName);
//  static ClassAd* FindResourceAd(const string& ResourceName, ClassAdListDoesNotDeleteAds& ResourceList);
  static string GetDomain(const string& CustomerName);

  bool DeleteClassAd(const string& Key);

  void SetAttributeInt(const string& Key, const string& AttrName, int AttrValue);
  void SetAttributeFloat(const string& Key, const string& AttrName, float AttrValue);
  void SetAttributeString(const string& Key, const string& AttrName, const string& AttrValue);

  bool GetAttributeInt(const string& Key, const string& AttrName, int& AttrValue);
  bool GetAttributeFloat(const string& Key, const string& AttrName, float& AttrValue);
  bool GetAttributeString(const string& Key, const string& AttrName, string& AttrValue);

  void ReportGroups(GroupEntry* group, ClassAd* ad, bool rollup, map<string, int>& gnmap);
};


extern void parse_group_name(const string& gname, vector<string>& gpath);

#endif
