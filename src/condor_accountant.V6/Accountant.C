#pragma implementation "HashTable.h"
#pragma implementation "Set.h"

#include "Accountant.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"


Accountant::Accountant(int MaxCustomers, int MaxResources) 
  : Customers(MaxCustomers, HashFunc), Resources(MaxResources, HashFunc)
{
  HalfLifePeriod=1.5;
  LastUpdateTime=SysCalls::GetTime();
  MinPriority=0.5;
  Epsilon=0.001;
}

void Accountant::SetAccountant(char* sin) 
{
  return;
}

double Accountant::GetPriority(const MyString& CustomerName) 
{
  CustomerRecord* Customer;
  double Priority=0;
  if (Customers.lookup(CustomerName,Customer)==0) Priority=Customer->Priority;
  if (Priority<MinPriority) Priority=MinPriority;
  return Priority;
}

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
}

void Accountant::RemoveMatch(const MyString& ResourceName)
{
  ResourceRecord* Resource;
  if (Resources.lookup(ResourceName,Resource)==-1) return;
  MyString CustomerName=Resource->CustomerName;
  delete Resource;
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) return;
  Customer->ResourceNames.Remove(ResourceName);
}

void Accountant::CheckMatches(ClassAdList& ResourceList) {
  ResourceList.Open();
  ClassAd* ResourceAd;
  while ((ResourceAd=ResourceList.Next())!=NULL) {
    if (NotClaimed(ResourceAd)) RemoveMatch(GetResourceName(ResourceAd));
  }
  return;
}

void Accountant::UpdatePriorities() 
{
  SysCalls::Time CurUpdateTime=SysCalls::GetTime();
  double TimePassed=SysCalls::DiffTime(LastUpdateTime,CurUpdateTime);
  double AgingFactor=SysCalls::Power(0.5,TimePassed/HalfLifePeriod);
  LastUpdateTime=CurUpdateTime;

#ifdef DEBUG_FLAG
  dprintf(D_ALWAYS,"Updating Priorities: AgingFactor=%5.3f , TimePassed=%5.3f\n",AgingFactor,TimePassed);
#endif
  CustomerRecord* Customer;
  MyString CustomerName;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    double Priority=Customer->Priority;
    double ResourcesUsed=Customer->ResourceNames.Count();
    Priority=Priority*AgingFactor+ResourcesUsed*(1-AgingFactor);
#ifdef DEBUG_FLAG
    dprintf(D_ALWAYS,"CustomerName=%s , Old Priority=%5.3f , New Priority=%5.3f , ResourcesUsed=%1.0f\n",CustomerName.Value(),Customer->Priority,Priority,ResourcesUsed);
    dprintf(D_ALWAYS,"Resources: ");
    Customer->ResourceNames.PrintSet();
#endif
    Customer->Priority=Priority;
    if (Customer->Priority<Epsilon && Customer->ResourceNames.Count()==0) {
      delete Customer;
      Customers.remove(CustomerName);
    }
  }
}

//---------------------------------------------------------------
// Utility functions
//---------------------------------------------------------------

// Extract resource name from class-ad
MyString Accountant::GetResourceName(ClassAd* ResourceAd) 
{
  char startdName[64];
  char startdAddr[64];
  
  if (!ResourceAd->LookupString (ATTR_NAME, startdName)) {
    EXCEPT ("ERROR in GetResourceName: Error - Name not specified\n");
  }
  MyString Name=startdName;

  if (!ResourceAd->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)) {
    EXCEPT ("ERROR in Accountant::GetResourceName - No IP addr in class ad\n");
  }
  Name+=startdAddr;
  
  return Name;
}

// Check class ad of startd to see if it's claimed: return 1 if it's not, 
// otherwise 0
int Accountant::NotClaimed(ClassAd* ResourceAd) {
  int state;
  if (!ResourceAd->LookupInteger (ATTR_STATE, state)) {
    dprintf (D_ALWAYS, "Warning in Accountant::NotClaimed - Could not lookup state... assuming CLAIMED\n");
    return 0;
  }
  
  return 0;
}
