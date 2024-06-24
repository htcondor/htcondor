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

#include <climits>
#include <math.h>
#include <iomanip>
#include <charconv>

#include "condor_accountant.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_state.h"
#include "condor_attributes.h"
#include "enum_utils.h"
#include "classad_log.h"
#include "HashTable.h"
#include "NegotiationUtils.h"
#include "matchmaker.h"
#include <string>
#include <deque>

#define MIN_PRIORITY_FACTOR (1.0)

std::string Accountant::AcctRecord="Accountant.";
std::string Accountant::CustomerRecord="Customer.";
std::string Accountant::ResourceRecord="Resource.";

static char const *PriorityAttr="Priority";
static char const *CeilingAttr="Ceiling";
static char const *FloorAttr="Floor";
static char const *ResourcesUsedAttr="ResourcesUsed";
static char const *WeightedResourcesUsedAttr="WeightedResourcesUsed";
static char const *HierWeightedResourcesUsedAttr="HierWeightedResourcesUsed";
static char const *UnchargedTimeAttr="UnchargedTime";
static char const *WeightedUnchargedTimeAttr="WeightedUnchargedTime";
static char const *AccumulatedUsageAttr="AccumulatedUsage";
static char const *WeightedAccumulatedUsageAttr="WeightedAccumulatedUsage";
static char const *BeginUsageTimeAttr="BeginUsageTime";
static char const *LastUsageTimeAttr="LastUsageTime"; 
static char const *PriorityFactorAttr="PriorityFactor";

static char const *LastUpdateTimeAttr="LastUpdateTime";

static char const *RemoteUserAttr="RemoteUser";
static char const *StartTimeAttr="StartTime";

static char const *SlotWeightAttr=ATTR_SLOT_WEIGHT;

static char const* NumCpMatches = "NumCpMatches";

/* Disable gcc warnings about floating point comparisons */
GCC_DIAG_OFF(float-equal)

//------------------------------------------------------------------
// Constructor - One time initialization
//------------------------------------------------------------------

Accountant::Accountant()
{
  MinPriority=0.5;
  AcctLog=NULL;
  DiscountSuspendedResources = false;
  UseSlotWeights = false;
  DefaultPriorityFactor = 1e3;
  HalfLifePeriod = 1.0f;
  LastUpdateTime = 0;
  MaxAcctLogSize = 1'000'000;
  NiceUserPriorityFactor = 1e10;
  RemoteUserPriorityFactor = 1e7;
  hgq_root_group = NULL;
}

//------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------

Accountant::~Accountant()
{
  if (AcctLog) delete AcctLog;
}

//------------------------------------------------------------------
// Initialize (or re-configure) and read configuration parameters
//------------------------------------------------------------------

void Accountant::Initialize(GroupEntry* root_group) 
{
  static bool first_time = true;

  // Default values

  char* tmp;
  NiceUserPriorityFactor=1e10;
  RemoteUserPriorityFactor=1e7;
  DefaultPriorityFactor=1e3;

  // Set up HGQ accounting-group related information
  hgq_root_group = root_group;
  GroupEntry::Initialize(hgq_root_group);
  
  HalfLifePeriod = param_double("PRIORITY_HALFLIFE");

  // get nice users priority factor

  tmp = param("NICE_USER_PRIO_FACTOR");
  if(tmp) {
	  NiceUserPriorityFactor=atof(tmp);
	  free(tmp);
  }

  // get remote users priority factor

  tmp = param("REMOTE_PRIO_FACTOR");
  if(tmp) {
	  RemoteUserPriorityFactor=atof(tmp);
	  free(tmp);
  }

  // get default priority factor

  tmp = param("DEFAULT_PRIO_FACTOR");
  if(tmp) {
	  DefaultPriorityFactor=atof(tmp);
	  free(tmp);
  }

  // get accountant local domain

  tmp = param("ACCOUNTANT_LOCAL_DOMAIN");
  if(tmp) {
	  AccountantLocalDomain=tmp;
	  free(tmp);
  }
  else {
      RemoteUserPriorityFactor=1;
  }

  MaxAcctLogSize = param_integer("MAX_ACCOUNTANT_DATABASE_SIZE",1'000'000);

  if ( ! param(LogFileName, "ACCOUNTANT_DATABASE_FILE")) {
	tmp = param("SPOOL");
	if (tmp) {
		LogFileName = tmp;
		LogFileName += "/Accountantnew.log";
		free(tmp);
	} else {
		EXCEPT("SPOOL not defined!");
	}
  }

  DiscountSuspendedResources = param_boolean(
                             "NEGOTIATOR_DISCOUNT_SUSPENDED_RESOURCES",false);
  UseSlotWeights = param_boolean("NEGOTIATOR_USE_SLOT_WEIGHTS",true);


  dprintf(D_ACCOUNTANT,"PRIORITY_HALFLIFE=%f\n",HalfLifePeriod);
  dprintf(D_ACCOUNTANT,"NICE_USER_PRIO_FACTOR=%f\n",NiceUserPriorityFactor);
  dprintf(D_ACCOUNTANT,"REMOTE_PRIO_FACTOR=%f\n",RemoteUserPriorityFactor);
  dprintf(D_ACCOUNTANT,"DEFAULT_PRIO_FACTOR=%f\n",DefaultPriorityFactor);
  dprintf(D_ACCOUNTANT,"ACCOUNTANT_LOCAL_DOMAIN=%s\n",
				  AccountantLocalDomain.c_str());
  dprintf( D_ACCOUNTANT, "MAX_ACCOUNTANT_DATABASE_SIZE=%d\n",
		   MaxAcctLogSize );

  if (!AcctLog) {
    AcctLog=new ClassAdLog<std::string,ClassAd*>();
    if (!AcctLog->InitLogFile(LogFileName.c_str())) {
      EXCEPT("Failed to initialize Accountant log!");
    }
    dprintf(D_ACCOUNTANT,"Accountant::Initialize - LogFileName=%s\n",
					LogFileName.c_str());
  }

  // get last update time

  LastUpdateTime=0;
  GetAttributeInt(AcctRecord,LastUpdateTimeAttr,LastUpdateTime);

  // if at startup, do a sanity check to make certain number of resource
  // records for a user and what the user record says jives
  if ( first_time ) {
	  std::string HK;
	  ClassAd* ad;
	  std::vector<std::string> users;
	  int resources_used, resources_used_really;
	  int total_overestimated_resources = 0;
	  int total_overestimated_users = 0;
	  double resourcesRW_used, resourcesRW_used_really;
	  double total_overestimated_resourcesRW = 0;
	  int total_overestimated_usersRW = 0;

      first_time = false;

	  dprintf(D_ACCOUNTANT,"Sanity check on number of resources per user\n");

		// first find all the users
	  AcctLog->table.startIterations();
	  while (AcctLog->table.iterate(HK,ad)) {
		char const *key = HK.c_str();
			// skip records that are not customer records...
		if (strncmp(CustomerRecord.c_str(),key,CustomerRecord.length())) continue;
		char const *thisUser = &(key[CustomerRecord.length()]);
		if (! isalpha(*thisUser)) {
			dprintf(D_ALWAYS, "questionable user %s\n", thisUser);
		}
			// if we made it here, append to our list of users
		users.emplace_back( thisUser );
	  }
		// ok, now vector users has all the users.  for each user,
		// compare what the customer record claims for usage -vs- actual
		// number of resources
	  for (const auto& user: users) {
		  resources_used = GetResourcesUsed(user);
		  resourcesRW_used = GetWeightedResourcesUsed(user);

		  CheckResources(user, resources_used_really, resourcesRW_used_really);

		  if ( resources_used == resources_used_really ) {
			dprintf(D_ACCOUNTANT,"Customer %s using %d resources\n",user.c_str(),
				  resources_used);
		  } else {
			dprintf(D_ALWAYS,
				"FIXING - Customer %s using %d resources, but only found %d\n",
				user.c_str(),resources_used,resources_used_really);
			SetAttributeInt(CustomerRecord+user,ResourcesUsedAttr,resources_used_really);
			if ( resources_used > resources_used_really ) {
				total_overestimated_resources += 
					( resources_used - resources_used_really );
				total_overestimated_users++;
			}
		  }
		  if ( resourcesRW_used == resourcesRW_used_really ) {
			dprintf(D_ACCOUNTANT,"Customer %s using %f weighted resources\n",user.c_str(),
				  resourcesRW_used);
		  } else {
			dprintf(D_ALWAYS,
				"FIXING - Customer record %s using %f weighted resources, but found %f\n",
				user.c_str(),resourcesRW_used,resourcesRW_used_really);
			SetAttributeFloat(CustomerRecord+user,WeightedResourcesUsedAttr,resourcesRW_used_really);
			if ( resourcesRW_used > resourcesRW_used_really ) {
				total_overestimated_resourcesRW += 
					( resourcesRW_used - resourcesRW_used_really );
				total_overestimated_usersRW++;
			}
		  }
	  }
	  if ( total_overestimated_users ) {
		  dprintf( D_ALWAYS,
			  "FIXING - Overestimated %d resources across %d users "
			  "(from a total of %zu users)\n",
			  total_overestimated_resources,total_overestimated_users,
			  users.size() );
	  }
	  if ( total_overestimated_usersRW ) {
		  dprintf( D_ALWAYS,
			  "FIXING - Overestimated %f resources across %d users "
			  "(from a total of %zu users)\n",
			  total_overestimated_resourcesRW,total_overestimated_users,
			  users.size() );
	  }
  }

  // Ensure that table entries for acct groups get created on startup and reconfig
  // Do this after loading the accountant log, to give log data precedence
  std::deque<GroupEntry*> grpq;
  grpq.push_back(hgq_root_group);
  while (!grpq.empty()) {
      GroupEntry* group = grpq.front();
      grpq.pop_front();
      // This creates entries if they don't already exist:
      GetPriority(group->name);
      for (std::vector<GroupEntry*>::iterator j(group->children.begin());  j != group->children.end();  ++j) {
          grpq.push_back(*j);
      }
  }

  UpdatePriorities();
}

