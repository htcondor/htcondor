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

#include "condor_common.h"

#include <math.h>
#include <iomanip.h>

#include "condor_accountant.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_state.h"
#include "condor_attributes.h"
#include "enum_utils.h"
#include "classad_log.h"
#include "string_list.h"

#define MIN_PRIORITY_FACTOR (1.0)

MyString Accountant::AcctRecord="Accountant.";
MyString Accountant::CustomerRecord="Customer.";
MyString Accountant::ResourceRecord="Resource.";

MyString Accountant::PriorityAttr="Priority";
MyString Accountant::ResourcesUsedAttr="ResourcesUsed";
MyString Accountant::UnchargedTimeAttr="UnchargedTime";
MyString Accountant::AccumulatedUsageAttr="AccumulatedUsage";
MyString Accountant::BeginUsageTimeAttr="BeginUsageTime";
MyString Accountant::LastUsageTimeAttr="LastUsageTime"; 
MyString Accountant::PriorityFactorAttr="PriorityFactor";

MyString Accountant::LastUpdateTimeAttr="LastUpdateTime";

MyString Accountant::RemoteUserAttr="RemoteUser";
MyString Accountant::StartTimeAttr="StartTime";


//------------------------------------------------------------------
// Constructor - One time initialization
//------------------------------------------------------------------

Accountant::Accountant()
{
  MinPriority=0.5;
  AcctLog=NULL;
  DiscountSuspendedResources = false;
  GroupNamesList = NULL;
}

//------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------

Accountant::~Accountant()
{
  if (AcctLog) delete AcctLog;
  if (GroupNamesList) delete GroupNamesList;
}

//------------------------------------------------------------------
// Initialize (or re-configure) and read configuration parameters
//------------------------------------------------------------------

