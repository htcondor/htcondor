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


#ifndef __BOOL_VALUE_H__
#define __BOOL_VALUE_H__

#include "list.h"
#include <string>

/// List of possible values in the ClassAd 4-value logic
enum BoolValue
{
		/// True
	TRUE_VALUE,
		/// False
	FALSE_VALUE,
		/// Undefined
	UNDEFINED_VALUE,
		/// Error
    ERROR_VALUE
};

bool And( BoolValue, BoolValue, BoolValue &result );
bool Or( BoolValue, BoolValue, BoolValue &result );
bool Not( BoolValue, BoolValue, BoolValue &result );
bool GetChar( BoolValue, char &result );

class BoolVector
{
 public:
	BoolVector( );
	virtual ~BoolVector( );
	bool Init( int _length );
	bool Init( BoolVector *bv );
	bool SetValue( int index, BoolValue bval );
	bool GetValue( int index, BoolValue &result );
	bool GetNumValues( int &result ) const;
	bool GetTotalTrue( int &result ) const;
	bool TrueEquals( BoolVector &, bool &result );
	bool IsTrueSubsetOf( BoolVector &, bool &result );
	bool ToString( std::string &buffer );
 protected:
	bool initialized;
	BoolValue* boolvector;
	int length;
	int totalTrue;
};

class AnnotatedBoolVector : public BoolVector
{
 public:
	AnnotatedBoolVector( );
	~AnnotatedBoolVector( );
	bool Init( int length, int numContexts, int _frequency );
	bool SetContext( int index, bool b );
	bool GetFrequency( int &result );
	bool HasContext( int index, bool &result );
	bool ToString( std::string &buffer ); 
	static bool MostFreqABV( List<AnnotatedBoolVector> &,
							 AnnotatedBoolVector *&result);
 private:
	int frequency;
	bool* contexts;
	int numContexts;
};

class BoolTable
{
 public:
	BoolTable( );
	~BoolTable( );
	bool Init( int numCols, int numRows );
	bool SetValue( int col, int row, BoolValue bval );
	bool GetValue( int col, int row, BoolValue &result );
	bool GetNumRows( int &result ) const;
	bool GetNumColumns(int &result ) const;
	bool RowTotalTrue( int row, int &result );
	bool ColumnTotalTrue( int col, int &result );
	bool AndOfRow( int row, BoolValue &result );
	bool AndOfColumn( int col, BoolValue &result );
	bool OrOfRow( int row, BoolValue &result );
	bool OrOfColumn( int col, BoolValue &result );
	bool GenerateMaxTrueABVList( List<AnnotatedBoolVector> &result );
	bool GenerateMaximalTrueBVList( List< BoolVector > &result );
	bool GenerateMinimalFalseBVList( List< BoolVector > &result );
	bool ToString( std::string &buffer );
 private:
	bool initialized;
	int numCols;
	int numRows;
	int* colTotalTrue;
	int* rowTotalTrue;
	BoolValue **table;
	bool CommonTrue( int col1, int col2, bool &result );
};

#endif	// __BOOL_VALUE_H__


