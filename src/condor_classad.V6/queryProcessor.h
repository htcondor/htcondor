#if !defined(QP_H)
#define QP_H

#include "rectangle.h"
#include <set>

class ClassAdIndex;
class IntervalTree;
typedef map<string,ClassAdIndex*,CaseIgnLTStr> Indexes;
typedef set<int> IndexEntries;
typedef map<int, KeySet> QueryOutcome;

class QueryProcessor {
public:
	QueryProcessor( );
	~QueryProcessor( );

	bool InitializeIndexes( Rectangles& );
	void ClearIndexes( );
	bool DoQuery( Rectangles&, KeySet&, KeySet& );
	void PurgeIndexEntries( int rId );
	bool MapRectangleID( int rId, int &portNum, int &pId, int &cId );
	bool UnMapClassAdID( int cId, int &brId, int &erId );
	void Display( FILE* );


private:
	void DumpQState( QueryOutcome &, QueryOutcome & );

	int				numRectangles;
	Indexes			imported, exported;
	Rectangles		*rectangles;
	set<int>		verifyExported;
	DimRectangleMap	verifyImported;
	DimRectangleMap	unexported;
};



class ClassAdIndex {
public:
	ClassAdIndex( );
	virtual ~ClassAdIndex( );
	virtual bool Delete( int, const Interval& )=0;
	virtual bool Filter( const Interval&, KeySet& )=0;
	virtual bool FilterAll( KeySet& )=0;
};

class ClassAdNumericIndex : public ClassAdIndex {
public:
	virtual ~ClassAdNumericIndex( );
	static ClassAdNumericIndex *MakeNumericIndex( OneDimension& );
	virtual bool Delete( int, const Interval& );
	virtual bool Filter( const Interval&, KeySet& );
	virtual bool FilterAll( KeySet& );

private:
	ClassAdNumericIndex( );
	IntervalTree	*intervalTree;
};

typedef map<string, IndexEntries, CaseIgnLTStr> StringIndex;
class ClassAdStringIndex : public ClassAdIndex {
public:
	virtual ~ClassAdStringIndex( );
	static ClassAdStringIndex *MakeStringIndex( OneDimension& );
	virtual bool Delete( int, const Interval& );
	virtual bool Filter( const Interval&, KeySet& );
	virtual bool FilterAll( KeySet& );
private:
	ClassAdStringIndex( );
	StringIndex	stringIndex;
};


class ClassAdBooleanIndex : public ClassAdIndex {
public:
	virtual ~ClassAdBooleanIndex( );
	static ClassAdBooleanIndex *MakeBooleanIndex( OneDimension& );
	virtual bool Delete( int, const Interval& );
	virtual bool Filter( const Interval&, KeySet& );
	virtual bool FilterAll( KeySet& );
private:
	ClassAdBooleanIndex( );
	IndexEntries	yup, nope;
};


#endif
