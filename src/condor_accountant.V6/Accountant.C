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
#include "condor_common.h"

#include <math.h>
#include <iomanip.h>

#include "condor_accountant.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_state.h"
#include "condor_attributes.h"

static char *_FileName_ = __FILE__;     // Used by EXCEPT (see condor_debug.h)

//------------------------------------------------------------------
// Constructor - One time initialization
//------------------------------------------------------------------

Accountant::Accountant(int MaxCustomers, int MaxResources)
  : Customers(MaxCustomers, HashFunc), Resources(MaxResources, HashFunc)
{
  HalfLifePeriod=600;
  MinPriority=0.5;
  NiceUserPriorityFactor=1000000;
  LastUpdateTime=Time::Now();
  LogEnabled=0;
}

//------------------------------------------------------------------
// Initialize and read configuration parameters
//------------------------------------------------------------------

void
Accountant::Initialize() 
{
  char* tmp;
  tmp = param("PRIORITY_HALFLIFE");
  if(tmp) {
	  HalfLifePeriod=atoi(tmp);
	  free(tmp);
  }
  dprintf(D_FULLDEBUG,"Accountant::Initialize - HalfLifePeriod=%f\n",HalfLifePeriod);

  tmp = param("SPOOL");
  if(tmp) {
	  LogFileName=tmp;
	  LogFileName+="/Accountant.log";
	  free(tmp);
  } else {
	  EXCEPT( "SPOOL not defined!" );
  }
  LoadState();
  LogEnabled=1;
  UpdatePriorities();
}

//------------------------------------------------------------------
// Return the number of resources used
//------------------------------------------------------------------

int Accountant::GetResourcesUsed(const MyString& CustomerName) 
{
  CustomerRecord* Customer;
  int ResourcesUsed=0;
  if (Customers.lookup(CustomerName,Customer)==0) ResourcesUsed=Customer->ResourceNames.Count();
  return ResourcesUsed;
}

//------------------------------------------------------------------
// Return the priority of a customer
//------------------------------------------------------------------

double Accountant::GetPriority(const MyString& CustomerName) 
{
  CustomerRecord* Customer;
  double PriorityFactor=1;
  if (strncmp(CustomerName.Value(),NiceUserName,strlen(NiceUserName))==0) PriorityFactor=NiceUserPriorityFactor;
  if (Customers.lookup(CustomerName,Customer)) {
	// if its a new customer, create a new customer record
	SetPriority(CustomerName, MinPriority*PriorityFactor);
	return MinPriority*PriorityFactor;
  }
  if (Customer->Priority<MinPriority) Customer->Priority=MinPriority;
  return Customer->Priority*PriorityFactor;;
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriority(const MyString& CustomerName, double Priority) 
{
  dprintf(D_FULLDEBUG,"Accountant::SetPriority - CustomerName=%s, Priority=%8.3f\n",CustomerName.Value(),Priority);

  AppendLogEntry("SetPriority",CustomerName,"*",Priority);

  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    if (Priority==0) return;
    Customer=new CustomerRecord;
    if (Customers.insert(CustomerName,Customer)==-1) {
      EXCEPT ("ERROR in Accountant::SetPriority - unable to insert to customers table");
    }
  }
  Customer->Priority=Priority;
}

//------------------------------------------------------------------
// Add a match
//------------------------------------------------------------------

int Accountant::MatchExist(const MyString& CustomerName, const MyString& ResourceName)
{
  ResourceRecord* Resource;
  if (Resources.lookup(ResourceName,Resource)==-1) return 0;
  if (CustomerName==Resource->CustomerName) return 1;
  return 0;
}

//------------------------------------------------------------------
// Add a match
//------------------------------------------------------------------

void Accountant::AddMatch(const MyString& CustomerName, ClassAd* ResourceAd) 
{
  AddMatch(CustomerName,GetResourceName(ResourceAd),Time::Now());
}

