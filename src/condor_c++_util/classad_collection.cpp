/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "classad_collection.h"


//----------------------------------------------------------------------------------
// Constructor (initialization)
//----------------------------------------------------------------------------------

ClassAdCollection::ClassAdCollection(const char* filename,int max_historical_logs_arg) 
  : ClassAdLog(filename,max_historical_logs_arg), Collections(97, HashFunc)
{
  LastCoID=0;
  Collections.insert(LastCoID,new ExplicitCollection("",true));

}

//----------------------------------------------------------------------------------
// Constructor (initialization)
//----------------------------------------------------------------------------------

ClassAdCollection::ClassAdCollection() 
  : ClassAdLog(), Collections(97, HashFunc)
{
  LastCoID=0;
  Collections.insert(LastCoID,new ExplicitCollection("",true));
}

//----------------------------------------------------------------------------------
// Destructor - frees the memory used by the collections
//----------------------------------------------------------------------------------

ClassAdCollection::~ClassAdCollection() {
  DeleteCollection(0);
}

//----------------------------------------------------------------------------------
/** Insert a new Class Ad -
 this inserts a classad with a given key, type, and target type. The class ad is
inserted as an empty class ad (use SetAttribute to set its attributes).
This operation is logged. The class ad is inserted into the root collection,
and into its children if appropriate.
Input: key - the unique key for the ad
       mytype - the ad's type
       targettype - the target type
Output: true on success, false otherwise
*/
//----------------------------------------------------------------------------------

