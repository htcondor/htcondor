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
#include "interval.h"

#include <iostream>

using namespace std;

// Interval methods

bool
Copy( Interval *src, Interval *dest )
{
	if( src == NULL || dest == NULL ) {
		cerr << "Copy: tried to pass null pointer" << endl;
		return false;
	}
	
	dest->key = src->key;
	dest->openUpper = src->openUpper;
	dest->openLower = src->openLower;
	dest->upper.CopyFrom( src->upper );
	dest->lower.CopyFrom( src->lower );
	return true;
}

bool
GetLowValue ( Interval * i, classad::Value &result )
{
	if( i == NULL ) {
		cerr << "GetLowValue: input interval is NULL" << endl;
		return false;
	}

	result.CopyFrom( i->lower );
	return true;
}

bool
GetHighValue ( Interval * i, classad::Value &result )
{
	if( i == NULL ) {
		cerr << "GetHighValue: input interval is NULL" << endl;
		return false;
	}

	result.CopyFrom( i->upper );
	return true;
}

bool
GetLowDoubleValue ( Interval * i, double &result )
{
	if( i == NULL ) {
		cerr << "GetLowDoubleValue: input interval is NULL"
			 << endl;
		return false;
	}

	double low;
	time_t lowTime;
	classad::abstime_t lowAbsTime;
	if( i->lower.IsNumber( low ) ) {
		result = low;
		return true;
	} else if( i->lower.IsAbsoluteTimeValue( lowAbsTime ) ) {
		result = ( double )lowAbsTime.secs;
		return true;
	} else if( i->lower.IsRelativeTimeValue( lowTime ) ) {
		result = ( double )lowTime;
		return true;
	}
	return false;
}

bool
GetHighDoubleValue ( Interval * i, double &result )
{
	if( i == NULL ) {
		cerr << "GetHighDoubleValue: input interval is NULL"
			 << endl;
		return false;
	}

	double high;
	time_t highTime;
	classad::abstime_t highAbsTime;
	if( i->upper.IsNumber( high ) ) {
		result = high;
		return true;
	} else if( i->upper.IsAbsoluteTimeValue( highAbsTime ) ) {
		result = (double )highAbsTime.secs;
		return true;
	} else if(i->upper.IsRelativeTimeValue( highTime ) ) {
		result = ( double )highTime;
		return true;
	}
	return false;

}

bool
Overlaps( Interval *i1, Interval *i2 )
{
	if( i1 == NULL || i2 == NULL ) {
		cerr << "Overlaps: input interval is NULL" << endl;
		return false;
	}

		// sanity check
	classad::Value::ValueType vt1, vt2;
	vt1 = GetValueType( i1 );
	vt2 = GetValueType( i2 );
	if( ( vt1 != vt2 ) && ( !Numeric( vt1 ) || !Numeric( vt2 ) ) ) {
		return false;
	}
	else if ( vt1 != classad::Value::RELATIVE_TIME_VALUE &&
			  vt1 != classad::Value::ABSOLUTE_TIME_VALUE &&
			  !Numeric( vt1) ) {
		return false;
	}

	double low1, high1, low2, high2;

	GetLowDoubleValue( i1, low1 );
	GetHighDoubleValue( i1, high1 );
	GetLowDoubleValue( i2, low2 );
	GetHighDoubleValue( i2, high2 );
	if( ( low1 > high2 ) || 
		( ( low1 == high2 ) && ( i1->openLower || i2->openUpper ) ) ||
		( high1 < low2 ) ||
		( ( high1 == low2 ) && ( i1->openUpper || i2->openLower ) ) ) {
		return false;
	}
	else{
		return true;
	}
}

bool
Precedes( Interval *i1, Interval *i2 )
{
	if( i1 == NULL || i2 == NULL ) {
		cerr << "Precedes: input interval is NULL" << endl;
		return false;
	}

		// sanity check
	classad::Value::ValueType vt1, vt2;
	vt1 = GetValueType( i1 );
	vt2 = GetValueType( i2 );
	if( ( vt1 != vt2 ) && ( !Numeric( vt1 ) || !Numeric( vt2 ) ) ) {
		return false;
	}
	else if ( vt1 != classad::Value::RELATIVE_TIME_VALUE &&
			  vt1 != classad::Value::ABSOLUTE_TIME_VALUE &&
			  !Numeric( vt1) ) {
		return false;
	}

	double low1, high1, low2, high2;

	GetLowDoubleValue( i1, low1 );
	GetHighDoubleValue( i1, high1 );
	GetLowDoubleValue( i2, low2 );
	GetHighDoubleValue( i2, high2 );
	if( ( high1 < low2 ) ||
		( ( high1 == low2 ) && ( i1->openUpper || i2->openLower ) ) ) {
	    return true;
	}
	else{
	    return false;
	}

}

bool
Consecutive( Interval *i1, Interval *i2 )
{
	if( i1 == NULL || i2 == NULL ) {
		cerr << "Consecutive: input interval is NULL" << endl;
		return false;
	}

		// sanity check
	classad::Value::ValueType vt1, vt2;
	vt1 = GetValueType( i1 );
	vt2 = GetValueType( i2 );
	if( ( vt1 != vt2 ) && ( !Numeric( vt1 ) || !Numeric( vt2 ) ) ) {
		return false;
	}
	else if ( vt1 != classad::Value::RELATIVE_TIME_VALUE &&
			  vt1 != classad::Value::ABSOLUTE_TIME_VALUE &&
			  !Numeric( vt1) ) {
		return false;
	}

	double low1, high1, low2, high2;

	GetLowDoubleValue( i1, low1 );
	GetHighDoubleValue( i1, high1 );
	GetLowDoubleValue( i2, low2 );
	GetHighDoubleValue( i2, high2 );
	if( ( high1 == low2 ) && (i1->openUpper != i2->openLower)){
	    return true;
	}
	else{
	    return false;
	}

}

bool
StartsBefore( Interval *i1, Interval *i2 )
{
	if( i1 == NULL || i2 == NULL ) {
		cerr << "Precedes: input interval is NULL" << endl;
		return false;
	}

		// sanity check
	classad::Value::ValueType vt1, vt2;
	vt1 = GetValueType( i1 );
	vt2 = GetValueType( i2 );
	if( ( vt1 != vt2 ) && ( !Numeric( vt1 ) || !Numeric( vt2 ) ) ) {
		return false;
	}
	else if ( vt1 != classad::Value::RELATIVE_TIME_VALUE &&
			  vt1 != classad::Value::ABSOLUTE_TIME_VALUE &&
			  !Numeric( vt1) ) {
		return false;
	}

	double low1, low2;

	GetLowDoubleValue( i1, low1 );
	GetLowDoubleValue( i2, low2 );
	if( ( low1 < low2 ) ||
		( ( low1 == low2 ) && !i1->openLower && i2->openLower ) ) {
	    return true;
	}
	else{
	    return false;
	}

}

bool
EndsAfter( Interval *i1, Interval *i2 )
{
	if( i1 == NULL || i2 == NULL ) {
		cerr << "Precedes: input interval is NULL" << endl;
		return false;
	}

		// sanity check
	classad::Value::ValueType vt1, vt2;
	vt1 = GetValueType( i1 );
	vt2 = GetValueType( i2 );
	if( ( vt1 != vt2 ) && ( !Numeric( vt1 ) || !Numeric( vt2 ) ) ) {
		return false;
	}
	else if ( vt1 != classad::Value::RELATIVE_TIME_VALUE &&
			  vt1 != classad::Value::ABSOLUTE_TIME_VALUE &&
			  !Numeric( vt1) ) {
		return false;
	}

	double high1, high2;

	GetHighDoubleValue( i1, high1 );
	GetHighDoubleValue( i2, high2 );
	if( ( high1 > high2 ) ||
		( ( high1 == high2 ) && !i1->openUpper && i2->openUpper ) ) {
	    return true;
	}
	else{
	    return false;
	}

}

classad::Value::ValueType
GetValueType( Interval * i )
{
	if( i == NULL ) {
		cerr << "GetValueType: input interval is NULL" << endl;
		return classad::Value::NULL_VALUE;
	}
	
	classad::Value::ValueType lowerType, upperType;
	lowerType = i->lower.GetType( );
	if( lowerType == classad::Value::STRING_VALUE ||
		lowerType == classad::Value::BOOLEAN_VALUE ) {
		return lowerType;
	}

	upperType = i->upper.GetType( );

	if( upperType == lowerType ) {
		return upperType;
	}

	double d;
	if( i->lower.IsRealValue( d ) && d == -( FLT_MAX ) ) {  // (-oo,x)
		if( i->upper.IsRealValue( d ) && d == FLT_MAX ) {	// (-oo,+oo)
			return classad::Value::NULL_VALUE;
		}
		else {
			return upperType;
		}
	}
	else if ( i->upper.IsRealValue( d ) && d == FLT_MAX ) { // (x,+oo)
		return lowerType;
	}

	return classad::Value::NULL_VALUE;
}

