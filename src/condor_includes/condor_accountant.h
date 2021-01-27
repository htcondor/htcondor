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

  int   GetResourcesUsed(const std::string& CustomerName); // get # of used resources (unweighted by SlotWeight)
  float GetWeightedResourcesUsed(const std::string& CustomerName);
  float GetPriority(const std::string& CustomerName); // get priority for a customer
  int   GetCeiling(const std::string& CustomerName); // get Ceiling for a customer
  void  SetPriority(const std::string& CustomerName, float Priority); // set priority for a customer
  void  SetCeiling(const std::string& CustomerName, int Ceiling); // set Ceiling for a customer

  void SetAccumUsage(const std::string& CustomerName, float AccumUsage); // set accumulated usage for a customer
  void SetBeginTime(const std::string& CustomerName, int BeginTime); // set begin usage time for a customer
  void SetLastTime(const std::string& CustomerName, int LastTime); // set Last usage time for a customer
  float GetPriorityFactor(const std::string& CustomerName); // get priority factor for a customer

  void SetPriorityFactor(const std::string& CustomerName, float PriorityFactor);
  void ResetAccumulatedUsage(const std::string& CustomerName);
  void DeleteRecord(const std::string& CustomerName);
  void ResetAllUsage();

  void AddMatch(const std::string& CustomerName, ClassAd* ResourceAd); // Add new match
  void RemoveMatch(const std::string& ResourceName); // remove a match

  float GetSlotWeight(ClassAd *candidate) const;
  void UpdatePriorities(); // update all the priorities
  void UpdateOnePriority(int T, int TimePassed, float AgingFactor, const char *key, ClassAd *ad); // Help function for above

  void CheckMatches(ClassAdListDoesNotDeleteAds& ResourceList);  // Remove matches that are not claimed

  int GetLastUpdateTime() const { return LastUpdateTime; }

  double GetLimit(const std::string& limit);
  double GetLimitMax(const std::string& limit);
  void ReportLimits(ClassAd *attrList);

  ClassAd* ReportState(bool rollup = false);
  ClassAd* ReportState(const std::string& CustomerName);

  void CheckResources(const std::string& CustomerName, int& NumResources, float& NumResourcesRW);

  void DisplayLog();
  void DisplayMatches();

  ClassAd* GetClassAd(const std::string& Key);

  // This maps submitter names to their assigned accounting group.
  // When called with a defined group name, it maps that group name to itself.
  GroupEntry* GetAssignedGroup(const std::string& CustomerName);
  GroupEntry* GetAssignedGroup(const std::string& CustomerName, bool& IsGroup);

  bool UsingWeightedSlots() const;

  struct ci_less {
      bool operator()(const std::string& a, const std::string& b) const {
          return strcasecmp(a.c_str(), b.c_str()) < 0;
      }
  };

private:

  //--------------------------------------------------------
  // Private methods Methods
  //--------------------------------------------------------
  
  void RemoveMatch(const std::string& ResourceName, time_t T);

  void LoadLimits(ClassAdListDoesNotDeleteAds &resourceList);
  void ClearLimits();
  void DumpLimits();

  void IncrementLimit(const std::string& limit);
  void DecrementLimit(const std::string& limit);

  void IncrementLimits(const std::string& limits);
  void DecrementLimits(const std::string& limits);

  // Get group priority helper function.
  float getGroupPriorityFactor(const std::string& CustomerName);

  //--------------------------------------------------------
  // Configuration variables
  //--------------------------------------------------------

  float MinPriority;        // Minimum priority (if no resources used)
  float NiceUserPriorityFactor;
  float RemoteUserPriorityFactor;
  float DefaultPriorityFactor;
  std::string AccountantLocalDomain;
  float HalfLifePeriod;     // The time in sec in which the priority is halved by aging
  std::string LogFileName;      // Name of Log file
  int	MaxAcctLogSize;		// Max size of log file
  bool  DiscountSuspendedResources;
  bool  UseSlotWeights; 

  //--------------------------------------------------------
  // Data members
  //--------------------------------------------------------

  ClassAdLog<std::string, ClassAd*> * AcctLog;
  int LastUpdateTime;

  HashTable<std::string, double> concurrencyLimits;

  GroupEntry* hgq_root_group;
  std::map<std::string, GroupEntry*, ci_less> hgq_submitter_group_map;

  //--------------------------------------------------------
  // Static values
  //--------------------------------------------------------

  static std::string AcctRecord;
  static std::string CustomerRecord;
  static std::string ResourceRecord;

  //--------------------------------------------------------
  // Utility functions
  //--------------------------------------------------------

  static std::string GetResourceName(ClassAd* Resource);
  bool GetResourceState(ClassAd* Resource, State& state);
  int IsClaimed(ClassAd* ResourceAd, std::string& CustomerName);
  int CheckClaimedOrMatched(ClassAd* ResourceAd, const std::string& CustomerName);
//  static ClassAd* FindResourceAd(const std::string& ResourceName, ClassAdListDoesNotDeleteAds& ResourceList);
  static std::string GetDomain(const std::string& CustomerName);

  bool DeleteClassAd(const std::string& Key);

  void SetAttributeInt(const std::string& Key, const std::string& AttrName, int AttrValue);
  void SetAttributeFloat(const std::string& Key, const std::string& AttrName, float AttrValue);
  void SetAttributeString(const std::string& Key, const std::string& AttrName, const std::string& AttrValue);

  bool GetAttributeInt(const std::string& Key, const std::string& AttrName, int& AttrValue);
  bool GetAttributeFloat(const std::string& Key, const std::string& AttrName, float& AttrValue);
  bool GetAttributeString(const std::string& Key, const std::string& AttrName, std::string& AttrValue);

  void ReportGroups(GroupEntry* group, ClassAd* ad, bool rollup, std::map<std::string, int>& gnmap);
};


extern void parse_group_name(const std::string& gname, std::vector<std::string>& gpath);

#endif