void Accountant::Initialize() 
{
  static bool first_time = true;

  // Default values

  char* tmp;
  NiceUserPriorityFactor=10000000;
  RemoteUserPriorityFactor=10000;
  DefaultPriorityFactor=1;
  HalfLifePeriod=86400;

  // get group names
  if ( GroupNamesList ) {
	  delete GroupNamesList;
	  GroupNamesList = NULL;
  }
  char *groups = param("GROUP_NAMES");
  if ( groups ) {
		GroupNamesList = new StringList;
		ASSERT(GroupNamesList);
		GroupNamesList->initializeFromString(groups);
		free(groups);
  }

  // get half life period
  
  tmp = param("PRIORITY_HALFLIFE");
  if(tmp) {
	  HalfLifePeriod=(float)atof(tmp);
	  free(tmp);
  }

  // get nice users priority factor

  tmp = param("NICE_USER_PRIO_FACTOR");
  if(tmp) {
	  NiceUserPriorityFactor=(float)atof(tmp);
	  free(tmp);
  }

  // get remote users priority factor

  tmp = param("REMOTE_PRIO_FACTOR");
  if(tmp) {
	  RemoteUserPriorityFactor=(float)atof(tmp);
	  free(tmp);
  }

  // get default priority factor

  tmp = param("DEFAULT_PRIO_FACTOR");
  if(tmp) {
	  DefaultPriorityFactor=(float)atof(tmp);
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

  tmp = param("MAX_ACCOUNTANT_DATABASE_SIZE");
  if( tmp ) {
	  MaxAcctLogSize = atoi( tmp );
	  free(tmp);
  } else {
	  MaxAcctLogSize = 1000000;
  }

  tmp = param("SPOOL");
  if(tmp) {
	  LogFileName=tmp;
	  LogFileName+="/Accountantnew.log";
	  free( tmp );
  } else {
	  EXCEPT( "SPOOL not defined!" );
  }

  DiscountSuspendedResources = param_boolean(
                             "NEGOTIATOR_DISCOUNT_SUSPENDED_RESOURCES",false);

  dprintf(D_ACCOUNTANT,"PRIORITY_HALFLIFE=%f\n",HalfLifePeriod);
  dprintf(D_ACCOUNTANT,"NICE_USER_PRIO_FACTOR=%f\n",NiceUserPriorityFactor);
  dprintf(D_ACCOUNTANT,"REMOTE_PRIO_FACTOR=%f\n",RemoteUserPriorityFactor);
  dprintf(D_ACCOUNTANT,"DEFAULT_PRIO_FACTOR=%f\n",DefaultPriorityFactor);
  dprintf(D_ACCOUNTANT,"ACCOUNTANT_LOCAL_DOMAIN=%s\n",
				  AccountantLocalDomain.Value());
  dprintf( D_ACCOUNTANT, "MAX_ACCOUNTANT_DATABASE_SIZE=%d\n",
		   MaxAcctLogSize );

  if (!AcctLog) {
    AcctLog=new ClassAdLog(LogFileName.Value());
    dprintf(D_ACCOUNTANT,"Accountant::Initialize - LogFileName=%s\n",
					LogFileName.Value());
  }

  // get last update time

  LastUpdateTime=0;
  GetAttributeInt(AcctRecord,LastUpdateTimeAttr,LastUpdateTime);

  // if at startup, do a sanity check to make certain number of resource
  // records for a user and what the user record says jives
  if ( first_time ) {
	  HashKey HK;
	  char key[_POSIX_PATH_MAX];
	  ClassAd* ad;
	  AttrList *unused;
	  StringList users;
	  char *next_user;
	  MyString user;
	  int resources_used, resources_used_really;
	  int total_overestimated_resources = 0;
	  int total_overestimated_users = 0;

	  dprintf(D_ACCOUNTANT,"Sanity check on number of resources per user\n");

		// first find all the users
	  AcctLog->table.startIterations();
	  while (AcctLog->table.iterate(HK,ad)) {
		HK.sprint(key);
			// skip records that are not customer records...
		if (strncmp(CustomerRecord.Value(),key,CustomerRecord.Length())) continue;
			// for now, skip records that are "group" customer records. 
			// TODO: we should fix the below sanity check code so it understands
			// fixing up group customer records as well.
		char *user = &(key[CustomerRecord.Length()]);
		if (GroupNamesList && GroupNamesList->contains_anycase(user)) continue;
			// if we made it here, append to our list of users
		users.append( user );
	  }
		// ok, now StringList users has all the users.  for each user,
		// compare what the customer record claims for usage -vs- actual
		// number of resources
	  users.rewind();
	  while( (next_user=users.next()) ) 
	  {
		  user = next_user;
		  resources_used = GetResourcesUsed(user);

		  /* It appears here that only the second variable is of interest to
		  	this part of the code, however, this function returns new'ed memory
			so keep track of it and delete it so we don't leak memory. */
		  unused = ReportState(user,&resources_used_really);
		  delete unused;

		  if ( resources_used == resources_used_really ) {
			dprintf(D_ACCOUNTANT,"Customer %s using %d resources\n",next_user,
				  resources_used);
		  } else {
			dprintf(D_ALWAYS,
				"FIXING - Customer %s using %d resources, but only found %d\n",
				next_user,resources_used,resources_used_really);
			SetAttributeInt(CustomerRecord+user,ResourcesUsedAttr,resources_used_really);
			if ( resources_used > resources_used_really ) {
				total_overestimated_resources += 
					( resources_used - resources_used_really );
				total_overestimated_users++;
			}
		  }
	  }
	  if ( total_overestimated_users ) {
		  dprintf( D_ALWAYS,
			  "FIXING - Overestimated %d resources across %d users "
			  "(from a total of %d users)\n",
			  total_overestimated_resources,total_overestimated_users,
			  users.number() );
	  }
  }

  // Update priorities

  UpdatePriorities();
}

//------------------------------------------------------------------
// Return the number of resources used
//------------------------------------------------------------------

int Accountant::GetResourcesUsed(const MyString& CustomerName) 
{
  int ResourcesUsed=0;
  GetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  return ResourcesUsed;
}

//------------------------------------------------------------------
// Return the priority of a customer
//------------------------------------------------------------------

float Accountant::GetPriority(const MyString& CustomerName) 
{
    // Warning!  This function has a side effect of a writing the
    // PriorityFactor.
  float PriorityFactor=GetPriorityFactor(CustomerName);
  float Priority=MinPriority;
  GetAttributeFloat(CustomerRecord+CustomerName,PriorityAttr,Priority);
  if (Priority<MinPriority) {
    Priority=MinPriority;
    // Warning!  This read function has a side effect of a write.
    SetPriority(CustomerName,Priority);// write and dprintf()
  }
  return Priority*PriorityFactor;
}

//------------------------------------------------------------------
// Return the priority factor of a customer
//------------------------------------------------------------------

// Get group priority local helper function.
static float getGroupPriorityFactor(const MyString& CustomerName) 
{
	float priorityFactor = 0.0;	// "error" value

	// Group names contain a '.' character.
	// To do:  This is a weak test.  Improve the group test by also checking
	// the GROUP_NAMES config macro value.
	int pos = CustomerName.FindChar('.');
	if ( pos == 0 ) return priorityFactor;

	// Group separator character found: Accounting groups
	MyString GroupName = CustomerName;
	GroupName.setChar(pos,'\0');
	MyString groupPrioFactorConfig;
	groupPrioFactorConfig.sprintf("GROUP_PRIO_FACTOR_%s",
			GroupName.Value() );
#define ERR_CONVERT_DEFPRIOFACTOR   (-1.0)
	float tmpPriorityFactor = param_float(groupPrioFactorConfig.Value(),
			   ERR_CONVERT_DEFPRIOFACTOR);
	if (tmpPriorityFactor != ERR_CONVERT_DEFPRIOFACTOR) {
		priorityFactor = tmpPriorityFactor;
	}
	return priorityFactor;
}

float Accountant::GetPriorityFactor(const MyString& CustomerName) 
{
  float PriorityFactor=0;
  GetAttributeFloat(CustomerRecord+CustomerName,PriorityFactorAttr,PriorityFactor);
  if (PriorityFactor < MIN_PRIORITY_FACTOR) {
    PriorityFactor=DefaultPriorityFactor;
	float groupPriorityFactor = 0.0;

	if (strncmp(CustomerName.Value(),NiceUserName,strlen(NiceUserName))==0) {
		// Nice user
		PriorityFactor=NiceUserPriorityFactor;
    }
	else if ( (groupPriorityFactor = getGroupPriorityFactor(CustomerName) )
				!= 0.0) {
		// Groups user
		PriorityFactor=groupPriorityFactor;
	} else if (   ! AccountantLocalDomain.IsEmpty() &&
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
  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* ad;

  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    HK.sprint(key);
    if (strncmp(CustomerRecord.Value(),key,CustomerRecord.Length())) continue;
	AcctLog->BeginTransaction();
    SetAttributeFloat(key,AccumulatedUsageAttr,0);
    SetAttributeInt(key,BeginUsageTimeAttr,T);
	AcctLog->CommitTransaction();
  }
  return;
}

//------------------------------------------------------------------
// Reset the Accumulated usage
//------------------------------------------------------------------

void Accountant::ResetAccumulatedUsage(const MyString& CustomerName) 
{
  dprintf(D_ACCOUNTANT,"Accountant::ResetAccumulatedUsage - CustomerName=%s\n",CustomerName.Value());
  AcctLog->BeginTransaction();
  SetAttributeFloat(CustomerRecord+CustomerName,AccumulatedUsageAttr,0);
  SetAttributeInt(CustomerRecord+CustomerName,BeginUsageTimeAttr,time(0));
  AcctLog->CommitTransaction();
}

//------------------------------------------------------------------
// Delete the record of a customer
//------------------------------------------------------------------

void Accountant::DeleteRecord(const MyString& CustomerName) 
{
  dprintf(D_ACCOUNTANT,"Accountant::DeleteRecord - CustomerName=%s\n",CustomerName.Value());
  AcctLog->BeginTransaction();
  DeleteClassAd(CustomerRecord+CustomerName);
  AcctLog->CommitTransaction();
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriorityFactor(const MyString& CustomerName, float PriorityFactor) 
{
  if ( PriorityFactor < MIN_PRIORITY_FACTOR) {
      dprintf(D_ALWAYS, "Error: invalid priority factor: %f, using %f\n",
              PriorityFactor, MIN_PRIORITY_FACTOR);
      PriorityFactor = MIN_PRIORITY_FACTOR;
  }
  dprintf(D_ACCOUNTANT,"Accountant::SetPriorityFactor - CustomerName=%s, PriorityFactor=%8.3f\n",CustomerName.Value(),PriorityFactor);
  SetAttributeFloat(CustomerRecord+CustomerName,PriorityFactorAttr,PriorityFactor);
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriority(const MyString& CustomerName, float Priority) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetPriority - CustomerName=%s, Priority=%8.3f\n",CustomerName.Value(),Priority);
  SetAttributeFloat(CustomerRecord+CustomerName,PriorityAttr,Priority);
}

//------------------------------------------------------------------
// Set the accumulated usage of a customer
//------------------------------------------------------------------

void Accountant::SetAccumUsage(const MyString& CustomerName, float AccumulatedUsage) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetAccumUsage - CustomerName=%s, Usage=%8.3f\n",CustomerName.Value(),AccumulatedUsage);
  SetAttributeFloat(CustomerRecord+CustomerName,AccumulatedUsageAttr,AccumulatedUsage);
}

//------------------------------------------------------------------
// Set the begin usage time of a customer
//------------------------------------------------------------------

void Accountant::SetBeginTime(const MyString& CustomerName, int BeginTime) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetBeginTime - CustomerName=%s, BeginTime=%8d\n",CustomerName.Value(),BeginTime);
  SetAttributeInt(CustomerRecord+CustomerName,BeginUsageTimeAttr,BeginTime);
}

//------------------------------------------------------------------
// Set the last usage time of a customer
//------------------------------------------------------------------

void Accountant::SetLastTime(const MyString& CustomerName, int LastTime) 
{
  dprintf(D_ACCOUNTANT,"Accountant::SetLastTime - CustomerName=%s, LastTime=%8d\n",CustomerName.Value(),LastTime);
  SetAttributeInt(CustomerRecord+CustomerName,LastUsageTimeAttr,LastTime);
}



//------------------------------------------------------------------
// Add a match
//------------------------------------------------------------------

void Accountant::AddMatch(const MyString& CustomerName, ClassAd* ResourceAd) 
{
  // Get resource name and the time
  MyString ResourceName=GetResourceName(ResourceAd);
  time_t T=time(0);
  AddMatch(CustomerName,ResourceName,T);
}

void Accountant::AddMatch(const MyString& CustomerName, const MyString& ResourceName, time_t T)
{
  // dprintf(D_ACCOUNTANT,"Accountant::AddMatch - CustomerName=%s, ResourceName=%s\n",CustomerName.Value(),ResourceName.Value());

  // Check if the resource is used
  MyString RemoteUser;
  if (GetAttributeString(ResourceRecord+ResourceName,RemoteUserAttr,RemoteUser)) {
    if (CustomerName==RemoteUser) {
      // dprintf(D_ACCOUNTANT,"Match already existed!\n");
      return;
    }
    RemoveMatch(ResourceName,T);
  }
  

  int ResourcesUsed=0;
  GetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  int UnchargedTime=0;
  GetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);

	// Determine if we need to update a second customer record w/ the group name.
  bool update_group_info = false;
  MyString GroupName;
  int GroupResourcesUsed=0;
  int GroupUnchargedTime=0;
  if ( GroupNamesList ) {
	  GroupName = CustomerName;
	  int pos = GroupName.FindChar('.');	// '.' is the group seperater
	  GroupName.setChar(pos,'\0');
		// if there is a group seperater character, and if the group name
		// is a valid one listed in the GroupNamesList, then we want to update
		// two customer records: one with the full name of the customer (group.user),
		// and one with just the name of the group (group).
	  if ( pos != -1 && GroupNamesList->contains_anycase(GroupName.Value()) ) {
			update_group_info = true;			
			GetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
			GetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
	  }
  }

  AcctLog->BeginTransaction(); 
  
  // Update customer's resource usage count
  ResourcesUsed++;
  SetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
  // add negative "uncharged" time if match starts after last update
  UnchargedTime-=T-LastUpdateTime;
  SetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);

  // Do everything we just to update the customer's record a second time if
  // there is a group record to update
  if ( update_group_info ) {
	  // Update customer's group resource usage count
	  GroupResourcesUsed++;
	  SetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
	  // add negative "uncharged" time if match starts after last update 
	  GroupUnchargedTime-=T-LastUpdateTime;
	  SetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
  }

  // Set reosurce's info: user, and start-time
  SetAttributeString(ResourceRecord+ResourceName,RemoteUserAttr,CustomerName);
  SetAttributeInt(ResourceRecord+ResourceName,StartTimeAttr,T);

  AcctLog->CommitTransaction();

  dprintf(D_ACCOUNTANT,"(ACCOUNTANT) Added match between customer %s and resource %s\n",CustomerName.Value(),ResourceName.Value());
}

