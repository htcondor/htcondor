#pragma implementation "HashTable.h"
#include "Accountant.h"
#include "debug.h"
#include "condor_attributes.h"

extern "C"
{
  void config(ClassAd*);
  void dprintf_config(char*, int);
  void dprintf(int, char*...); 
}

int Accountant::GetPriority(const MyString& CustomerName) 
{
  CustomerRecord* Customer=GetCustomer(CustomerName);
  if (!Customer) return 0;
  int Priority=Customer->Priority;
  CheckDeadCustomer(CustomerName,Customer);
  return Priority;
}

void Accountant::SetPriority(const MyString& CustomerName, int Priority) 
{
  CustomerRecord* Customer=NewCustomer(CustomerName);
  Customer->Priority=Priority;
  CheckDeadCustomer(CustomerName,Customer);
}

void Accountant::AddMatch(const MyString& CustomerName, ClassAd* Resource) 
{
  CustomerRecord* Customer=NewCustomer(CustomerName);
  MyString ResourceName=GetResourceName(Resource);
  Customer->Resources.Add(ResourceName);
  AddResource(ResourceName,Resource);
}

void Accountant::RemoveMatch(const MyString& CustomerName, ClassAd* Resource)
{
  CustomerRecord* Customer=GetCustomer(CustomerName);
  if (!Customer) return;
  MyString ResourceName=GetResourceName(Resource);
  Customer->Resources.Remove(ResourceName);
  RemoveResource(ResourceName);
  CheckDeadCustomer(CustomerName,Customer);
}

void Accountant::UpdatePriorities() 
{
  CustomerRecord* Customer;
  MyString CustomerName;
  int TotResources=0;
  Customers.startIterations();
  while (Customers.Iterate(CustomerName, Customer)) {
    int Priority=Customer->Priority;
    int ResourcesUsed=Customer->Resources.Count();
    TotResources+=ResourcesUsed;
    Priority=int(Priority*0.6)-ResourcesUsed*10;
    Customer->Priority=Priority;
    CheckDeadCustomer(CustomerName,Customer);
  }
#ifndef DEBUG_FLAG
#include <iostream.h>
  cout << "TotResources:" << TotResources << endl;
#endif
}

//---------------------------------------------------------------
// Resource functions
//---------------------------------------------------------------

// Extract resource name from class-ad
MyString Accountant::GetResourceName(ClassAd* Resource) 
{
  ExprTree *tree;
  
  // get the name of the startd
  tree=Resource->Lookup("Name");
  if (!tree) tree=Resource->Lookup("Machine");
  if (!tree) {
    dprintf (D_ALWAYS, "GetResourceName: Error: Neither 'Name' nor 'Machine' specified\n");
    throw int(1);
  }
  MyString Name=((String *)tree->RArg())->Value();
        
  // get the IP and port of the startd 
  tree=Resource->Lookup (ATTR_STARTD_IP_ADDR);
  if (!tree) tree=Resource->Lookup("STARTD_IP_ADDR");
  if (!tree) {
    dprintf (D_ALWAYS, "GetResourceName: Error: No IP addr in class ad\n");
    throw int(1);
  }
  Name+=((String *)tree->RArg())->Value();

  return Name;
}

//-------------------------------------------------------------
// Resource add/remove/find functions
//-------------------------------------------------------------

// Get resource class ad
ClassAd* Accountant::GetResource(const MyString& ResourceName)
{
  ClassAd* Resource;
  if (Resources.lookup(ResourceName,Resource)==-1) Resource=NULL;
  return Resource;
}

// Remove a resource
void Accountant::RemoveResource(const MyString& ResourceName)
{
  ClassAd* Resource;
  if (Resources.lookup(ResourceName,Resource)==0) {
    delete Resource;
    Resources.remove(ResourceName);
  }
}

// Add (or replace) a resource class ad
ClassAd* Accountant::AddResource(const MyString& ResourceName, ClassAd* Resource)
{
  RemoveResource(ResourceName);
  ClassAd* Ad=new ClassAd(*Resource);
  Resources.insert(ResourceName,Ad);
  return Ad;
}

//-------------------------------------------------------------------
// Customer functions
//-------------------------------------------------------------------

// Returns 1 if Customer can be deleted else 0
void Accountant::CheckDeadCustomer(const MyString& CustomerName, CustomerRecord* Customer) {
  if (Customer->Priority==0 && Customer->Resources.Count()==0) {
    delete Customer;
    Customers.remove(CustomerName);
  }
}

// Get customer record, or NULL if it doesn't exist
Accountant::CustomerRecord* Accountant::GetCustomer(const MyString& CustomerName)
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) Customer=NULL;
  return Customer;
}

// Check customer record, and create it if it doesn't exist
Accountant::CustomerRecord* Accountant::NewCustomer(const MyString& CustomerName)
{
  CustomerRecord* Customer=GetCustomer(CustomerName);
  if (Customer==NULL) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
  }  
  return Customer;
}

