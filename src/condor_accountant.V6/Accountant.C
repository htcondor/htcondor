#pragma implementation "HashTable.h"
#pragma implementation "Set.h"

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
  PriorityFile=param("SPOOL");
  PriorityFile+="/Priority.log";
}

//------------------------------------------------------------------
// Reset to empty
//------------------------------------------------------------------

void Accountant::Reset() 
{
  MyString Name;

  CustomerRecord* Customer;
  Customers.startIterations();
  while (Customers.iterate(Name, Customer)) delete Customer;
  Customers.clear();

  ResourceRecord* Resource;
  Resources.startIterations();
  while (Resources.iterate(Name, Resource)) delete Resource;
  Resources.clear();

  LastUpdateTime=Time::Now();
  return;
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
  MyString ResourceName=GetResourceName(ResourceAd);
  RemoveMatch(ResourceName);
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
  Resource->Ad=new ClassAd(*ResourceAd);
  Resource->StartTime=Time::Now();

  double UnchargedTime=Time::DiffTime(LastUpdateTime, Resource->StartTime);
  Customer->UnchargedTime-=UnchargedTime;
}

//------------------------------------------------------------------
// Remove a match
//------------------------------------------------------------------

void Accountant::RemoveMatch(const MyString& ResourceName)
{
  ResourceRecord* Resource;
  if (Resources.lookup(ResourceName,Resource)==-1) return;
  Resources.remove(ResourceName);
  MyString CustomerName=Resource->CustomerName;
  
  Time CurrTime=Time::Now();
  Time StartTime=Resource->StartTime;
  if (StartTime<LastUpdateTime) StartTime=LastUpdateTime;
  double UnchargedTime=Time::DiffTime(StartTime, CurrTime);
  
  delete Resource;
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) return;
  Customer->ResourceNames.Remove(ResourceName);
  Customer->UnchargedTime+=UnchargedTime;
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
// Load priorities from file
//------------------------------------------------------------------

void Accountant::LoadPriorities()
{
  dprintf(D_FULLDEBUG,"Loading priorities\n");
  FILE* fp=fopen((const char*)PriorityFile, "r");
  if (fp==NULL) {
    dprintf(D_ALWAYS, "ERROR in Accountant::LoadPriorities - failed to open priorities file %s\n",(const char*) PriorityFile);
    return;
  }

  char s[40];
  float f;
  while (1) {
    if (fscanf(fp,"%s %f\n",s,&f)==EOF) break;
    dprintf(D_FULLDEBUG,"Customer=%s, Priority=%5.3f\n",s,f);
    SetPriority(s,f);
  }
  fclose(fp);
  return;
}

//------------------------------------------------------------------
// Save priorities to file
//------------------------------------------------------------------

void Accountant::SavePriorities()
{
  dprintf(D_FULLDEBUG,"Saving priorities\n");
  FILE* fp=fopen((const char*)PriorityFile, "w");
  if (fp==NULL) {
    dprintf(D_ALWAYS, "ERROR in Accountant::SavePriorities - failed  to open priorities file %s\n",(const char*) PriorityFile);
    return;
  }
  CustomerRecord* Customer;
  MyString CustomerName;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    fprintf(fp,"%-30s %8.3f\n", (const char*) CustomerName,Customer->Priority);
    dprintf(D_FULLDEBUG,"%s %8.3f\n", (const char*) CustomerName,Customer->Priority);
  }
  fclose(fp);
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
    EXCEPT ("ERROR in GetResourceName: Error - Name not specified\n");
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
