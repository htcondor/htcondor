/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _Condor_Accountant_H_
#define _Condor_Accountant_H_

#include <iostream.h>

#include "condor_classad.h"
// #include "classad_log.h"

#include "MyString.h"

// this is the required minimum separation between two priorities for them
// to be considered distinct values
static const float PriorityDelta = 0.5;

class ClassAdLog;

class Accountant {

public:

  //--------------------------------------------------------
  // User Functions
  //--------------------------------------------------------

  Accountant();
  ~Accountant();

  void Initialize();  // Configuration

  int GetResourcesUsed(const MyString& CustomerName); // get # of used resources
  float GetPriority(const MyString& CustomerName); // get priority for a customer
  void SetPriority(const MyString& CustomerName, float Priority); // set priority for a customer

  float GetPriorityFactor(const MyString& CustomerName); // get priority factor for a customer

  void SetPriorityFactor(const MyString& CustomerName, float PriorityFactor);
  void ResetAccumulatedUsage(const MyString& CustomerName);
  void DeleteRecord(const MyString& CustomerName); 
  void ResetAllUsage();

  void AddMatch(const MyString& CustomerName, ClassAd* ResourceAd); // Add new match
  void RemoveMatch(const MyString& ResourceName); // remove a match

  void UpdatePriorities(); // update all the priorities

  void CheckMatches(ClassAdList& ResourceList);  // Remove matches that are not claimed
  AttrList* ReportState();
  AttrList* ReportState(const MyString& CustomerName, int * NumResources = NULL);
                                                
  void DisplayLog();
  void DisplayMatches();

private:

  //--------------------------------------------------------
  // Private methods Methods
  //--------------------------------------------------------
  
  void AddMatch(const MyString& CustomerName, const MyString& ResourceName, time_t T);
  void RemoveMatch(const MyString& ResourceName, time_t T);

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
  StringList *GroupNamesList;

  //--------------------------------------------------------
  // Data members
  //--------------------------------------------------------

  ClassAdLog* AcctLog;
  int LastUpdateTime;

  //--------------------------------------------------------
  // Static values
  //--------------------------------------------------------

  static MyString AcctRecord;
  static MyString CustomerRecord;
  static MyString ResourceRecord;

  static MyString PriorityAttr;
  static MyString UnchargedTimeAttr;
  static MyString ResourcesUsedAttr;
  static MyString AccumulatedUsageAttr;
  static MyString BeginUsageTimeAttr;
  static MyString LastUsageTimeAttr;
  static MyString PriorityFactorAttr;

  static MyString LastUpdateTimeAttr;

  static MyString RemoteUserAttr;
  static MyString StartTimeAttr;

  //--------------------------------------------------------
  // Utility functions
  //--------------------------------------------------------

  static MyString GetResourceName(ClassAd* Resource);
  static int IsClaimed(ClassAd* ResourceAd, MyString& CustomerName);
  static int CheckClaimedOrMatched(ClassAd* ResourceAd, const MyString& CustomerName);
  static ClassAd* FindResourceAd(const MyString& ResourceName, ClassAdList& ResourceList);
  static MyString GetDomain(const MyString& CustomerName);

  ClassAd* GetClassAd(const MyString& Key);
  bool DeleteClassAd(const MyString& Key);

  void SetAttributeInt(const MyString& Key, const MyString& AttrName, int AttrValue);
  void SetAttributeFloat(const MyString& Key, const MyString& AttrName, float AttrValue);
  void SetAttributeString(const MyString& Key, const MyString& AttrName, const MyString& AttrValue);

  bool GetAttributeInt(const MyString& Key, const MyString& AttrName, int& AttrValue);
  bool GetAttributeFloat(const MyString& Key, const MyString& AttrName, float& AttrValue);
  bool GetAttributeString(const MyString& Key, const MyString& AttrName, MyString& AttrValue);

  bool LoadState(const MyString& OldLogFileName);
};

#endif
