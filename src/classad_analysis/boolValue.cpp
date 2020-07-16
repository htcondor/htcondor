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


#include "condor_common.h"
#include "boolValue.h"
#include "list.h"

using namespace std;

// F && (T|F|U|E) = F
// E && (T|F|U|E) = E 
// (T|U) && F = F
// (T|U) && U = U
// (T|U) && E = E
// T && T = T
// U && T = U

bool
And( BoolValue bv1, BoolValue bv2, BoolValue &result )
{	
	if( bv1 == FALSE_VALUE ) { result = FALSE_VALUE; return true; }
	else if( bv1 == ERROR_VALUE ) { result = ERROR_VALUE; return true; }
	else if( bv2 == FALSE_VALUE ) {	result = FALSE_VALUE; return true; }
	else if( bv2 == UNDEFINED_VALUE ) {	result = UNDEFINED_VALUE; return true;}
	else if( bv2 == ERROR_VALUE ) {	result = ERROR_VALUE; return true; }
	else if( bv1 == TRUE_VALUE ) { result = TRUE_VALUE;	return true; }
	else if( bv1 == UNDEFINED_VALUE ) {	result = UNDEFINED_VALUE; return true;}
	else { return false; }
}

// T || (T|F|U|E) = T
// E || (T|F|U|E) = E 
// (F|U) || T = T
// (F|U) || U = U
// (F|U) || E = E
// F || F = F
// U || F = U

bool
Or( BoolValue bv1, BoolValue bv2, BoolValue &result )
{
	if( bv1 == TRUE_VALUE )	{ result = TRUE_VALUE; return true;}
	else if( bv1 == ERROR_VALUE ) { result = ERROR_VALUE; return true;}
	else if( bv2 == TRUE_VALUE ) { result = TRUE_VALUE; return true;}
	else if( bv2 == UNDEFINED_VALUE ) { result = UNDEFINED_VALUE; return true;}
	else if( bv2 == ERROR_VALUE ) { result = ERROR_VALUE; return true;}
	else if( bv1 == FALSE_VALUE ) { result = FALSE_VALUE; return true;}
	else if( bv1 == UNDEFINED_VALUE ) { result = UNDEFINED_VALUE; return true;}
	else  { return false; }
}

bool
Not( BoolValue bv, BoolValue &result )
{
	switch( bv ){
	case TRUE_VALUE:	{ result = FALSE_VALUE; break; }	// !T = F
	case FALSE_VALUE:	{ result = TRUE_VALUE; break; }		// !F = T
	case UNDEFINED_VALUE:									// !U = U
	case ERROR_VALUE:	{ result = bv; }						// !E = E
				//@fallthrough@
	default: { return false; }
	}
	return true;
}

bool
GetChar( BoolValue bv, char &result )
{
	switch( bv ) {
	case TRUE_VALUE:		{ result = 'T'; break; }
	case FALSE_VALUE:	   	{ result = 'F'; break; }
	case UNDEFINED_VALUE: 	{ result = 'U'; break; }
	case ERROR_VALUE: 		{ result = 'E'; break; }
	default: 				{ result = '?'; return false; }
	}
	return true;
}


// BoolVector methods

BoolVector::
BoolVector( )
{
	initialized = false;
	boolvector = NULL;
	length = 0;
	totalTrue = 0;
}

BoolVector::
~BoolVector( )
{
	if( boolvector ) {
		delete [] boolvector;
	}
}

bool BoolVector::
Init( int _length )
{
	if( boolvector ) {
		delete [] boolvector;
	}
	boolvector = new BoolValue[_length];
	length = _length;
	totalTrue = 0;
	initialized = true;
	return true;
}

bool BoolVector::
Init( BoolVector *bv )
{
	if( boolvector) {
		delete [] boolvector;
	}
	boolvector = new BoolValue[bv->length];
	length = bv->length;
	totalTrue = bv->totalTrue;
	for( int i = 0; i < length; i++ ) {
		boolvector[i] = bv->boolvector[i];
	}
	initialized = true;
	return true;
}