void Accountant::AddMatch(const MyString& CustomerName, const MyString& ResourceName, const Time& T) 
{
  // Check if match exists, if so no need to register it again
  if (MatchExist(CustomerName,ResourceName)) return;
  // check if the resource is used by any other customer, if so remove that natch
  RemoveMatch(ResourceName,T);

  dprintf(D_FULLDEBUG,"Accountant::AddMatch - CustomerName=%s, ResourceName=%s\n",CustomerName.Value(),ResourceName.Value());
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    Customer=new CustomerRecord;
    if (Customers.insert(CustomerName,Customer)==-1) {
      EXCEPT ("ERROR in Accountant::AddMatch - unable to insert to Customers table");
    }
  }
  Customer->ResourceNames.Add(ResourceName);

  ResourceRecord* Resource=new ResourceRecord;
  if (Resources.insert(ResourceName,Resource)==-1) {
    EXCEPT ("ERROR in Accountant::AddMatch - unable to insert to Resources table");
  }
  Resource->CustomerName=CustomerName;
  // Resource->Ad=new ClassAd(*ResourceAd);
  Resource->StartTime=T;

  // add negative "uncharged" time if match starts after last update
  if (T>LastUpdateTime) Customer->UnchargedTime-=T-LastUpdateTime;

  // add entry to log
  AppendLogEntry("AddMatch",CustomerName,ResourceName,(double) T);
}

//------------------------------------------------------------------
// Remove a match
//------------------------------------------------------------------

void Accountant::RemoveMatch(const MyString& ResourceName)
{
  RemoveMatch(ResourceName,Time::Now());
}

void Accountant::RemoveMatch(const MyString& ResourceName, const Time& T)
{
  ResourceRecord* Resource;
  if (Resources.lookup(ResourceName,Resource)==-1) return;
  Resources.remove(ResourceName);
  MyString CustomerName=Resource->CustomerName;

  // add entry to log
  AppendLogEntry("RemoveMatch",CustomerName,ResourceName,(double) T);
  
  Time StartTime=Resource->StartTime;
  delete Resource;

  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) return;

  dprintf(D_FULLDEBUG,"Accountant::RemoveMatch - ResourceName=%s\n",ResourceName.Value());
  Customer->ResourceNames.Remove(ResourceName);

  if (StartTime<LastUpdateTime) StartTime=LastUpdateTime;
  if (T>LastUpdateTime) Customer->UnchargedTime+=T-StartTime;
}

//------------------------------------------------------------------
// Update priorites for all customers (schedd's)
// Based on the time that passed since the last update
//------------------------------------------------------------------

void Accountant::UpdatePriorities() 
{
  UpdatePriorities(Time::Now());
}

void Accountant::UpdatePriorities(const Time& T) 
{
  double TimePassed=T-LastUpdateTime;
  double AgingFactor=pow(0.5,TimePassed/HalfLifePeriod);
  LastUpdateTime=T;

  dprintf(D_FULLDEBUG,"Accountant::UpdatePriorities - AgingFactor=%8.3f , TimePassed=%5.3f\n",AgingFactor,TimePassed);

  // Trace
  MyString TraceFileName=LogFileName+".trace";
  struct stat buf;
  int TraceFlag=0;
  ofstream TraceFile;
  if (stat(TraceFileName.Value(), &buf)==0) {
    TraceFlag=1;
    TraceFile.open(TraceFileName.Value(),ios::app);
    TraceFile << setprecision(3);
    TraceFile.setf(ios::fixed);
    if (!TraceFile) TraceFlag=0;
  }

  CustomerRecord* Customer;
  MyString CustomerName;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    double Priority=Customer->Priority;
	if (Priority<MinPriority) Priority=MinPriority;
    double UnchargedResources=0;
    if (TimePassed>0) UnchargedResources=(Customer->UnchargedTime/TimePassed);
    double ResourcesUsed=Customer->ResourceNames.Count();
    Priority=Priority*AgingFactor+(ResourcesUsed+UnchargedResources)*(1-AgingFactor);

    dprintf(D_FULLDEBUG,"CustomerName=%s , Old Priority=%5.3f , New Priority=%5.3f , ResourcesUsed=%1.0f\n",CustomerName.Value(),Customer->Priority,Priority,ResourcesUsed);
    dprintf(D_FULLDEBUG,"UnchargedResources=%8.3f , UnchargedTime=%5.3f\n",UnchargedResources,Customer->UnchargedTime);
#ifdef DEBUG_FLAG
    dprintf(D_FULLDEBUG,"Resources: "); Customer->ResourceNames.PrintSet();
#endif

    Customer->UnchargedTime=0;
    Customer->Priority=Priority;
    if (Customer->Priority<MinPriority && Customer->ResourceNames.Count()==0) {
      delete Customer;
      Customers.remove(CustomerName);
    }
    else if (TraceFlag) {
      TraceFile << T << "," << CustomerName << "," << Priority << "," << ResourcesUsed << endl;
    }	

  }

  if (TraceFlag) TraceFile.close();

  SaveState();
}

