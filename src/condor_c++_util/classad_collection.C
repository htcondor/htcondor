#include "condor_common.h"
#include "condor_debug.h"

#include "classad_collection.h"

static char *_FileName_ = __FILE__;

//----------------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------------

ClassAdCollection::ClassAdCollection() 
  : Collections(97, HashFunc)
{
  LastCoID=0;
  Collections.insert(LastCoID,new ExplicitCollection(-1,"",true));
  
}

//----------------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------------

ClassAdCollection::ClassAdCollection(const char* filename) 
  : Collections(97, HashFunc), ClassAdLog(filename)
{
  LastCoID=0;
  Collections.insert(LastCoID,new ExplicitCollection(-1,"",true));

  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* Ad;
  table.startIterations();
  while(table.iterate(HK,Ad)) {
    HK.sprint(key);
    AddClassAd(0,key,Ad);
  }
}

//----------------------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------------------

ClassAdCollection::~ClassAdCollection() {
  DeleteCollection(0);
}

//----------------------------------------------------------------------------------
// New Class Ad
//----------------------------------------------------------------------------------

bool ClassAdCollection::NewClassAd(const char *key, const char *mytype, const char *targettype)
{
  LogRecord* log=new LogNewClassAd(key,mytype,targettype);
  ClassAdLog::AppendLog(log);
  return AddClassAd(0,key);
}

//----------------------------------------------------------------------------------
// Destroy Class Ad
//----------------------------------------------------------------------------------

bool ClassAdCollection::DestroyClassAd(const char *key)
{
  LogRecord* log=new LogDestroyClassAd(key);
  ClassAdLog::AppendLog(log);
  return RemoveClassAd(0,key);
}

//----------------------------------------------------------------------------------
// Change attribute of a class ad
//----------------------------------------------------------------------------------

bool ClassAdCollection::SetAttribute(const char *key, const char *name, const char *value)
{
  LogRecord* log=new LogSetAttribute(key,name,value);
  ClassAdLog::AppendLog(log);
  return ChangeClassAd(0,key);
}

//----------------------------------------------------------------------------------
// New Class Ad
//----------------------------------------------------------------------------------

bool ClassAdCollection::DeleteAttribute(const char *key, const char *name)
{
  LogRecord* log=new LogDeleteAttribute(key,name);
  ClassAdLog::AppendLog(log);
  return ChangeClassAd(0,key);
}

//----------------------------------------------------------------------------------
// Create an Explicit Collection
//----------------------------------------------------------------------------------

int ClassAdCollection::CreateExplicitCollection(int ParentCoID, const MyString& Rank, bool FullFlag)
{
  // Lookup the parent
  BaseCollection* Parent;
  if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

  // Initialize and insert to collection table
  ExplicitCollection* Coll=new ExplicitCollection(ParentCoID,Rank,FullFlag);
  int CoID=LastCoID+1;
  if (Collections.insert(CoID,Coll)==-1) return -1;
  LastCoID=CoID;

  // Add to parent's children
  Parent->Children.Add(CoID);

  // Add Parents members to new collection
  MyString OID;
  Parent->Members.StartIterations();
  while(Parent->Members.Iterate(OID)) AddClassAd(CoID,OID);

  return LastCoID;
}

//----------------------------------------------------------------------------------
// Create a constraint collection
//----------------------------------------------------------------------------------

int ClassAdCollection::CreateConstraintCollection(int ParentCoID, const MyString& Rank, const MyString& Constraint)
{
  // Lookup the parent
  BaseCollection* Parent;
  if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

  // Initialize and insert to collection table
  ConstraintCollection* Coll=new ConstraintCollection(ParentCoID,Rank,Constraint);
  int CoID=LastCoID+1;
  if (Collections.insert(CoID,Coll)==-1) return -1;
  LastCoID=CoID;

  // Add to parent's children
  Parent->Children.Add(CoID);

  // Add Parents members to new collection
  MyString OID;
  Parent->Members.StartIterations();
  while(Parent->Members.Iterate(OID)) AddClassAd(CoID,OID);

  return LastCoID;
}

//----------------------------------------------------------------------------------
// Delete a collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::DeleteCollection(int CoID)
{
  return TraverseTree(CoID,RemoveCollection);
}

//----------------------------------------------------------------------------------
// Include Class Ad in a collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::AddClassAd(int CoID, const MyString& OID)
{
  // Get the ad
  ClassAd* Ad;
  if (table.lookup(HashKey(OID.Value()),Ad)==-1) return false;

  return AddClassAd(CoID,OID,Ad);
}

//----------------------------------------------------------------------------------
// Include Class Ad in a collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::AddClassAd(int CoID, const MyString& OID, ClassAd* Ad)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;
  if (Coll->Members.Exist(OID)) return false;

  // Check if ad matches the collection
  if (!Coll->CheckClassAd(Ad)) return false;

  // Insert the add in the correct place in the list of members
  ClassAd* CurrAd;
  MyString CurrOID;
  bool Inserted=false;
  Coll->Members.StartIterations();
  while (Coll->Members.Iterate(CurrOID)) {
    if (table.lookup(HashKey(CurrOID.Value()),CurrAd)==-1) return false;
    if (CompareClassAds(Ad,CurrAd,Coll->GetRank())<=0) {
      Coll->Members.Insert(OID);
      Inserted=true;
      break;
    }
  }
  if (!Inserted) Coll->Members.Insert(OID);
    
  // Insert into chldren
  int ChildCoID;
  BaseCollection* ChildColl;
  Coll->Children.StartIterations();
  while (Coll->Children.Iterate(ChildCoID)) AddClassAd(ChildCoID,OID,Ad);

  return true;
}