bool BoolVector::
SetValue( int index, BoolValue bval )
{
	if( !initialized || 
		index < 0 || index >= length ) {
		return false;
	}
	boolvector[index] = bval;
	if( bval == TRUE_VALUE ) {
		totalTrue++;
	}
	return true;
}

bool BoolVector::
GetValue( int index, BoolValue &result )
{
	if( !initialized || 
		index < 0 || index >= length ) {
		return false;
	}
	result = boolvector[index];
	return true;
}

bool BoolVector::
GetNumValues( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = length;
	return true;
}

bool BoolVector::
GetTotalTrue( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = totalTrue;
	return true;
}


bool BoolVector::
TrueEquals( BoolVector &bv, bool &result )
{
	if(	!initialized || 
		!bv.initialized ||
		length != bv.length ) {
		return false;
	}
	for( int i = 0; i < length; i++ ) {
		if( ( boolvector[i] == TRUE_VALUE &&
			  bv.boolvector[i] != TRUE_VALUE ) ||
			( boolvector[i] != TRUE_VALUE &&
			  bv.boolvector[i] == TRUE_VALUE ) ) {
			result = false;
			return true;
		}
	}
	result = true;
	return true;
}

bool BoolVector::
IsTrueSubsetOf( BoolVector &bv, bool &result )
{
	if( !initialized ||
		!bv.initialized ||
		length != bv.length ) {
		return false;
	}

	for( int i = 0; i < length; i++ ) {
		if( boolvector[i] == TRUE_VALUE &&
			bv.boolvector[i] != TRUE_VALUE ) {
			result = false;
			return true;
		}
	}
	result = true;
	return true;
}

bool BoolVector::
ToString( string &buffer )
{
	if( !initialized ){
		return false;
	}
	char item;
	buffer += '[';
	for( int i = 0; i < length; i++ ) {
		if( i > 0) {
			buffer += ',';
		}
		GetChar( boolvector[i], item );
		buffer += item;
	}
	buffer += ']';
	return true;
}

// AnnotatedBoolVector methods

AnnotatedBoolVector::
AnnotatedBoolVector( )
{
	frequency = 0;
	contexts = NULL;
	numContexts = 0;
}

AnnotatedBoolVector::
~AnnotatedBoolVector( )
{
	if( contexts ) {
		delete [] contexts;
	}
}

bool AnnotatedBoolVector::
Init( int _length, int _numContexts, int _frequency )
{
	if( !BoolVector::Init( _length ) ) {
		return false;
	}
	if( contexts ) {
		delete [] contexts;
	}
	boolvector = new BoolValue[_length];
	numContexts = _numContexts;
	contexts = new bool[_numContexts];
	frequency = _frequency;
	initialized = true;
	return true;
}

bool AnnotatedBoolVector::
SetContext( int index, bool b )
{
	if( !initialized ||
		index < 0 ||
		index >= numContexts ) {
		return false;
	}
	contexts[index] = b;
	return true;
}

bool AnnotatedBoolVector::
GetFrequency( int &result )
{
	if( !initialized ) {
		return false;
	}
	result = frequency;
	return true;
}

bool AnnotatedBoolVector::
HasContext( int index, bool &result )
{
	if( !initialized ||
		index < 0 ||
		index >= numContexts ) {
		return false;
	}
	result = contexts[index];
	return true;
}

bool AnnotatedBoolVector::
ToString( string &buffer )
{
	if( !initialized ) {
		return false;
	}
	char item;
	char tempBuf[512];
	buffer += '[';
	for( int i = 0; i < length; i++ ) {
		if( i > 0) {
			buffer += ',';
		}
		GetChar( boolvector[i], item );
		buffer += item;
	}
	buffer += ']';
	buffer += ':';
	sprintf( tempBuf, "%d", frequency );
	buffer += tempBuf;
	buffer += ':';
	buffer += '{';
	bool firstItem = true;
	for( int i = 0; i < numContexts; i++ ) {
		if( contexts[i] ) {
			if( firstItem ) {
				firstItem = false;
			} else {
				buffer += ',';
			}
			sprintf( tempBuf, "%d", i );
			buffer += tempBuf;
		}
	}
	buffer += '}';
	return true;
}