bool Accountant::UsingWeightedSlots() const {
    return UseSlotWeights;
}

//------------------------------------------------------------------
// Return the number of resources used
//------------------------------------------------------------------

int Accountant::GetResourcesUsed(const std::string& CustomerName) 
{
  int ResourcesUsed=0;
  GetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  return ResourcesUsed;
}

//------------------------------------------------------------------
// Return the number of resources used (floating point version)
//------------------------------------------------------------------

double Accountant::GetWeightedResourcesUsed(const std::string& CustomerName)
{
  double WeightedResourcesUsed=0.0;
  GetAttributeFloat(CustomerRecord+CustomerName,WeightedResourcesUsedAttr,WeightedResourcesUsed);
  return WeightedResourcesUsed;
}

//------------------------------------------------------------------
// Return the priority of a customer
//------------------------------------------------------------------

double Accountant::GetPriority(const std::string& CustomerName) 
{
    // Warning!  This function has a side effect of a writing the
    // PriorityFactor.
  double PriorityFactor=GetPriorityFactor(CustomerName);
  double Priority=MinPriority;
  GetAttributeFloat(CustomerRecord+CustomerName,PriorityAttr,Priority);
  if (Priority<MinPriority) {
    Priority=MinPriority;
    // Warning!  This read function has a side effect of a write.
    SetPriority(CustomerName,Priority);// write and dprintf()
  }
  return Priority*PriorityFactor;
}

int Accountant::GetCeiling(const std::string& CustomerName) 
{
  int ceiling = -1; // Bogus value
  GetAttributeInt(CustomerRecord+CustomerName,CeilingAttr,ceiling);
  if (ceiling < 0) {
    ceiling = -1; // Meaning unlimited
  }
  return ceiling;
}

int Accountant::GetFloor(const std::string& CustomerName) 
{
  int floor = 0; // unlimited value
  GetAttributeInt(CustomerRecord+CustomerName,FloorAttr,floor);
  if (floor < 0) {
    floor = 0; // Meaning no floor at all
  }
  return floor;
}

//------------------------------------------------------------------
// Return the priority factor of a customer
//------------------------------------------------------------------

// Get group priority local helper function.
double Accountant::getGroupPriorityFactor(const std::string& CustomerName) 
{
	std::string GroupName = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName)->name;

	std::string groupPrioFactorConfig;
	formatstr(groupPrioFactorConfig, "GROUP_PRIO_FACTOR_%s", GroupName.c_str());
	double priorityFactor = param_double(groupPrioFactorConfig.c_str(), 0.0);

	return priorityFactor;
}


double Accountant::GetPriorityFactor(const std::string& CustomerName) 
{
  double PriorityFactor=0;
  GetAttributeFloat(CustomerRecord+CustomerName,PriorityFactorAttr,PriorityFactor);
  if (PriorityFactor < MIN_PRIORITY_FACTOR) {
    PriorityFactor=DefaultPriorityFactor;
	double groupPriorityFactor = 0.0;

	if (strncmp(CustomerName.c_str(),NiceUserName,strlen(NiceUserName))==0) {
		// Nice user
		PriorityFactor=NiceUserPriorityFactor;
    }
	else if ( (groupPriorityFactor = getGroupPriorityFactor(CustomerName) )
				!= 0.0) {
		// Groups user
		PriorityFactor=groupPriorityFactor;
	} else if (   ! AccountantLocalDomain.empty() &&
           AccountantLocalDomain!=GetDomain(CustomerName) ) {
		// Remote user
		PriorityFactor=RemoteUserPriorityFactor;
	}
		// if AccountantLocalDomain is empty, all users are considered local

    // Warning!  This read function has a side effect of a write.
    SetPriorityFactor(CustomerName, PriorityFactor); // write and dprintf()
  }
  return PriorityFactor;
}


//------------------------------------------------------------------
// Reset the Accumulated usage for all users
//------------------------------------------------------------------

void Accountant::ResetAllUsage() 
{
  dprintf(D_ACCOUNTANT,"Accountant::ResetAllUsage\n");
  time_t T=time(0);
  std::string HK;
  ClassAd* ad;

  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
	char const *key = HK.c_str();
	if (strncmp(CustomerRecord.c_str(),key,CustomerRecord.length())) continue;
	AcctLog->BeginTransaction();
    SetAttributeFloat(key,AccumulatedUsageAttr,0);
    SetAttributeFloat(key,WeightedAccumulatedUsageAttr,0);
    SetAttributeInt(key,BeginUsageTimeAttr,T);
	AcctLog->CommitTransaction();
  }
  return;
}

//------------------------------------------------------------------
// Reset the Accumulated usage
//------------------------------------------------------------------

void Accountant::ResetAccumulatedUsage(const std::string& CustomerName) 
{
  dprintf(D_ACCOUNTANT,"Accountant::ResetAccumulatedUsage - CustomerName=%s\n",CustomerName.c_str());
  AcctLog->BeginTransaction();
  SetAttributeFloat(CustomerRecord+CustomerName,AccumulatedUsageAttr,0);
  SetAttributeFloat(CustomerRecord+CustomerName,WeightedAccumulatedUsageAttr,0);
  SetAttributeInt(CustomerRecord+CustomerName,BeginUsageTimeAttr,time(0));
  AcctLog->CommitTransaction();
}

//------------------------------------------------------------------
// Delete the record of a customer
//------------------------------------------------------------------

void Accountant::DeleteRecord(const std::string& CustomerName) 
{
  dprintf(D_ACCOUNTANT,"Accountant::DeleteRecord - CustomerName=%s\n",CustomerName.c_str());
  AcctLog->BeginTransaction();
  DeleteClassAd(CustomerRecord+CustomerName);
  AcctLog->CommitTransaction();
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriorityFactor(const std::string& CustomerName, double PriorityFactor) 
{
  if ( PriorityFactor < MIN_PRIORITY_FACTOR) {
      dprintf(D_ALWAYS, "Error: invalid priority factor: %f, using %f\n",
              PriorityFactor, MIN_PRIORITY_FACTOR);
      PriorityFactor = MIN_PRIORITY_FACTOR;
  }
  dprintf(D_ACCOUNTANT,"Accountant::SetPriorityFactor - CustomerName=%s, PriorityFactor=%8.3f\n",CustomerName.c_str(),PriorityFactor);
  SetAttributeFloat(CustomerRecord+CustomerName,PriorityFactorAttr,PriorityFactor);
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriority(const std::string& CustomerName, double Priority) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetPriority - CustomerName=%s, Priority=%8.3f\n",CustomerName.c_str(),Priority);
  SetAttributeFloat(CustomerRecord+CustomerName,PriorityAttr,Priority);
}
//
//------------------------------------------------------------------
// Set the Ceiling of a customer
//------------------------------------------------------------------

void Accountant::SetCeiling(const std::string& CustomerName, int ceiling) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetCeiling - CustomerName=%s, Ceiling=%d\n",CustomerName.c_str(),ceiling);
  SetAttributeInt(CustomerRecord+CustomerName,CeilingAttr,ceiling);
}

//
//------------------------------------------------------------------
// Set the Floor of a customer
//------------------------------------------------------------------

void Accountant::SetFloor(const std::string& CustomerName, int floor) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetFloor - CustomerName=%s, Floor=%d\n",CustomerName.c_str(),floor);
  SetAttributeInt(CustomerRecord+CustomerName,FloorAttr, floor);
}


//------------------------------------------------------------------
// Set the accumulated usage of a customer
//------------------------------------------------------------------

void Accountant::SetAccumUsage(const std::string& CustomerName, double AccumulatedUsage) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetAccumUsage - CustomerName=%s, Usage=%8.3f\n",CustomerName.c_str(),AccumulatedUsage);
  SetAttributeFloat(CustomerRecord+CustomerName,WeightedAccumulatedUsageAttr,AccumulatedUsage);
}

//------------------------------------------------------------------
// Set the begin usage time of a customer
//------------------------------------------------------------------

void Accountant::SetBeginTime(const std::string& CustomerName, int BeginTime) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetBeginTime - CustomerName=%s, BeginTime=%8d\n",CustomerName.c_str(),BeginTime);
  SetAttributeInt(CustomerRecord+CustomerName,BeginUsageTimeAttr,BeginTime);
}

//------------------------------------------------------------------
// Set the last usage time of a customer
//------------------------------------------------------------------

void Accountant::SetLastTime(const std::string& CustomerName, int LastTime) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetLastTime - CustomerName=%s, LastTime=%8d\n",CustomerName.c_str(),LastTime);
  SetAttributeInt(CustomerRecord+CustomerName,LastUsageTimeAttr,LastTime);
}



//------------------------------------------------------------------
// Add a match
//------------------------------------------------------------------

