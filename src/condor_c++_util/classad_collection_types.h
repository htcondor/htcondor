#ifndef _Collections_H
#define _Collections_H

//-------------------------------------------------------------------------

#include "condor_classad.h"
#include "Set.h"
#include "MyString.h"

//-------------------------------------------------------------------------
// Collection Hierarchy
//-------------------------------------------------------------------------

typedef Set<MyString> StringSet;
typedef Set<int> IntegerSet;

enum CollectionType { ExplicitCollection_e, ConstraintCollection_e, PartitionParent_e, PartitionChild_e };

//-------------------------------------------------------------------------

class BaseCollection {

public:

  BaseCollection(int parentCoID, const MyString& rank) {
    ParentCoID=parentCoID;
    Rank=rank;
  }

  virtual bool CheckClassAd(ClassAd* Ad)=0;

  virtual int Type()=0;

  MyString GetRank() { return Rank; }
  
  int GetParentCoID() { return ParentCoID; }

  IntegerSet Children;
  StringSet Members;

protected:

  MyString Rank;
  int ParentCoID;

};

//-------------------------------------------------------------------------

class ExplicitCollection : public BaseCollection {

public:

  ExplicitCollection(int parentCoID, const MyString& rank, bool fullFlag) 
  : BaseCollection(parentCoID,rank) {
    FullFlag=fullFlag;
  }

  virtual bool CheckClassAd(ClassAd* Ad) {
    return FullFlag;
  }

  virtual int Type() { return ExplicitCollection_e; }

protected:

  bool FullFlag;

};

//-------------------------------------------------------------------------

class ConstraintCollection : public BaseCollection {

public:

  ConstraintCollection(int parentCoID, const MyString& rank, const MyString& constraint)
  : BaseCollection(parentCoID,rank) {
    Constraint=constraint;
  }
  
  virtual bool CheckClassAd(ClassAd* Ad) {
    ExprTree* tree;
    EvalResult result;

    if (Parse(Constraint.Value(), tree) != 0) {
        return false;
    }

    if (!tree->EvalTree(NULL, Ad, &result)) {
        delete tree;
        return false;
    }

    delete tree;
    if (result.type == LX_INTEGER) {
        return (bool)result.i;
    }

    return false;
  }

  virtual int Type() { return ConstraintCollection_e; }

protected:

  MyString Constraint;

};

//-------------------------------------------------------------------------

class PartitionParent : public BaseCollection {

public:

  PartitionParent(int parentCoID, const MyString& rank, char** attributes)
  : BaseCollection(parentCoID,rank) {
    for (;*attributes;attributes++) Attributes.Add(*attributes);
  }

  virtual bool CheckClassAd(ClassAd* Ad) {
    return true;
  }

  virtual int Type() { return PartitionParent_e; }

protected:

  StringSet Attributes;

};

//-------------------------------------------------------------------------

class PartitionChild : public BaseCollection {

public:

  PartitionChild(int parentCoID, const MyString& rank, char** values)
  : BaseCollection(parentCoID,rank) {
    for (;*values;values++) Values.Add(*values);
  }

  virtual bool CheckClassAd(ClassAd* Ad) {
    return true;
  }

  virtual int Type() { return PartitionChild_e; }

private:

  StringSet Values;

};

//-------------------------------------------------------------------------

#endif