bool ClassAdCollection::NewClassAd(const char* key, const char* mytype, const char* targettype)
{
  LogRecord* log=new LogNewClassAd(key,mytype,targettype);
  ClassAdLog::AppendLog(log);
  // return AddClassAd(0,key);
  return true;
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::NewClassAd(const char* key, ClassAd* ad)
{
  LogRecord* log=new LogNewClassAd(key,ad->GetMyTypeName(),ad->GetTargetTypeName());
  ClassAdLog::AppendLog(log);
  const char *name;
  ExprTree* expr;
  ad->ResetExpr();
  while (ad->NextExpr(name, expr)) {
    LogRecord* l=new LogSetAttribute(key,name,ExprTreeToString(expr));
    ClassAdLog::AppendLog(l);
  }
  // return AddClassAd(0,key);
  return true;
}

//----------------------------------------------------------------------------------
/** Delete a Class Ad
Delete a class ad. The ad is deleted from all the collections. This operation
is logged. 
Input: key - the class ad's key.
Output: true on success, false otherwise
*/
//----------------------------------------------------------------------------------

bool ClassAdCollection::DestroyClassAd(const char *key)
{
  LogRecord* log=new LogDestroyClassAd(key);
  ClassAdLog::AppendLog(log);
  // return RemoveClassAd(0,key);
  return true;
}

//----------------------------------------------------------------------------------
/** Change attribute of a class ad - this operation is logged.
Any effects on collection membership will also take place.
Input: key - the class ad's key
       name - the attribute name
       value - the attribute value
Output: true on success, false otherwise
*/
//----------------------------------------------------------------------------------

bool ClassAdCollection::SetAttribute(const char *key, const char *name, const char *value)
{
  LogRecord* log=new LogSetAttribute(key,name,value);
  ClassAdLog::AppendLog(log);
  // return ChangeClassAd(key);
  return true;
}

//----------------------------------------------------------------------------------
/** Delete an attribute of a class ad - this operation is logged.
Any effects on collection membership will also take place.
Input: key - the class ad's key
       name - the attribute name
Output: true on success, false otherwise
*/
//----------------------------------------------------------------------------------

bool ClassAdCollection::DeleteAttribute(const char *key, const char *name)
{
  LogRecord* log=new LogDeleteAttribute(key,name);
  ClassAdLog::AppendLog(log);
  // return ChangeClassAd(key);
  return true;
}


#if 0 // NOT USED
//----------------------------------------------------------------------------------
/* Create an Explicit Collection - create a new collection as a child of
an exisiting collection. This operation, as well as other collection management
operation is not logged.
Input: ParentCoID - the ID of the parent collection
       Rank - the rank expression for the collection
       FullFlag - indicates wether ads which are added to the parent should automatically
                  be added to the child
Output: the new collectionID
*/
//----------------------------------------------------------------------------------

int ClassAdCollection::CreateExplicitCollection(int ParentCoID, const MyString& Rank, bool FullFlag)
{
  // Lookup the parent
  BaseCollection* Parent;
  if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

  // Initialize and insert to collection table
  ExplicitCollection* Coll=new ExplicitCollection(Rank,FullFlag);
  int CoID=LastCoID+1;
  if (Collections.insert(CoID,Coll)==-1) return -1;
  LastCoID=CoID;

  // Add to parent's children
  Parent->Children.Add(CoID);

  // Add Parents members to new collection
  RankedClassAd RankedAd;
  Parent->Members.StartIterations();
  while(Parent->Members.Iterate(RankedAd)) AddClassAd(CoID,RankedAd.OID);

  return LastCoID;
}
#endif

//----------------------------------------------------------------------------------
/** Create a constraint Collection - create a new collection as a child of
an exisiting collection. This operation, as well as other collection management
operation is not logged.
Input: ParentCoID - the ID of the parent collection
       Rank - the rank expression for the collection
       Constraint - the constraint expression (must result in a boolean)
Output: the new collectionID
*/
//----------------------------------------------------------------------------------

#if 0	// Todd: not used
int ClassAdCollection::CreateConstraintCollection(int ParentCoID, const MyString& Rank, const MyString& Constraint)
{
  // Lookup the parent
  BaseCollection* Parent;
  if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

  // Initialize and insert to collection table
  ConstraintCollection* Coll=new ConstraintCollection(Rank,Constraint);
  int CoID=LastCoID+1;
  if (Collections.insert(CoID,Coll)==-1) return -1;
  LastCoID=CoID;

  // Add to parent's children
  Parent->Children.Add(CoID);

  // Add Parents members to new collection
  RankedClassAd RankedAd;
  Parent->Members.StartIterations();
  while(Parent->Members.Iterate(RankedAd)) AddClassAd(CoID,RankedAd.OID);

  return LastCoID;
}
#endif

//----------------------------------------------------------------------------------

#if 0	// Todd - not used yet
int ClassAdCollection::CreatePartition(int ParentCoID, const MyString& Rank, StringSet& AttrList)
{
  // Lookup the parent
  BaseCollection* Parent;
  if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

  // Initialize and insert to collection table
  PartitionParent* Coll=new PartitionParent(Rank,AttrList);
  int CoID=LastCoID+1;
  if (Collections.insert(CoID,Coll)==-1) return -1;
  LastCoID=CoID;

  // Add to parent's children
  Parent->Children.Add(CoID);

  // Add Parents members to new collection
  RankedClassAd RankedAd;
  Parent->Members.StartIterations();
  while(Parent->Members.Iterate(RankedAd)) AddClassAd(CoID,RankedAd.OID);

  return LastCoID;
}
#endif

//----------------------------------------------------------------------------------
/** Delete a collection
Input: CoID - the ID of the collection
Output: true on success, false otherwise
*/
//----------------------------------------------------------------------------------

bool ClassAdCollection::DeleteCollection(int CoID)
{
  return TraverseTree(CoID,&ClassAdCollection::RemoveCollection);
}

//----------------------------------------------------------------------------------
/// Include Class Ad in a collection (private method)
//----------------------------------------------------------------------------------

bool ClassAdCollection::AddClassAd(int CoID, const MyString& OID)
{
  // Get the ad
  ClassAd* Ad;
  if (table.lookup(HashKey(OID.Value()),Ad)==-1) return false;

  return AddClassAd(CoID,OID,Ad);
}

//----------------------------------------------------------------------------------
/// Include Class Ad in a collection (private method)
//----------------------------------------------------------------------------------

bool ClassAdCollection::AddClassAd(int CoID, const MyString& OID, ClassAd* Ad)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;

  // Check if ad matches the collection
  if (!CheckClassAd(Coll,OID,Ad)) return false;

  // Create the Ranked Ad
  RankedClassAd RankedAd(OID,GetClassAdRank(Ad,Coll->GetRank()));
  if (Coll->Members.Exist(RankedAd)) return false;

  // Insert the add in the correct place in the list of members
  RankedClassAd CurrRankedAd;
  bool Inserted=false;
  Coll->Members.StartIterations();
  while (Coll->Members.Iterate(CurrRankedAd)) {
    if (RankedAd.Rank<=CurrRankedAd.Rank) {
      Coll->Members.Insert(RankedAd);
      Inserted=true;
      break;
    }
  }
  if (!Inserted) Coll->Members.Insert(RankedAd);
    
  // Insert into chldren
  int ChildCoID;
  /*BaseCollection* ChildColl;*/
  Coll->Children.StartIterations();
  while (Coll->Children.Iterate(ChildCoID)) AddClassAd(ChildCoID,OID,Ad);

  return true;
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::CheckClassAd(BaseCollection* Coll,const MyString& OID, ClassAd* Ad) 
{
  if (Coll->Type()==PartitionParent_e) {
    PartitionParent* ParentColl=(PartitionParent*) Coll;
    StringSet Values;
    MyString AttrName;
    MyString AttrValue;
    ParentColl->Attributes.StartIterations();
// printf("Checking OID %s\n",OID.Value());
    while (ParentColl->Attributes.Iterate(AttrName)) {
      ExprTree* expr=Ad->LookupExpr(AttrName.Value());
      if (expr) {
		  AttrValue = ExprTreeToString( expr );
      } else {
		  AttrValue = "";
	  }
      Values.Add(AttrValue);
    }
// Values.StartIterations(); while (Values.Iterate(AttrValue)) { printf("Val: AttrValue=%s\n",AttrValue.Value()); }
    int CoID;
    PartitionChild* ChildColl=NULL;
    ParentColl->Children.StartIterations();
    while (ParentColl->Children.Iterate(CoID)) {
      if (Collections.lookup(CoID,Coll)==-1) continue;
      ChildColl=(PartitionChild*) Coll;
// ChildColl->Values.StartIterations(); while (ChildColl->Values.Iterate(AttrValue)) { printf("ChildVal: AttrValue=%s\n",AttrValue.Value()); }
      if (EqualSets(ChildColl->Values,Values)) break;
      ChildColl=NULL;
    }

    if (ChildColl==NULL) {
      // Create a new child collection
      ChildColl=new PartitionChild(ParentColl->Rank,Values);
      CoID=LastCoID+1;
      if (Collections.insert(CoID,ChildColl)==-1) return false;
      LastCoID=CoID;

      // Add to parent's children
      ParentColl->Children.Add(CoID);
   }
 
   // Add to child
   AddClassAd(CoID,OID,Ad);
   return false;
  }
  else {
    return Coll->CheckClassAd(Ad);
  }
}

//----------------------------------------------------------------------------------

bool ClassAdCollection::EqualSets(StringSet& S1, StringSet& S2) 
{
  S1.StartIterations();
  S2.StartIterations();
  MyString OID1;
  MyString OID2;
  while(S1.Iterate(OID1)) {
    if (!S2.Iterate(OID2)) return false;
    if (OID1!=OID2) return false;
  }
  return (!S2.Iterate(OID2));
}

//----------------------------------------------------------------------------------
/// Remove Class Ad from a collection (private method)
//----------------------------------------------------------------------------------

bool ClassAdCollection::RemoveClassAd(int CoID, const MyString& OID)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;

  // Check if ad is in the collection and remove it
  if (!Coll->Members.Exist(RankedClassAd(OID)) && Coll->Type()!=PartitionParent_e) return false;
  Coll->Members.Remove(RankedClassAd(OID));
    
  // remove from children
  int ChildCoID;
  /*BaseCollection* ChildColl;*/
  Coll->Children.StartIterations();
  while (Coll->Children.Iterate(ChildCoID)) RemoveClassAd(ChildCoID,OID);

  return true;
}