void Accountant::AddMatch(const std::string& CustomerName, ClassAd* ResourceAd) 
{
  // Get resource name and the time
	std::string ResourceName=GetResourceName(ResourceAd);
  time_t T=time(0);

  dprintf(D_ACCOUNTANT,"Accountant::AddMatch - CustomerName=%s, ResourceName=%s\n",CustomerName.c_str(),ResourceName.c_str());

  double SlotWeight = 0;
  double match_cost = 0;
  if (ResourceAd->LookupFloat(CP_MATCH_COST, match_cost)) {
      // If CP_MATCH_COST attribute is present, we are dealing with a p-slot resource
      // having a consumption policy, during the matchmaking process.  In this
      // situation, we need to maintain multiple matches against a single resource.
      // This allows resource usage accounting and also concurrency limit accounting
      // to work properly with multiple matches against one resource ad.

      // For CP matches, maintain a count of matches during this negotiation cycle:
      int num_cp_matches = 0;
      if (!GetAttributeInt(ResourceRecord+ResourceName, NumCpMatches, num_cp_matches)) num_cp_matches = 0;
	  std::string suffix;
      formatstr(suffix, "_cp_match_%03d", num_cp_matches);
      num_cp_matches += 1;
      SetAttributeInt(ResourceRecord+ResourceName, NumCpMatches, num_cp_matches);

      // Now insert a match under a unique pseudonym for resource name,
      // and using match cost for slot weight:
      SlotWeight = match_cost;
      ResourceName += suffix;
  } else {
      // Check if the resource is used
	  std::string RemoteUser;
      if (GetAttributeString(ResourceRecord+ResourceName,RemoteUserAttr,RemoteUser)) {
        if (CustomerName==RemoteUser) {
    	  dprintf(D_ACCOUNTANT,"Match already existed!\n");
          return;
        }
        RemoveMatch(ResourceName,T);
      }
      SlotWeight = GetSlotWeight(ResourceAd);
  }

  int ResourcesUsed=0;
  GetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  double WeightedResourcesUsed=0.0;
  GetAttributeFloat(CustomerRecord+CustomerName,WeightedResourcesUsedAttr,WeightedResourcesUsed);
  int UnchargedTime=0;
  GetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);
  double WeightedUnchargedTime=0.0;
  GetAttributeFloat(CustomerRecord+CustomerName,WeightedUnchargedTimeAttr,WeightedUnchargedTime);


  AcctLog->BeginTransaction(); 
  
  // Update customer's resource usage count
  ResourcesUsed += 1;
  SetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  WeightedResourcesUsed += SlotWeight;
  SetAttributeFloat(CustomerRecord+CustomerName,WeightedResourcesUsedAttr,WeightedResourcesUsed);
  // add negative "uncharged" time if match starts after last update
  UnchargedTime-=T-LastUpdateTime;
  WeightedUnchargedTime-=(T-LastUpdateTime)*SlotWeight;
  SetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);
  SetAttributeFloat(CustomerRecord+CustomerName,WeightedUnchargedTimeAttr,WeightedUnchargedTime);

  // Do everything we just to update the customer's record a second time if
  // there is a group record to update
  std::string GroupName = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName)->name;


  int GroupResourcesUsed=0;
  double GroupWeightedResourcesUsed = 0.0;
  int GroupUnchargedTime=0;
  double WeightedGroupUnchargedTime=0.0;

  dprintf(D_ACCOUNTANT, "Customername %s GroupName is: %s\n",CustomerName.c_str(), GroupName.c_str());

  GetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
  GetAttributeFloat(CustomerRecord+GroupName,WeightedResourcesUsedAttr,GroupWeightedResourcesUsed);
  GetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
  GetAttributeFloat(CustomerRecord+GroupName,WeightedUnchargedTimeAttr,WeightedGroupUnchargedTime);

  // Update customer's group resource usage count
  GroupWeightedResourcesUsed += SlotWeight;
  GroupResourcesUsed += 1;

  dprintf(D_ACCOUNTANT, "GroupWeightedResourcesUsed=%f SlotWeight=%f\n", GroupWeightedResourcesUsed,SlotWeight);
  SetAttributeFloat(CustomerRecord+GroupName,WeightedResourcesUsedAttr,GroupWeightedResourcesUsed);
  SetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
  // add negative "uncharged" time if match starts after last update 
  GroupUnchargedTime-=T-LastUpdateTime;
  WeightedGroupUnchargedTime-=(T-LastUpdateTime)*SlotWeight;
  SetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
  SetAttributeFloat(CustomerRecord+GroupName,WeightedUnchargedTimeAttr,WeightedGroupUnchargedTime);

  // If this is a nested group (group_a.b.c), update usage up the tree
  std::string GroupNamePart = GroupName;
  while (GroupNamePart.length() > 0) {
	double GroupHierWeightedResourcesUsed = 0.0;
  	GetAttributeFloat(CustomerRecord+GroupNamePart,HierWeightedResourcesUsedAttr,GroupHierWeightedResourcesUsed);
	GroupHierWeightedResourcesUsed += SlotWeight;
  	SetAttributeFloat(CustomerRecord+GroupNamePart,HierWeightedResourcesUsedAttr,GroupHierWeightedResourcesUsed);

  	size_t last_dot = GroupNamePart.find_last_of(".");
  	if (last_dot == std::string::npos) {
		GroupNamePart = "";
  } else {
		GroupNamePart = GroupNamePart.substr(0, last_dot);
  }
  }


  // Set resource's info: user, and start-time
  SetAttributeString(ResourceRecord+ResourceName,RemoteUserAttr,CustomerName);
  SetAttributeFloat(ResourceRecord+ResourceName,SlotWeightAttr,SlotWeight);
  SetAttributeInt(ResourceRecord+ResourceName,StartTimeAttr,T);

  std::string str;
  if (ResourceAd->LookupString(ATTR_MATCHED_CONCURRENCY_LIMITS, str)) {
    SetAttributeString(ResourceRecord+ResourceName,ATTR_MATCHED_CONCURRENCY_LIMITS,str);
    IncrementLimits(str);
  }    

  AcctLog->CommitNondurableTransaction();

  dprintf(D_ACCOUNTANT,"(ACCOUNTANT) Added match between customer %s and resource %s\n",CustomerName.c_str(),ResourceName.c_str());
}

//------------------------------------------------------------------
// Remove a match
//------------------------------------------------------------------

void Accountant::RemoveMatch(const std::string& ResourceName)
{
  RemoveMatch(ResourceName,time(0));
}

void Accountant::RemoveMatch(const std::string& ResourceName, time_t T)
{
  dprintf(D_ACCOUNTANT,"Accountant::RemoveMatch - ResourceName=%s\n",ResourceName.c_str());

  int num_cp_matches = 0;
  if (GetAttributeInt(ResourceRecord+ResourceName, NumCpMatches, num_cp_matches)) {
      // If this attribute is present, this p-slot match is a placeholder for one or more
      // pseudo-matches with resource name having a suffix of "_cp_match_xxx".   These
      // special matches are created to allow proper accounting for resources having a
      // consumption policy.  They are removed during the CheckMatches stage on the 
      // cycle after they are created, due to not actually existing in the resource list.
      // They are replaced by a "real" d-slot record, similar to the way in which a
      // "traditional" p-slot record is removed and replaced by a d-slot match.

      // Delete the placeholder p-slot rec
      DeleteClassAd(ResourceRecord+ResourceName);
      return;
  }

  std::string CustomerName;
  if (!GetAttributeString(ResourceRecord+ResourceName,RemoteUserAttr,CustomerName)) {
      DeleteClassAd(ResourceRecord+ResourceName);
      return;
  }
  time_t StartTime=0;
  GetAttributeInt(ResourceRecord+ResourceName,StartTimeAttr,StartTime);
  int ResourcesUsed=0;
  GetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  double WeightedResourcesUsed=0;
  GetAttributeFloat(CustomerRecord+CustomerName,WeightedResourcesUsedAttr,WeightedResourcesUsed);
  
  int UnchargedTime=0;
  GetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);
  double WeightedUnchargedTime=0.0;
  GetAttributeFloat(CustomerRecord+CustomerName,WeightedUnchargedTimeAttr,WeightedUnchargedTime);
  
  double SlotWeight=1.0;
  GetAttributeFloat(ResourceRecord+ResourceName,SlotWeightAttr,SlotWeight);
  
  int GroupResourcesUsed=0;
  double GroupWeightedResourcesUsed=0.0;
  int GroupUnchargedTime=0;
  double WeightedGroupUnchargedTime=0.0;
  double HierWeightedResourcesUsed = 0.0;
  
  std::string GroupName = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName)->name;
  dprintf(D_ACCOUNTANT, "Customername %s GroupName is: %s\n",CustomerName.c_str(), GroupName.c_str());
  
  GetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
  GetAttributeFloat(CustomerRecord+GroupName,WeightedResourcesUsedAttr,GroupWeightedResourcesUsed);
  GetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
  GetAttributeFloat(CustomerRecord+GroupName,WeightedUnchargedTimeAttr,WeightedGroupUnchargedTime);
  GetAttributeFloat(CustomerRecord+GroupName,HierWeightedResourcesUsedAttr,HierWeightedResourcesUsed);
  
  AcctLog->BeginTransaction();
  // Update customer's resource usage count
  if   (ResourcesUsed>0) ResourcesUsed -= 1;
  SetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  WeightedResourcesUsed -= SlotWeight;
  if( WeightedResourcesUsed < 0 ) {
      WeightedResourcesUsed = 0;
  }
  SetAttributeFloat(CustomerRecord+CustomerName,WeightedResourcesUsedAttr,WeightedResourcesUsed);
  // update uncharged time
  if (StartTime<LastUpdateTime) StartTime=LastUpdateTime;
  UnchargedTime+=T-StartTime;
  WeightedUnchargedTime+=(T-StartTime)*SlotWeight;
  SetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);
  SetAttributeFloat(CustomerRecord+CustomerName,WeightedUnchargedTimeAttr,WeightedUnchargedTime);

  // Do everything we just to update the customer's record a second time if
  // there is a group record to update
  // Update customer's group resource usage count
  GroupResourcesUsed -= 1;
  if (GroupResourcesUsed < 0) GroupResourcesUsed = 0;

  GroupWeightedResourcesUsed -= SlotWeight;
  if(GroupWeightedResourcesUsed < 0.0) {
      GroupWeightedResourcesUsed = 0.0;
  }
  // If this is a nested group (group_a.b.c), update usage up the tree
  std::string GroupNamePart = GroupName;
  while (GroupNamePart.length() > 0) {
	double GroupHierWeightedResourcesUsed = 0.0;
  	GetAttributeFloat(CustomerRecord+GroupNamePart,HierWeightedResourcesUsedAttr,GroupHierWeightedResourcesUsed);
	GroupHierWeightedResourcesUsed -= SlotWeight;
	if (GroupHierWeightedResourcesUsed < 0) GroupHierWeightedResourcesUsed = 0;
  	SetAttributeFloat(CustomerRecord+GroupNamePart,HierWeightedResourcesUsedAttr,GroupHierWeightedResourcesUsed);

  	size_t last_dot = GroupNamePart.find_last_of(".");
  	if (last_dot == std::string::npos) {
		GroupNamePart = "";
  } else {
		GroupNamePart = GroupNamePart.substr(0, last_dot);
  }
  }
  dprintf(D_ACCOUNTANT, "GroupResourcesUsed =%d GroupWeightedResourcesUsed= %f SlotWeight=%f\n",
          GroupResourcesUsed ,GroupWeightedResourcesUsed,SlotWeight);

  SetAttributeFloat(CustomerRecord+GroupName,WeightedResourcesUsedAttr,GroupWeightedResourcesUsed);

  SetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
  // update uncharged time
  GroupUnchargedTime+=T-StartTime;
  WeightedGroupUnchargedTime+=(T-StartTime)*SlotWeight;
  SetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
  SetAttributeFloat(CustomerRecord+GroupName,WeightedUnchargedTimeAttr,WeightedGroupUnchargedTime);

  DeleteClassAd(ResourceRecord+ResourceName);
  AcctLog->CommitNondurableTransaction();

  dprintf(D_ACCOUNTANT, "(ACCOUNTANT) Removed match between customer %s and resource %s\n",
          CustomerName.c_str(),ResourceName.c_str());
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void Accountant::DisplayLog()
{
  std::string HK;
  ClassAd* ad;
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    printf("------------------------------------------------\nkey = %s\n",HK.c_str());
    fPrintAd(stdout, *ad);
  }
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void Accountant::DisplayMatches()
{
  std::string HK;
  ClassAd* ad;
  std::string ResourceName;
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
	char const *key = HK.c_str();
    if (strncmp(ResourceRecord.c_str(),key,ResourceRecord.length())) continue;
    ResourceName=key+ResourceRecord.length();
    std::string RemoteUser;
    ad->LookupString(RemoteUserAttr,RemoteUser);
    printf("Customer=%s , Resource=%s\n",RemoteUser.c_str(),ResourceName.c_str());
  }
}

