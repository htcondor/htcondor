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


#if !defined(QP_H)
#define QP_H

#include "rectangle.h"
#include <set>

class ClassAdIndex;
class IntervalTree;
typedef std::map<std::string,ClassAdIndex*,classad::CaseIgnLTStr> Indexes;
typedef std::set<int> IndexEntries;
typedef std::map<int, KeySet> QueryOutcome;

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

typedef std::map<std::string, IndexEntries, classad::CaseIgnLTStr> StringIndex;
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