bool AnnotatedBoolVector::
MostFreqABV( List<AnnotatedBoolVector> &abvList, AnnotatedBoolVector *&result )
{
	int maxFreq = 0;
	int currentFreq = 0;
	AnnotatedBoolVector *curr;

		// find the maximum frequency
		// return the first abv with the maximum frequency
		// NOTE: explicit copy should probably be used here.
	abvList.Rewind();
	while( abvList.Next( curr ) ) {
		currentFreq = curr->frequency;
		if( currentFreq > maxFreq ) {
			maxFreq = currentFreq;
			result = curr;
		}
	}
	return true;
}


// BoolTable methods

BoolTable::
BoolTable( )
{
	initialized = false;
	numCols = 0;
	numRows = 0;
	colTotalTrue = NULL;
	rowTotalTrue = NULL;
	table = NULL;
}

BoolTable::
~BoolTable( )
{
	if( colTotalTrue ) {
		delete [] colTotalTrue;
	}
	if( rowTotalTrue ) {
		delete [] rowTotalTrue;
	}
	if( table ) {
		for( int i = 0; i < numCols; i++ ){
			delete [] table[i];
		}
		delete [] table;
	}
}

bool BoolTable::
Init( int _numCols, int _numRows )
{
	if( colTotalTrue ) {
		delete [] colTotalTrue;
	}
	if( rowTotalTrue ) {
		delete [] rowTotalTrue;
	}
	if( table ) {
		for( int i = 0; i < numCols; i++ ){
			delete [] table[i];
		}
		delete [] table;
	}

	numCols = _numCols;
	numRows = _numRows;

	colTotalTrue = new int[_numCols];
	rowTotalTrue = new int[_numRows];
	table = new BoolValue*[_numCols];
		//table = ( BoolValue ** )new BoolValue[_numCols][_numRows]
	for( int col = 0; col < _numCols; col++ ) {
		table[col] = new BoolValue[_numRows];
		for( int row = 0; row < _numRows; row++ ) {
			table[col][row] = FALSE_VALUE;
		}
	}
	for( int col = 0; col < _numCols; col++ ) {
		colTotalTrue[col] = 0;
	}
	for( int row = 0; row < _numRows; row++ ) {
		rowTotalTrue[row] = 0;
	}
	initialized = true;
    return true;
}

bool BoolTable::
SetValue( int col, int row, BoolValue bval )
{
	if( !initialized ||
		col >= numCols || row >= numRows || col < 0 || row < 0 ) {
		return false;
	}
	table[col][row] = bval;
	if( bval == TRUE_VALUE ) {
		rowTotalTrue[row]++;
		colTotalTrue[col]++;
	}
	return true;
}

bool BoolTable::
GetValue( int col, int row, BoolValue &result )
{
	if( !initialized || 
		col >= numCols || row >= numRows || col < 0 || row < 0 ) {
		return false;
	}
	result = table[col][row];
	return true;
}
	
bool BoolTable::
GetNumRows( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numRows;
	return true;
}

bool BoolTable::
GetNumColumns( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numCols;
	return true;
}

bool BoolTable::
RowTotalTrue( int row, int &result )
{	
	if( !initialized ||
		row < 0 || row >= numRows ){
		return false;
	}
	result = rowTotalTrue[row];
	return true;
}

bool BoolTable::
ColumnTotalTrue( int col, int &result )
{
	if( !initialized ||
		col < 0 || col >= numCols ){
		return false;
	}
	result = colTotalTrue[col];
	return true;
}


bool BoolTable::
AndOfRow( int row, BoolValue &result )
{
	if( !initialized ||
		row < 0 || row >= numRows ){
		return false;
	}
	BoolValue bval = TRUE_VALUE;
	for( int col = 0; col < numCols; col++ ) {
		if( !And( bval, table[col][row], bval ) ){
			return false;
		}
	}
	result = bval;
	return true;
}

bool BoolTable::
AndOfColumn( int col, BoolValue &result )
{
	if( !initialized ||
		col < 0 || col >= numCols ){
		return false;
	}
	BoolValue bval = TRUE_VALUE;
	for( int row = 0; row < numRows; row++ ) {
		if( !And( bval, table[col][row], bval ) ) {
			return false;
		}
	}
	result = bval;
	return true;
}