bool
IntervalToString( Interval *i, string &buffer )
{
	if( i == NULL ) {
		return false;
	}
	classad::PrettyPrint pp;
	switch( GetValueType( i ) ) {
	case classad::Value::BOOLEAN_VALUE:
	case classad::Value::STRING_VALUE: {
		buffer += "[";
		pp.Unparse( buffer, i->lower );
		buffer += "]";
		return true;
	}
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE: {
		double low = 0;
		double high = 0;
		GetLowDoubleValue( i, low );
		GetHighDoubleValue( i, high );

		if( i->openLower ) {
			buffer += '(';
		}
		else {
			buffer += '[';
		}

		if( low == -( FLT_MAX ) ) {
			buffer += "-oo";
		}
		else {
			pp.Unparse( buffer, i->lower );
		}

		buffer += ',';

		if( high == FLT_MAX ) {
			buffer += "+oo";
		}
		else {
			pp.Unparse( buffer, i->upper );
		}

		if( i->openUpper ) {
			buffer += ')';
		}
		else {
			buffer += ']';
		}
		return true;
	}
	default: {
		buffer += "[???]";
		return true;
	}
	}
}

bool
Numeric( classad::Value::ValueType vt )
{
	return ( ( vt == classad::Value::INTEGER_VALUE) || 
			 ( vt == classad::Value::REAL_VALUE ) );
}

bool
SameType( classad::Value::ValueType vt1, classad::Value::ValueType vt2 )
{
	return ( ( vt1 == vt2 ) || ( Numeric( vt1 ) && Numeric( vt2 ) ) ); 
}

bool
GetDoubleValue ( classad::Value &val, double &d )
{
	time_t dTime;
	classad::abstime_t dAbsTime;
	if( val.IsNumber( d ) ) {
		return true;
	} else if( val.IsAbsoluteTimeValue( dAbsTime ) ) {
		d = ( double )dAbsTime.secs;
		return true;
	} else if( val.IsRelativeTimeValue( dTime ) ) {
		d = ( double )dTime;
		return true;
	}
	return false;
}

bool
EqualValue( classad::Value &v1, classad::Value &v2 )
{
	if( v1.GetType( ) != v2.GetType( ) ) {
		return false;
	}
	switch( v1.GetType( ) ) {
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE: {
		double d1, d2;
		GetDoubleValue( v1, d1 );
		GetDoubleValue( v2, d2 );
		if( d1 == d2 ) {
			return true;
		}
		else {
			return false;
		}
	}
	case classad::Value::BOOLEAN_VALUE: {
		bool b1 = false, b2 = false;
		(void) v1.IsBooleanValue( b1 );
		(void) v2.IsBooleanValue( b2 );
		if( b1 == b2 ) {
			return true;
		}
		else {
			return false;
		}
	}
	case classad::Value::STRING_VALUE: {
		string s1, s2;
		(void) v1.IsStringValue( s1 );
		(void) v2.IsStringValue( s2 );
		if( s1.compare( s2 ) == 0 ) {
			return true;
		}
		else {
			return false;
		}
	}
	default: return false;
	}
}

bool
IncrementValue( classad::Value &val )
{
	int i;
	double d, c;
	time_t t;
	classad::abstime_t a;
	if( val.IsIntegerValue( i ) ) {
		val.SetIntegerValue( i + 1 );
		return true;
	}
	else if( val.IsRealValue( d ) ) {
		c = ceil( d );
		if( d == c ) {
			val.SetRealValue( d + 1 );
		}	
		else {
			val.SetRealValue( c );
		}
		return true;
	}
	else if( val.IsAbsoluteTimeValue( a ) ) {
		a.secs++;
		val.SetAbsoluteTimeValue( a );
		return true;
	}
	else if( val.IsRelativeTimeValue( t ) ) {
		val.SetRelativeTimeValue( t + 1 );
		return true;
	}
	else {
		return false;
	}
}
	
bool
DecrementValue( classad::Value &val )
{
	int i;
	double d, c;
	time_t t;
	classad::abstime_t a;
	if( val.IsIntegerValue( i ) ) {
		val.SetIntegerValue( i - 1 );
		return true;
	}
	else if( val.IsRealValue( d ) ) {
		c = floor( d );
		if( d == c ) {
			val.SetRealValue( d - 1 );
		}	
		else {
			val.SetRealValue( c );
		}
		return true;
	}
	else if( val.IsAbsoluteTimeValue( a ) ) {
		a.secs--;
		val.SetAbsoluteTimeValue( a );
		return true;
	}
	else if( val.IsRelativeTimeValue( t ) ) {
		val.SetRelativeTimeValue( t - 1 );
		return true;
	}
	else {
		return false;
	}
}



// IndexSet methods

IndexSet::
IndexSet( )
{
	initialized = false;
	inSet = NULL;
	size = 0;
	cardinality = 0;
}

IndexSet::
~IndexSet( )
{
	if( inSet ) {
		delete [] inSet;
	}
}

bool IndexSet::
Init( int _size )
{
	if( _size <= 0 ) {
		cerr << "IndexSet::Init: size out of range: " << _size << endl;
		return false;
	}

	if( inSet ) {
		delete [] inSet;
	}

	if( !( inSet = new bool[_size] ) ) {
		cerr << "IndexSet::Init: out of memory" << endl;
		return false;
	}

	size = _size;
	for( int i = 0; i < size; i++ ) {
		inSet[i] = false;
	}
	cardinality = 0;
	initialized = true;
	return true;
}

bool IndexSet::
Init( IndexSet &is )
{
	if( !is.initialized ) {
		cerr << "IndexSet::Init: IndexSet not initialized" << endl;
		return false;
	}

	if( inSet ) {
		delete [] inSet;
	}

	if( !( inSet = new bool[is.size] ) ) {
		cerr << "IndexSet::Init: out of memory" << endl;
		return false;
	}

	size = is.size;
	for( int i = 0; i < size; i++ ) {
		inSet[i] = is.inSet[i];
	}
	cardinality = is.cardinality;
	initialized = true;
	return true;
}

bool IndexSet::
AddIndex( int index )
{
	if( !initialized ) {
		return false;
	}
	if( index < 0 || index >= size ) {
		cerr << "IndexSet::AddIndex: index out of range" << endl;
		return false;
	}
	else if( inSet[index] ) {
		return true;
	}
	else {
		inSet[index] = true;
		cardinality++;
		return true;
	}		
}

bool IndexSet::
RemoveIndex( int index )
{
	if( !initialized ) {
		return false;
	}
	if( index < 0 || index >= size ) {
		cerr << "IndexSet::RemoveIndex: index out of range" << endl;
		return false;
	}
	else if( inSet[index] ) {
		inSet[index] = false;
		cardinality--;
	}
	return true;
}

bool IndexSet::
RemoveAllIndeces( )
{
	if( !initialized ) {
		return false;
	}
	for( int i = 0; i < size; i++ ) {
		inSet[i] = false;
	}
	cardinality = 0;
	return true;
}

bool IndexSet::
AddAllIndeces( )
{
	if( !initialized ) {
		return false;
	}
	for( int i = 0; i < size; i++ ) {
		inSet[i] = true;
	}
	cardinality = size;
	return true;
}

bool IndexSet::
GetCardinality( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = cardinality;
	return true;
}

bool IndexSet::
Equals( IndexSet &is )
{
	if( !initialized ) {
		cerr << "IndexSet::Equals: IndexSet not initialized" << endl;
		return false;
	}
	else if( !is.initialized ) {
		cerr << "IndexSet::Equals: IndexSet not initialized" << endl;
		return false;
	}
	else if( size != is.size || cardinality != is.cardinality ) {
		return false;
	}
	else {
		for( int i = 0; i < size; i++ ) {
			if( inSet[i] != is.inSet[i] ) {
				return false;
			}
		}
		return true;
	}
}

bool IndexSet::
IsEmpty( ) const
{
	if( !initialized ) {
		cerr << "IndexSet::IsEmpty: IndexSet not initialized" << endl;
		return false;
	}
	else {
		return cardinality == 0;
	}
}

bool IndexSet::
HasIndex( int index )
{
	if( !initialized ) {
		cerr << "IndexSet::HasIndex: IndexSet not initialized" << endl;
		return false;
	}
	else if( index < 0 || index >= size ) {
		cerr << "IndexSet::HasIndex: index out of range" << endl;
		return false;
	}
	else {
		return inSet[index];
	}
}
	