//----------------------------------------------------------------------------------
/// Change a class-ad (private method)
//----------------------------------------------------------------------------------

bool ClassAdCollection::ChangeClassAd(const MyString& OID)
{
  RemoveClassAd(0,OID);
  return AddClassAd(0,OID);
}

//----------------------------------------------------------------------------------
/// Remove a collection (private method)
//----------------------------------------------------------------------------------

bool ClassAdCollection::RemoveCollection(int CoID, BaseCollection* Coll)
{
  delete Coll;
  if (Collections.remove(CoID)==-1) return false;
  return true;
}

//----------------------------------------------------------------------------------
/// Start iterating on cllection IDs
//----------------------------------------------------------------------------------

void ClassAdCollection::StartIterateAllCollections()
{
  Collections.startIterations();
}

//----------------------------------------------------------------------------------
/// Get the next collection ID
//----------------------------------------------------------------------------------

bool ClassAdCollection::IterateAllCollections(int& CoID)
{
  BaseCollection* Coll;
  if (Collections.iterate(CoID,Coll)) return true;
  return false;
}

//----------------------------------------------------------------------------------
/// Start iterating on child collections of some parent collection
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
/// Get the next child collection
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
/// Start iterating on class ads in a collection
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
/// Get the next class ad and rank in the collection
//----------------------------------------------------------------------------------