//------------------------------------------------------------------
// Update priorites for all customers (schedd's)
// Based on the time that passed since the last update
//------------------------------------------------------------------

void Accountant::UpdatePriorities() 
{
  time_t T=time(0);
  int TimePassed=T-LastUpdateTime;
  if (TimePassed==0) return;

  // TimePassed might be less than zero for a number of reasons, including
  // the clock being reset on this machine, and somehow getting a really
  // bad entry in the Accountnew.log  -- Ballard 5/17/00
  if (TimePassed < 0) {
	LastUpdateTime=T;
	return;
  }
  double AgingFactor=::pow(0.5,double(TimePassed)/HalfLifePeriod);
  LastUpdateTime=T;
  SetAttributeInt(AcctRecord,LastUpdateTimeAttr,LastUpdateTime);

  dprintf(D_ACCOUNTANT,"(ACCOUNTANT) Updating priorities - AgingFactor=%8.3f , TimePassed=%d\n",AgingFactor,TimePassed);

  std::string HK;
  ClassAd* ad;

	  // Each iteration of the loop should be atomic for consistency,
	  // but instead of doing one transaction per iteration, wrap the
	  // whole loop in one transaction for efficiency.
  AcctLog->BeginTransaction();

  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
	char const *key = HK.c_str();
	if (strncmp(CustomerRecord.c_str(),key,CustomerRecord.length())) continue;
		UpdateOnePriority(T, TimePassed, AgingFactor, key, ad);
  }

  AcctLog->CommitTransaction();

  // Check if the log needs to be truncated
  struct stat statbuf;
  if( stat(LogFileName.c_str(),&statbuf) ) {
    dprintf( D_ALWAYS, "ERROR in Accountant::UpdatePriorities - "
			 "can't stat database (%s)", LogFileName.c_str() );
  } else if( statbuf.st_size > MaxAcctLogSize ) {
	  AcctLog->TruncLog();
	  dprintf( D_ACCOUNTANT, "Accountant::UpdatePriorities - "
			   "truncating database (prev size=%lu)\n", 
			   (unsigned long)statbuf.st_size ); 
		  // Now that we truncated, check the size, and allow it to
		  // grow to at least double in size before truncating again.
	  if( stat(LogFileName.c_str(),&statbuf) ) {
		  dprintf( D_ALWAYS, "ERROR in Accountant::UpdatePriorities - "
				   "can't stat database (%s)", LogFileName.c_str() );
	  } else {
		  if( statbuf.st_size * 2 > MaxAcctLogSize ) {
			  MaxAcctLogSize = statbuf.st_size * 2;
			  dprintf( D_ACCOUNTANT, "Database has grown, expanding "
					   "MAX_ACCOUNTANT_DATABASE_SIZE to %d\n", 
					   MaxAcctLogSize );
		  }
	  }
  }
}

void
Accountant::UpdateOnePriority(int T, int TimePassed, double AgingFactor, const char *key, ClassAd *ad) {

	double Priority, OldPrio, PriorityFactor;
	int UnchargedTime;
	double WeightedUnchargedTime;
	double AccumulatedUsage, OldAccumulatedUsage;
	double WeightedAccumulatedUsage, OldWeightedAccumulatedUsage;
	double RecentUsage;
	double WeightedRecentUsage;
	int ResourcesUsed;
	double WeightedResourcesUsed;
	int BeginUsageTime;
    // lookup values in the ad
	
    if (ad->LookupFloat(PriorityAttr,Priority)==0) Priority=0;
	if (Priority<MinPriority) Priority=MinPriority;
    OldPrio=Priority;

    // set_prio_factor indicates whether a priority factor has been explicitly set,
    // in which case the record should be kept to preserve the setting
    bool set_prio_factor = true;
    if (ad->LookupFloat(PriorityFactorAttr,PriorityFactor)==0) {
	   	PriorityFactor = DefaultPriorityFactor;
        set_prio_factor = false;
	}

    if (ad->LookupInteger(UnchargedTimeAttr,UnchargedTime)==0) UnchargedTime=0;
    if (ad->LookupFloat(AccumulatedUsageAttr,AccumulatedUsage)==0) AccumulatedUsage=0;
    if (ad->LookupFloat(WeightedUnchargedTimeAttr,WeightedUnchargedTime)==0) WeightedUnchargedTime=0;
    if (ad->LookupFloat(WeightedAccumulatedUsageAttr,WeightedAccumulatedUsage)==0) WeightedAccumulatedUsage=AccumulatedUsage;
    if (ad->LookupInteger(BeginUsageTimeAttr,BeginUsageTime)==0) BeginUsageTime=0;
    if (ad->LookupInteger(ResourcesUsedAttr,ResourcesUsed)==0) ResourcesUsed=0;
	if (ad->LookupFloat(WeightedResourcesUsedAttr,WeightedResourcesUsed)==0) WeightedResourcesUsed=0.0;

    RecentUsage=double(ResourcesUsed)+double(UnchargedTime)/TimePassed;
    WeightedRecentUsage=double(WeightedResourcesUsed)+double(WeightedUnchargedTime)/TimePassed;

	// For groups that may have a hierarchy, use the sum of the usage in the hierarchy,
	// not the usage at this one node in the group.
	double HierWeightedResourcesUsed = 0.0;
	if (ad->LookupFloat(HierWeightedResourcesUsedAttr,HierWeightedResourcesUsed)==0) HierWeightedResourcesUsed=0.0;
	if (HierWeightedResourcesUsed > 0.0) {
				WeightedRecentUsage = HierWeightedResourcesUsed;
	}

	// Age out the existing priority, and add in any new usage
    Priority = Priority * AgingFactor + WeightedRecentUsage * (1 - AgingFactor);

	if (Priority < MinPriority) {
		Priority = MinPriority;
	}

	OldAccumulatedUsage = AccumulatedUsage;
    AccumulatedUsage+=ResourcesUsed*TimePassed+UnchargedTime;

	OldWeightedAccumulatedUsage = WeightedAccumulatedUsage;
    WeightedAccumulatedUsage+=WeightedResourcesUsed*TimePassed+WeightedUnchargedTime;

	if (OldPrio != Priority) {
    	SetAttributeFloat(key,PriorityAttr,Priority);
	}

	if (OldAccumulatedUsage != AccumulatedUsage) {
    	SetAttributeFloat(key,AccumulatedUsageAttr,AccumulatedUsage);
	}

	if (OldWeightedAccumulatedUsage != WeightedAccumulatedUsage) {
    	SetAttributeFloat(key,WeightedAccumulatedUsageAttr,WeightedAccumulatedUsage);
	}

    if (AccumulatedUsage>0 && BeginUsageTime==0) {
		SetAttributeInt(key,BeginUsageTimeAttr,T);
	}

    if (RecentUsage>0) {
		SetAttributeInt(key,LastUsageTimeAttr,T);
	}

		// This attribute is almost always 0, so don't write it unless needed
	int oldUnchargedTime = -1;
	GetAttributeInt(key,UnchargedTimeAttr, oldUnchargedTime);
	if (oldUnchargedTime != 0) {
    	SetAttributeInt(key,UnchargedTimeAttr,0);
	}

	double oldWeightedUnchargedTime = -1.0;
	GetAttributeFloat(key,WeightedUnchargedTimeAttr, oldWeightedUnchargedTime);
	if (oldWeightedUnchargedTime != 0.0) {
    	SetAttributeFloat(key,WeightedUnchargedTimeAttr,0.0);
	}

    if (Priority<MinPriority && ResourcesUsed==0 && AccumulatedUsage==0 && !set_prio_factor) {
		DeleteClassAd(key);
	}

	// This isn't logged, but clear out the submitterLimit and share
	ad->Assign("SubmitterLimit", 0.0);
	ad->Assign("SubmitterShare", 0.0);
    dprintf(D_ACCOUNTANT,"CustomerName=%s , Old Priority=%5.3f , New Priority=%5.3f , ResourcesUsed=%d , WeightedResourcesUsed=%f\n",key,OldPrio,Priority,ResourcesUsed,WeightedResourcesUsed);
    dprintf(D_ACCOUNTANT,"RecentUsage=%8.3f (unweighted %8.3f), UnchargedTime=%8.3f (unweighted %d), AccumulatedUsage=%5.3f (unweighted %5.3f), BeginUsageTime=%d\n",WeightedRecentUsage,RecentUsage,WeightedUnchargedTime,UnchargedTime,WeightedAccumulatedUsage,AccumulatedUsage,BeginUsageTime);

}

