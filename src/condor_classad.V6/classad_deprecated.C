/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "common.h"
#include "classad.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_classad.h"

// AttrList methods


ClassAd::
ClassAd( FILE *file, char *delimitor, int &isEOF, int&error, int &empty )
{
	char		buffer[ATTRLIST_MAX_EXPRESSION];
	int			delimLen = strlen( delimitor );

	buffer[0] = '\0';
	while( 1 ) {

			// get a line from the file
		if( fgets( buffer, delimLen+1, file ) == NULL ) {
			error = ( isEOF = feof( file ) ) ? 0 : errno;
			return;
		}

			// did we hit the delimitor?
		if( strncmp( buffer, delimitor, delimLen ) == 0 ) {
				// yes ... stop
			isEOF = feof( file );
			error = 0;
			return;
		} else {
				// no ... read the rest of the line (into the same buffer)
			if( fgets( buffer+delimLen, ATTRLIST_MAX_EXPRESSION-delimLen,file )
					== NULL ) {
				error = ( isEOF = feof( file ) ) ? 0 : errno;
				return;
			}
		}

			// if the string is empty, try reading again
		if( strlen( buffer ) == 0 || strcmp( buffer, "\n" ) == 0 ) {
			continue;
		}

			// Insert the string into the classad
		if( Insert( buffer ) == FALSE ) { 	
				// print out where we barfed to the log file
			dprintf(D_ALWAYS,"failed to create classad; bad expr = %s\n",
				buffer);
				// read until delimitor or EOF; whichever comes first
			while( strncmp( buffer, delimitor, delimLen ) && !feof( file ) ) {
				fgets( buffer, delimLen+1, file );
			}
			isEOF = feof( file );
			error = -1;
			return;
		} else {
			empty = FALSE;
		}
	}
	return;
}

int ClassAd::
Insert( const char *str )
{
	ClassAdParser parser;
	ClassAd *newAd;
	vector< pair< string, ExprTree *> > vec;
	vector< pair< string, ExprTree *> >::iterator itr;

	string newAdStr = "[" + string( str ) + "]";
	newAd = parser.ParseClassAd( newAdStr );
	newAd->GetComponents( vec );
	
	for( itr = vec.begin( ); itr != vec.end( ); itr++ ) {
		if( !Insert( itr->first, itr->second ) ) {
			return FALSE;
		}
		itr->first = "";
		itr->second = NULL;
	}
	return TRUE;
}

//  void ClassAd::
//  ResetExpr() { this->ptrExpr = exprList; }

//  ExprTree* ClassAd::
//  NextExpr(){}

//  void ClassAd::
//  ResetName() { this->ptrName = exprList; }

//  const char* ClassAd::
//  NextNameOriginal(){}


//  ExprTree* ClassAd::
//  Lookup(char *) const{}

//  ExprTree* ClassAd::
//  Lookup(const char*) const{}

int ClassAd::
LookupString( const char *name, char *value )
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	strcpy( value, strVal.c_str( ) );
	return 1;
} 

int ClassAd::
LookupInteger( const char *name, int &value )
{
	bool boolVal;
	if( !EvaluateAttrInt( string( name ), value ) ) {
		if( !EvaluateAttrBool( string( name ), boolVal ) ) {
			return 0;
		}
		value = boolVal ? 1 : 0;
	}
	return 1;
}

int ClassAd::
LookupFloat( const char *name, float &value )
{
	double doubleVal;
	int intVal;
	if( !EvaluateAttrReal( string( name ), doubleVal ) ) {
		if( !EvaluateAttrInt( string( name ), intVal ) ) {
			return 0;
		}
		value = (float)intVal;
		return 1;
	}
	value = (float)doubleVal;
	return 1;
}

int ClassAd::
LookupBool( const char *name, int &value )
{
	int intVal;
	int retVal = LookupInteger( name, intVal );
	if( retVal ) {
		value = intVal ? 1 : 0;
		return 0;
	}
	else {
		return retVal;
	}
}

int ClassAd::
EvalString( const char *name, class ClassAd *target, char *value )
{
	ExprTree *tree;
	Value val;
	string strVal;

	tree = Lookup( name );
	if( !tree ) {
		if( target ) {
			tree = target->Lookup( name );
		}
		else {
			evalFromEnvironment( name, val );
			if( val.IsStringValue( strVal ) ) {
				strcpy( value, strVal.c_str( ) );
				return 1;
			}
			return 0;
		}
	}

	if( target->EvaluateExpr( tree, val ) && val.IsStringValue( strVal ) ) {
		strcpy( value, strVal.c_str( ) );
		return 1;
	}

	return 0;
}