bool BoolTable::
OrOfRow( int row, BoolValue &result )
{
	if( !initialized ||
		row < 0 || row >= numRows ){
		return false;
	}
	BoolValue bval = FALSE_VALUE;
	for( int col = 0; col < numCols; col++ ) {
		if( !Or( bval, table[col][row], bval ) ) {
			return false;
		}
	}
	result = bval;
	return true;
}

bool BoolTable::
OrOfColumn( int col, BoolValue &result )
{
	if( !initialized ||
		col < 0 || col >= numCols ){
		return false;
	}
	BoolValue bval = FALSE_VALUE;
	for( int row = 0; row < numRows; row++ ) {
		if( !Or( bval, table[col][row], bval ) ) {
			return false;
		}
	}
	result = bval;
	return true;
}



bool BoolTable::
GenerateMaxTrueABVList( List<AnnotatedBoolVector> &result )
{
	if( !initialized ) {
		return false;
	}

	AnnotatedBoolVector *abv;
	int frequency = 0;
	bool *seen = new bool[numCols];
	bool *tempContexts = new bool[numCols];
	for( int col = 0; col < numCols; col ++ ) {
		seen[col] = false;
		tempContexts[col] = false;
	}
	bool hasCommonTrue = false;

		// find maximum colTotalTrue value
	int maxTotalTrue = 0;
	for( int i = 0; i < numCols; i++ ) {
	    if( colTotalTrue[i] > maxTotalTrue ) {
			maxTotalTrue = colTotalTrue[i];
	    }
	}

		// only create ABVs for cols with max colTotalTrue values
	for( int i = 0; i < numCols; i++ ) {
	    if( colTotalTrue[i] == maxTotalTrue ) { // check initial 
			if( !seen[i] ) {
				frequency = 1;
				tempContexts[i] = true;
				for( int j = i + 1; j < numCols; j++ ) {
					if( colTotalTrue[j] == maxTotalTrue ) { // check compare
						if( !seen[j] ){
							CommonTrue( i, j, hasCommonTrue );
							if( hasCommonTrue ) {
								frequency++;
								seen[j] = true;
								tempContexts[j] = true;
							}
						}
					}
				}
				abv = new AnnotatedBoolVector;
				abv->Init( numRows, numCols, frequency );
				for( int row = 0; row < numRows; row++ ) {
					abv->SetValue( row, table[i][row] );
				}
				for( int col = 0; col < numCols; col++ ) {
					abv->SetContext( col, tempContexts[col] );
					tempContexts[col] = false;
				}
				result.Append( abv );
			}
		}
	}
    delete [] seen;
    delete [] tempContexts;
	return true;
}

bool BoolTable::
GenerateMaximalTrueBVList( List< BoolVector > &result )
{
	BoolVector *newBV = NULL;
	BoolVector *oldBV = NULL;
	for( int i = 0; i < numCols; i++ ) {
		newBV = new BoolVector( );
		newBV->Init( numRows );
		for( int row = 0; row < numRows; row++ ) {
			newBV->SetValue( row, table[i][row] );
		}
		result.Rewind( );
		bool addBV = true;
		bool isSubset = false;
		while( result.Next( oldBV ) ) {
			newBV->IsTrueSubsetOf( *oldBV, isSubset );
			if( isSubset ) {
				addBV = false;
				break;
			}
			oldBV->IsTrueSubsetOf( *newBV, isSubset );
			if( isSubset ) { 
				result.DeleteCurrent( );
			}
		}
		if( addBV ) {
			result.Append( newBV );
		} else {
			delete newBV;
		}
	}
	return true;
}

