#ifndef _Accountant_H_
#define _Accountant_H_

#include "condor_classad.h"
#include "MyString.h"
#include "HashTable.h"

class Accountant {

public:

  // Customer=owner@uid_domain
  // Resource=startd_name@ip_addr

  int GetPriority(MyString& CustomerName); // get priority for a customer
  void SetPriority(MyString& CustomerName, int Priority); // set priority for a customer

  void AddMatch(MyString& CustomerName, ClassAd* Resource); // Add new match
  void RemoveMatch(MyString& CustomerName, ClassAd* Resource); // remove a match

  // void CheckMatches(ClassAdList& ResourceList);  // Remove matches that are not claimed
  void UpdatePriorities(); // update all the priorities
  
  static int HashFunc(MyString& Key, int TableSize) {
    int count=0;
    int length=Key.Length();
    for(int i=0; i<length; i++) count+=Key[i];
    return (count % TableSize);
  }
  
  Accountant(int MaxCustomers) : Customers(MaxCustomers, HashFunc) {}

private:

  // int LoadPriorities(); // Save to file
  // int SavePriorities(); // Read from file
			      
  struct CustomerRecord {
    int Priority;
    ClassAdList ResourceList;
  };

  typedef HashTable<MyString, CustomerRecord*> CustomerTable;
  CustomerTable Customers;

};

#endif
