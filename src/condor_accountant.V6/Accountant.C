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
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  if (!Customer) return 0;
  int Priority=Customer->Priority;
  if (DeadCustomer(Customer)) Customers.remove(CustomerName);
  return Priority;
}

void Accountant::SetPriority(const MyString& CustomerName, int Priority) 
{
  CustomerRecord* Customer=NewCustomerRecord(CustomerName);
  Customer->Priority=Priority;
  if (DeadCustomer(Customer)) Customers.remove(CustomerName);
}

void Accountant::AddMatch(const MyString& CustomerName, ClassAd* Resource) 
{
  CustomerRecord* Customer=NewCustomerRecord(CustomerName);
  MyString ResourceName=GetResourceName(Resource);
  Customer->Resources.Add(ResourceName);
}

void Accountant::RemoveMatch(const MyString& CustomerName, ClassAd* Resource)
{
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  if (!Customer) return;
  MyString ResourceName=GetResourceName(Resource);
  Customer->Resources.Remove(ResourceName);
  if (DeadCustomer(Customer)) Customers.remove(CustomerName);
}

void Accountant::UpdatePriorities() 
{
  CustomerRecord* Customer;
  MyString CustomerName;
  Customers.startIterations();
  while (Customers.iterate(CustomerName, Customer)) {
    int Priority=Customer->Priority;
    int ResourcesUsed=Customer->Resources.Count();
    Priority=int(Priority*0.6)-ResourcesUsed;
    Customer->Priority=Priority;
    if (DeadCustomer(Customer)) Customers.remove(CustomerName);
  }
}

//---------------------------------------------------------------

// Returns 1 if Customer can be deleted else 0
int Accountant::DeadCustomer(CustomerRecord* Customer) {
  if (Customer->Priority==0 && Customer->Resources.Count()==0) return 1;
  return 0;
}

// Get customer record, or NULL if it doesn't exist
Accountant::CustomerRecord* Accountant::GetCustomerRecord(const MyString& CustomerName)
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) Customer=NULL;
  return Customer;
}



// Check customer record, and create it if it doesn't exist
Accountant::CustomerRecord* Accountant::NewCustomerRecord(const MyString& CustomerName)
{
  CustomerRecord* Customer=GetCustomerRecord(CustomerName);
  if (Customer==NULL) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
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
