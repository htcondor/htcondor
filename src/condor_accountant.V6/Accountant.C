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

int Accountant::GetPriority(MyString& CustomerName) 
{
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  return Customer->Priority;
}

void Accountant::SetPriority(MyString& CustomerName, int Priority) 
{
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  Customer->Priority=Priority;
}

void Accountant::AddMatch(MyString& CustomerName, ClassAd* Resource) 
{
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  MyString ResourceName=GetResourceName(Resource);
  Customer->Resources.Add(ResourceName);
}

void Accountant::RemoveMatch(MyString& CustomerName, ClassAd* Resource)
{
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  MyString ResourceName=GetResourceName(Resource);
  Customer->Resources.Remove(ResourceName);
}

void Accountant::UpdatePriorities() 
{
  CustomerRecord* Customer;
  Customers.startIterations();
  while (Customers.iterate(Customer)) {
    int CurrPriority=Customer->Priority;
    CurrPriority=int(CurrPriority*0.6);
    CurrPriority-=Customer->Resources.Count();
  }
}

//---------------------------------------------------------------

Accountant::CustomerRecord* Accountant::GetCustomerRecord(MyString& CustomerName)
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
    Customer->Priority=0;
  }
  return Customer;
}

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
