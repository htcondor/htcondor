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


#ifndef __INTERVAL_H__
#define __INTERVAL_H__

#include <iostream>
#include "classad/classad_distribution.h"
#include "list.h"
#include "extArray.h"


struct Interval
{
	Interval() { key = -1; openLower = openUpper = false; }
	int				key;
	classad::Value 	lower, upper;
	bool			openLower, openUpper;
};

bool Copy( Interval *src, Interval *dest );

bool GetLowValue ( Interval *, classad::Value &result );
bool GetHighValue ( Interval *, classad::Value &result );
bool GetLowDoubleValue ( Interval *, double & );
bool GetHighDoubleValue ( Interval *, double & );

bool Overlaps( Interval *, Interval * );
bool Precedes( Interval *, Interval * );
bool Consecutive( Interval *, Interval * );
bool StartsBefore( Interval *, Interval * );
bool EndsAfter( Interval *, Interval * );

classad::Value::ValueType GetValueType( Interval * );
bool IntervalToString( Interval *, std::string & );

bool Numeric( classad::Value::ValueType );
bool SameType( classad::Value::ValueType vt1, classad::Value::ValueType vt2 );
bool GetDoubleValue ( classad::Value &, double & );
bool EqualValue( classad::Value &, classad::Value & );
bool IncrementValue( classad::Value & );
bool DecrementValue( classad::Value & );


class HyperRect;

class IndexSet
{
 public:
	IndexSet( );
	~IndexSet( );
	bool Init( int _size );
	bool Init( IndexSet & );
	bool AddIndex( int );
	bool RemoveIndex( int );
	bool RemoveAllIndeces( );
	bool AddAllIndeces( );
	bool GetCardinality( int & ) const;
	bool Equals( IndexSet & );
	bool IsEmpty( ) const;
	bool HasIndex( int );
	bool ToString( std::string &buffer );
	bool Union( IndexSet & );
	bool Intersect( IndexSet & );

	static bool Translate( IndexSet &, int *map, int mapSize, int newSize, 
						   IndexSet &result );
	static bool Union( IndexSet &, IndexSet &, IndexSet &result );
	static bool Intersect( IndexSet &, IndexSet &, IndexSet &result );

 private:
	bool initialized;
	int size;
	int cardinality;
	bool *inSet;
};

struct MultiIndexedInterval
{
	MultiIndexedInterval( ) { ival = NULL; }
	Interval *ival;
	IndexSet iSet;
};

class ValueRange
{
 public:
	ValueRange( );
	~ValueRange( );
	bool Init( Interval *, bool undef=false, bool notString=false );
	bool Init2( Interval *, Interval *, bool undef=false );
	bool InitUndef( bool undef=true );
	bool Init( ValueRange *, int index, int numIndeces );
	bool Intersect( Interval *, bool undef=false, bool notString=false );
	bool Intersect2( Interval *, Interval *, bool undef=false );
	bool IntersectUndef( bool undef=true );
	bool Union( ValueRange *, int index );
	bool IsEmpty( );
	bool EmptyOut( );
	bool GetDistance( classad::Value &pt, classad::Value &min,
					  classad::Value &max, double &result,
					  classad::Value &nearestVal );

	static bool BuildHyperRects( ExtArray< ValueRange * > &, int dimensions,
								 int numContexts, 
								 List< ExtArray< HyperRect * > > & );
   
	bool ToString( std::string & );
	bool IsInitialized( ) const;
 private:
	bool initialized;
	classad::Value::ValueType type;
	bool multiIndexed;
	List< MultiIndexedInterval > miiList;
	int numIndeces;
	List< Interval > iList;
	bool anyOtherString;
	IndexSet anyOtherStringIS;
	bool undefined;
	IndexSet undefinedIS;
};


class ValueRangeTable
{
 public:
	ValueRangeTable( );
	~ValueRangeTable( );
	bool Init( int numCols, int numRows );
	bool SetValueRange( int col, int row, ValueRange * );
	bool GetValueRange( int col, int row, ValueRange *& );
	bool GetNumRows( int & ) const;
	bool GetNumColumns( int & ) const;
	bool ToString( std::string &buffer );
 private:
	bool initialized;
	int numCols;
	int numRows;
	ValueRange ***table;
};

class ValueTable
{
 public:
	ValueTable( );
	~ValueTable( );
	bool Init( int numCols, int numRows );
	bool SetOp( int row, classad::Operation::OpKind );
	bool SetValue( int col, int row, classad::Value & );
	bool GetValue( int col, int row, classad::Value & );
	bool GetNumRows( int & ) const;
	bool GetNumColumns( int & ) const;
	bool GetUpperBound( int row, classad::Value & );
	bool GetLowerBound( int row, classad::Value & );
	bool ToString( std::string &buffer );
 private:
	bool initialized;
	int numCols;
	int numRows;
	bool inequality;
	classad::Value ***table;
	Interval **bounds;
	static bool IsInequality( classad::Operation::OpKind );
	static bool OpToString( std::string &buffer, classad::Operation::OpKind );
};

class HyperRect
{
 public:
	HyperRect( );
	~HyperRect( );
	bool Init( int dimensions, int numContexts );
	bool Init( int dimensions, int numContexts, Interval **& );
	bool GetInterval( int dim, Interval *& );
	bool SetIndexSet( IndexSet & );
	bool GetIndexSet( IndexSet & );
	bool FillIndexSet( );
	bool GetDimensions( int & ) const;
	bool GetNumContexts( int & ) const;
	bool ToString( std::string &buffer );
 private:
	// The assignment overload was VERY broken. Make it private and remove the
	// old implementation. If someone uses it, they'll know at
	// compile time, check here, read this comment, be upset, and hopefully
	// implement the correct assignment overload. -psilord
    HyperRect& operator=(const HyperRect & /* copy */);

	bool initialized;
	int dimensions;
	int numContexts;
	IndexSet iSet;
	Interval **ivals;
};


#endif

