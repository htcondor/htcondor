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
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) return 0;
  return Customer->Priority;
}

void Accountant::SetPriority(const MyString& CustomerName, int Priority) 
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    if (Priority==0) return;
    Customer=new CustomerRecord;
    if (Customers.insert(CustomerName,Customer)==-1) {
      dprintf (D_ALWAYS, "ERROR in Accountant::SetPriority - unable to insert to customers table");
      throw int(1);
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
      dprintf (D_ALWAYS, "ERROR in Accountant::AddMatch - unable to insert to Customers table");
      throw int(1);
    }
  }
  Customer->ResourceNames.Add(ResourceName);
  ResourceRecord* Resource=new ResourceRecord;
  if (Resources.insert(ResourceName,Resource)==-1) {
    dprintf (D_ALWAYS, "ERROR in Accountant::AddMatch - unable to insert to Resources table");
    throw int(1);
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

void Accountant::UpdatePriorities() 
{
#ifdef DEBUG_FLAG
#include <iostream.h>
  cout << "Updating Priorities" << endl;
  int TotResources=0;
#endif
  CustomerRecord* Customer;
  MyString CustomerName;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    int Priority=Customer->Priority;
    int ResourcesUsed=Customer->ResourceNames.Count();
#ifdef DEBUG_FLAG
    TotResources+=ResourcesUsed;
    cout << "CustomerName: " << CustomerName.Value() << " Priority: " << Priority << " ResourcesUsed: " << ResourcesUsed << endl << "Resources: ";
    Customer->ResourceNames.PrintSet();
#endif
    Priority=int(Priority*0.6)+ResourcesUsed*10;
    Customer->Priority=Priority;
    if (Customer->Priority==0 && Customer->ResourceNames.Count()==0) {
      delete Customer;
      Customers.remove(CustomerName);
    }
  }
#ifdef DEBUG_FLAG
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