//------------------------------------------------------------------
// Remove a match
//------------------------------------------------------------------

void Accountant::RemoveMatch(const MyString& ResourceName)
{
  RemoveMatch(ResourceName,time(0));
}

void Accountant::RemoveMatch(const MyString& ResourceName, time_t T)
{
  dprintf(D_ACCOUNTANT,"Accountant::RemoveMatch - ResourceName=%s\n",ResourceName.Value());

  MyString CustomerName;
  if (GetAttributeString(ResourceRecord+ResourceName,RemoteUserAttr,CustomerName)) {
    int StartTime=0;
    GetAttributeInt(ResourceRecord+ResourceName,StartTimeAttr,StartTime);
    int ResourcesUsed=0;
    GetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
    int UnchargedTime=0;
    GetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);

	// Determine if we need to update a second customer record w/ the group name.
	bool update_group_info = false;
	MyString GroupName;
	int GroupResourcesUsed=0;
	int GroupUnchargedTime=0;
	if ( GroupNamesList ) {
	  GroupName = CustomerName;
	  int pos = GroupName.FindChar('.');	// '.' is the group seperater
	  GroupName.setChar(pos,'\0');
		// if there is a group seperater character, and if the group name
		// is a valid one listed in the GroupNamesList, then we want to update
		// two customer records: one with the full name of the customer (group.user),
		// and one with just the name of the group (group).
	  if ( pos != -1 && GroupNamesList->contains_anycase(GroupName.Value()) ) {
			update_group_info = true;			
			GetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
			GetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
	  }
	}


	AcctLog->BeginTransaction();
    // Update customer's resource usage count
    if (ResourcesUsed>0) ResourcesUsed--;
    SetAttributeInt(CustomerRecord+CustomerName,ResourcesUsedAttr,ResourcesUsed);
    // update uncharged time
    if (StartTime<LastUpdateTime) StartTime=LastUpdateTime;
    UnchargedTime+=T-StartTime;
    SetAttributeInt(CustomerRecord+CustomerName,UnchargedTimeAttr,UnchargedTime);

	// Do everything we just to update the customer's record a second time if
	// there is a group record to update
	if ( update_group_info ) {
	  // Update customer's group resource usage count
      if (GroupResourcesUsed>0) GroupResourcesUsed--;
	  SetAttributeInt(CustomerRecord+GroupName,ResourcesUsedAttr,GroupResourcesUsed);
	  // update uncharged time
	  GroupUnchargedTime+=T-StartTime;
	  SetAttributeInt(CustomerRecord+GroupName,UnchargedTimeAttr,GroupUnchargedTime);
	}

	DeleteClassAd(ResourceRecord+ResourceName);
	AcctLog->CommitTransaction();

    dprintf(D_ACCOUNTANT,
		"(ACCOUNTANT) Removed match between customer %s and resource %s\n",
			CustomerName.Value(),ResourceName.Value());
  } else {  
      DeleteClassAd(ResourceRecord+ResourceName);
  }
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void Accountant::DisplayLog()
{
  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* ad;
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    HK.sprint(key);
    printf("------------------------------------------------\nkey = %s\n",key);
    ad->fPrint(stdout);
  }
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------