//------------------------------------------------------------------
// Go through the list of startd ad's and check of matches
// that were broken (by checkibg the claimed state)
//------------------------------------------------------------------

void Accountant::CheckMatches(ClassAdListDoesNotDeleteAds& ResourceList) 
{
  dprintf(D_ACCOUNTANT,"(Accountant) Checking Matches\n");

  ClassAd* ResourceAd;
  std::string HK;
  ClassAd* ad;
  std::string ResourceName;
  std::string CustomerName;

	  // Create a hash table for speedier lookups of Resource ads.
  HashTable<std::string,ClassAd *> resource_hash(hashFunction);
  ResourceList.Open();
  while ((ResourceAd=ResourceList.Next())!=NULL) {
    ResourceName = GetResourceName(ResourceAd);
    bool success = ( resource_hash.insert( ResourceName, ResourceAd ) == 0 );
    if (!success) {
      dprintf(D_ALWAYS, "WARNING: found duplicate key: %s\n", ResourceName.c_str());
      dPrintAd(D_FULLDEBUG, *ResourceAd);
    }
  }
  ResourceList.Close();

  // Remove matches that were broken
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    char const *key = HK.c_str();
    if (strncmp(ResourceRecord.c_str(),key,ResourceRecord.length())) continue;
    ResourceName=key+ResourceRecord.length();
    if( resource_hash.lookup(ResourceName,ResourceAd) < 0 ) {
      dprintf(D_ACCOUNTANT,"Resource %s class-ad wasn't found in the resource list.\n",ResourceName.c_str());
      RemoveMatch(ResourceName);
    }
	else {
		// Here we need to figure out the CustomerName.
      ad->LookupString(RemoteUserAttr,CustomerName);
      if (!CheckClaimedOrMatched(ResourceAd, CustomerName)) {
        dprintf(D_ACCOUNTANT,"Resource %s was not claimed by %s - removing match\n",ResourceName.c_str(),CustomerName.c_str());
        RemoveMatch(ResourceName);
      }
    }
  }

  // Scan startd ads and add matches that are not registered
  ResourceList.Open();
  while ((ResourceAd=ResourceList.Next())!=NULL) {
	  std::string cust_name;
    if (IsClaimed(ResourceAd, cust_name)) AddMatch(cust_name, ResourceAd);
  }
  ResourceList.Close();

	  // Recalculate limits from the set of resources that are reporting
  LoadLimits(ResourceList);

  return;
}

//------------------------------------------------------------------
// Report the list of Matches for a customer
//------------------------------------------------------------------

ClassAd* Accountant::ReportState(const std::string& CustomerName) {
    dprintf(D_ACCOUNTANT,"Reporting State for customer %s\n",CustomerName.c_str());

    std::string HK;
    ClassAd* ResourceAd;
    time_t StartTime;

    ClassAd* ad = new ClassAd();

    bool isGroup=false;
	std::string cgrp = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName, isGroup)->name;
    // This is a defunct group:
    if (isGroup && (cgrp != CustomerName)) return ad;

    int ResourceNum=1;
    AcctLog->table.startIterations();
    while (AcctLog->table.iterate(HK,ResourceAd)) {
        if (strncmp(ResourceRecord.c_str(), HK.c_str(), ResourceRecord.length())) continue;

        std::string rname;
        if (ResourceAd->LookupString(RemoteUserAttr, rname)==0) continue;

        if (isGroup) {
			std::string rgrp = GroupEntry::GetAssignedGroup(hgq_root_group, rname)->name;
            if (cgrp != rgrp) continue;
        } else {
            // customername is a traditional submitter: group.username@host
            if (CustomerName != rname) continue;

			std::string tmp;
            formatstr(tmp, "Name%d", ResourceNum);
            ad->Assign(tmp, HK.c_str()+ResourceRecord.length());

            if (ResourceAd->LookupInteger(StartTimeAttr,StartTime)==0) StartTime=0;
            formatstr(tmp, "StartTime%d", ResourceNum);
            ad->Assign(tmp, StartTime);
        }

        ResourceNum++;
    }

    return ad;
}

void Accountant::CheckResources(const std::string& CustomerName, int& NumResources, double& NumResourcesRW) {
    dprintf(D_ACCOUNTANT, "Checking Resources for customer %s\n", CustomerName.c_str());

    NumResources = 0;
    NumResourcesRW = 0;

    bool isGroup=false;
	std::string cgrp = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName.c_str(), isGroup)->name;
    // This is a defunct group:
    if (isGroup && (cgrp != CustomerName)) return;

    std::string HK;
    ClassAd* ResourceAd;
    AcctLog->table.startIterations();
    while (AcctLog->table.iterate(HK, ResourceAd)) {
        if (strncmp(ResourceRecord.c_str(), HK.c_str(), ResourceRecord.length())) continue;

		std::string rname;
        if (ResourceAd->LookupString(RemoteUserAttr, rname) == 0) continue;

        if (isGroup) {
            if (cgrp != GroupEntry::GetAssignedGroup(hgq_root_group, rname)->name) continue;
        } else {
            if (CustomerName != rname) continue;
        }

        NumResources += 1;
        double SlotWeight = 1.0;
        ResourceAd->LookupFloat(SlotWeightAttr, SlotWeight);
        NumResourcesRW += SlotWeight;
    }
}


//------------------------------------------------------------------
// Report the whole list of priorities
//------------------------------------------------------------------

ClassAd* Accountant::ReportState(bool rollup) {
    dprintf(D_ACCOUNTANT, "Reporting State%s\n", (rollup) ? " using rollup mode" : "");

    ClassAd* ad = new ClassAd();
    ad->Assign("LastUpdate", LastUpdateTime);

    // assign acct group index numbers first, breadth first ordering
    int EntryNum=1;
	std::map<std::string, int> gnmap;
    std::deque<GroupEntry*> grpq;
    grpq.push_back(hgq_root_group);
    while (!grpq.empty()) {
        GroupEntry* group = grpq.front();
        grpq.pop_front();
        gnmap[group->name] = EntryNum++;
        for (std::vector<GroupEntry*>::iterator j(group->children.begin());  j != group->children.end();  ++j) {
            grpq.push_back(*j);
        }
    }

    // populate the ad with acct group entries, with optional rollup of
    // attributes up the group hierarchy
    ReportGroups(hgq_root_group, ad, rollup, gnmap);

    std::string HK;
    ClassAd* CustomerAd = NULL;
    AcctLog->table.startIterations();
    while (AcctLog->table.iterate(HK,CustomerAd)) {
        if (strncmp(CustomerRecord.c_str(), HK.c_str(), CustomerRecord.length())) continue;
		std::string CustomerName = HK.c_str()+CustomerRecord.length();

        bool isGroup=false;
        GroupEntry* cgrp = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName, isGroup);

        // entries for acct groups are now handled in ReportGroups(), above
        if (isGroup) continue;

        std::string cgname(cgrp->name);
        int snum = EntryNum++;

		std::string tmp;
        formatstr(tmp, "Name%d", snum);
        ad->Assign(tmp, CustomerName);

        formatstr(tmp, "IsAccountingGroup%d", snum);
        ad->Assign(tmp, isGroup);

        formatstr(tmp, "AccountingGroup%d", snum);
        ad->Assign(tmp, cgname);

        double Priority = GetPriority(CustomerName);
        formatstr(tmp, "Priority%d", snum);
        ad->Assign(tmp, Priority);

        int Ceiling = GetCeiling(CustomerName);
        formatstr(tmp, "Ceiling%d", snum);
        ad->Assign(tmp, Ceiling);

        int Floor = GetFloor(CustomerName);
        formatstr(tmp, "Floor%d", snum);
        ad->Assign(tmp, Floor);

        double PriorityFactor = 0;
        if (CustomerAd->LookupFloat(PriorityFactorAttr,PriorityFactor)==0) PriorityFactor=0;
        formatstr(tmp, "PriorityFactor%d", snum);
        ad->Assign(tmp, PriorityFactor);

        int ResourcesUsed = 0;
        if (CustomerAd->LookupInteger(ResourcesUsedAttr,ResourcesUsed)==0) ResourcesUsed=0;
        formatstr(tmp, "ResourcesUsed%d", snum);
        ad->Assign(tmp, ResourcesUsed);
        
        double WeightedResourcesUsed = 0;
        if (CustomerAd->LookupFloat(WeightedResourcesUsedAttr,WeightedResourcesUsed)==0) WeightedResourcesUsed=0;
        formatstr(tmp, "WeightedResourcesUsed%d", snum);
        ad->Assign(tmp, WeightedResourcesUsed);
        
        double AccumulatedUsage = 0;
        if (CustomerAd->LookupFloat(AccumulatedUsageAttr,AccumulatedUsage)==0) AccumulatedUsage=0;
        formatstr(tmp, "AccumulatedUsage%d", snum);
        ad->Assign(tmp, AccumulatedUsage);
        
        double WeightedAccumulatedUsage = 0;
        if (CustomerAd->LookupFloat(WeightedAccumulatedUsageAttr,WeightedAccumulatedUsage)==0) WeightedAccumulatedUsage=0;
        formatstr(tmp, "WeightedAccumulatedUsage%d", snum);
        ad->Assign(tmp, WeightedAccumulatedUsage);
        
        double SubmitterShare = 0;
        if (CustomerAd->LookupFloat("SubmitterShare",SubmitterShare)==0) SubmitterShare=0;
        formatstr(tmp, "SubmitterShare%d", snum);
        ad->Assign(tmp, SubmitterShare);

        double SubmitterLimit = 0;
        if (CustomerAd->LookupFloat("SubmitterLimit",SubmitterLimit)==0) SubmitterLimit=0;
        formatstr(tmp, "SubmitterLimit%d", snum);
        ad->Assign(tmp, SubmitterLimit);

        int BeginUsageTime = 0;
        if (CustomerAd->LookupInteger(BeginUsageTimeAttr,BeginUsageTime)==0) BeginUsageTime=0;
        formatstr(tmp, "BeginUsageTime%d", snum);
        ad->Assign(tmp, BeginUsageTime);
        
        int LastUsageTime = 0;
        if (CustomerAd->LookupInteger(LastUsageTimeAttr,LastUsageTime)==0) LastUsageTime=0;
        formatstr(tmp, "LastUsageTime%d", snum);
        ad->Assign(tmp, LastUsageTime);
    }

    // total number of accountant entries, for acct groups and submittors
    ad->Assign("NumSubmittors", EntryNum-1);

    // include concurrency limit information
	// eventually would like to put this info in the collector somewhere
    ReportLimits(ad);

    return ad;
}