//----------------------------------------------------------------------------------
// Remove Class Ad from a collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::RemoveClassAd(int CoID, const MyString& OID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;

  // Check if ad is in the collection and remove it
  if (!Coll->Members.Exist(OID)) return false;
  Coll->Members.Remove(OID);
    
  // remove from children
  int ChildCoID;
  BaseCollection* ChildColl;
  Coll->Children.StartIterations();
  while (Coll->Children.Iterate(ChildCoID)) RemoveClassAd(ChildCoID,OID);

  return true;
}

//----------------------------------------------------------------------------------
// Change a class-ad
//----------------------------------------------------------------------------------

bool ClassAdCollection::ChangeClassAd(int CoID, const MyString& OID)
{
  // Get the ad
  ClassAd* Ad;
  if (table.lookup(HashKey(OID.Value()),Ad)==-1) return false;

  return ChangeClassAd(CoID,OID,Ad);
}

//----------------------------------------------------------------------------------
// Change a class-ad
//----------------------------------------------------------------------------------

bool ClassAdCollection::ChangeClassAd(int CoID, const MyString& OID, ClassAd* Ad)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;

  // Check that it exists in the parent
  if (CoID!=0) {
    BaseCollection* Parent;
    if (Collections.lookup(Coll->GetParentCoID(),Parent)==-1) return false;
    if (!Parent->Members.Exist(OID)) return false;
  }
  
  // it it didn't exist before just add it (and check that it belongs)
  bool Existed=Coll->Members.Exist(OID);
  if (!Existed) return AddClassAd(CoID,OID,Ad);

  // if it doesn't belong anymore, remove it
  bool Matches=Coll->CheckClassAd(Ad);
  if (!Matches) return RemoveClassAd(CoID,OID);

  // Do recursive calls on children
  int ChildCoID;
  BaseCollection* ChildColl;
  Coll->Children.StartIterations();
  while (Coll->Children.Iterate(ChildCoID)) ChangeClassAd(ChildCoID,OID,Ad);

  return false;  // Ad remains in the collection (no change)
}

//----------------------------------------------------------------------------------
// Compare Class Ads according to a rank expression
//----------------------------------------------------------------------------------

int ClassAdCollection::CompareClassAds(ClassAd* Ad1, ClassAd* Ad2, const MyString& Rank)
{
  return 0;
}

//----------------------------------------------------------------------------------
// Remove a collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::RemoveCollection(int CoID, BaseCollection* Coll)
{
  delete Coll;
  if (Collections.remove(CoID)==-1) return false;
  return true;
}

//----------------------------------------------------------------------------------
// Iteration Methods
//----------------------------------------------------------------------------------

void ClassAdCollection::StartIterateAllCollections()
{
  Collections.startIterations();
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::IterateAllCollections(int& CoID)
{
  BaseCollection* Coll;
  if (Collections.iterate(CoID,Coll)) return true;
  return false;
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::StartIterateChildCollections(int ParentCoID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(ParentCoID,Coll)==-1) return false;
  
  Coll->Children.StartIterations();
  return true;
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::IterateChildCollections(int ParentCoID, int& CoID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(ParentCoID,Coll)==-1) return false;
  
  if (Coll->Children.Iterate(CoID)) return true;
  return false;
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::StartIterateClassAds(int CoID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;
  
  Coll->Members.StartIterations();
  return true;
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::IterateClassAds(int CoID, MyString& OID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;
  
  if (Coll->Members.Iterate(OID)) return true;
  return false;
}

//----------------------------------------------------------------------------------
// Misc methods
//----------------------------------------------------------------------------------

int ClassAdCollection::GetCollectionType(int CoID)
{
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return -1;
  return Coll->Type();
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::TraverseTree(int CoID, bool (ClassAdCollection::*Func)(int,BaseCollection*))
{
  BaseCollection* CurrNode;
  int ChildCoID;
  if (Collections.lookup(CoID,CurrNode)==-1) return false;
  CurrNode->Children.StartIterations();
  while (CurrNode->Children.Iterate(ChildCoID)) {
    if (!TraverseTree(ChildCoID,Func)) return false;
  }
  return Func(CoID,CurrNode);
}

//----------------------------------------------------------------------------------
  
void ClassAdCollection::Print()
{
  int CoID;
  MyString OID;
  int ChildCoID;
  BaseCollection* Coll;
  printf("-----------------------------------------\n");
  Collections.startIterations();
  while (Collections.iterate(CoID,Coll)) {
    MyString Rank=Coll->GetRank();
    printf("CoID=%d Type=%d Rank=%s\n",CoID,Coll->Type(),Rank.Value());
    printf("Children: ");
    Coll->Children.StartIterations();
    while (Coll->Children.Iterate(ChildCoID)) printf("%d ",ChildCoID);
    printf("\nMembers: ");
    Coll->Members.StartIterations();
    while(Coll->Members.Iterate(OID)) printf("%s ",OID.Value());
    printf("\n-----------------------------------------\n");
  }
}