//------------------------------------------------------------------
// Go through the list of startd ad's and check of matches
// that were broken (by checkibg the claimed state)
//------------------------------------------------------------------

void Accountant::CheckMatches(ClassAdList& ResourceList) {
  dprintf(D_FULLDEBUG,"Accountant::CheckMatches\n");
  ClassAd* ResourceAd;
  MyString ResourceName;
  MyString CustomerName;
  ResourceRecord* Resource;

  // Remove matches that were broken
  Resources.startIterations();
  while (Resources.iterate(ResourceName, Resource)) {
    ResourceAd=FindResourceAd(ResourceName, ResourceList);
    if (!ResourceAd) 
      RemoveMatch(ResourceName);
	else if (!CheckClaimedOrMatched(ResourceAd, Resource->CustomerName))
      RemoveMatch(ResourceName);
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
// Report the whole list of priorities
//------------------------------------------------------------------

AttrList* Accountant::ReportState() {

  dprintf(D_FULLDEBUG,"Reporting State\n");

  MyString CustomerName, ResourceName;
  CustomerRecord* Customer;
  AttrList* ad=new AttrList();
  char tmp[512];
  double d=(double) LastUpdateTime;
  int l=(int) d;
  sprintf(tmp, "LastUpdate = %d", l);
  ad->Insert(tmp);

  int OwnerNum=1;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    // if (Customer->Priority<MinPriority) continue;
    sprintf(tmp,"Name%d = \"%s\"",OwnerNum,CustomerName.Value());
    ad->Insert(tmp);
    sprintf(tmp,"Priority%d = %f",OwnerNum,GetPriority(CustomerName));
    ad->Insert(tmp);
    sprintf(tmp,"ResourcesUsed%d = %d",OwnerNum,Customer->ResourceNames.Count());
    ad->Insert(tmp);
    OwnerNum++;
  }
  sprintf(tmp,"NumSubmittors = %d", OwnerNum-1);
  ad->Insert(tmp);
  return ad;
}

//------------------------------------------------------------------
// Append action to matches log file
//------------------------------------------------------------------

void Accountant::AppendLogEntry(const MyString& Action, const MyString& CustomerName, const MyString& ResourceName, double d)
{
  if (!LogEnabled) return;

  ofstream LogFile(LogFileName.Value(),ios::app);
  if (!LogFile) {
    dprintf(D_ALWAYS, "ERROR in Accountant::AppendLogEntry - failed to open match file %s\n",LogFileName.Value());
    return;
  }
  LogFile << setprecision(3);
  LogFile.setf(ios::fixed);
  LogFile << Action << " " << CustomerName << " " << ResourceName << " " << d << endl;
  LogFile.close(); 
}

//------------------------------------------------------------------
// Save state
//------------------------------------------------------------------

void Accountant::SaveState()
{
  if (!LogEnabled) return;

  dprintf(D_FULLDEBUG,"Saving State\n");
  MyString CustomerName, ResourceName;

  MyString TmpFileName=LogFileName+".tmp";

  ofstream LogFile(TmpFileName.Value());
  if (!LogFile) {
    dprintf(D_ALWAYS, "ERROR in Accountant::SaveState - failed to open log file %s\n",LogFileName.Value());
    return;
  }
  LogFile << setprecision(3);
  LogFile.setf(ios::fixed);
  LogFile << LastUpdateTime << endl;

  CustomerRecord* Customer;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    LogFile << "SetPriority "<< CustomerName << " * " << Customer->Priority << endl;
  }
  
  ResourceRecord* Resource;
  Resources.startIterations();
  while (Resources.iterate(ResourceName, Resource)) {
    LogFile << "AddMatch " << Resource->CustomerName << " " << ResourceName;
	LogFile << " " << ((double) Resource->StartTime) << endl;
  }
  LogFile.close(); 

  if (rename(TmpFileName.Value(),LogFileName.Value())) {
	dprintf(D_ALWAYS,"Error %d: Cannot rename log file %s to %s\n",errno,TmpFileName.Value(),LogFileName.Value());
  }
  
  return;
}

