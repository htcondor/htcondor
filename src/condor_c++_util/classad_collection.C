/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "classad_collection.h"

using namespace std;

//----------------------------------------------------------------------------------
// Constructor (initialization)
//----------------------------------------------------------------------------------

OldClassAdCollection::OldClassAdCollection(const char* filename) 
  : ClassAdLog(filename), Collections(97, HashFunc)
{
  LastCoID=0;
  Collections.insert(LastCoID,new ExplicitCollection("",true));

#if 0		// Todd: this code just uses cycles
  HashKey HK;
  char key[_POSIX_PATH_MAX];
  ClassAd* Ad;
  table.startIterations();
  while(table.iterate(HK,Ad)) {
    HK.sprint(key);
    AddClassAd(0,key,Ad);
  }
#endif
}

//----------------------------------------------------------------------------------
// Constructor (initialization)
//----------------------------------------------------------------------------------

OldClassAdCollection::OldClassAdCollection() 
  : ClassAdLog(), Collections(97, HashFunc)
{
  LastCoID=0;
  Collections.insert(LastCoID,new ExplicitCollection("",true));
}

//----------------------------------------------------------------------------------
// Destructor - frees the memory used by the collections
//----------------------------------------------------------------------------------

OldClassAdCollection::~OldClassAdCollection() {
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

bool OldClassAdCollection::NewClassAd(const char* key, const char* mytype, const char* targettype)
{
  LogRecord* log=new LogNewClassAd(key,mytype,targettype);
  ClassAdLog::AppendLog(log);
  // return AddClassAd(0,key);
  return true;
}

//----------------------------------------------------------------------------------

bool OldClassAdCollection::NewClassAd(const char* key, ClassAd* ad)
{
  LogRecord* log=new LogNewClassAd(key,ad->GetMyTypeName(),ad->GetTargetTypeName());
  ClassAdLog::AppendLog(log);
  ClassAdUnParser unp;
  ExprTree* expr;
//  ExprTree* L_expr;
//  ExprTree* R_expr;
//  char name[1000];
//  char *value;
  string name;
  string value;
//  ad->ResetExpr();
//  while ((expr=ad->NextExpr())!=NULL) {
  ClassAd::iterator adIter;
  for( adIter = ad->begin( ); adIter != ad->end( ); adIter++ ) {
//	strcpy(name,"");
//	L_expr=expr->LArg();
//	L_expr->PrintToStr(name);
//	R_expr=expr->RArg();
//	value = NULL;
//  R_expr->PrintToNewStr(&value);
	  name = adIter->first;
	  expr = adIter->second;
	  unp.Unparse( value, expr );
//    LogRecord* log=new LogSetAttribute(key,name,value);
	  LogRecord* log = new LogSetAttribute( key, name.c_str(), value.c_str() );
//	free(value);
	  ClassAdLog::AppendLog(log);
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

bool OldClassAdCollection::DestroyClassAd(const char *key)
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

bool OldClassAdCollection::SetAttribute(const char *key, const char *name, const char *value)
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

bool OldClassAdCollection::DeleteAttribute(const char *key, const char *name)
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

int OldClassAdCollection::CreateExplicitCollection(int ParentCoID, const MyString& Rank, bool FullFlag)
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
int OldClassAdCollection::CreateConstraintCollection(int ParentCoID, const MyString& Rank, const MyString& Constraint)
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
int OldClassAdCollection::CreatePartition(int ParentCoID, const MyString& Rank, StringSet& attrList)
{
  // Lookup the parent
  BaseCollection* Parent;
  if (Collections.lookup(ParentCoID,Parent)==-1) return -1;

  // Initialize and insert to collection table
  PartitionParent* Coll=new PartitionParent(Rank,attrList);
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

bool OldClassAdCollection::DeleteCollection(int CoID)
{
  return TraverseTree(CoID,&OldClassAdCollection::RemoveCollection);
}

//----------------------------------------------------------------------------------
/// Include Class Ad in a collection (private method)
//----------------------------------------------------------------------------------

bool OldClassAdCollection::AddClassAd(int CoID, const MyString& OID)
{
  // Get the ad
  ClassAd* Ad;
  if (table.lookup(HashKey(OID.Value()),Ad)==-1) return false;

  return AddClassAd(CoID,OID,Ad);
}

//----------------------------------------------------------------------------------
/// Include Class Ad in a collection (private method)
//----------------------------------------------------------------------------------

bool OldClassAdCollection::AddClassAd(int CoID, const MyString& OID, ClassAd* Ad)
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

bool OldClassAdCollection::CheckClassAd(BaseCollection* Coll,const MyString& OID, ClassAd* Ad) 
{
  if (Coll->Type()==PartitionParent_e) {
	  ClassAdUnParser unp;
    PartitionParent* ParentColl=(PartitionParent*) Coll;
    StringSet Values;
    MyString AttrName;
    MyString AttrValue;
//    char tmp[1000];
	string tmp;
    ParentColl->Attributes.StartIterations();
// printf("Checking OID %s\n",OID.Value());
    while (ParentColl->Attributes.Iterate(AttrName)) {
//      *tmp='\0';
      ExprTree* expr=Ad->Lookup(AttrName.Value());
      if (expr) {
//        expr=expr->RArg();
//        expr->PrintToStr(tmp);
		  unp.Unparse( tmp, expr );
      }
//      AttrValue=tmp;
	  AttrValue = tmp.c_str( );
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

bool OldClassAdCollection::EqualSets(StringSet& S1, StringSet& S2) 
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

bool OldClassAdCollection::RemoveClassAd(int CoID, const MyString& OID)
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

bool OldClassAdCollection::ChangeClassAd(const MyString& OID)
{
  RemoveClassAd(0,OID);
  return AddClassAd(0,OID);
}

//----------------------------------------------------------------------------------
/// Remove a collection (private method)
//----------------------------------------------------------------------------------

bool OldClassAdCollection::RemoveCollection(int CoID, BaseCollection* Coll)
{
  delete Coll;
  if (Collections.remove(CoID)==-1) return false;
  return true;
}

//----------------------------------------------------------------------------------
/// Start iterating on cllection IDs
//----------------------------------------------------------------------------------

void OldClassAdCollection::StartIterateAllCollections()
{
  Collections.startIterations();
}

//----------------------------------------------------------------------------------
/// Get the next collection ID
//----------------------------------------------------------------------------------

bool OldClassAdCollection::IterateAllCollections(int& CoID)
{
  BaseCollection* Coll;
  if (Collections.iterate(CoID,Coll)) return true;
  return false;
}

//----------------------------------------------------------------------------------
/// Start iterating on child collections of some parent collection
//----------------------------------------------------------------------------------

bool OldClassAdCollection::StartIterateChildCollections(int ParentCoID)
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

bool OldClassAdCollection::IterateChildCollections(int ParentCoID, int& CoID)
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

bool OldClassAdCollection::StartIterateClassAds(int CoID)
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

bool OldClassAdCollection::IterateClassAds(int CoID, RankedClassAd& RankedAd)
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

float OldClassAdCollection::GetClassAdRank(ClassAd* Ad, const MyString& RankExpr)
{
  if (RankExpr.Length()==0) return 0.0;
  char tmp[200];
  sprintf(tmp,"%s=%s",ATTR_RANK,RankExpr.Value());
  ClassAdParser parser;
  ClassAd RankingAd;
  parser.ParseClassAd( string(tmp), RankingAd );
  float Rank;
  if (!RankingAd.EvalFloat(ATTR_RANK,Ad,Rank)) Rank=0.0;
  return Rank;
}

//----------------------------------------------------------------------------------
/// Find a collection's type
//----------------------------------------------------------------------------------

int OldClassAdCollection::GetCollectionType(int CoID)
{
  BaseCollection* Coll;
  if (Collections.lookup(CoID,Coll)==-1) return -1;
  return Coll->Type();
}

//----------------------------------------------------------------------------------
/// Traverse the collection tree
//----------------------------------------------------------------------------------

bool OldClassAdCollection::TraverseTree(int CoID, bool (OldClassAdCollection::*Func)(int,BaseCollection*))
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
  
void OldClassAdCollection::Print()
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
  
void OldClassAdCollection::Print(int CoID)
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
