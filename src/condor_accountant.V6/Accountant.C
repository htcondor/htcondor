#if !defined(WIN32)
#pragma implementation "HashTable.h"
#pragma implementation "Set.h"
#endif

#include <math.h>

#include "condor_accountant.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_state.h"
#include "condor_common.h"
#include "condor_attributes.h"

//------------------------------------------------------------------
// Constructor - One time initialization
//------------------------------------------------------------------

Accountant::Accountant(int MaxCustomers, int MaxResources) 
  : Customers(MaxCustomers, HashFunc), Resources(MaxResources, HashFunc)
{
  HalfLifePeriod=1.5;
  LastUpdateTime=Time::Now();
  MinPriority=0.5;
  Epsilon=0.001;
  PriorityFileName=MatchFileName=param("SPOOL");
  PriorityFileName+="/Priority.log";
  MatchFileName+="/Match.log";
  LoadState();
}

//------------------------------------------------------------------
// Return the priority of a customer
//------------------------------------------------------------------

double Accountant::GetPriority(const MyString& CustomerName) 
{
  CustomerRecord* Customer;
  double Priority=0;
  if (Customers.lookup(CustomerName,Customer)==0) Priority=Customer->Priority;
  if (Priority<MinPriority) Priority=MinPriority;
  return Priority;
}

//------------------------------------------------------------------
// Set the priority of a customer
//------------------------------------------------------------------

void Accountant::SetPriority(const MyString& CustomerName, double Priority) 
{
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
  Time T=Time::Now();
  MyString ResourceName=GetResourceName(ResourceAd);
  AddMatch(CustomerName,ResourceName,T);
  LogAction(1,CustomerName,ResourceName,T);
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
  if (T>LastUpdateTime) Customer->UnchargedTime-=Time::DiffTime(LastUpdateTime, T);
}

//------------------------------------------------------------------
// Remove a match
//------------------------------------------------------------------

void Accountant::RemoveMatch(const MyString& ResourceName)
{
  Time T=Time::Now();
  RemoveMatch(ResourceName,T);
  LogAction(0,"*",ResourceName,T);
}

void Accountant::RemoveMatch(const MyString& ResourceName, const Time& T)
{
  ResourceRecord* Resource;
  if (Resources.lookup(ResourceName,Resource)==-1) return;
  Resources.remove(ResourceName);
  MyString CustomerName=Resource->CustomerName;
  
  Time StartTime=Resource->StartTime;
  delete Resource;

  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) return;
  Customer->ResourceNames.Remove(ResourceName);

  if (StartTime<LastUpdateTime) StartTime=LastUpdateTime;
  if (T>LastUpdateTime) Customer->UnchargedTime+=Time::DiffTime(StartTime, T);
}

//------------------------------------------------------------------
// Update priorites for all customers (schedd's)
// Based on the time that passed since the last update
//------------------------------------------------------------------

void Accountant::UpdatePriorities() 
{
  Time CurUpdateTime=Time::Now();
  double TimePassed=Time::DiffTime(LastUpdateTime,CurUpdateTime);
  double AgingFactor=pow(0.5,TimePassed/HalfLifePeriod);
  LastUpdateTime=CurUpdateTime;

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
// Write Match Log Entry
//------------------------------------------------------------------

void Accountant::WriteLogEntry(ofstream& os, int AddMatch, const MyString& CustomerName, const MyString& ResourceName, const Time& T)
{
  os << (AddMatch ? "A" : "R");
  os << " " << CustomerName << " " << ResourceName << " " << T << endl; 
}

//------------------------------------------------------------------
// Append action to matches log file
//------------------------------------------------------------------

void Accountant::LogAction(int AddMatch, const MyString& CustomerName, const MyString& ResourceName, const Time& T)
{
  ofstream MatchFile(MatchFileName,ios::app);
  if (!MatchFile) {
    dprintf(D_ALWAYS, "ERROR in Accountant::LogMatchAction - failed to open match file %s\n",(const char*) MatchFileName);
    return;
  }
  WriteLogEntry(MatchFile, AddMatch, CustomerName, ResourceName, T); 
  int FilePos=MatchFile.tellp();
  MatchFile.close(); 
  if (FilePos>=64000) SaveState();
}

//------------------------------------------------------------------
// Save state
//------------------------------------------------------------------

void Accountant::SaveState()
{
  dprintf(D_FULLDEBUG,"Saving State\n");
  MyString CustomerName, ResourceName;

  ofstream PriorityFile(PriorityFileName);
  if (!PriorityFile) {
    dprintf(D_ALWAYS, "ERROR in Accountant::SaveState - failed to open priorities file %s\n",(const char*) PriorityFileName);
    return;
  }
  PriorityFile << LastUpdateTime << endl;

  CustomerRecord* Customer;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    PriorityFile << CustomerName << " " << Customer->Priority << endl;
  }
  PriorityFile.close();
  
  ofstream MatchFile(MatchFileName);
  if (!MatchFile) {
    dprintf(D_ALWAYS, "ERROR in Accountant::SaveState - failed to open match file %s\n",(const char*) MatchFileName);
    return;
  }

  ResourceRecord* Resource;
  Resources.startIterations();
  while (Resources.iterate(ResourceName, Resource)) {
    WriteLogEntry(MatchFile,1,Resource->CustomerName,ResourceName,Resource->StartTime);
  }
  MatchFile.close(); 
  
  return;
}

//------------------------------------------------------------------
// Load State
//------------------------------------------------------------------

void Accountant::LoadState()
{
  dprintf(D_FULLDEBUG,"Loading State\n");
  MyString CustomerName, ResourceName;
  MyString S;
  double Priority;
  Time T;

  // Clear all match & priority information
  CustomerRecord* Customer;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) delete Customer;
  Customers.clear();
  ResourceRecord* Resource;
  Resources.startIterations();
  while (Resources.iterate(ResourceName, Resource)) delete Resource;
  Resources.clear();

  // Open priority log file
  ifstream PriorityFile(PriorityFileName);
  if (!PriorityFile) {
    dprintf(D_ALWAYS, "ERROR in Accountant::LoadState - failed to open priorities file %s\n",(const char*) PriorityFileName);
    return;
  }

  // Read priority log file
  PriorityFile >> LastUpdateTime;
  while(1) {
    PriorityFile >> CustomerName >> Priority;
    if (PriorityFile.eof()) break;
    SetPriority(CustomerName, Priority);
  }
  PriorityFile.close();

  // Open match log file
  ifstream MatchFile(MatchFileName);
  if (!MatchFileName) {
    dprintf(D_ALWAYS, "ERROR in Accountant::LoadState - failed to open match file %s\n",(const char*) MatchFileName);
    return;
  }

  // Read match log file
  while(1) {
    MatchFile >> S >> CustomerName >> ResourceName >> T;
    if (MatchFile.eof()) break;
    if (S=="A") AddMatch(CustomerName,ResourceName,T);
    else RemoveMatch(ResourceName);
  }
  MatchFile.close();

  UpdatePriorities();

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