//------------------------------------------------------------------
// Load State
//------------------------------------------------------------------

void Accountant::LoadState()
{
  dprintf(D_FULLDEBUG,"Loading State\n");
  MyString Action, CustomerName, ResourceName;
  double d;

  LogEnabled=0;  // disable logging while reading the log file

  // Open log file
  ifstream LogFile(LogFileName.Value());
  if (!LogFile) {
    dprintf(D_ALWAYS, "Accountant::LoadState - can't open Log file %s - starting with no previous match information\n",LogFileName.Value());
    return;
  }

  // Read log file
  LogFile >> d;
  LastUpdateTime=Time(d);

  while(1) {
    LogFile >> Action >> CustomerName >> ResourceName >> d;
    if (LogFile.eof()) break;
    if (Action=="SetPriority") SetPriority(CustomerName,d);
	else if (Action=="AddMatch") AddMatch(CustomerName,ResourceName,Time(d));
	else if (Action=="RemoveMatch") RemoveMatch(ResourceName,Time(d));	
  }
  LogFile.close();
  return;
}

//------------------------------------------------------------------
// Extract resource name from class-ad
//------------------------------------------------------------------

MyString Accountant::GetResourceName(ClassAd* ResourceAd) 
{
  char startdName[64];
  char startdAddr[64];
  
  if (!ResourceAd->LookupString (ATTR_NAME, startdName)) {
    EXCEPT ("ERROR in Accountant::GetResourceName - Name not specified\n");
  }
  MyString Name=startdName;
  Name+="@";

  if (!ResourceAd->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)) {
    EXCEPT ("ERROR in Accountant::GetResourceName - No IP addr in class ad\n");
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
  if (!ResourceAd->LookupString(ATTR_REMOTE_USER, RemoteUser)) {
    dprintf (D_ALWAYS, "Could not lookup remote user --- assuming not claimed\n");
    return 0;
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
  if (string_to_state(state)!=claimed_state) return 0;
  
  char RemoteUser[512];
  if (!ResourceAd->LookupString(ATTR_REMOTE_USER, RemoteUser)) {
    dprintf (D_ALWAYS, "Could not lookup remote user --- assuming not claimed\n");
    return 0;
  }

  if (CustomerName==RemoteUser) return 1;
  return 0;

}

//------------------------------------------------------------------
// Find a resource ad in class ad list (by name)
//------------------------------------------------------------------

ClassAd* Accountant::FindResourceAd(const MyString& ResourceName, ClassAdList& ResourceList)
{
  ClassAd* ResourceAd;
  char Name[512];
  
  ResourceList.Open();
  while ((ResourceAd=ResourceList.Next())!=NULL) {
	if (ResourceName==GetResourceName(ResourceAd)) break;
  }
  ResourceList.Close();
  return ResourceAd;
}