bool IndexSet::
ToString( string &buffer )
{
	if( !initialized ) {
		cerr << "IndexSet::ToString: IndexSet not initialized" << endl;
		return false;
	}
	else {
		char tempBuff[32];
		bool firstItem = true;
		buffer += '{';
		for( int i = 0; i < size; i++ ) {
			if( inSet[i] ) {
				if( !firstItem ) {
					buffer += ',';
				}
				else {
					firstItem = false;
				}
				sprintf( tempBuff, "%d", i );
				buffer += tempBuff;
			}
		}
		buffer += '}';
		return true;
	}
}

bool IndexSet::
Union( IndexSet &is )
{
	if( !initialized || !is.initialized ) {
		cerr << "IndexSet::Union: IndexSet not initialized" << endl;
		return false;
	}
	else if( size != is.size ) {
		cerr << "IndexSet::Union: incompatible IndexSets" << endl;
		return false;
	}
	else {
		for( int i = 0; i < size; i++) {
			if( !inSet[i] && is.inSet[i] ) {
				inSet[i] = true;
				cardinality++;
			}
		}
		return true;
	}
}

bool IndexSet::
Intersect( IndexSet & is )
{
	if( !initialized || !is.initialized ) {
		cerr << "IndexSet::Union: IndexSet not initialized" << endl;
		return false;
	}
	else if( size != is.size ) {
		cerr << "IndexSet::Union: incompatible IndexSets" << endl;
		return false;
	}
	else {
		for( int i = 0; i < size; i++) {
			if( inSet[i] && !is.inSet[i] ) {
				inSet[i] = false;
				cardinality--;
			}
		}
		return true;
	}
}

// static methods

bool IndexSet::
Translate( IndexSet &is, int *map, int mapSize, int newSize, IndexSet &result )
{
	if( !is.initialized ) {
		cerr << "IndexSet::Translate: IndexSet not initialized" << endl;
		return false;
	}
	else if( map == NULL ) {
		cerr << "IndexSet::Translate: map not initialized" << endl;
		return false;
	}
	else if( mapSize != is.size ) {
		cerr << "IndexSet::Translate: map not same size as IndexSet" << endl;
		return false;
	}
	else if( newSize <=0 ) {
		cerr << "IndexSet::Translate: newSize <=0" << endl;
		return false;
	}
	else {
		result.Init( newSize );
		for( int i = 0; i < is.size; i++ ) {
			if( map[i] < 0 || map[i] >= newSize ) {
				cerr << "IndexSet::Translate: map contains invalid index: "
					 << map[i] << " at element " << i
					 << endl;
				return false;
			}
			else if ( is.inSet[i] ) {
				result.AddIndex( map[i] );
			}
		}
		return true;
	}
}

bool IndexSet::
Union( IndexSet &is1, IndexSet &is2, IndexSet &result )
{
	if( !is1.initialized || !is2.initialized ) {
		cerr << "IndexSet::Union: IndexSet not initialized" << endl;
		return false;
	}
	else if( is1.size != is2.size ) {
		cerr << "IndexSet::Union: incompatible IndexSets" << endl;
		return false;
	}
	else {
		result.Init( is1.size );
		for( int i = 0; i < is1.size; i++ ) {
			if( is1.inSet[i] || is2.inSet[i] ) {
				result.AddIndex( i );
			}
		}
		return true;
	}
}

bool IndexSet::
Intersect( IndexSet &is1, IndexSet &is2, IndexSet &result )
{
	if( !is1.initialized || !is2.initialized ) {
		cerr << "IndexSet::Intersect: IndexSet not initialized" << endl;
		return false;
	}
	else if( is1.size != is2.size ) {
		cerr << "IndexSet::Intersect: incompatible IndexSets" << endl;
		return false;
	}
	else {
		result.Init( is1.size );
		for( int i = 0; i < is1.size; i++ ) {
			if( is1.inSet[i] && is2.inSet[i] ) {
				result.AddIndex( i );
			}
		}
		return true;
	}
}


// ValueRange methods
ValueRange::
ValueRange( )
{
	initialized = false;
	multiIndexed = false;
	numIndeces = 0;
	undefined = false;
	anyOtherString = false;
	type = classad::Value::BOOLEAN_VALUE;
	
}

ValueRange::
~ValueRange( )
{
	Interval *ival = NULL;
	iList.Rewind( );
	while( iList.Next( ival ) ) {
		if( ival ) {
			delete ival;
		}
	}
	MultiIndexedInterval *mii = NULL;
	miiList.Rewind( );
	while( miiList.Next( mii ) ) {
		if( mii ) {
			delete mii;
		}
	}
}

bool ValueRange::
Init( Interval *i, bool undef, bool notString )
{
	if( i == NULL ) {
		cerr << "ValueRange::Init: interval is NULL" << endl;
		return false;
	}
	type = GetValueType( i );
	multiIndexed = false;
	undefined = undef;
	anyOtherString = notString;

	switch( type ) {
	case classad::Value::BOOLEAN_VALUE:
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE:
	case classad::Value::STRING_VALUE: {
		Interval *i_new = new Interval;
		Copy( i, i_new );		
		iList.Append( i_new );
		iList.Rewind( );
		break;
	}		
	default: {
		cerr << "ValueRange::Init: interval value unknown:"
			 << (int)type << endl;
		return false;
	}
	}

	initialized = true;
	return true;
}

bool ValueRange::
Init2( Interval *i1, Interval *i2, bool undef )
{
	if( i1 == NULL ) {
		return false;
	}
	if( i2 == NULL ) {
		return false;
	}
	classad::Value::ValueType vt1 = GetValueType( i1 );
	classad::Value::ValueType vt2 = GetValueType( i2 );
	if( !SameType( vt1, vt2 ) ) {
		return false;
	}
	undefined = undef;
	type = vt1;
	switch( type ) {
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE: {

		Interval *i_new = new Interval;

		if( Overlaps( i1, i2 ) || 
			Consecutive( i1, i2 ) ||
			Consecutive( i2, i1 ) ) {
			if( StartsBefore( i1, i2 ) ) {
				if( EndsAfter( i1, i2 ) ) {	// i2 is contained within i1
					Copy( i1, i_new );
				}
				else {						// i1 starts first, i2 ends last
					Copy( i1, i_new );
					i_new->upper.CopyFrom( i2->upper );
					i_new->openUpper = i2->openUpper;
				}
			}
			else {
				if( EndsAfter( i1, i2 ) ) { // i2 starts first, i1 ends last
					Copy( i1, i_new );
					i_new->lower.CopyFrom( i2->lower );
					i_new->openLower = i2->openLower;
				}
				else {						// i1 is contained within i2
					Copy( i2, i_new );
				}
			}
			iList.Append( i_new );
		}
		else if ( Precedes( i1, i2 ) ) {
			Copy( i1, i_new );		
			iList.Append( i_new );
			i_new = new Interval;
			Copy( i2, i_new );
			iList.Append( i_new );			
		}
		else if ( Precedes( i2, i1 ) ) {
			Copy( i2, i_new );
			iList.Append( i_new );			
			i_new = new Interval;
			Copy( i1, i_new );		
			iList.Append( i_new );
		} else {
			delete i_new;
		}			
		initialized = true;
		iList.Rewind( );
		return true;
	}
	default: {
		return false;
	}
	}
}

bool ValueRange::
InitUndef( bool undef )
{
	undefined = undef;
	initialized = true;
	return true;
}

bool ValueRange::
Init( ValueRange *vr, int _index, int _numIndeces )
{
	if( vr == NULL ) {
		return false;
	}
	if( vr->multiIndexed ) {
		return false;
	}
	if( _numIndeces <= 0 || _index < 0 || _index >= _numIndeces ) {
		return false;
	}
	multiIndexed = true;
	numIndeces = _numIndeces;
	type = vr->type;
	if( vr->undefined ) {
		undefined = true;
		undefinedIS.Init( _numIndeces );
		undefinedIS.AddIndex( _index );
	}
	else {
		undefined = false;
	}
	if( vr->anyOtherString ) {
		anyOtherString = true;
		anyOtherStringIS.Init(_numIndeces );
		anyOtherStringIS.AddIndex( _index );
	}
	else {
		anyOtherString = false;
	}
	MultiIndexedInterval *mii = NULL;
	Interval *i_curr = NULL;
	Interval *i_new = NULL;
	vr->iList.Rewind( );
	while( vr->iList.Next( i_curr ) ) {
		mii = new MultiIndexedInterval;
		i_new = new Interval;
		Copy( i_curr, i_new );
		mii->ival = i_new;
		mii->iSet.Init( _numIndeces );
		if( !anyOtherString ) {
			mii->iSet.AddIndex( _index );
		}
		miiList.Append( mii );
	}
	vr->iList.Rewind( );
	miiList.Rewind( );
	initialized = true;
	return true;
}

