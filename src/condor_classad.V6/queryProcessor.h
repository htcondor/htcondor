/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

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
	bool InitializeIndexes( Rectangles&, bool useSummaries=true );
	void ClearIndexes( );
	bool DoQuery( Rectangles&, KeySet& );
	void PurgeRectangle( int rId );
	bool MapRectangleID( int srId, int &rId, int &portNum, int &pId, int &cId );
	bool MapPortID( int pId, int &portNum, int &cId );
	bool UnMapClassAdID( int cId, int &brId, int &erId );
	void Display( FILE* );

	static int numQueries;
//private:
	void DumpQState( QueryOutcome & );
	int				numRectangles, numSummaries;
	Rectangles		*rectangles, summaries;

	Representatives	reps;
	Constituents	consts;
	KeyMap			constRepMap;
	bool			summarize;

		// only indexes here; deviant/absent info in rectangles/summaries
	Indexes			imported, exported;
};


class Query {
public:
	~Query( );
	Query *MakeQuery( Rectangles* );
	Query *MakeConjunctQuery( Query*, Query* );
	Query *MakeDisjunctQuery( Query*, Query* );
	bool   RunQuery( QueryProcessor&, KeySet& );
private:
	Query( );
	enum Op { IDENT, AND, OR };
	Op			op;
	Query		*left, *right;
	Rectangles 	*rectangles;
};


	
//----------------------------------------------------------------------------

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