void Accountant::DisplayMatches()
{
  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* ad;
  MyString ResourceName;
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    HK.sprint(key);
    if (strncmp(ResourceRecord.Value(),key,ResourceRecord.Length())) continue;
    ResourceName=key+ResourceRecord.Length();
    ad->LookupString(RemoteUserAttr.Value(),key);
    printf("Customer=%s , Resource=%s\n",key,ResourceName.Value());
  }
}

//------------------------------------------------------------------
// Update priorites for all customers (schedd's)
// Based on the time that passed since the last update
//------------------------------------------------------------------

void Accountant::UpdatePriorities() 
{
  int T=time(0);
  int TimePassed=T-LastUpdateTime;
  if (TimePassed==0) return;

  // TimePassed might be less than zero for a number of reasons, including
  // the clock being reset on this machine, and somehow getting a really
  // bad entry in the Accountnew.log  -- Ballard 5/17/00
  if (TimePassed < 0) {
	LastUpdateTime=T;
	return;
  }
  float AgingFactor=(float) pow(0.5,float(TimePassed)/HalfLifePeriod);
  LastUpdateTime=T;
  SetAttributeInt(AcctRecord,LastUpdateTimeAttr,LastUpdateTime);

  dprintf(D_ACCOUNTANT,"(ACCOUNTANT) Updating priorities - AgingFactor=%8.3f , TimePassed=%d\n",AgingFactor,TimePassed);

  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* ad;
  float Priority, OldPrio, PriorityFactor;
  int UnchargedTime;
  float AccumulatedUsage;
  float RecentUsage;
  int ResourcesUsed;
  int BeginUsageTime;

  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    HK.sprint(key);
    if (strncmp(CustomerRecord.Value(),key,CustomerRecord.Length())) continue;

    // lookup values in the ad
    if (ad->LookupFloat(PriorityAttr.Value(),Priority)==0) Priority=0;
	if (Priority<MinPriority) Priority=MinPriority;
    OldPrio=Priority;

    if (ad->LookupFloat(PriorityFactorAttr.Value(),PriorityFactor)==0) {
	   	PriorityFactor = DefaultPriorityFactor;
	}

    if (ad->LookupInteger(UnchargedTimeAttr.Value(),UnchargedTime)==0) UnchargedTime=0;
    if (ad->LookupFloat(AccumulatedUsageAttr.Value(),AccumulatedUsage)==0) AccumulatedUsage=0;
    if (ad->LookupInteger(BeginUsageTimeAttr.Value(),BeginUsageTime)==0) BeginUsageTime=0;
    if (ad->LookupInteger(ResourcesUsedAttr.Value(),ResourcesUsed)==0) ResourcesUsed=0;

    RecentUsage=float(ResourcesUsed)+float(UnchargedTime)/TimePassed;
    Priority=Priority*AgingFactor+RecentUsage*(1-AgingFactor);
    AccumulatedUsage+=ResourcesUsed*TimePassed+UnchargedTime;

	AcctLog->BeginTransaction();
    SetAttributeFloat(key,PriorityAttr,Priority);
    SetAttributeFloat(key,AccumulatedUsageAttr,AccumulatedUsage);
    if (AccumulatedUsage>0 && BeginUsageTime==0) SetAttributeInt(key,BeginUsageTimeAttr,T);
    if (RecentUsage>0) SetAttributeInt(key,LastUsageTimeAttr,T);
    SetAttributeInt(key,UnchargedTimeAttr,0);

    if (Priority<MinPriority && ResourcesUsed==0 &&
		   	AccumulatedUsage==0 && PriorityFactor==DefaultPriorityFactor ) {
		DeleteClassAd(key);
	}

	AcctLog->CommitTransaction();
	
    dprintf(D_ACCOUNTANT,"CustomerName=%s , Old Priority=%5.3f , New Priority=%5.3f , ResourcesUsed=%d\n",key,OldPrio,Priority,ResourcesUsed);
    dprintf(D_ACCOUNTANT,"RecentUsage=%8.3f, UnchargedTime=%d, AccumulatedUsage=%5.3f, BeginUsageTime=%d\n",RecentUsage,UnchargedTime,AccumulatedUsage,BeginUsageTime);

  }

  // Check if the log needs to be truncated
  struct stat statbuf;
  if( stat(LogFileName.Value(),&statbuf) ) {
    dprintf( D_ALWAYS, "ERROR in Accountant::UpdatePriorities - "
			 "can't stat database (%s)", LogFileName.Value() );
  } else if( statbuf.st_size > MaxAcctLogSize ) {
	  AcctLog->TruncLog();
	  dprintf( D_ACCOUNTANT, "Accountant::UpdatePriorities - "
			   "truncating database (prev size=%lu)\n", 
			   (unsigned long)statbuf.st_size ); 
		  // Now that we truncated, check the size, and allow it to
		  // grow to at least double in size before truncating again.
	  if( stat(LogFileName.Value(),&statbuf) ) {
		  dprintf( D_ALWAYS, "ERROR in Accountant::UpdatePriorities - "
				   "can't stat database (%s)", LogFileName.Value() );
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

//------------------------------------------------------------------
// Go through the list of startd ad's and check of matches
// that were broken (by checkibg the claimed state)
//------------------------------------------------------------------

void Accountant::CheckMatches(ClassAdList& ResourceList) 
{
  dprintf(D_ACCOUNTANT,"(Accountant) Checking Matches\n");

  ClassAd* ResourceAd;
  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* ad;
  MyString ResourceName;
  MyString CustomerName;

  // Remove matches that were broken
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ad)) {
    HK.sprint(key);
    if (strncmp(ResourceRecord.Value(),key,ResourceRecord.Length())) continue;
    ResourceName=key+ResourceRecord.Length();
    ResourceAd=FindResourceAd(ResourceName, ResourceList);
    if (!ResourceAd) {
      dprintf(D_ACCOUNTANT,"Resource %s class-ad wasn't found in the resource list.\n",ResourceName.Value());
      RemoveMatch(ResourceName);
    }
	else {
		// Here we need to figure out the CustomerName.
      ad->LookupString(RemoteUserAttr.Value(),key);
      CustomerName=key;
      if (!CheckClaimedOrMatched(ResourceAd, CustomerName)) {
        dprintf(D_ACCOUNTANT,"Resource %s was not claimed by %s - removing match\n",ResourceName.Value(),CustomerName.Value());
        RemoveMatch(ResourceName);
      }
    }
  }

  // Scan startd ads and add matches that are not registered
  ResourceList.Open();
  while ((ResourceAd=ResourceList.Next())!=NULL) {
    if (IsClaimed(ResourceAd, CustomerName)) AddMatch(CustomerName, ResourceAd);
  }
  ResourceList.Close();

  return;
}