void Accountant::ReportGroups(GroupEntry* group, ClassAd* ad, bool rollup, std::map<std::string, int>& gnmap) {
    // begin by loading straight "non-rolled" data into the attributes for (group)
	std::string CustomerName = group->name;

    ClassAd* CustomerAd = NULL;
	std::string HK(CustomerRecord + CustomerName);
    if (AcctLog->table.lookup(HK, CustomerAd) == -1) {
        dprintf(D_ALWAYS, "WARNING: Expected AcctLog entry \"%s\" to exist", HK.c_str());
        return;
    } 

    bool isGroup=false;
    GroupEntry* cgrp = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName, isGroup);

    std::string cgname;
    int gnum = 0;
    if (isGroup && cgrp) {
        cgname = (cgrp->parent != NULL) ? cgrp->parent->name : cgrp->name;
        gnum = gnmap[cgrp->name];
    } else {
        dprintf(D_ALWAYS, "WARNING: Expected \"%s\" to be a defined group in the accountant", CustomerName.c_str());
        return;
    }

	std::string tmp;
    formatstr(tmp, "Name%d", gnum);
    ad->Assign(tmp, CustomerName);

    formatstr(tmp, "IsAccountingGroup%d", gnum);
    ad->Assign(tmp, isGroup);
    
    formatstr(tmp, "AccountingGroup%d", gnum);
    ad->Assign(tmp, cgname);
    
    double Priority = (!rollup) ? GetPriority(CustomerName) : 0;
    formatstr(tmp, "Priority%d", gnum);
    ad->Assign(tmp, Priority);
    
    double PriorityFactor = 0;
	if (!rollup && cgrp) {
		PriorityFactor = getGroupPriorityFactor( cgrp->name );
	}
	else if (CustomerAd->LookupFloat(PriorityFactorAttr,PriorityFactor)==0) {
		PriorityFactor=0;
	}
    formatstr(tmp, "PriorityFactor%d", gnum);
    ad->Assign(tmp, PriorityFactor);
    
    if (cgrp) {
        formatstr(tmp, "EffectiveQuota%d", gnum);
        ad->Assign(tmp, cgrp->quota);
        formatstr(tmp, "ConfigQuota%d", gnum);
        ad->Assign(tmp, cgrp->config_quota);
        formatstr(tmp, "SubtreeQuota%d", gnum);
        ad->Assign(tmp, cgrp->subtree_quota);
        formatstr(tmp, "GroupSortKey%d", gnum);
        ad->Assign(tmp, cgrp->sort_key);
        formatstr(tmp, "SurplusPolicy%d", gnum);
        const char * policy = "no";
        if (cgrp->autoregroup) policy = "regroup";
        else if (cgrp->accept_surplus) policy = "byquota";
        ad->Assign(tmp, policy);

        formatstr(tmp, "Requested%d", gnum);
        ad->Assign(tmp, cgrp->currently_requested);
    }

    int ResourcesUsed = 0;
    if (CustomerAd->LookupInteger(ResourcesUsedAttr,ResourcesUsed)==0) ResourcesUsed=0;
    formatstr(tmp, "ResourcesUsed%d", gnum);
    ad->Assign(tmp, ResourcesUsed);
    
    double WeightedResourcesUsed = 0;
    if (CustomerAd->LookupFloat(WeightedResourcesUsedAttr,WeightedResourcesUsed)==0) WeightedResourcesUsed=0;
    formatstr(tmp, "WeightedResourcesUsed%d", gnum);
    ad->Assign(tmp, WeightedResourcesUsed);
    
    double AccumulatedUsage = 0;
    if (CustomerAd->LookupFloat(AccumulatedUsageAttr,AccumulatedUsage)==0) AccumulatedUsage=0;
    formatstr(tmp, "AccumulatedUsage%d", gnum);
    ad->Assign(tmp, AccumulatedUsage);

    double HierWeightedResourcesUsed = 0;
    if (CustomerAd->LookupFloat(HierWeightedResourcesUsedAttr,HierWeightedResourcesUsed)==0) HierWeightedResourcesUsed=0;
    formatstr(tmp, "HierWeightedResourcesUsed%d", gnum);
    ad->Assign(tmp, HierWeightedResourcesUsed);
    
    
    double WeightedAccumulatedUsage = 0;
    if (CustomerAd->LookupFloat(WeightedAccumulatedUsageAttr,WeightedAccumulatedUsage)==0) WeightedAccumulatedUsage=0;
    formatstr(tmp, "WeightedAccumulatedUsage%d", gnum);
    ad->Assign(tmp, WeightedAccumulatedUsage);
    
    int BeginUsageTime = 0;
    if (CustomerAd->LookupInteger(BeginUsageTimeAttr,BeginUsageTime)==0) BeginUsageTime=0;
    formatstr(tmp, "BeginUsageTime%d", gnum);
    ad->Assign(tmp, BeginUsageTime);
    
    int LastUsageTime = 0;
    if (CustomerAd->LookupInteger(LastUsageTimeAttr,LastUsageTime)==0) LastUsageTime=0;
    formatstr(tmp, "LastUsageTime%d", gnum);
    ad->Assign(tmp, LastUsageTime);
    
    // Populate group's children recursively, if it has any
    // Recursion is to allow for proper rollup from children to parents
    for (std::vector<GroupEntry*>::iterator j(group->children.begin());  j != group->children.end();  ++j) {
        ReportGroups(*j, ad, rollup, gnmap);
    }

    // if we aren't doing rollup, finish now
    if (!rollup || (NULL == group->parent)) return;

    // get the index of our parent
    int pnum = gnmap[group->parent->name];

    int ival = 0;
    double fval = 0;

    // roll up values to parent
    formatstr(tmp, "ResourcesUsed%d", gnum);
    ad->LookupInteger(tmp, ResourcesUsed);
    formatstr(tmp, "ResourcesUsed%d", pnum);
    ad->LookupInteger(tmp, ival);
    ad->Assign(tmp, ival + ResourcesUsed);

    formatstr(tmp, "WeightedResourcesUsed%d", gnum);
    ad->LookupFloat(tmp, WeightedResourcesUsed);
    formatstr(tmp, "WeightedResourcesUsed%d", pnum);
    ad->LookupFloat(tmp, fval);
    ad->Assign(tmp, fval + WeightedResourcesUsed);

    formatstr(tmp, "AccumulatedUsage%d", gnum);
    ad->LookupFloat(tmp, AccumulatedUsage);
    formatstr(tmp, "AccumulatedUsage%d", pnum);
    ad->LookupFloat(tmp, fval);
    ad->Assign(tmp, fval + AccumulatedUsage);

    formatstr(tmp, "WeightedAccumulatedUsage%d", gnum);
    ad->LookupFloat(tmp, WeightedAccumulatedUsage);
    formatstr(tmp, "WeightedAccumulatedUsage%d", pnum);
    ad->LookupFloat(tmp, fval);
    ad->Assign(tmp, fval + WeightedAccumulatedUsage);

    formatstr(tmp, "BeginUsageTime%d", gnum);
    ad->LookupInteger(tmp, BeginUsageTime);
    formatstr(tmp, "BeginUsageTime%d", pnum);
    ad->LookupInteger(tmp, ival);
    ad->Assign(tmp, std::min(ival, BeginUsageTime));

    formatstr(tmp, "LastUsageTime%d", gnum);
    ad->LookupInteger(tmp, LastUsageTime);
    formatstr(tmp, "LastUsageTime%d", pnum);
    ad->LookupInteger(tmp, ival);
    ad->Assign(tmp, std::max(ival, LastUsageTime));
}

