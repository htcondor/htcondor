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
#ifndef _Collections_H
#define _Collections_H

//-------------------------------------------------------------------------

#include "condor_classad.h"
#include "Set.h"
#include "MyString.h"

//-------------------------------------------------------------------------

class RankedClassAd {

public:

  RankedClassAd() { Rank=0.0; }
  RankedClassAd(const MyString& oid) { OID=oid; Rank=0.0; }
  RankedClassAd(const MyString& oid, float rank) { OID=oid; Rank=rank; }
  MyString OID;
  float Rank;
  friend bool operator==(const RankedClassAd& RankedAd1, const RankedClassAd& RankedAd2) {
    return ((RankedAd1.OID==RankedAd2.OID) ? true : false);
  }

  friend bool operator!=(const RankedClassAd& RankedAd1, const RankedClassAd& RankedAd2) {
    return ((RankedAd1.OID==RankedAd2.OID) ? false : true);
  }

};

//-------------------------------------------------------------------------
// Collection Hierarchy
//-------------------------------------------------------------------------

typedef Set<MyString> StringSet;
typedef Set<int> IntegerSet;
typedef Set<RankedClassAd> RankedAdSet;

enum CollectionType { ExplicitCollection_e, ConstraintCollection_e, PartitionParent_e, PartitionChild_e };

//-------------------------------------------------------------------------

class BaseCollection {

public:
	virtual ~BaseCollection(){};

  BaseCollection(const MyString& rank) {
    Rank=rank;
  }

  virtual bool CheckClassAd(ClassAd* Ad)=0;

  virtual int Type()=0;

  MyString GetRank() { return Rank; }
  
  IntegerSet Children;
  RankedAdSet Members;

  MyString Rank;

};

//-------------------------------------------------------------------------

class ExplicitCollection : public BaseCollection {

public:
	virtual ~ExplicitCollection(){};

  ExplicitCollection(const MyString& rank, bool fullFlag) 
  : BaseCollection(rank) {
    FullFlag=fullFlag;
  }

  virtual bool CheckClassAd(ClassAd* Ad) {
    return FullFlag;
  }

  virtual int Type() { return ExplicitCollection_e; }

  bool FullFlag;

};

//-------------------------------------------------------------------------

class ConstraintCollection : public BaseCollection {

public:
	virtual ~ConstraintCollection(){};

  ConstraintCollection(const MyString& rank, const MyString& constraint)
  : BaseCollection(rank) {
    Constraint=constraint;
  }
  
  virtual bool CheckClassAd(ClassAd* Ad) {
    ExprTree* tree;
//    EvalResult result;
	Value result;
	ClassAdParser parser;
	bool boolValue;
	int intValue;

//    if (Parse(Constraint.Value(), tree) != 0) {
	if( !parser.ParseExpression( std::string( Constraint.Value( ) ), tree ) ) {
		return false;
    }

//    if (!tree->EvalTree(NULL, Ad, &result)) {
	tree->SetParentScope( Ad );
	if( !Ad->EvaluateExpr( tree, result ) ) {
        delete tree;
        return false;
    }

    delete tree;
//    if (result.type == LX_INTEGER) {
//        return (bool)result.i;
//   }
	if( result.IsBooleanValue( boolValue ) ) {
		return boolValue;
	}
	else if( result.IsIntegerValue( intValue ) ) {
		return (bool)intValue;
	}

    return false;
  }

  virtual int Type() { return ConstraintCollection_e; }

  MyString Constraint;

};

//-------------------------------------------------------------------------

class PartitionParent : public BaseCollection {

public:

	virtual ~PartitionParent(){};
  PartitionParent(const MyString& rank, StringSet& attributes)
  : BaseCollection(rank) {
    Attributes=attributes;
  }

  virtual bool CheckClassAd(ClassAd* Ad) {
    return false;
  }

  virtual int Type() { return PartitionParent_e; }

  StringSet Attributes;

};

//-------------------------------------------------------------------------

class PartitionChild : public BaseCollection {

public:
	virtual ~PartitionChild(){};

  PartitionChild(const MyString& rank, StringSet& values)
  : BaseCollection(rank) {
    Values=values;
  }

  virtual bool CheckClassAd(ClassAd* Ad) {
    return true;
  }

  virtual int Type() { return PartitionChild_e; }

  StringSet Values;

};

//-------------------------------------------------------------------------

#endif