bool ValueRange::
Intersect( Interval *i, bool undef, bool notString)
{
	if( !initialized ) {
		return false;
	}
	if( i == NULL ) {
		return false;
	}
	if( multiIndexed ) {
		return false;
	}
	if( iList.IsEmpty( ) && !anyOtherString && !undefined ) {
		return true;
	}
	if( !SameType( type, GetValueType( i ) ) ) {
		cerr << "ValueRange::Intersect: type mismatch" << endl;
		return false;
	}
	Interval *i_curr = NULL;
	Interval *i_new = NULL;
	switch( type ) {

	case classad::Value::BOOLEAN_VALUE: {
		undefined = undefined && undef;
		bool b, b_curr;
		if( !i->lower.IsBooleanValue( b ) ) {
			return false;
		}
		iList.Rewind( );
		while( iList.Next( i_curr ) ) {
			if( !i_curr->lower.IsBooleanValue( b_curr ) ) {
				iList.Rewind( );
				return false;
			}
			if( b == b_curr ) {
				iList.Rewind( );
				return true;
			}
		}
		i_new = new Interval;
		Copy( i, i_new );
		iList.Append( i_new );
		iList.Rewind( );
		return true;
	}
				
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE: {
		undefined = undefined && undef;
		i_new = new Interval;
		Copy( i, i_new );
		iList.Rewind( );
		while( iList.Next( i_curr ) ) {
			if( Precedes( i_curr, i_new ) ) {
				continue;
			}
			if( Precedes( i_new, i_curr ) ) {
				iList.Rewind( );
				return true;
			}
			if( Overlaps( i_new, i_curr ) ) {
				if( StartsBefore( i_curr, i_new ) ) {
						// truncate lower end of i_curr
					i_curr->lower.CopyFrom( i_new->lower );
					i_curr->openLower = i_new->openLower;
				}
				if( EndsAfter( i_curr, i_new ) ) {
						// truncate upper end of i_curr
					i_curr->upper.CopyFrom( i_new->upper );
					i_curr->openUpper = i_new->openUpper;
					iList.Rewind( );
					return true;
				}
				if( EndsAfter( i_new, i_curr ) ) {
						// truncate lower end of i_new
					i_new->lower.CopyFrom( i_curr->upper );
					i_new->openLower = !i_curr->openUpper;
					continue;
				}
			}
		}
			// at end of list
		iList.Rewind( );
		delete i_new;
		return true;
	}
	case classad::Value::STRING_VALUE: {
		undefined = undefined && undef;
		string s_new, s_curr;
		if( !i->lower.IsStringValue( s_new ) ) {
			return false;
		}
		if( iList.IsEmpty( ) ) {
			anyOtherString = notString;
			i_new = new Interval;
			Copy( i, i_new );
			iList.Append( i_new );
			iList.Rewind( );
			return true;
		} 
		iList.Rewind( );
		while( iList.Next( i_curr ) ) {
			if( !i_curr->lower.IsStringValue( s_curr ) ) {
				iList.Rewind( );
				return false;
			}
			int compare = strcmp( s_new.c_str( ), s_curr.c_str( ) );
			if( compare < 0 ) {
				if( anyOtherString ) {
					i_new = new Interval;
					Copy( i, i_new );
					if( notString ) {
						iList.Insert( i_new );
						iList.Rewind( );
						return true;
					}
					else {
						EmptyOut( );
						iList.Append( i_new );
						iList.Rewind( );
						return true;
					}
				}
				else {
					iList.Rewind( );
					return true;
				}
			}
			else if( compare == 0 ) {
				if( anyOtherString == notString ) {
					iList.Rewind( );
					return true;
				}
				else if( anyOtherString ) {	// AOS = T, NS = F
					EmptyOut( );
					iList.Rewind( );
					return true;
				}
				else {  					// AOS = F, NS = T 
					iList.DeleteCurrent( );
					iList.Rewind( );
					return true;
				}
			}
			continue;
		}
		if( anyOtherString ) {
			i_new = new Interval;
			Copy( i, i_new );
			if( !notString ) {
				EmptyOut( );
			}
			iList.Append( i_new );
		}
		iList.Rewind( );
		return true;
	}
	default: {
		cerr << "ValueRange::Intersect: unexpected/unkown ValueType: "
			 << (int)type << endl;
		return false;
	}
	}
	return true;
}

bool ValueRange::
Intersect2( Interval *i1, Interval *i2, bool undef )
{
	if( !initialized ) {
		return false;
	}
	if( i1 == NULL ) {
		return false;
	}
	if( i2 == NULL ) {
		return false;
	}
	if( multiIndexed ) {
		return false;
	}
	if( iList.IsEmpty( ) ) {
		return true;
	}
	ValueRange tempVR;
	tempVR.Init( i1, i2 );
	if( tempVR.IsEmpty( ) ) {
		EmptyOut( );
		return true;
	}
	undefined = undefined && undef;
	Interval *i_curr = NULL;
	Interval *i_new = NULL;
	if( type != tempVR.type ) {
		cerr << "ValueRange::Intersect2: Type error" << endl;
		return false;
	}

	iList.Rewind( );
	if( !iList.Next( i_curr ) ) {
		return true;
	}
	tempVR.iList.Rewind( );
	if( !tempVR.iList.Next( i_new ) ) {
		iList.DeleteCurrent( );
		while( iList.Next( i_curr ) ) {
			iList.DeleteCurrent( );
		}
		return true;
	}
	while( true ) {		
		switch( type ) {
		case classad::Value::INTEGER_VALUE:
		case classad::Value::REAL_VALUE:
		case classad::Value::RELATIVE_TIME_VALUE:
		case classad::Value::ABSOLUTE_TIME_VALUE: {
			if( Precedes( i_curr, i_new ) ) {
				if( !iList.Next( i_curr ) ) {
					iList.Rewind( );
					return true;
				}
				continue;
			}
			else if( Precedes( i_new, i_curr ) ) {
				if( !tempVR.iList.Next( i_new ) ) {
					iList.DeleteCurrent( );
					while( iList.Next( i_curr ) ) {
						iList.DeleteCurrent( );
					}
					return true;
				}
				continue;
			}
			else if( Overlaps( i_new, i_curr ) ) {
				if( StartsBefore( i_curr, i_new ) ) {
						// truncate lower end of i_curr
					i_curr->lower.CopyFrom( i_new->lower );
					i_curr->openLower = i_new->openLower;
				}
				if( EndsAfter( i_curr, i_new ) ) {
						// truncate upper end of i_curr, get next i_curr
					i_curr->upper.CopyFrom( i_new->upper );
					i_curr->openUpper = i_new->openUpper;
					if( !tempVR.iList.Next( i_new ) ) {
						while( iList.Next( i_curr ) ) {
							iList.DeleteCurrent( );
						}
						return true;
					}
					continue;
				}
				else if( EndsAfter( i_new, i_curr ) ) {
						// truncate lower end of i_new, get next i_new
					i_new->lower.CopyFrom( i_curr->upper );
					i_new->openLower = !i_curr->openUpper;
					if( !iList.Next( i_curr ) ) {
						iList.Rewind( );
						return true;
					}
					continue;
				}
				else {	// i_new and i_curr have identical upper bound
						// get next i_new and next i_curr
					if( !iList.Next( i_curr ) ) {
						iList.Rewind( );
						return true;
					}
					if( !tempVR.iList.Next( i_new ) ) {
						while( iList.Next( i_curr ) ) {
							iList.DeleteCurrent( );
						}
						return true;
					}
					continue;
				}
			}
			else {
					// should not reach here
				cerr << "ValueRange::Intersect2: interval problem" << endl;
				return false;
			}
		}		
		default: {
			cerr << "ValueRange::Intersect2: unexpected/unkown ValueType: "
				 << (int)type << endl;
			return false;
		}
		}
	}
		// should not reach here
	cerr << "ValueRange::Intersect2: left while loop " << endl;
	return false;
}	

bool ValueRange::
IntersectUndef( bool undef )
{
	if( !initialized ) {
		return false;
	}
	if( multiIndexed ) {
		return false;
	}
	EmptyOut( );
	undefined = undef;
	return true;
}
		