bool Accountant::ReportState(ClassAd& queryAd, ClassAdList & ads, bool rollup /*= false*/)
{
	dprintf(D_ACCOUNTANT, "Reporting State for AccountingAd query\n");

	// check for a qyery constraint, treat a trivial true constraint as no constraint. 
	bool bval = true;
	classad::ExprTree * constraint = queryAd.Lookup(ATTR_REQUIREMENTS);
	if (constraint && ExprTreeIsLiteralBool(constraint, bval) && bval) {
		constraint = nullptr;
	}

	// is there a result limit?
	long long result_limit = 0;
	bool has_limit = queryAd.EvaluateAttrInt(ATTR_LIMIT_RESULTS, result_limit);

	std::string HK;
	ClassAd* CustomerAd = NULL;
	AcctLog->table.startIterations();
	while (AcctLog->table.iterate(HK, CustomerAd)) {
		if (strncmp(CustomerRecord.c_str(), HK.c_str(), CustomerRecord.length())) continue;
		std::string CustomerName = HK.c_str() + CustomerRecord.length();

		if (has_limit && ads.Length() >= result_limit) {
			break;
		}

		bool isGroup = false;
		GroupEntry* cgrp = GroupEntry::GetAssignedGroup(hgq_root_group, CustomerName, isGroup);

		// GetPriority has side effects, It can end up setting Priority and PriorityFactor
		// also it actually returns Priority*PriorityFactor which is what we put into
		// the Priority field of ads we send to the collector (or return from a direct query)
		double effectivePriority = GetPriority(CustomerName);
		if (isGroup && rollup) { effectivePriority = 0; }
		int ceiling = GetCeiling(CustomerName);
		int floor   = GetFloor(CustomerName);

		ClassAd * ad = new ClassAd(*CustomerAd);
		ad->Assign(ATTR_NAME, CustomerName);
		SetMyTypeName(*ad, ACCOUNTING_ADTYPE); // MyType in the accounting log is * (so is target type actually)
		// SetTargetTypeName(*ad, "none");
		ad->Assign(PriorityAttr, effectivePriority);
		ad->Assign(CeilingAttr, ceiling);
		ad->Assign(FloorAttr, floor);
		ad->Assign("IsAccountingGroup", isGroup);
		if (cgrp) {
			ad->Assign("AccountingGroup", cgrp->name);
			if (isGroup) {
				ad->Assign("EffectiveQuota", cgrp->quota);
				ad->Assign("ConfigQuota", cgrp->config_quota);
				ad->Assign("SubtreeQuota", cgrp->subtree_quota);
				ad->Assign("GroupSortKey", cgrp->sort_key);
				const char * policy = "no";
				if (cgrp->autoregroup) policy = "regroup";
				else if (cgrp->accept_surplus) policy = "byquota";
				ad->Assign("SurplusPolicy", policy);
				ad->Assign("Requested", cgrp->currently_requested);
			}
		}
		ad->Delete("CurrentTime"); // some ads have a current time attribute

		// delete some attributes to facilitate comparison between -moduler and non-modular queries
		const bool testing_hack = true;
		if (testing_hack) {
			ad->Delete(ATTR_MY_TYPE);
			ad->Delete(ATTR_TARGET_TYPE);
			ad->Delete("UnchargedTime");
			ad->Delete("WeightedUnchargedTime");
			double truncated_prio = (double)effectivePriority;
			ad->Assign(PriorityAttr, truncated_prio);
		}

		// now that we have built the ad, we can check it against the constraint
		// and either insert it into the result list, or delete it
		if ( ! constraint || EvalExprBool(ad, constraint)) {
			//dPrintAd(D_FULLDEBUG, *ad, false);
			ads.Insert(ad);
		} else {
			delete ad;
		}
	}

	return true;
}


//------------------------------------------------------------------
// Extract resource name from class-ad
//------------------------------------------------------------------

std::string Accountant::GetResourceName(ClassAd* ResourceAd) 
{
  std::string startdName;
  std::string startdAddr;
  
  if (!ResourceAd->LookupString (ATTR_NAME, startdName)) {
    //This should never happen, because unnamed startd ads are rejected.
    EXCEPT ("ERROR in Accountant::GetResourceName - Name not specified");
  }
  std::string Name=startdName;
  Name+="@";

  if (!ResourceAd->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)) {
    //This may happen for grid site ClassAds.
    //Actually, the collector now inserts an IP address if none is provided,
    //but it is more robust to not assume that behavior here.
	dprintf(D_FULLDEBUG, "in Account::GetResourceName - no IP address for %s (no problem if this is a grid site ClassAd).\n", startdName.c_str());
  }
  Name+=startdAddr;
  
  return Name;
}

//------------------------------------------------------------------
// Extract the resource's state
//------------------------------------------------------------------

bool Accountant::GetResourceState(ClassAd* ResourceAd, State& state)
{
  char *str;

  if (!ResourceAd->LookupString(ATTR_STATE, &str)) {
    return false;
  }

  state = string_to_state(str);

  free(str);
  return true;
}

//------------------------------------------------------------------
// Check class ad of startd to see if it's claimed
// return 1 if it is (and set CustomerName to its remote_user), otherwise 0
//------------------------------------------------------------------

int Accountant::IsClaimed(ClassAd* ResourceAd, std::string& CustomerName) {
  State state;
  if (!GetResourceState(ResourceAd, state)) {
    dprintf (D_ALWAYS, "Could not lookup state --- assuming not claimed\n");
    return 0;
  }

  if (state!=claimed_state && state!=preempting_state) return 0;
  
  std::string RemoteUser;
  if (!ResourceAd->LookupString(ATTR_ACCOUNTING_GROUP, RemoteUser)) {
	  if (!ResourceAd->LookupString(ATTR_REMOTE_USER, RemoteUser)) {	// TODDCORE
		dprintf (D_ALWAYS, "Could not lookup remote user --- assuming not claimed\n");
		return 0;
	  }
  }
  if(DiscountSuspendedResources) {
	  std::string RemoteActivity;
    if(!ResourceAd->LookupString(ATTR_ACTIVITY, RemoteActivity)) {
       dprintf(D_ALWAYS, "Could not lookup remote activity\n");
       return 0;
    }
    if (!strcasecmp(getClaimStateString(CLAIM_SUSPENDED), RemoteActivity.c_str())) { 
       dprintf(D_ACCOUNTANT, "Machine is claimed but suspended\n");
       return 0;
    }
  }
  CustomerName=RemoteUser;
  return 1;

}

//------------------------------------------------------------------
// Check class ad of startd to see if it's claimed
// (by a specific user) or matched: 
// return 1 if it is, otherwise 0
//------------------------------------------------------------------

int Accountant::CheckClaimedOrMatched(ClassAd* ResourceAd, const std::string& CustomerName) {
  State state;
  if (!GetResourceState(ResourceAd, state)) {
    dprintf (D_ALWAYS, "Could not lookup state --- assuming not claimed\n");
    return 0;
  }

  if (state==matched_state) return 1;
  if (state!=claimed_state && state!=preempting_state) {
	  dprintf(D_ACCOUNTANT,"State was %s - not claimed or matched\n",
              state_to_string(state));
    return 0;
  }

  std::string RemoteUser;
  if (!ResourceAd->LookupString(ATTR_ACCOUNTING_GROUP, RemoteUser)) {
	  if (!ResourceAd->LookupString(ATTR_REMOTE_USER, RemoteUser)) {	// TODDCORE
		dprintf (D_ALWAYS, "Could not lookup remote user --- assuming not claimed\n");
		return 0;
	  }
  }

  if (CustomerName!=RemoteUser) {
    if(IsDebugLevel(D_ACCOUNTANT)) {
		std::string PreemptingUser;
      if(ResourceAd->LookupString(ATTR_PREEMPTING_ACCOUNTING_GROUP, PreemptingUser) ||
         ResourceAd->LookupString(ATTR_PREEMPTING_USER, PreemptingUser))
      {
		  if(CustomerName == PreemptingUser) {
			  dprintf(D_ACCOUNTANT,"Remote user for startd ad (%s) does not match customer name, because customer %s is still waiting for %s to retire.\n",RemoteUser.c_str(),PreemptingUser.c_str(),RemoteUser.c_str());
			  return 0;
		  }
      }
    }
    dprintf(D_ACCOUNTANT,"Remote user for startd ad (%s) does not match customer name\n",RemoteUser.c_str());
    return 0;
  }

  if(DiscountSuspendedResources) {
	  std::string RemoteActivity;
    if(!ResourceAd->LookupString(ATTR_ACTIVITY, RemoteActivity)) {
        dprintf(D_ALWAYS, "Could not lookup remote activity\n");
        return 0;
    }
    if (!strcasecmp(getClaimStateString(CLAIM_SUSPENDED), RemoteActivity.c_str())) { 
       dprintf(D_ACCOUNTANT, "Machine claimed by %s but suspended\n", 
               RemoteUser.c_str());
       return 0;
    }
  }
  return 1;

}

//------------------------------------------------------------------
// Get Class Ad
//------------------------------------------------------------------

ClassAd* Accountant::GetClassAd(const std::string& Key)
{
  ClassAd* ad=NULL;
  std::ignore = AcctLog->table.lookup(Key,ad);
  return ad;
}

//------------------------------------------------------------------
// Delete Class Ad
//------------------------------------------------------------------

