/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

#ifndef __BOOL_VALUE_H__
#define __BOOL_VALUE_H__

#include "list.h"
#include <string>
using namespace std;

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
	bool GetNumValues( int &result );
	bool GetTotalTrue( int &result );
	bool TrueEquals( BoolVector &, bool &result );
	bool IsTrueSubsetOf( BoolVector &, bool &result );
	bool ToString( string &buffer );
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
	bool ToString( string &buffer ); 
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
	bool GetNumRows( int &result );
	bool GetNumColumns(int &result );
	bool RowTotalTrue( int row, int &result );
	bool ColumnTotalTrue( int col, int &result );
	bool AndOfRow( int row, BoolValue &result );
	bool AndOfColumn( int col, BoolValue &result );
	bool OrOfRow( int row, BoolValue &result );
	bool OrOfColumn( int col, BoolValue &result );
	bool GenerateABVList( List<AnnotatedBoolVector> &result);
	bool GenerateMaxTrueABVList( List<AnnotatedBoolVector> &result );
	bool GenerateMaximalTrueBVList( List< BoolVector > &result );
	bool GenerateMinimalFalseBVList( List< BoolVector > &result );
	bool ToString( string &buffer );
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