bool ValueRange::
Union( ValueRange *vr, int index )
{
	if( !initialized ) {
		return false;
	}
	if( vr == NULL ) {
		return false;
	}
	if( !multiIndexed ) {
		return false;
	}
	if( vr->multiIndexed ) {
		return false;
	}
	if( !SameType( vr->type, type ) ) {
		return false;
	}
	if( index >= numIndeces && index < 0 ) {
		return false;
	}

	if( vr->undefined ) {
		if( !undefined ) {
			undefined = true;
			undefinedIS.Init( numIndeces );
		}
		undefinedIS.AddIndex( index );
	}

	if( vr->anyOtherString ) {
		if( !anyOtherString ) {
			anyOtherString = true;
			anyOtherStringIS.Init( numIndeces );
		}
		anyOtherStringIS.AddIndex( index );
	}

	if( vr->iList.IsEmpty( ) ) {
		return true;
	}

	Interval *interval1 = NULL;
	Interval *interval2 = NULL;
	Interval *i_new = NULL;
	MultiIndexedInterval *mii_curr = NULL;
	MultiIndexedInterval *mii_new = NULL;

	switch ( type ) {
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE: {

		miiList.Rewind( );
		vr->iList.Rewind( );
		bool done = false;
		(void) vr->iList.Next( interval2 );  // vr->iList is not empty
		if( !miiList.Next( mii_curr ) ) {
				// add all intervals in vr->iList
			i_new = new Interval;
			Copy( interval2, i_new );
			mii_new = new MultiIndexedInterval;
			mii_new->ival = i_new;
			mii_new->iSet.Init( numIndeces );
			mii_new->iSet.AddIndex( index );
			miiList.Append( mii_new );
			while( vr->iList.Next( interval2 ) ) {
				i_new = new Interval;
				Copy( interval2, i_new );
				mii_new = new MultiIndexedInterval;
				mii_new->ival = i_new;
				mii_new->iSet.Init( numIndeces );
				mii_new->iSet.AddIndex( index );
				miiList.Append( mii_new );
			}
			vr->iList.Rewind( );
			miiList.Rewind( );
			done = true;
		}
		else {
			interval1 = mii_curr->ival;
		}

		while( !done ) {

				// CASE 1: no overlap
			
				// CASE 1.1:
				// interval1: |----|
				// interval2:        |-----|
			if( Precedes( interval1, interval2 ) ) {
					// get new interval1
				if( !miiList.Next( mii_curr ) ) {
						// add all intervals in vr->iList
					i_new = new Interval;
					Copy( interval2, i_new );
					mii_new = new MultiIndexedInterval;
					mii_new->ival = i_new;
					mii_new->iSet.Init( numIndeces );
					mii_new->iSet.AddIndex( index );
					miiList.Append( mii_new );
					while( vr->iList.Next( interval2 ) ) {
						i_new = new Interval;
						Copy( interval2, i_new );
						mii_new = new MultiIndexedInterval;
						mii_new->ival = i_new;
						mii_new->iSet.Init( numIndeces );
						mii_new->iSet.AddIndex( index );
						miiList.Append( mii_new );
					}
					vr->iList.Rewind( );
					miiList.Rewind( );
					done = true;
					continue;
				}
				interval1 = mii_curr->ival;
				continue;
			}

				// CASE 1.2:
				// interval1:        |----|
				// interval2: |-----|
			else if( Precedes( interval2, interval1 ) ) {
				i_new = new Interval;
				Copy( interval2, i_new );
				mii_new = new MultiIndexedInterval;
				mii_new->ival = i_new;
				mii_new->iSet.Init( numIndeces );
				mii_new->iSet.AddIndex( index );
				miiList.Insert( mii_new );

					// get new interval2
				if( !vr->iList.Next( interval2 ) ) {
					vr->iList.Rewind( );
					miiList.Rewind( );
					done = true;
				}
				continue;
			}

			else{
					// CASE 2: overlap with different lower bounds
				
					// CASE 2.1:
					// interval1: |--------?
					// interval2:    |-----?
					//
					// interval1 starts first - split interval1
				if( StartsBefore( interval1, interval2 ) ) {
						// add new interval
					i_new = new Interval;
					Copy( interval1, i_new );
					i_new->upper.CopyFrom( interval2->lower );
					i_new->openUpper = !interval2->openLower;
					mii_new = new MultiIndexedInterval( );
					mii_new->ival = i_new;
					mii_new->iSet.Init( mii_curr->iSet );
					miiList.Insert( mii_new );

						// truncate interval1
					interval1->lower.CopyFrom( interval2->lower );
					interval1->openLower = interval2->openLower;
				}
					// CASE 2.2:
					// interval1:    |-----?
					// interval2: |--------?
					//
					// interval2 starts first - split interval2
				else if( StartsBefore( interval2, interval1 ) ) {
						// add new interval
					i_new = new Interval;
					Copy( interval2, i_new );
					i_new->upper.CopyFrom( interval1->lower );
					i_new->openUpper = !interval1->openLower;
					mii_new = new MultiIndexedInterval( );
					mii_new->ival = i_new;
					mii_new->iSet.Init( numIndeces );
					mii_new->iSet.AddIndex( index );
					miiList.Insert( mii_new );
					
						// truncate interval2
					i_new = new Interval;
					Copy( interval2, i_new );
					i_new->lower.CopyFrom( interval1->lower );
					i_new->openLower = interval1->openLower;
					interval2 = i_new;
				}
				
					// CASE 3: overlap with same lower bounds

					// CASE 3.1:
					// interval1: |--------|
					// interval2: |----|
					//
					// interval 1 ends last - split interval1
				if( EndsAfter( interval1, interval2 ) ) {
						// create new interval
					i_new = new Interval;
					Copy( interval1, i_new );
					i_new->lower.CopyFrom( interval2->upper );
					i_new->openLower = !interval2->openUpper;
					
						// truncate interval1 and add index
					interval1->upper.CopyFrom( interval2->upper );
					interval1->openUpper = interval2->openUpper;
					mii_curr->iSet.AddIndex( index );

						// get new interval2
					if( !vr->iList.Next( interval2 ) ) {
						vr->iList.Rewind( );
						miiList.Rewind( );
						done = true;
					}
					continue;
				}

					// CASE 3.2:
					// interval1: |----|
					// interval2: |--------|
					//
					// interval 2 ends last - split interval2
				else if( EndsAfter( interval2, interval1 ) ) {
						// add index
					mii_curr->iSet.AddIndex( index );

						// truncate interval2
					i_new = new Interval;
					Copy( interval2, i_new );
					i_new->lower.CopyFrom( interval1->upper );
					i_new->openLower = !interval1->openUpper;
					interval2 = i_new;

						// get new interval1
					if( !miiList.Next( mii_curr ) ) {
							// add all intervals in vr->iList
						i_new = new Interval;
						Copy( interval2, i_new );
						mii_new = new MultiIndexedInterval;
						mii_new->ival = i_new;
						mii_new->iSet.Init( numIndeces );
						mii_new->iSet.AddIndex( index );
						miiList.Append( mii_new );
						while( vr->iList.Next( interval2 ) ) {
							i_new = new Interval;
							Copy( interval2, i_new );
							mii_new = new MultiIndexedInterval;
							mii_new->ival = i_new;
							mii_new->iSet.Init( numIndeces );
							mii_new->iSet.AddIndex( index );
							miiList.Append( mii_new );
						}
						vr->iList.Rewind( );
						miiList.Rewind( );
						done = true;
						continue;
					}	
					interval1 = mii_curr->ival;
					continue;
				}

					// CASE 3.3:
					// interval1: |----|
					// interval2: |----|
					//
					// interval 1 and 2 are identical - just merge
				else{
						// add index
					mii_curr->iSet.AddIndex( index );

						// get new interval1
					if( !miiList.Next( mii_curr ) ) {
							// add all intervals in vr->iList
						while( vr->iList.Next( interval2 ) ) {
							i_new = new Interval;
							Copy( interval2, i_new );
							mii_new = new MultiIndexedInterval;
							mii_new->ival = i_new;
							mii_new->iSet.Init( numIndeces );
							mii_new->iSet.AddIndex( index );
							miiList.Append( mii_new );
						}
						vr->iList.Rewind( );
						miiList.Rewind( );
						done = true;
						continue;
					}
					interval1 = mii_curr->ival;

						// get new interval2
					if( !vr->iList.Next( interval2 ) ) {
						vr->iList.Rewind( );
						miiList.Rewind( );
						done = true;
					}
					continue;
				}
			}
		}
	
		miiList.Rewind( );  // just in case

			// coalesce consecutive intervals with the same indeces
		if( miiList.Number( ) > 1 ) {
			MultiIndexedInterval *mii_next = NULL;
			(void) miiList.Next( mii_curr );
			while( miiList.Next( mii_next ) ) {
				if( mii_curr->iSet.Equals( mii_next->iSet ) ) {
						// coalesce mii_next into mii_curr
					mii_curr->ival->upper.CopyFrom( mii_next->ival->upper );
					mii_curr->ival->openUpper = mii_next->ival->openUpper;
					miiList.DeleteCurrent( );
				}
				else {
					mii_curr = mii_next;
				}
			}
			miiList.Rewind( );
		}
		return true;
	}
	case classad::Value::BOOLEAN_VALUE: {
		if( vr->iList.Number( ) > 1 ) {
			// the assumption is made here that vr will have only one boolean
			// valued interval.  Init2 does not allow boolean values, and
			// Intersect can not add boolean values.
			return false;
		}
		miiList.Rewind( );
		vr->iList.Rewind( );
		if( !miiList.Next( mii_curr ) ) {
				// add all intervals in vr->iList
			while( vr->iList.Next( interval2 ) ) {
				i_new = new Interval;
				Copy( interval2, i_new );
				mii_new = new MultiIndexedInterval;
				mii_new->ival = i_new;
				mii_new->iSet.Init( numIndeces );
				mii_new->iSet.AddIndex( index );
				miiList.Append( mii_new );
			}
			vr->iList.Rewind( );
			miiList.Rewind( );
		}
		if( !vr->iList.Next( interval2 ) ) {
			vr->iList.Rewind( );
			miiList.Rewind( );
			return true;
		}
		interval1 = mii_curr->ival;
		bool b1 = false;
		bool b2 = false;
		while( true ) {
			if( !interval1->lower.IsBooleanValue( b1 ) ) {
				vr->iList.Rewind( );
				miiList.Rewind( );
				return false;
			}
			if( !interval2->lower.IsBooleanValue( b2 ) ) {
				vr->iList.Rewind( );
				miiList.Rewind( );
				return false;
			}
			if( b1 == b2 ) {
				mii_curr->iSet.AddIndex( index );
				vr->iList.Rewind( );
				miiList.Rewind( );
				return true;
			}
			else if( !miiList.Next( mii_curr ) ) {
					// add all intervals in vr->iList
				i_new = new Interval;
				Copy( interval2, i_new );
				mii_new = new MultiIndexedInterval;
				mii_new->ival = i_new;
				mii_new->iSet.Init( numIndeces );
				mii_new->iSet.AddIndex( index );
				miiList.Append( mii_new );
				while( vr->iList.Next( interval2 ) ) {
					i_new = new Interval;
					Copy( interval2, i_new );
					mii_new = new MultiIndexedInterval;
					mii_new->ival = i_new;
					mii_new->iSet.Init( numIndeces );
					mii_new->iSet.AddIndex( index );
					miiList.Append( mii_new );
				}
				vr->iList.Rewind( );
				miiList.Rewind( );
			}
			else {
				interval1 = mii_curr->ival;
			}
		}
	}
	case classad::Value::STRING_VALUE: {
		if( !miiList.Next( mii_curr ) ) {
				// add all intervals in vr->iList
			i_new = new Interval;
			Copy( interval2, i_new );
			mii_new = new MultiIndexedInterval;
			mii_new->ival = i_new;
			mii_new->iSet.Init( numIndeces );
			if( !vr->anyOtherString ) {
				mii_new->iSet.AddIndex( index );
			}
			miiList.Append( mii_new );
			while( vr->iList.Next( interval2 ) ) {
				i_new = new Interval;
				Copy( interval2, i_new );
				mii_new = new MultiIndexedInterval;
				mii_new->ival = i_new;
				mii_new->iSet.Init( numIndeces );
				if( !vr->anyOtherString ) {
					mii_new->iSet.AddIndex( index );
				}
				miiList.Append( mii_new );
			}
			vr->iList.Rewind( );
			miiList.Rewind( );
			return true;
		}
		if( !vr->iList.Next( interval2 ) ) {
			vr->iList.Rewind( );
			miiList.Rewind( );
			return true;
		}
		interval1 = mii_curr->ival;
		string s1, s2;

		while( true ) {
			if( !interval1->lower.IsStringValue( s1 ) ) {
				vr->iList.Rewind( );
				miiList.Rewind( );
				return false;
			}
			if( !interval2->lower.IsStringValue( s2 ) ) {
				vr->iList.Rewind( );
				miiList.Rewind( );
				return false;
			}
			int compare = strcmp( s1.c_str( ), s2.c_str( ) );
			if( compare < 0 ) { // string1 precedes string2
					// if vr represents a not expression then interval1
					// should include the current index
				if( vr->anyOtherString ) {
					mii_curr->iSet.AddIndex( index );
				}
					// get new interval1
				if( !miiList.Next( mii_curr ) ) {
						// add all intervals in vr->iList
					i_new = new Interval;
					Copy( interval2, i_new );
					mii_new = new MultiIndexedInterval;
					mii_new->ival = i_new;
					mii_new->iSet.Init( numIndeces );
					if( !vr->anyOtherString ) {
						mii_new->iSet.AddIndex( index );
					}
					miiList.Append( mii_new );
					while( vr->iList.Next( interval2 ) ) {
						i_new = new Interval;
						Copy( interval2, i_new );
						mii_new = new MultiIndexedInterval;
						mii_new->ival = i_new;
						mii_new->iSet.Init( numIndeces );
						if( !vr->anyOtherString ) {
							mii_new->iSet.AddIndex( index );
						}
						miiList.Append( mii_new );
					}
					vr->iList.Rewind( );
					miiList.Rewind( );
					return true;
				}
				continue;
			}

			else if( compare > 0 ) { // string2 precedes string1
				i_new = new Interval;
				Copy( interval2, i_new );
				mii_new = new MultiIndexedInterval;
				mii_new->ival = i_new;
				mii_new->iSet.Init( numIndeces );
				if( !vr->anyOtherString ) {
					mii_new->iSet.AddIndex( index );
				}
				miiList.Insert( mii_new );
				
					// get new interval2
				if( !vr->iList.Next( interval2 ) ) {
					vr->iList.Rewind( );
					miiList.Rewind( );
					return true;
				}
				continue;
			}
			else if( compare == 0 ) {
				if( !vr->anyOtherString ) {
					mii_curr->iSet.AddIndex( index );
				}
					// get new interval1
				if( !miiList.Next( mii_curr ) ) {
						// add all intervals in vr->iList
					while( vr->iList.Next( interval2 ) ) {
						i_new = new Interval;
						Copy( interval2, i_new );
						mii_new = new MultiIndexedInterval;
						mii_new->ival = i_new;
						mii_new->iSet.Init( numIndeces );
						if( !vr->anyOtherString ) {
							mii_new->iSet.AddIndex( index );
						}
						miiList.Append( mii_new );
					}
					vr->iList.Rewind( );
					miiList.Rewind( );
					return true;
				}
					// get new interval2
				if( !vr->iList.Next( interval2 ) ) {
					vr->iList.Rewind( );
					miiList.Rewind( );
					return true;
				}
			}
		}
	}
	default: {
		return false;
	}
	}
}