bool Accountant::DeleteClassAd(const std::string& Key)
{
  ClassAd* ad=NULL;
  if (AcctLog->table.lookup(Key,ad)==-1)
	  return false;

  LogDestroyClassAd* log=new LogDestroyClassAd(Key.c_str());
  AcctLog->AppendLog(log);
  return true;
}

//------------------------------------------------------------------
// Set an Integer attribute
//------------------------------------------------------------------

void Accountant::SetAttributeInt(const std::string& Key, const std::string& AttrName, int64_t AttrValue)
{
  if (AcctLog->AdExistsInTableOrTransaction(Key) == false) {
    LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*");
    AcctLog->AppendLog(log);
  }
  char value[24] = { 0 };
  std::to_chars(value, value+sizeof(value)-1, AttrValue);
  LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value);
  AcctLog->AppendLog(log);
}
  
//------------------------------------------------------------------
// Set a Float attribute
//------------------------------------------------------------------

void Accountant::SetAttributeFloat(const std::string& Key, const std::string& AttrName, double AttrValue)
{
  if (AcctLog->AdExistsInTableOrTransaction(Key) == false) {
    LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*");
    AcctLog->AppendLog(log);
  }
  
  char value[255];
  snprintf(value,sizeof(value),"%f",AttrValue);
  LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value);
  AcctLog->AppendLog(log);
}

//------------------------------------------------------------------
// Set a String attribute
//------------------------------------------------------------------

void Accountant::SetAttributeString(const std::string& Key, const std::string& AttrName, const std::string& AttrValue)
{
  if (AcctLog->AdExistsInTableOrTransaction(Key) == false) {
    LogNewClassAd* log=new LogNewClassAd(Key.c_str(),"*");
    AcctLog->AppendLog(log);
  }
  
  std::string value;
  formatstr(value,"\"%s\"",AttrValue.c_str());
  LogSetAttribute* log=new LogSetAttribute(Key.c_str(),AttrName.c_str(),value.c_str());
  AcctLog->AppendLog(log);
}

//------------------------------------------------------------------
// Retrieve a Integer attribute
//------------------------------------------------------------------

bool Accountant::GetAttributeInt(const std::string& Key, const std::string& AttrName, int& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(Key,ad)==-1) return false;
  if (ad->LookupInteger(AttrName,AttrValue)==0) return false;
  return true;
}

bool Accountant::GetAttributeInt(const std::string& Key, const std::string& AttrName, long& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(Key,ad)==-1) return false;
  if (ad->LookupInteger(AttrName,AttrValue)==0) return false;
  return true;
}

bool Accountant::GetAttributeInt(const std::string& Key, const std::string& AttrName, long long& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(Key,ad)==-1) return false;
  if (ad->LookupInteger(AttrName,AttrValue)==0) return false;
  return true;
}

//------------------------------------------------------------------
// Retrieve a Float attribute
//------------------------------------------------------------------

bool Accountant::GetAttributeFloat(const std::string& Key, const std::string& AttrName, double& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(Key,ad)==-1) return false;
  if (ad->LookupFloat(AttrName,AttrValue)==0) return false;
  return true;
}

//------------------------------------------------------------------
// Retrieve a String attribute
//------------------------------------------------------------------

bool Accountant::GetAttributeString(const std::string& Key, const std::string& AttrName, std::string& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(Key,ad)==-1) return false;

  if (ad->LookupString(AttrName,AttrValue)==0) return false;
  return true;
}

//------------------------------------------------------------------
// Find a resource ad in class ad list (by name)
//------------------------------------------------------------------

#if 0
ClassAd* Accountant::FindResourceAd(const string& ResourceName, ClassAdListDoesNotDeleteAds& ResourceList)
{
  ClassAd* ResourceAd;
  
  ResourceList.Open();
  while ((ResourceAd=ResourceList.Next())!=NULL) {
	if (ResourceName==GetResourceName(ResourceAd)) break;
  }
  ResourceList.Close();
  return ResourceAd;
}
#endif

//------------------------------------------------------------------
// Get the users domain
//------------------------------------------------------------------

std::string Accountant::GetDomain(const std::string& CustomerName)
{
  std::string S;
  size_t pos=CustomerName.find('@');
  if (pos== std::string::npos) return S;
  S=CustomerName.substr(pos+1);
  return S;
}

//------------------------------------------------------------------
// Functions for accessing and changing Concurrency Limits
//------------------------------------------------------------------

void Accountant::LoadLimits(ClassAdListDoesNotDeleteAds &resourceList)
{
	ClassAd *resourceAd;

		// Wipe out all the knowledge of limits we think we know
	dprintf(D_ACCOUNTANT, "Previous Limits --\n");
	ClearLimits();

		// Record all the limits that are actually in use in the pool
	resourceList.Open();
	while (NULL != (resourceAd = resourceList.Next())) {
		std::string limits;

		if (resourceAd->LookupString(ATTR_CONCURRENCY_LIMITS, limits)) {
			std::transform(limits.begin(), limits.end(), limits.begin(), ::tolower);
			IncrementLimits(limits);
		}

		if (resourceAd->LookupString(ATTR_PREEMPTING_CONCURRENCY_LIMITS,
									  limits)) {
			std::transform(limits.begin(), limits.end(), limits.begin(), ::tolower);
			IncrementLimits(limits);
		}

			// If the resource is just in the Matched state it will
			// not have information about Concurrency Limits
			// associated, but we have that information in the log.
		State state;
		if (GetResourceState(resourceAd, state) && matched_state == state) {
			 std::string name = GetResourceName(resourceAd);
			 std::string str;
			GetAttributeString(ResourceRecord+name,ATTR_MATCHED_CONCURRENCY_LIMITS,str);
			IncrementLimits(str);
		}
	}
	resourceList.Close();

		// Print out the new limits, at D_ACCOUNTANT. This is useful
		// because the list printed from ClearLimits can be compared
		// to see if anything may be going wrong
	dprintf(D_ACCOUNTANT, "Current Limits --\n");
	DumpLimits();
}

double Accountant::GetLimit(const  std::string& limit)
{
	auto it = concurrencyLimits.find(limit);
	if (it == concurrencyLimits.end()) {
		dprintf(D_ACCOUNTANT,
				"Looking for Limit '%s' count, which does not exist\n",
				limit.c_str());
		return 0.0;
	}

	return it->second;
}

double Accountant::GetLimitMax(const  std::string& limit)
{
    double deflim = param_double("CONCURRENCY_LIMIT_DEFAULT", 2308032);
    std::string::size_type pos = limit.find_last_of('.');
    if (pos != std::string::npos) {
		std::string scopedef("CONCURRENCY_LIMIT_DEFAULT_");
        scopedef += limit.substr(0,pos);
        deflim = param_double(scopedef.c_str(), deflim);
    }
	return param_double((limit + "_LIMIT").c_str(), deflim);
}

void Accountant::DumpLimits()
{
	for (const auto& [limit, count]: concurrencyLimits) {
		dprintf(D_ACCOUNTANT, "  Limit: %s = %f\n", limit.c_str(), count);
	}
}

void Accountant::ReportLimits(ClassAd *attrList)
{
	for (const auto& [limit, count]: concurrencyLimits) {
		std::string attr;
        formatstr(attr, "ConcurrencyLimit_%s", limit.c_str());
        // classad wire protocol doesn't currently support attribute names that include
        // punctuation or symbols outside of '_'.  If we want to include '.' or any other
        // punct, we need to either model these as string values, or add support for quoted
        // attribute names in wire protocol:
        std::replace(attr.begin(), attr.end(), '.', '_');
        attrList->Assign(attr, count);
	}
}

void Accountant::ClearLimits()
{
	for (auto& [limit, count]: concurrencyLimits) {
		dprintf(D_ACCOUNTANT, "  Limit: %s = %f\n", limit.c_str(), count);
		count = 0.0;
	}
}

void Accountant::IncrementLimit(const std::string& _limit)
{
	char *limit = strdup(_limit.c_str());
	double increment;

	dprintf(D_ACCOUNTANT, "IncrementLimit(%s)\n", limit);

	if ( ParseConcurrencyLimit(limit, increment) ) {

		concurrencyLimits[limit] = GetLimit(limit) + increment;

	} else {
		dprintf( D_FULLDEBUG, "Ignoring invalid concurrency limit '%s'\n",
				 limit );
	}

	free(limit);
}

void Accountant::DecrementLimit(const std::string& _limit)
{
	char *limit = strdup(_limit.c_str());
	double increment;

	dprintf(D_ACCOUNTANT, "DecrementLimit(%s)\n", limit);

	if ( ParseConcurrencyLimit(limit, increment) ) {

		concurrencyLimits[limit] = GetLimit(limit) - increment;

	} else {
		dprintf( D_FULLDEBUG, "Ignoring invalid concurrency limit '%s'\n",
				 limit );
	}

	free(limit);
}

void Accountant::IncrementLimits(const std::string& limits)
{
	for (const auto& limit: StringTokenIterator(limits)) {
		IncrementLimit(limit);
	}
}

void Accountant::DecrementLimits(const std::string& limits)
{
	for (const auto& limit: StringTokenIterator(limits)) {
		DecrementLimit(limit);
	}
}

double Accountant::GetSlotWeight(ClassAd *candidate) const 
{
	double SlotWeight = 1.0;
	if(!UseSlotWeights) {
		return SlotWeight;
	}

	if(candidate->LookupFloat(SlotWeightAttr, SlotWeight) == 0 || SlotWeight<0) {
		std::string candidateName;
		candidate->LookupString(ATTR_NAME, candidateName);
		dprintf(D_FULLDEBUG, "Can't get SlotWeight for '%s'; using 1.0\n", 
				candidateName.c_str());
		SlotWeight = 1.0;
	}
	return SlotWeight;
}

GCC_DIAG_ON(float-equal)