int ClassAd::
EvalInteger (const char *name, class ClassAd *target, int &value) {
	ExprTree *tree;
	Value val;

	tree = Lookup( name );
	if( !tree ) {
		if( target ) {
			tree = target->Lookup( name );
		}
		else {
			evalFromEnvironment( name, val );
			if( val.IsIntegerValue( value ) ) {
				return 1;
			}
			return 0;
		}
	}

	if( target->EvaluateExpr( tree, val ) && val.IsIntegerValue( value ) ) {
		return 1;
	}

	return 0;
}

int ClassAd::
EvalFloat (const char *name, class ClassAd *target, float &value)
{
	ExprTree *tree;
	Value val;
	double doubleVal;
	int intVal;

	tree = Lookup( name );
	if( !tree ) {
		if( target ) {
			tree = target->Lookup( name );
		}
		else {
			evalFromEnvironment( name, val );
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				return 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				return 1;
			}
			return 0;
		}
	}

	if( target->EvaluateExpr( tree, val ) ) {
		if( val.IsRealValue( doubleVal ) ) {
			value = ( float )doubleVal;
			return 1;
		}
		if( val.IsIntegerValue( intVal ) ) {
			value = ( float )intVal;
			return 1;
		}
	}

	return 0;
}

int ClassAd::
EvalBool  (const char *name, class ClassAd *target, int &value)
{
	ExprTree *tree;
	Value val;
	double doubleVal;
	int intVal;
	bool boolVal;

	tree = Lookup( name );
	if( !tree ) {
		if( target ) {
			tree = target->Lookup( name );
		}
		else {
			evalFromEnvironment( name, val );
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				return 1;
			}
			if( val.IsIntegerValue( value ) ) {
				value = intVal ? 1 : 0;
				return 1;
			}
			return 0;
		}
	}

	if( target->EvaluateExpr( tree, val ) ) {
		if( val.IsBooleanValue( boolVal ) ) {
			value = boolVal ? 1 : 0;
			return 1;
		}
		if( val.IsIntegerValue( value ) ) {
			value = intVal ? 1 : 0;
			return 1;
		}
		if( val.IsRealValue( doubleVal ) ) {
			value = doubleVal ? 1 : 0;
			return 1;
		}
	}

	return 0;
}

        // shipping functions
int ClassAd::
put( Stream &s )
{
	if( !putOldClassAd( &s, *this ) ) {
		return FALSE;
	}
	return TRUE;
}

int ClassAd::
initFromStream(Stream& s)
{
	ClassAd *newAd;
	if( !( newAd = getOldClassAd( &s ) ) ) {
		return FALSE;
	}
	if( ! CopyFrom( *newAd ) ) {
		return FALSE;
	}
	return TRUE;
}

		// output functions
int	ClassAd::
fPrint( FILE *f )
{
	PrettyPrint pp;
	pp.SetOldClassAd( true );
	string buffer;

	if( !f ) {
		return FALSE;
	}

	pp.Unparse( buffer, this );
	fprintf( f, "%s", buffer.c_str( ) );

	return TRUE;
}

void ClassAd::
dPrint( int level )
{
	PrettyPrint pp;
	pp.SetOldClassAd( true );
	string buffer;

	pp.Unparse( buffer, this );
	dprintf( level, "%s", buffer.c_str( ) );
}


// ClassAd methods

		// Type operations
void ClassAd::
SetMyTypeName( const char *myType )
{
	if( myType ) {
		InsertAttr( ATTR_MY_TYPE, string( myType ) );
	}
}

const char*	ClassAd::
GetMyTypeName( )
{
	string myTypeStr;
	if( !EvaluateAttrString( ATTR_MY_TYPE, myTypeStr ) ) {
		return NULL;
	}
	return myTypeStr.c_str( );
}

void ClassAd::
SetTargetTypeName( const char *targetType )
{
	if( targetType ) {
		InsertAttr( ATTR_TARGET_TYPE, string( targetType ) );
	}
}

const char*	ClassAd::
GetTargetTypeName( )
{
	string targetTypeStr;
	if( !EvaluateAttrString( ATTR_TARGET_TYPE, targetTypeStr ) ) {
		return NULL;
	}
	return targetTypeStr.c_str( );
}


// private methods
void ClassAd::
evalFromEnvironment( const char *name, Value val )
{
	if (strcmp (name, "CurrentTime") == 0)
	{
		time_t	now = time (NULL);
		if (now == (time_t) -1)
		{
			val.SetErrorValue( );
			return;
		}
		val.SetIntegerValue( ( int ) now );
		return;
	}

	val.SetUndefinedValue( );
	return;

}