//------------------------------------------------------------------
// Report the list of Matches for a customer
//------------------------------------------------------------------

AttrList* Accountant::ReportState(const MyString& CustomerName, int * NumResources) {

  dprintf(D_ACCOUNTANT,"Reporting State for customer %s\n",CustomerName.Value());

  HashKey HK;
  char key[200];
  ClassAd* ResourceAd;
  MyString ResourceName;
  int StartTime;

  AttrList* ad=new AttrList();
  char tmp[512];

  int ResourceNum=1;
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,ResourceAd)) {
    HK.sprint(key);
    if (strncmp(ResourceRecord.Value(),key,ResourceRecord.Length())) continue;

    if (ResourceAd->LookupString(RemoteUserAttr.Value(),tmp)==0) continue;
    if (CustomerName!=MyString(tmp)) continue;

    ResourceName=key+ResourceRecord.Length();
    sprintf(tmp,"Name%d = \"%s\"",ResourceNum,ResourceName.Value());
    ad->Insert(tmp);

    if (ResourceAd->LookupInteger(StartTimeAttr.Value(),StartTime)==0) StartTime=0;
    sprintf(tmp,"StartTime%d = %d",ResourceNum,StartTime);
    ad->Insert(tmp);

    ResourceNum++;
  }

  if ( NumResources ) {
	  *NumResources = ResourceNum - 1;
  }

  return ad;
}

