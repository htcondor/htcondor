#ifndef _Accountant_H_
#define _Accountant_H_

#include "condor_classad.h"
#include "MyString.h"
#include "HashTable.h"
#include "StringSet.h"

class Accountant {

public:

  // User Functions

  int GetPriority(const MyString& CustomerName); // get priority for a customer
  void SetPriority(const MyString& CustomerName, int Priority); // set priority for a customer

  void AddMatch(const MyString& CustomerName, ClassAd* ResourceAd); // Add new match
  void RemoveMatch(const MyString& ResourceAd); // remove a match

  // void CheckMatches(ClassAdList& ResourceList);  // Remove matches that are not claimed
  void UpdatePriorities(); // update all the priorities

  //----------------------------------------------------------------------

  static int HashFunc(const MyString& Key, int TableSize) {
    int count=0;
    int length=Key.Length();
    for(int i=0; i<length; i++) count+=Key[i];
    return (count % TableSize);
  }
  
  Accountant(int MaxCustomers, int MaxResources) 
    : Customers(MaxCustomers, HashFunc), Resources(MaxResources, HashFunc) {}
                                                
private:

  // int LoadPriorities(); // Save to file
  // int SavePriorities(); // Read from file

  struct CustomerRecord {
    int Priority;
    StringSet ResourceNames;
    CustomerRecord() { Priority=0; }
  };

  typedef HashTable<MyString, CustomerRecord*> CustomerTable;
  CustomerTable Customers;

  struct ResourceRecord {
    MyString CustomerName;
    ClassAd* Ad;
    ResourceRecord() { Ad=NULL; }
    ~ResourceRecord() { if (Ad) delete Ad; }
  };

  typedef HashTable<MyString, ResourceRecord*> ResourceTable;
  ResourceTable Resources;

  // --------------------------------------------------------

  MyString GetResourceName(ClassAd* Resource);

};

#endif