bool ValueRange::
IsEmpty( )
{
	if( !initialized ) {
		cerr << "ValueRange::IsEmpty: ValueRange not initialized" << endl;
		return false;
	}

	if( multiIndexed ) {
		return miiList.IsEmpty( );
	}
	else {
		return iList.IsEmpty( );
	}
}

bool ValueRange::
EmptyOut( )
{
	if( !initialized ) {
		return false;
	}

	if( !iList.IsEmpty( ) ) {
		if( multiIndexed ) {
			MultiIndexedInterval *mii_curr;
			miiList.Rewind( );
			while( miiList.Next( mii_curr ) ) {
				miiList.DeleteCurrent( );
			}
		}
		else {
			Interval *i_curr;
			iList.Rewind( );
			while( iList.Next( i_curr ) ) {
				iList.DeleteCurrent( );
			}
		}
	}
	anyOtherString = false;
	undefined = false;
	return true;
}

bool ValueRange::
GetDistance( classad::Value &pt, classad::Value &min, classad::Value &max,
			 double &result, classad::Value &nearestVal )
{
	if( !initialized ) {
		result = 1;
		nearestVal.SetUndefinedValue( );
		return false;
	}
	if( multiIndexed ) {
		result = 1;
		nearestVal.SetUndefinedValue( );
		return false;
	}

	if( iList.IsEmpty( ) ) {
		result = 1;
		nearestVal.SetUndefinedValue( );
		return true;
	}
	
	switch( pt.GetType( ) ) {
	case classad::Value::INTEGER_VALUE:
	case classad::Value::REAL_VALUE:
	case classad::Value::RELATIVE_TIME_VALUE:
	case classad::Value::ABSOLUTE_TIME_VALUE: {
		double minD, maxD, currDist, minDist, lower, upper, ptD;
		GetDoubleValue( min, minD );
		GetDoubleValue( max, maxD );
		GetDoubleValue( pt, ptD );
		if( maxD < minD ) {
			result = 1;
			return false;
		}
		if( ptD < minD ) {
			minD = ptD;
		}
		if( ptD > maxD ) {
			maxD = ptD;
		}
		
		Interval *ival = NULL;
		minDist = FLT_MAX;
		bool upperIsNearest = true;
		iList.Rewind( );
		while( iList.Next( ival ) ) {
			GetLowDoubleValue( ival, lower );
			GetHighDoubleValue( ival, upper );
				// reset min if necessary
			if( lower < minD && lower != - FLT_MAX ) {
				minD = lower;
			}
			else if( upper < minD ) {
				minD = upper;
			}
				// reset max if necessary
			if( upper > maxD && upper != FLT_MAX ) {
				maxD = upper;
			}
			else if( lower > maxD ) {
				maxD = lower;
			}
				// calculate distance
			if( lower > ptD ) {
				currDist = lower - ptD;
				upperIsNearest = false;
			}
			else if( upper < ptD ) {
				currDist = ptD - upper;
				upperIsNearest = false;				
			}
			else {
				currDist = 0;
				nearestVal.SetUndefinedValue( );
			}
				// check if distance is lower than current lowest
			if( currDist < minDist ) {
				minDist = currDist;
				if( currDist > 0 ) {
					if( upperIsNearest ) {
						nearestVal.CopyFrom( ival->upper );
					}
					else{
						nearestVal.CopyFrom( ival->lower );
					}
				}
				else {
					nearestVal.SetUndefinedValue( );
				}
			}
		}
		result = minDist / ( maxD - minD );
		return true;
	}
	default: {
		result = 1;
		nearestVal.SetUndefinedValue( );
		return false;
	}
	}
}