//------------------------------------------------------------------
// Report the whole list of priorities
//------------------------------------------------------------------

AttrList* Accountant::ReportState() {

  dprintf(D_ACCOUNTANT,"Reporting State\n");

  HashKey HK;
  char key[200];
  ClassAd* CustomerAd;
  MyString CustomerName;
  float PriorityFactor;
  float AccumulatedUsage;
  int BeginUsageTime;
  int LastUsageTime;
  int ResourcesUsed;

  AttrList* ad=new AttrList();
  char tmp[512];
  sprintf(tmp, "LastUpdate = %d", LastUpdateTime);
  ad->Insert(tmp);

  int OwnerNum=1;
  AcctLog->table.startIterations();
  while (AcctLog->table.iterate(HK,CustomerAd)) {
    HK.sprint(key);
    if (strncmp(CustomerRecord.Value(),key,CustomerRecord.Length())) continue;

	// The following Insert() calls are passed 'false' to prevent
	// AttrList from checking for duplicates. This is to enhance
	// performance, but is admittedly dangerous if we're not certain
	// that the items we're inserting are unique. Use caution.

    CustomerName=key+CustomerRecord.Length();
    sprintf(tmp,"Name%d = \"%s\"",OwnerNum,CustomerName.Value());
    ad->Insert(tmp, false);

    sprintf(tmp,"Priority%d = %f",OwnerNum,GetPriority(CustomerName));
    ad->Insert(tmp, false);

    if (CustomerAd->LookupInteger(ResourcesUsedAttr.Value(),ResourcesUsed)==0) ResourcesUsed=0;
    sprintf(tmp,"ResourcesUsed%d = %d",OwnerNum,ResourcesUsed);
    ad->Insert(tmp, false);

    if (CustomerAd->LookupFloat(AccumulatedUsageAttr.Value(),AccumulatedUsage)==0) AccumulatedUsage=0;
    sprintf(tmp,"AccumulatedUsage%d = %f",OwnerNum,AccumulatedUsage);
    ad->Insert(tmp, false);

    if (CustomerAd->LookupInteger(BeginUsageTimeAttr.Value(),BeginUsageTime)==0) BeginUsageTime=0;
    sprintf(tmp,"BeginUsageTime%d = %d",OwnerNum,BeginUsageTime);
    ad->Insert(tmp, false);

    if (CustomerAd->LookupInteger(LastUsageTimeAttr.Value(),LastUsageTime)==0) LastUsageTime=0;
    sprintf(tmp,"LastUsageTime%d = %d",OwnerNum,LastUsageTime);
    ad->Insert(tmp, false);

    if (CustomerAd->LookupFloat(PriorityFactorAttr.Value(),PriorityFactor)==0) PriorityFactor=0;
    sprintf(tmp,"PriorityFactor%d = %f",OwnerNum,PriorityFactor);
    ad->Insert(tmp, false);

    OwnerNum++;
  }

  sprintf(tmp,"NumSubmittors = %d", OwnerNum-1);
  ad->Insert(tmp, false);
  return ad;
}