bool ClassAdCollection::IterateClassAds(int CoID, RankedClassAd& RankedAd)
{
  // Get collection pointer
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return false;
  
  if (Coll->Members.Iterate(RankedAd)) return true;
  return false;
}

//----------------------------------------------------------------------------------
/// Calculate a class-ad's rank
//----------------------------------------------------------------------------------

float ClassAdCollection::GetClassAdRank(ClassAd* Ad, const MyString& RankExpr)
{
  if (RankExpr.Length()==0) return 0.0;
  AttrList RankingAd;
  RankingAd.AssignExpr( ATTR_RANK, RankExpr.Value() );
  float Rank;
  if (!RankingAd.EvalFloat(ATTR_RANK,Ad,Rank)) Rank=0.0;
  return Rank;
}

//----------------------------------------------------------------------------------
/// Find a collection's type
//----------------------------------------------------------------------------------

int ClassAdCollection::GetCollectionType(int CoID)
{
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return -1;
  return Coll->Type();
}

//----------------------------------------------------------------------------------
/// Traverse the collection tree
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
  return (this->*Func)(CoID,CurrNode);
}

//----------------------------------------------------------------------------------
/// Print out all the collections
//----------------------------------------------------------------------------------
  
void ClassAdCollection::Print()
{
  int CoID;
  MyString OID;
  RankedClassAd RankedAd;
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
    while(Coll->Members.Iterate(RankedAd)) printf("%s(%.1f) ",RankedAd.OID.Value(),RankedAd.Rank);
    printf("\n-----------------------------------------\n");
  }
}

//----------------------------------------------------------------------------------
/// Print out a single collection
//----------------------------------------------------------------------------------
  
void ClassAdCollection::Print(int CoID)
{
  MyString OID;
  RankedClassAd RankedAd;
  int ChildCoID;
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return;
  printf("-----------------------------------------\n");
  MyString Rank=Coll->GetRank();
  printf("CoID=%d Type=%d Rank=%s\n",CoID,Coll->Type(),Rank.Value());
  printf("Children: ");
  Coll->Children.StartIterations();
  while (Coll->Children.Iterate(ChildCoID)) printf("%d ",ChildCoID);
  printf("\nMembers: ");
  Coll->Members.StartIterations();
  while(Coll->Members.Iterate(RankedAd)) printf("%s(%.1f) ",RankedAd.OID.Value(),RankedAd.Rank);
  printf("\n-----------------------------------------\n");
}