bool ValueRange::
BuildHyperRects( ExtArray< ValueRange * > &vrs, int dimensions,
				 int numContexts, List< ExtArray< HyperRect * > > &hrLists )
{
	Interval *ival = NULL;
	ValueRange *currVR = NULL;
	HyperRect *oldHR = NULL;
	HyperRect *newHR = NULL;
	Interval **ivals = NULL;
	MultiIndexedInterval *mii = NULL;
	List< HyperRect > *oldHRs = new List< HyperRect >;
	List< HyperRect > *tempHRs = new List< HyperRect >;

	for( int i = 0; i < dimensions; i++ ) {
		currVR = vrs[i];
		if( currVR == NULL ) {
			if( i == 0 ) {
				newHR = new HyperRect( );
				ivals = new Interval*[1];
				ivals[0] = NULL;
				newHR->Init( 1, numContexts, ivals );
				newHR->FillIndexSet( );
				tempHRs->Append( newHR );
				delete [] ivals;
			}
			else {
				oldHRs->Rewind( );
				while( oldHRs->Next( oldHR ) ) {
					newHR = new HyperRect( );
					ivals = new Interval*[i+1];
					for( int j = 0; j < i; j++ ) {
						ival = new Interval;
						oldHR->GetInterval( j, ival );
						ivals[j] = ival;
					}
					ivals[i] = NULL;
					newHR->Init( i+1, numContexts, ivals );
					IndexSet newIS;
					newIS.Init( numContexts );
					oldHR->GetIndexSet( newIS );
					newHR->SetIndexSet( newIS );
					tempHRs->Append( newHR );
					delete [] ivals;
				}
			}
		}
		else {
			if( !currVR->multiIndexed ) {
				delete oldHRs;
				delete tempHRs;
				return false;
			}
			if( numContexts != currVR->numIndeces ) {
					// IndexSets are not compatible
				delete oldHRs;
				delete tempHRs;
				return false;
			}
			if( i == 0 ) {
				currVR->miiList.Rewind( );
				while( currVR->miiList.Next( mii ) ) {
					newHR = new HyperRect( );
					ivals = new Interval*[1];
					ival = new Interval;
					Copy( mii->ival, ival );
					ivals[0] = ival;
					newHR->Init( 1, numContexts, ivals );
					newHR->SetIndexSet( mii->iSet );
					tempHRs->Append( newHR );
					delete ival;
					delete [] ivals;
				}
			}
			else {
				oldHRs->Rewind( );
				while( oldHRs->Next( oldHR ) ) {
					currVR->miiList.Rewind( );
					while( currVR->miiList.Next( mii ) ) {
						IndexSet newIS;
						newIS.Init( numContexts );
						oldHR->GetIndexSet( newIS );
						newIS.Intersect( mii->iSet );
						if( !newIS.IsEmpty( ) ) {
							newHR = new HyperRect( );
							ivals = new Interval*[i+1];
							for( int j = 0; j < i; j++ ) {
								ival = new Interval;
								oldHR->GetInterval( j, ival );
								ivals[j] = ival;
							}
							ivals[i] = new Interval;
							Copy( mii->ival, ivals[i] );
							newHR->Init( i+1, numContexts, ivals );
							newHR->SetIndexSet( newIS );
							tempHRs->Append( newHR );
							for( int j = 0; j < i; j++ ) {
								if( ivals[j] ) delete ivals[j];
							}
							delete [] ivals;
						}
					}
				}
			}
		}		
		oldHRs->Rewind( );
		while( oldHRs->Next( oldHR ) ) {
			delete oldHR;
		}
		delete oldHRs;
		oldHRs = tempHRs;
		tempHRs = new List< HyperRect >;
	}
	delete tempHRs;

	ExtArray< HyperRect * > *hrs;
	hrs = new ExtArray< HyperRect * >( oldHRs->Number( ) );

	oldHRs->Rewind( );
	for( int i = 0; i < hrs->getsize( ); i++ ) {
		( *hrs )[i] = oldHRs->Next( );
	}
	hrLists.Append( hrs );
	delete oldHRs;
	return true;
}

bool ValueRange::
ToString( string &buffer )
{
	if( !initialized ) {
		return false;
	}
	buffer += '{';
	if( anyOtherString ) {
		buffer += "AOS:";
		if( multiIndexed ) {
			anyOtherStringIS.ToString( buffer );
		}
	}
	if( undefined ) {
		buffer += "U:";
		if( multiIndexed ) {
			undefinedIS.ToString( buffer );
		}
	}
	if( multiIndexed ) {
		MultiIndexedInterval *mii_curr;
		miiList.Rewind( );
		while( miiList.Next( mii_curr ) ) {
			IntervalToString( mii_curr->ival, buffer );
			buffer += ':';
			mii_curr->iSet.ToString( buffer );
		}
	}
	else {
		Interval *i_curr = NULL;
		iList.Rewind( );
		while( iList.Next( i_curr ) ) {
			IntervalToString( i_curr, buffer );
		}
	}
	buffer += '}';
	return true;
}

bool ValueRange::
IsInitialized( ) const
{
	return initialized;
}


// ValueRangeTable methods

ValueRangeTable::
ValueRangeTable( )
{
	initialized = false;
	numCols = 0;
	numRows = 0;
	table = NULL;
}

ValueRangeTable::
~ValueRangeTable( )
{
	if( table ) {
		for( int i = 0; i < numCols; i++ ){
			delete [] table[i];
		}
		delete [] table;
	}
}

bool ValueRangeTable::
Init( int _numCols, int _numRows )
{
	if( table ) {
		for( int i = 0; i < numCols; i++ ){
			delete [] table[i];
		}
		delete [] table;
	}

	numCols = _numCols;
	numRows = _numRows;

	table = new ValueRange **[_numCols];
	for( int col = 0; col < _numCols; col++ ) {
		table[col] = new ValueRange *[_numRows];
		for( int row = 0; row < _numRows; row++ ) {
			table[col][row] = NULL;
		}
	}

	initialized = true;
	return true;
}

bool ValueRangeTable::
SetValueRange( int col, int row, ValueRange *vr )
{
	if( !initialized ||
		col >= numCols || row >= numRows || col < 0 || row < 0 ) {
		return false;
	}
	table[col][row] = vr;
	return true;
}

bool ValueRangeTable::
GetValueRange( int col, int row, ValueRange *&vr )
{
	if( !initialized || 
		col >= numCols || row >= numRows || col < 0 || row < 0 ) {
		return false;
	}
	vr = table[col][row];
	return true;
}

bool ValueRangeTable::
GetNumRows( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numRows;
	return true;
}

bool ValueRangeTable::
GetNumColumns( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numCols;
	return true;
}

bool ValueRangeTable::
ToString( string &buffer )
{
	if( !initialized ) {
		return false;
	}
	char tempBuf[512];
	
	sprintf( tempBuf, "%d", numCols );
	buffer += "numCols = ";
	buffer += tempBuf;
	buffer += "\n";

	sprintf( tempBuf, "%d", numRows );
	buffer += "numRows = ";
	buffer += tempBuf;
	buffer += "\n";

	ValueRange *vr = NULL;
	for( int row = 0; row < numRows; row++ ) {
		for( int col = 0; col < numCols; col++ ) {
			vr = table[col][row];
			if( vr == NULL ) {
				buffer += "{NULL}";
			}
			else {
				vr->ToString( buffer );
			}
		}
		buffer += "\n";
	}
	return true;
}

