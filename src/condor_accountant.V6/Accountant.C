#pragma implementation "HashTable.h"
#include "Accountant.h"

int Accountant::GetPriority(MyString& CustomerName) 
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
    Customer->Priority=0;
  }
  return Customer->Priority;
}

void Accountant::SetPriority(MyString& CustomerName, int Priority) 
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
    Customer->Priority=0;
  }
  Customer->Priority=Priority;
}

void Accountant::AddMatch(MyString& CustomerName, ClassAd* Resource) 
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
    Customer->Priority=0;
  }
  Customer->ResourceList.Insert(Resource);
}

void Accountant::RemoveMatch(MyString& CustomerName, ClassAd* Resource)
{
  CustomerRecord* Customer;
  if (Customers.lookup(CustomerName,Customer)==-1) {
    Customer=new CustomerRecord();
    Customers.insert(CustomerName,Customer);
    Customer->Priority=0;
  }
  Customer->ResourceList.Delete(Resource);
}

void Accountant::UpdatePriorities() 
{
  CustomerRecord* Customer;
  Customers.startIterations();
  while (Customers.iterate(Customer)) {
    int CurrPriority=Customer->Priority;
    CurrPriority=int(CurrPriority*0.6);
    CurrPriority+=Customer->ResourceList.MyLength();
  }
}