//------------------------------------------------------------------
// Extract resource name from class-ad
//------------------------------------------------------------------

MyString Accountant::GetResourceName(ClassAd* ResourceAd) 
{
  char startdName[64];
  char startdAddr[64];
  
  if (!ResourceAd->LookupString (ATTR_NAME, startdName)) {
    //This should never happen, because unnamed startd ads are rejected.
    EXCEPT ("ERROR in Accountant::GetResourceName - Name not specified\n");
  }
  MyString Name=startdName;
  Name+="@";

  if (!ResourceAd->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)) {
    //This may happen for grid site ClassAds.
    //Actually, the collector now inserts an IP address if none is provided,
    //but it is more robust to not assume that behavior here.
    startdAddr[0] = '\0';
	dprintf(D_FULLDEBUG,"in Account::GetResourceName - no IP address for %s (no problem if this is a grid site ClassAd).\n",startdName);
  }
  Name+=startdAddr;
  
  return Name;
}

//------------------------------------------------------------------
// Check class ad of startd to see if it's claimed
// return 1 if it is (and set CustomerName to its remote_user), otherwise 0
//------------------------------------------------------------------

int Accountant::IsClaimed(ClassAd* ResourceAd, MyString& CustomerName) {
  char state[16];
  if (!ResourceAd->LookupString(ATTR_STATE, state)) {
    dprintf (D_ALWAYS, "Could not lookup state --- assuming not claimed\n");
    return 0;
  }

  if (string_to_state(state)!=claimed_state) return 0;
  
  char RemoteUser[512];
  if (!ResourceAd->LookupString(ATTR_ACCOUNTING_GROUP, RemoteUser)) {
	  if (!ResourceAd->LookupString(ATTR_REMOTE_USER, RemoteUser)) {	// TODDCORE
		dprintf (D_ALWAYS, "Could not lookup remote user --- assuming not claimed\n");
		return 0;
	  }
  }
  if(DiscountSuspendedResources) {
    char RemoteActivity[512];
    if(!ResourceAd->LookupString(ATTR_ACTIVITY, RemoteActivity)) {
       dprintf(D_ALWAYS, "Could not lookup remote activity\n");
       return 0;
    }
    if (!stricmp(getClaimStateString(CLAIM_SUSPENDED), RemoteActivity)) { 
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

int Accountant::CheckClaimedOrMatched(ClassAd* ResourceAd, const MyString& CustomerName) {
  char state[16];
  if (!ResourceAd->LookupString(ATTR_STATE, state)) {
    dprintf (D_ALWAYS, "Could not lookup state --- assuming not claimed\n");
    return 0;
  }

  if (string_to_state(state)==matched_state) return 1;
  if (string_to_state(state)!=claimed_state) {
    dprintf(D_ACCOUNTANT,"State was %s - not claimed or matched\n",state);
    return 0;
  }

  char RemoteUser[512];
  if (!ResourceAd->LookupString(ATTR_ACCOUNTING_GROUP, RemoteUser)) {
	  if (!ResourceAd->LookupString(ATTR_REMOTE_USER, RemoteUser)) {	// TODDCORE
		dprintf (D_ALWAYS, "Could not lookup remote user --- assuming not claimed\n");
		return 0;
	  }
  }

  if (CustomerName!=MyString(RemoteUser)) {
    dprintf(D_ACCOUNTANT,"Remote user for startd ad: %s does not match customer name\n",RemoteUser);
    return 0;
  }

  if(DiscountSuspendedResources) {
    char RemoteActivity[512];
    if(!ResourceAd->LookupString(ATTR_ACTIVITY, RemoteActivity)) {
        dprintf(D_ALWAYS, "Could not lookup remote activity\n");
        return 0;
    }
    if (!stricmp(getClaimStateString(CLAIM_SUSPENDED), RemoteActivity)) { 
       dprintf(D_ACCOUNTANT, "Machine claimed by %s but suspended\n", 
       RemoteUser);
       return 0;
    }
  }
  return 1;

}

//------------------------------------------------------------------
// Get Class Ad
//------------------------------------------------------------------

ClassAd* Accountant::GetClassAd(const MyString& Key)
{
  ClassAd* ad=NULL;
  AcctLog->table.lookup(HashKey(Key.Value()),ad);
  return ad;
}

//------------------------------------------------------------------
// Delete Class Ad
//------------------------------------------------------------------

bool Accountant::DeleteClassAd(const MyString& Key)
{
  ClassAd* ad=NULL;
  if (AcctLog->table.lookup(HashKey(Key.Value()),ad)==-1) 
	  return false;

  LogDestroyClassAd* log=new LogDestroyClassAd(Key.Value());
  AcctLog->AppendLog(log);
  return true;
}

//------------------------------------------------------------------
// Set an Integer attribute
//------------------------------------------------------------------

void Accountant::SetAttributeInt(const MyString& Key, const MyString& AttrName, int AttrValue)
{
  if (AcctLog->AdExistsInTableOrTransaction(Key.Value()) == false) {
    LogNewClassAd* log=new LogNewClassAd(Key.Value(),"*","*");
    AcctLog->AppendLog(log);
  }
  char value[_POSIX_PATH_MAX];
  sprintf(value,"%d",AttrValue);
  LogSetAttribute* log=new LogSetAttribute(Key.Value(),AttrName.Value(),value);
  AcctLog->AppendLog(log);
}
  
//------------------------------------------------------------------
// Set a Float attribute
//------------------------------------------------------------------

void Accountant::SetAttributeFloat(const MyString& Key, const MyString& AttrName, float AttrValue)
{
  if (AcctLog->AdExistsInTableOrTransaction(Key.Value()) == false) {
    LogNewClassAd* log=new LogNewClassAd(Key.Value(),"*","*");
    AcctLog->AppendLog(log);
  }
  
  char value[_POSIX_PATH_MAX];
  sprintf(value,"%f",AttrValue);
  LogSetAttribute* log=new LogSetAttribute(Key.Value(),AttrName.Value(),value);
  AcctLog->AppendLog(log);
}

//------------------------------------------------------------------
// Set a String attribute
//------------------------------------------------------------------

void Accountant::SetAttributeString(const MyString& Key, const MyString& AttrName, const MyString& AttrValue)
{
  if (AcctLog->AdExistsInTableOrTransaction(Key.Value()) == false) {
    LogNewClassAd* log=new LogNewClassAd(Key.Value(),"*","*");
    AcctLog->AppendLog(log);
  }
  
  char value[_POSIX_PATH_MAX];
  sprintf(value,"\"%s\"",AttrValue.Value());
  LogSetAttribute* log=new LogSetAttribute(Key.Value(),AttrName.Value(),value);
  AcctLog->AppendLog(log);
}

//------------------------------------------------------------------
// Retrieve a Integer attribute
//------------------------------------------------------------------

bool Accountant::GetAttributeInt(const MyString& Key, const MyString& AttrName, int& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(HashKey(Key.Value()),ad)==-1) return false;
  if (ad->LookupInteger(AttrName.Value(),AttrValue)==0) return false;
  return true;
}

//------------------------------------------------------------------
// Retrieve a Float attribute
//------------------------------------------------------------------

bool Accountant::GetAttributeFloat(const MyString& Key, const MyString& AttrName, float& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(HashKey(Key.Value()),ad)==-1) return false;
  if (ad->LookupFloat(AttrName.Value(),AttrValue)==0) return false;
  return true;
}

//------------------------------------------------------------------
// Retrieve a String attribute
//------------------------------------------------------------------

bool Accountant::GetAttributeString(const MyString& Key, const MyString& AttrName, MyString& AttrValue)
{
  ClassAd* ad;
  if (AcctLog->table.lookup(HashKey(Key.Value()),ad)==-1) return false;
  char tmp[_POSIX_PATH_MAX];
  if (ad->LookupString(AttrName.Value(),tmp)==0) return false;
  AttrValue=tmp;
  return true;
}

//------------------------------------------------------------------
// Find a resource ad in class ad list (by name)
//------------------------------------------------------------------

ClassAd* Accountant::FindResourceAd(const MyString& ResourceName, ClassAdList& ResourceList)
{
  ClassAd* ResourceAd;
  
  ResourceList.Open();
  while ((ResourceAd=ResourceList.Next())!=NULL) {
	if (ResourceName==GetResourceName(ResourceAd)) break;
  }
  ResourceList.Close();
  return ResourceAd;
}

//------------------------------------------------------------------
// Get the users domain
//------------------------------------------------------------------

MyString Accountant::GetDomain(const MyString& CustomerName)
{
  MyString S;
  int pos=CustomerName.FindChar('@');
  if (pos==-1) return S;
  S=CustomerName.Substr(pos+1,CustomerName.Length()-1);
  return S;
}
