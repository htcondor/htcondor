#ifndef _ClassAdCollection_H
#define _ClassAdCollection_H

//--------------------------------------------------------------------------

#include "condor_classad.h"
#include "Set.h"
#include "HashTable.h"
#include "MyString.h"
#include "classad_log.h"

#include "classad_collection_types.h"

//--------------------------------------------------------------------------

typedef HashTable<int,BaseCollection*> CollectionHashTable;

//--------------------------------------------------------------------------

class ClassAdCollection : private ClassAdLog {

public:

  // Constructors and destructor

  ClassAdCollection();
  ClassAdCollection(const char* filename);
  ~ClassAdCollection();

  // ClassAd and attribute handling methods

  bool NewClassAd(const char *key, const char *mytype, const char *targettype);
  bool DestroyClassAd(const char *key);
  bool SetAttribute(const char *key, const char *name, const char *value);
  bool DeleteAttribute(const char *key, const char *name);

  bool LookupClassAd(const char* key, ClassAd*& Ad) { return (table.lookup(HashKey(key), Ad)==0); }
  
  // Creation and deletion of collections

  int CreateExplicitCollection(int ParentCoID, const MyString& Rank, bool FullFlag=false);
  int CreateConstraintCollection(int ParentCoID, const MyString& Rank, const MyString& Constraint);
  // int CreatePartition(int ParentCoID, const MyString& Rank, char** AttrList);
  bool DeleteCollection(int CoID);

  // Iteration functions

  void StartIterateAllCollections();
  bool IterateAllCollections(int& CoID);

  bool StartIterateChildCollections(int ParentCoID);
  bool IterateChildCollections(int ParentCoID, int& CoID);

  bool StartIterateClassAds(int CoID);
  bool IterateClassAds(int CoID, MyString& OID);

  void StartIterateAllClassAds() { table.startIterations(); }
  bool IterateAllClassAds(ClassAd*& Ad) { return (table.iterate(Ad)==1); }

  // Inherited functions

  void TruncLog() { ClassAdLog::TruncLog(); }
  void BeginTransaction() { ClassAdLog::BeginTransaction(); }
  void AbortTransaction() { ClassAdLog::AbortTransaction(); }
  void CommitTransaction() { ClassAdLog::CommitTransaction(); }
  int LookupInTransaction(const char *key, const char *name, char *&val) { return LookupInTransaction(key,name,val); }
  
  // Misc methods

  int GetCollectionType(int CoID);

  void Print();

  static int HashFunc(const int& Key, int TableSize) { return (Key % TableSize); }

private:

  // Data Members

  CollectionHashTable Collections;
  int LastCoID;

  // Private Methods

  bool AddClassAd(int CoID, const MyString& OID);
  bool AddClassAd(int CoID, const MyString& OID, ClassAd* ad);
  bool RemoveClassAd(int CoID, const MyString& OID);
  bool ChangeClassAd(int CoID, const MyString& OID);
  bool ChangeClassAd(int CoID, const MyString& OID, ClassAd* ad);

  int CompareClassAds(ClassAd* Ad1, ClassAd* Ad2, const MyString& Rank);
  bool RemoveCollection(int CoID, BaseCollection* Coll);
  bool TraverseTree(int CoID, bool (ClassAdCollection::*Func)(int,BaseCollection*));
};

//-----------------------------------------------------------------------------------
// Overloaded ClassAdLog operations that update the collections
//-----------------------------------------------------------------------------------

#endif