bool BoolTable::
GenerateMinimalFalseBVList( List< BoolVector > &result )
{
	List< BoolVector > *baseBVList = new List< BoolVector >( );
	List< BoolVector > *oldBVList = new List< BoolVector >( );
	List< BoolVector > *newBVList = new List< BoolVector >( );
	BoolVector *bv = NULL;
	BoolValue bval = FALSE_VALUE;
	GenerateMaximalTrueBVList( *baseBVList );
	if( baseBVList->IsEmpty( ) ) {
		delete baseBVList;
		delete oldBVList;
		delete newBVList;
		return true;
	}

		// NEGATE
	baseBVList->Rewind( );
	while( baseBVList->Next( bv ) ) {
		for( int i = 0; i < numRows; i++ ) {
			bv->GetValue( i, bval );
			if( bval == TRUE_VALUE ) {
				bv->SetValue( i, FALSE_VALUE );
			}
			else {
				bv->SetValue( i, TRUE_VALUE );
			}
		}
	}

		// DISTRIB & COMBINE
	BoolVector *baseBV = NULL;
	BoolVector *newBV = NULL;
	BoolVector *oldBV = NULL;
	baseBVList->Rewind( );

	while( baseBVList->Next( baseBV ) ) {
		for( int i = 0; i < numRows; i++ ) {
			baseBV->GetValue( i, bval );
			if( bval == TRUE_VALUE ) {
				if( oldBVList->IsEmpty( ) ) {
					newBV = new BoolVector( );
					newBV->Init( numRows );
					for( int j = 0; j < numRows; j++ ) {
						if( i == j ) {
							newBV->SetValue( j, TRUE_VALUE );
						}
						else {
							newBV->SetValue( j, FALSE_VALUE );
						}
					}
					newBVList->Append( newBV );
				}
				else {
					oldBVList->Rewind( );
					while( oldBVList->Next( oldBV ) ) {
						newBV = new BoolVector( );
						newBV->Init( oldBV );
						newBV->SetValue( i, TRUE_VALUE );
						newBVList->Append( newBV );
					}
				}
			}
		}
		oldBVList->Rewind( );
		while( oldBVList->Next( oldBV ) ) {
			delete oldBV;
		}
		delete oldBVList;
		oldBVList = newBVList;
		newBVList = new List< BoolVector >( );
	}

		// PRUNE
	oldBVList->Rewind( );
	while( oldBVList->Next( newBV ) ) {
		result.Rewind( );
		bool addBV = true;
		bool isSubset = false;
		while( result.Next( oldBV ) ) {
			oldBV->IsTrueSubsetOf( *newBV, isSubset );
			if( isSubset ) {
				addBV = false;
				break;
			}
			newBV->IsTrueSubsetOf( *oldBV, isSubset );
			if( isSubset ) { 
				result.DeleteCurrent( );
			}
		}
		if( addBV ) {
			result.Append( newBV );
		}
		else delete newBV;
	}

	baseBVList->Rewind( );
	while( baseBVList->Next( oldBV ) ) {
		delete oldBV;
	}
	delete baseBVList;
	delete newBVList;
	delete oldBVList;
	return true;
}

bool BoolTable::
ToString( string &buffer )
{
	if( !initialized ) {
		return false;
	}
	char item;
	char tempBuf[512];
	
	sprintf( tempBuf, "%d", numCols );
	buffer += "numCols = ";
	buffer += tempBuf;
	buffer += "\n";

	sprintf( tempBuf, "%d", numRows );
	buffer += "numRows = ";
	buffer += tempBuf;
	buffer += "\n";

	for( int row = 0; row < numRows; row++ ) {
		for( int col = 0; col < numCols; col++ ) {
			GetChar( table[col][row], item );
			buffer += item;
		}
		sprintf( tempBuf, "%d", rowTotalTrue[row] );
		buffer += " ";
		buffer += tempBuf;
		buffer += "\n";
	}
	for( int col = 0; col < numCols; col++ ) {
		sprintf( tempBuf, "%d", colTotalTrue[col] );
		buffer += tempBuf;
	}
	buffer += "\n";
	return true;
}

bool BoolTable::
CommonTrue( int col1, int col2, bool &result ) {
	for( int i = 0; i < numRows; i++ ) {
		if( ( table[col1][i] == TRUE_VALUE &&
			  table[col2][i] != TRUE_VALUE ) ||
			( table[col1][i] != TRUE_VALUE &&
			  table[col2][i] == TRUE_VALUE ) ) {
			result = false;
			return true;
		}
	}
	result = true;
	return true;
}
