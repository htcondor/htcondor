#ifndef _Collections_H
#define _Collections_H

//-------------------------------------------------------------------------

#include "condor_classad.h"
#include "Set.h"
#include "MyString.h"

int partitionHashFcn( const MyString &, int );

//-------------------------------------------------------------------------

class RankedClassAd {
public:
	RankedClassAd() { Rank=0.0; }
	RankedClassAd(const MyString& oid) { OID=oid; Rank=0.0; }
	RankedClassAd(const MyString& oid, float rank) { OID=oid; Rank=rank; }

	MyString OID;
	float Rank;

	friend bool operator==(const RankedClassAd& RankedAd1, 
		const RankedClassAd& RankedAd2) 
	{
		return ((RankedAd1.OID==RankedAd2.OID) ? true : false);
	}

	friend bool operator!=(const RankedClassAd& RankedAd1, 
		const RankedClassAd& RankedAd2) 
	{
		return ((RankedAd1.OID==RankedAd2.OID) ? false : true);
	}
};

//-------------------------------------------------------------------------
// Collection Hierarchy
//-------------------------------------------------------------------------

typedef Set<MyString> StringSet;
typedef Set<int> IntegerSet;
typedef Set<RankedClassAd> RankedAdSet;

enum CollectionType { 
	ExplicitCollection_e, 
	ConstraintCollection_e, 
	PartitionParent_e, 
	PartitionChild_e 
};

//-------------------------------------------------------------------------
class CollChildIterator;
class CollContentIterator;

class BaseCollection {
public:

	BaseCollection(const MyString& rank);
	BaseCollection(ExprTree *tree);
	virtual ~BaseCollection( );

	virtual bool CheckClassAd(ClassAd* Ad)=0;
	virtual int Type()=0;

	ExprTree *GetRankExpr();
	double GetRankValue( ClassAd* ad );

		// iterator management
	void RegisterChildItor( CollChildIterator* );
	void UnregisterChildItor( CollChildIterator* );
	void RegisterContentItor( CollContentIterator* );
	void UnregisterContentItor( CollContentIterator* );

		// tables of active iterators
	ExtArray<CollChildIterator*> 	childItors;
	ExtArray<CollContentIterator*> 	contentItors;

	IntegerSet 		Children;
	RankedAdSet 	Members;
	MatchClassAd 	rankCtx;
};

//-------------------------------------------------------------------------

class ExplicitCollection : public BaseCollection {
public:

	ExplicitCollection(const MyString& rank, bool fullFlag) 
		: BaseCollection(rank) 
	{
		FullFlag=fullFlag;
	}

	ExplicitCollection(ExprTree *rank, bool fullFlag) 
		: BaseCollection(rank) 
	{
		FullFlag=fullFlag;
	}

	virtual bool CheckClassAd(ClassAd*) { return FullFlag; }
	virtual int Type() { return ExplicitCollection_e; }

	bool FullFlag;
};

//-------------------------------------------------------------------------

class ConstraintCollection : public BaseCollection {
public:
	ConstraintCollection(const MyString& rank, const MyString& constraint);
	ConstraintCollection(ExprTree *rank, ExprTree *constraint);
	virtual bool CheckClassAd(ClassAd* Ad);

	virtual int Type() { return ConstraintCollection_e; }

	ExprTree 	*constraint;
	MatchClassAd constraintCtx;
};

//-------------------------------------------------------------------------

class PartitionParent : public BaseCollection {
public:

	PartitionParent(const MyString& rank, StringSet& attributes)
		: BaseCollection(rank), childPartitions( 27, partitionHashFcn )
	{
		Attributes=attributes;
	}

	PartitionParent(ExprTree *rank, StringSet& attributes)
		: BaseCollection(rank), childPartitions( 27, partitionHashFcn )
	{
		Attributes=attributes;
	}

	virtual bool CheckClassAd(ClassAd*) { return false; }
	virtual int Type() { return PartitionParent_e; }

	StringSet Attributes;
	HashTable<MyString,int> childPartitions;
};

//-------------------------------------------------------------------------

class PartitionChild : public BaseCollection {
public:

	PartitionChild(ExprTree *rank, MyString& values) :BaseCollection(rank) 
	{
		PartitionValues=values;
	}

	virtual bool CheckClassAd(ClassAd*) { return true; }
	virtual int Type() { return PartitionChild_e; }

	MyString PartitionValues;
};

//-------------------------------------------------------------------------

enum CollItorStatus {
	COLL_NIL_STATUS		= 0,
	COLL_ITOR_OK 		= ( 1 << 0 ),
	COLL_BEFORE_START	= ( 1 << 1 ),
	COLL_AT_END	 		= ( 1 << 2 ),
	COLL_ITOR_MOVED		= ( 1 << 3 ),
	COLL_ITEM_ADDED 	= ( 1 << 4 ),
	COLL_ITEM_REMOVED 	= ( 1 << 5 ),
	COLL_ITOR_INVALID	= ( 1 << 6 )
};

class ClassAdCollection;

class CollChildIterator {
friend class BaseCollection;
public:
	CollChildIterator( );
	CollChildIterator( const CollChildIterator & );
	~CollChildIterator( );

	int CurrentCollection( int & );
	int NextCollection( int & );
	int GetItorStatus( ) const { return status; }
	bool IteratorOk( ) const { return( status & COLL_ITOR_OK ); }
	bool AtEnd( ) const { return( status & COLL_AT_END ); }
	bool IteratorMoved( ) const { return( status & COLL_ITOR_MOVED ); }
	bool CollectionsAdded( ) const { return( status & COLL_ITEM_ADDED ); }
	bool CollectionsRemoved( ) const { return( status & COLL_ITEM_REMOVED ); }
	bool IsInvalid( ) const { return( status & COLL_ITOR_INVALID ); }

private:
	void initialize(ClassAdCollection*,BaseCollection*,int,const IntegerSet&);
	void updateForInsertion( );
	void updateForDeletion( int );
	void invalidate( );

	int	 				collID;
	BaseCollection		*baseCollection;
	ClassAdCollection	*collManager;
	SetIterator<int> 	itor;
	int					status;
};


class CollContentIterator {
friend class BaseCollection;
public:
	CollContentIterator( );
	CollContentIterator( const CollContentIterator & );
	~CollContentIterator( );

	int CurrentAd( const ClassAd *&ad ) const;
	int CurrentAdKey( char *key ) const;
	int CurrentAdRank( double &rank ) const;
	int NextAd( const ClassAd *&ad );
	bool IteratorOk( ) const { return( status & COLL_ITOR_OK ); }
	bool AtEnd( ) const { return( status & COLL_AT_END ); }
	bool IteratorMoved( ) const { return( status & COLL_ITOR_MOVED ); }
	bool ClassAdsAdded( ) const { return( status & COLL_ITEM_ADDED ); }
	bool ClassAdsRemoved( ) const { return( status & COLL_ITEM_REMOVED ); }
	bool IsInvalid( ) const { return( status & COLL_ITOR_INVALID ); }

private:
	void initialize(ClassAdCollection*,BaseCollection*,int,const RankedAdSet&);
	void updateForInsertion( );
	void updateForDeletion( const RankedClassAd & );
	void invalidate( );

	int							collID;
	BaseCollection 				*baseCollection;
	ClassAdCollection			*collManager;
	SetIterator<RankedClassAd> 	itor;
	int							status;
};

#endif