// ValueTable methods
ValueTable::
ValueTable( )
{
	initialized = false;
	numCols = 0;
	numRows = 0;
	table = NULL;
	bounds = NULL;
	inequality = false;
}

ValueTable::
~ValueTable( )
{
	if( table ) {
		for( int i = 0; i < numCols; i++ ){
			for( int j = 0; j < numRows; j++ ) {
				if( table[i][j] ) delete table[i][j];
			}
			delete [] table[i];
		}
		delete [] table;
	}
	if( bounds ) {
		for( int i = 0; i < numRows; i++ ){
			if( bounds[i] ) delete bounds[i];
		}
		delete [] bounds;
	}
}

bool ValueTable::
Init( int _numCols, int _numRows )
{
	if( table ) {
		for( int i = 0; i < numCols; i++ ){
			for( int j = 0; j < numRows; j++ ) {
				if( table[i][j] ) delete table[i][j];
			}
			delete [] table[i];
		}
		delete [] table;
	}
	if( bounds ) {
		for( int i = 0; i < numRows; i++ ){
			if( bounds[i] ) delete bounds[i];
		}
		delete [] bounds;
	}

	numCols = _numCols;
	numRows = _numRows;

	table = new classad::Value **[_numCols];
	for( int col = 0; col < _numCols; col++ ) {
		table[col] = new classad::Value *[_numRows];
		for( int row = 0; row < _numRows; row++ ) {
			table[col][row] = NULL;
		}
	}

	bounds = new Interval *[_numRows];
	for( int row = 0; row < _numRows; row++ ) {
		bounds[row] = NULL;
	}

	inequality = false;
	initialized = true;
	return true;
}

bool ValueTable::
SetOp( int row, classad::Operation::OpKind op )
{
	if( !initialized || row >= numRows || row < 0 || 
		!( op >= classad::Operation::__COMPARISON_START__ && 
		   op <= classad::Operation::__COMPARISON_END__ ) ) {
		return false;
	}
	if( IsInequality( op ) ) {
		inequality = true;
	}
	else {
		inequality = false;
	}
	return true;
}

bool ValueTable::
SetValue( int col, int row, classad::Value &val )
{
	if( !initialized ||
		col >= numCols || row >= numRows || col < 0 || row < 0 ) {
		return false;
	}
	table[col][row] = new classad::Value( );
	table[col][row]->CopyFrom( val );

	if( inequality ) {
		if( bounds[row] == NULL ) {
			bounds[row] = new Interval;
			bounds[row]->lower.CopyFrom( val );
			bounds[row]->upper.CopyFrom( val );
		}
		double dVal, min, max;
		if( !GetDoubleValue( val, dVal ) ||
			!GetDoubleValue( bounds[row]->upper, max ) ||
			!GetDoubleValue( bounds[row]->lower, min ) ) {
			return false;
		}
		if( dVal < min ) {
			bounds[row]->lower.CopyFrom( val );
			return true;
		}
		if( dVal > max ) {
			bounds[row]->upper.CopyFrom( val );
			return true;
		}
	}
	return true;
}

bool ValueTable::
GetValue( int col, int row, classad::Value &val )
{
	if( !initialized || 
		col >= numCols || row >= numRows || col < 0 || row < 0 ) {
		return false;
	}
	val.CopyFrom( *( table[col][row] ) );
	return true;
}

bool ValueTable::
GetNumRows( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numRows;
	return true;
}

bool ValueTable::
GetNumColumns( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numCols;
	return true;
}

bool ValueTable::
GetUpperBound( int row, classad::Value &result ) {
	if( !initialized ) {
		return false;
	}

	if( bounds[row] == NULL ) {
		return false;
	}
	else {
		result.CopyFrom( bounds[row]->upper );
		return true;
	}
}

bool ValueTable::
GetLowerBound( int row, classad::Value &result ) {
	if( !initialized ) {
		return false;
	}

	if( bounds[row] == NULL ) {
		return false;
	}
	else {
		result.CopyFrom( bounds[row]->lower );
		return true;
	}
}

bool ValueTable::
ToString( string &buffer )
{
	if( !initialized ) {
		return false;
	}
	char tempBuf[512];
	classad::PrettyPrint pp;
	
	sprintf( tempBuf, "%d", numCols );
	buffer += "numCols = ";
	buffer += tempBuf;
	buffer += "\n";

	sprintf( tempBuf, "%d", numRows );
	buffer += "numRows = ";
	buffer += tempBuf;
	buffer += "\n";

	classad::Value *val = NULL;
	for( int row = 0; row < numRows; row++ ) {
		for( int col = 0; col < numCols; col++ ) {
			val = table[col][row];
			if( val == NULL ) {
				buffer += "NULL";
			}
			else {
				pp.Unparse( buffer, *val );
			}
			buffer += "|";
		}
		if( bounds[row] ) {
			buffer += " bound=";
			IntervalToString( bounds[row], buffer );
		}
		buffer += "\n";
	}
	return true;
}

bool ValueTable::
IsInequality( classad::Operation::OpKind op ) {
	if( op == classad::Operation::LESS_THAN_OP || 
		op == classad::Operation::LESS_OR_EQUAL_OP ||
		op == classad::Operation::GREATER_OR_EQUAL_OP || 
		op == classad::Operation::GREATER_THAN_OP ) {
		return true;
	}
	else {
		return false;
	}
}

bool ValueTable::
OpToString( string &buffer, classad::Operation::OpKind op )
{
	switch( op ) {
	case classad::Operation::LESS_THAN_OP: {
		buffer += "< ";
		return true;
	}
	case classad::Operation::LESS_OR_EQUAL_OP: {
		buffer += "<=";
		return true;
	}
	case classad::Operation::GREATER_THAN_OP: {
		buffer += "> ";
		return true;
	}
	case classad::Operation::GREATER_OR_EQUAL_OP: {
		buffer += ">=";
		return true;
	}
	default:
		buffer += "  ";
		return false;
	}
}

// HyperRect methods

HyperRect::
HyperRect( )
{
	dimensions = 0;
	numContexts = 0;
	initialized = false;
	ivals = 0;
}

HyperRect::
~HyperRect( )
{
	if( ivals ) {
		for( int i = 0; i < dimensions ; i++ ) {
			delete ivals[i];
		}
		delete [] ivals;
	}
}

bool HyperRect::
Init( int _dimensions, int _numContexts )
{
	dimensions = _dimensions;
	numContexts = _numContexts;
	iSet.Init( _numContexts );
	ivals = new Interval*[_dimensions];
	for( int i = 0; i < dimensions; i++ ) {
		ivals[i] = NULL;
	}
	initialized = true;
	return true;
}

bool HyperRect::
Init( int _dimensions, int _numContexts, Interval **&_ivals )
{
	dimensions = _dimensions;
	numContexts = _numContexts;
	iSet.Init( _numContexts );
	ivals = new Interval*[_dimensions];
	for( int i = 0; i < dimensions; i++ ) {
		ivals[i] = new Interval;
		if( _ivals[i] == NULL ) {
			ivals[i] = NULL;
		}
		else {
			Copy( _ivals[i], ivals[i] );
		}
	}
	initialized = true;
	return true;
}

bool HyperRect::
GetInterval( int dim, Interval *&ival )
{
	if( !initialized ) {
		return false;
	}
	if( dim < 0 || dim >= dimensions ) {
		return false;
	}
	if( ivals[dim] == NULL ) {
		ival = NULL;
		return true;
	}
	ival = new Interval;
	if( Copy( ivals[dim], ival ) ) {
		delete ival;
		return true;
	}
	else {
		delete ival;
		return false;
	}
}

bool HyperRect::
GetIndexSet( IndexSet &_iSet )
{
	if( !initialized ) {
		return false;
	}
	return _iSet.Init( iSet);
}

bool HyperRect::
SetIndexSet( IndexSet &_iSet )
{
	if( !initialized ) {
		return false;
	}
	return iSet.Init( _iSet);
}

bool HyperRect::
FillIndexSet( )
{
	if( !initialized ) {
		return false;
	}
	return iSet.AddAllIndeces( );
}

bool HyperRect::
GetDimensions( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = dimensions;
	return true;
}

bool HyperRect::
GetNumContexts( int &result ) const
{
	if( !initialized ) {
		return false;
	}
	result = numContexts;
	return true;
}

bool HyperRect::
ToString( string &buffer )
{
	if( !initialized ) {
		return false;
	}
	buffer += '{';
	iSet.ToString( buffer );
	buffer += ':';
	for( int i = 0; i < dimensions; i++ ) {
		if( ivals[i] == NULL ) {
			buffer += "(NULL)";
		}
		else {
			IntervalToString( ivals[i], buffer );
		}
	}
	buffer += '}';
	return true;
}
