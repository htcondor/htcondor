#include "condor_common.h"

#if !defined(WIN32)
#pragma implementation "HashTable.h"
#pragma implementation "Set.h"
#endif

#include <math.h>
#include <iomanip.h>

#include "condor_accountant.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_state.h"
#include "condor_attributes.h"

//------------------------------------------------------------------
// Constructor - One time initialization
//------------------------------------------------------------------

Accountant::Accountant(int MaxCustomers, int MaxResources)
  : Customers(MaxCustomers, HashFunc), Resources(MaxResources, HashFunc)
{
  HalfLifePeriod=600;
  MinPriority=0.5;
  Epsilon=0.001;
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
		  // LogFileName+="/p/condor/workspaces/adiel/local/spool";
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
// Return the priority of a customer
//------------------------------------------------------------------

double Accountant::GetPriority(const MyString& CustomerName) 
{
  CustomerRecord* Customer;
  double Priority=0;
  if (CustomerName==NiceUserName) return HUGE_VAL;
  if (Customers.lookup(CustomerName,Customer)==0) Priority=Customer->Priority;
  if (Priority<MinPriority) Priority=MinPriority;
  return Priority;
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriority(const MyString& CustomerName, double Priority) 
{
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

void Accountant::AddMatch(const MyString& CustomerName, ClassAd* ResourceAd) 
{
  AddMatch(CustomerName,GetResourceName(ResourceAd),Time::Now());
}

void Accountant::AddMatch(const MyString& CustomerName, const MyString& ResourceName, const Time& T) 
{
  RemoveMatch(ResourceName,T);
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

  dprintf(D_ALWAYS,"Updating Priorities: AgingFactor=%8.3f , TimePassed=%5.3f\n",AgingFactor,TimePassed);

  CustomerRecord* Customer;
  MyString CustomerName;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    double Priority=Customer->Priority;
    double UnchargedResources=0;
    if (TimePassed>0) UnchargedResources=(Customer->UnchargedTime/TimePassed);
    double ResourcesUsed=Customer->ResourceNames.Count();
    Priority=Priority*AgingFactor+(ResourcesUsed+UnchargedResources)*(1-AgingFactor);

    dprintf(D_FULLDEBUG,"CustomerName=%s , Old Priority=%5.3f , New Priority=%5.3f , ResourcesUsed=%1.0f\n",CustomerName.Value(),Customer->Priority,Priority,ResourcesUsed);
    dprintf(D_FULLDEBUG,"UnchargedResources=%8.3f , UnchargedTime=%5.3f\n",UnchargedResources,Customer->UnchargedTime);
#ifdef DEBUG_FLAG
    dprintf(D_ALWAYS,"Resources: "); Customer->ResourceNames.PrintSet();
#endif

    Customer->UnchargedTime=0;
    Customer->Priority=Priority;
    if (Customer->Priority<Epsilon && Customer->ResourceNames.Count()==0) {
      delete Customer;
      Customers.remove(CustomerName);
    }
  }

  SaveState();
}

//------------------------------------------------------------------
// Go through the list of startd ad's and check of matches
// that were broken (by checkibg the claimed state)
//------------------------------------------------------------------

void Accountant::CheckMatches(ClassAdList& ResourceList) {
  ResourceList.Open();
  ClassAd* ResourceAd;
  while ((ResourceAd=ResourceList.Next())!=NULL) {
    if (NotClaimed(ResourceAd)) RemoveMatch(GetResourceName(ResourceAd));
  }
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
    sprintf(tmp,"Name%d = \"%s\"",OwnerNum,CustomerName.Value());
    ad->Insert(tmp);
    sprintf(tmp,"Priority%d = %f",OwnerNum,Customer->Priority);
    ad->Insert(tmp);
    sprintf(tmp,"ResourcesUsed%d = %d",OwnerNum,Customer->ResourceNames.Count());
    ad->Insert(tmp);
    OwnerNum++;
  }

  return ad;
}

//------------------------------------------------------------------
// Append action to matches log file
//------------------------------------------------------------------

void Accountant::AppendLogEntry(const MyString& Action, const MyString& CustomerName, const MyString& ResourceName, double d)
{
  if (!LogEnabled) return;

  ofstream LogFile(LogFileName,ios::app);
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

  ofstream LogFile(LogFileName);
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
  ifstream LogFile(LogFileName);
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
// Check class ad of startd to see if it's claimed: 
// return 1 if it's not, otherwise 0
//------------------------------------------------------------------

int Accountant::NotClaimed(ClassAd* ResourceAd) {
  char state[16];
  if (!ResourceAd->LookupString (ATTR_STATE, state))
    {
        dprintf (D_ALWAYS, "Could not lookup state --- assuming CLAIMED\n");
        return 0;
    }

  return (string_to_state(state) != claimed_state);
}
