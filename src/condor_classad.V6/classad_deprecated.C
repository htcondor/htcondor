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
	nodeKind = CLASSAD_NODE;

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
LookupString(const char *name, char *value, int max_len)
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	strncpy( value, strVal.c_str( ), max_len );
	return 1;
}

int ClassAd::
LookupString (const char *name, char **value)
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	const char *strValCStr = strVal.c_str( );
	*value = (char *) malloc(strlen(strValCStr) + 1);
	if (*value != NULL) {
		strcpy(*value, strValCStr);
		return 1;
	}

	return 0;
}

int ClassAd::
LookupInteger( const char *name, int &value )
{
	bool    boolVal;
	int     haveInteger;
	string  sName;

	sName = string(name);
	if( EvaluateAttrInt(sName, value ) ) {
		haveInteger = TRUE;
	} else if( EvaluateAttrBool(sName, boolVal ) ) {
		value = boolVal ? 1 : 0;
		haveInteger = TRUE;
	} else {
		haveInteger = FALSE;
	}
	return haveInteger;
}

int ClassAd::
LookupFloat( const char *name, float &value )
{
	double  doubleVal;
	int     intVal;
	int     haveFloat;

	if(EvaluateAttrReal( string( name ), doubleVal ) ) {
		haveFloat = TRUE;
		value = (float) doubleVal;
	} else if(EvaluateAttrInt( string( name ), intVal ) ) {
		haveFloat = TRUE;
		value = (float)intVal;
	} else {
		haveFloat = FALSE;
	}
	return haveFloat;
}

int ClassAd::
LookupBool( const char *name, int &value )
{
	int   intVal;
	bool  boolVal;
	int haveBool;
	string sName;

	sName = string(name);

	if (EvaluateAttrBool(name, boolVal)) {
		haveBool = true;
		value = boolVal ? 1 : 0;
	} else if (EvaluateAttrInt(name, intVal)) {
		haveBool = true;
		value = (intVal != 0) ? 1 : 0;
	} else {
		haveBool = false;
		value = 0;
	}
	return haveBool;
}

int ClassAd::
EvalString( const char *name, class ClassAd *target, char *value )
{
	ExprTree *tree;
	Value val;
	ClassAd *env;
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

	if( target ) {
		env = target;
	}
	else {
		env = this;
	}

	if( env->EvaluateExpr( tree, val ) && 
		val.IsStringValue( strVal ) ) {
		strcpy( value, strVal.c_str( ) );
		return 1;
	}

	return 0;
}

int ClassAd::
EvalInteger (const char *name, class ClassAd *target, int &value) {
	ExprTree *tree;
	Value val;
	ClassAd *env;

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

	if( target ) {
		env = target;
	}
	else {
		env = this;
	}

	if( env->EvaluateExpr( tree, val ) && 
		val.IsIntegerValue( value ) ) {
		return 1;
	}

	return 0;
}

int ClassAd::
EvalFloat (const char *name, class ClassAd *target, float &value)
{
	ExprTree *tree;
	Value val;
	ClassAd *env;
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

	if( target ) {
		env = target;
	}
	else {
		env = this;
	}

	if( env->EvaluateExpr( tree, val ) ) {
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
	ClassAd *env;
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

	if( target ) {
		env = target;
	}
	else {
		env = this;
	}

	if( env->EvaluateExpr( tree, val ) ) {
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
	ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	string buffer;

	if( !f ) {
		return FALSE;
	}

	unp.Unparse( buffer, this );
	fprintf( f, "%s", buffer.c_str( ) );

	return TRUE;
}

void ClassAd::
dPrint( int level )
{
	ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	string buffer;

	unp.Unparse( buffer, this );
	dprintf( level, "%s", buffer.c_str( ) );

	return;
}


// ClassAd methods

		// Type operations
void ClassAd::
SetMyTypeName( const char *myType )
{
	if( myType ) {
		InsertAttr( ATTR_MY_TYPE, string( myType ) );
	}

	return;
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

	return;
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

// Back compatibility helper methods

bool ClassAd::
AddExplicitConditionals( ExprTree *expr, ExprTree *&newExpr )
{
	if( expr == NULL ) {
		return false;
	}
	newExpr = AddExplicitConditionals( expr );
	return true;
}

ClassAd *ClassAd::
AddExplicitTargetRefs( )
{
	string attr = "";
	set< string, CaseIgnLTStr > definedAttrs;
	
	for( AttrList::iterator a = begin( ); a != end( ); a++ ) {
		definedAttrs.insert( a->first );
	}
	
	ClassAd *newAd = new ClassAd( );
	for( AttrList::iterator a = begin( ); a != end( ); a++ ) {
		newAd->Insert( a->first, 
					   AddExplicitTargetRefs( a->second, definedAttrs ) );
	}
	return newAd;
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

ExprTree *ClassAd::
AddExplicitConditionals( ExprTree *expr )
{
	if( expr == NULL ) {
		return NULL;
	}
	ExprTree *currentExpr = expr;
	ExprTree::NodeKind nKind = expr->GetKind( );
	switch( nKind ) {
	case ExprTree::ATTRREF_NODE: {
			// replace "attr" with "(IsBoolean(attr) ? ( attr ? 1 : 0) : attr)"
		ExprTree *fnExpr = NULL;
		vector< ExprTree * > params( 1 );
		params[0] = expr->Copy( );
		ExprTree *condExpr = NULL;
		ExprTree *parenExpr = NULL;
		ExprTree *condExpr2 = NULL;
		ExprTree *parenExpr2 = NULL;
		Value val0, val1;
		val0.SetIntegerValue( 0 );
		val1.SetIntegerValue( 1 );
		fnExpr = FunctionCall::MakeFunctionCall( "IsBoolean", params );
		condExpr = Operation::MakeOperation( Operation::TERNARY_OP,
											 expr->Copy( ), 
											 Literal::MakeLiteral( val1 ),
											 Literal::MakeLiteral( val0 ) );
		parenExpr = Operation::MakeOperation( Operation::PARENTHESES_OP,
											  condExpr, NULL, NULL );
		condExpr2 = Operation::MakeOperation( Operation::TERNARY_OP,
											  fnExpr, parenExpr, 
											  expr->Copy( ) );
		parenExpr2 = Operation::MakeOperation( Operation::PARENTHESES_OP,
										 condExpr2, NULL, NULL );
		return parenExpr2;
	}
	case ExprTree::FN_CALL_NODE:
	case ExprTree::CLASSAD_NODE:
	case ExprTree::EXPR_LIST_NODE: {
		return NULL;
	}
	case ExprTree::LITERAL_NODE: {
		Value val;
		( ( Literal *)expr )->GetValue( val );
		bool b;
		if( val.IsBooleanValue( b ) ) {
			if( b ) {
					// replace "true" with "1"
				val.SetIntegerValue( 1 );
			}
			else {
					// replace "false" with "0"
				val.SetIntegerValue( 0 );
			}
			return Literal::MakeLiteral( val );
		}
		else {
			return NULL;
		}
	}
	case ExprTree::OP_NODE: {
		Operation::OpKind oKind;
		ExprTree * expr1 = NULL;
		ExprTree * expr2 = NULL;
		ExprTree * expr3 = NULL;
		( ( Operation * )expr )->GetComponents( oKind, expr1, expr2, expr3 );
		while( oKind == Operation::PARENTHESES_OP ) {
			currentExpr = expr1;
			( ( Operation * )expr1 )->GetComponents(oKind,expr1,expr2,expr3);
		}
		if( ( Operation::__COMPARISON_START__ <= oKind &&
			  oKind <= Operation::__COMPARISON_END__ ) ||
			( Operation::__LOGIC_START__ <= oKind &&
			  oKind <= Operation::__LOGIC_END__ ) ) {
				// Comparison/Logic Operation expression
				// replace "expr" with "expr ? 1 : 0"
			Value val0, val1;
			val0.SetIntegerValue( 0 );
			val1.SetIntegerValue( 1 );
			ExprTree *tern = NULL;
			tern = Operation::MakeOperation( Operation::TERNARY_OP,
											 expr->Copy( ),
											 Literal::MakeLiteral( val1 ),
											 Literal::MakeLiteral( val0 ) );
			return Operation::MakeOperation( Operation::PARENTHESES_OP,
											 tern, NULL, NULL );
		}
		else if( Operation::__ARITHMETIC_START__ <= oKind &&
				 oKind <= Operation::__ARITHMETIC_END__ ) {
			ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
			if( oKind == Operation::UNARY_PLUS_OP || 
				oKind == Operation::UNARY_MINUS_OP ) {
				if( newExpr1 != NULL ) {
					return Operation::MakeOperation(oKind,newExpr1,NULL,NULL);
				}
				else {
					return NULL;
				}
			}
			else {
				ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
				if( newExpr1 != NULL || newExpr2 != NULL ) {
					if( newExpr1 == NULL ) {
						newExpr1 = expr1->Copy( );
					}
					if( newExpr2 == NULL ) {
						newExpr2 = expr2->Copy( );
					}
					return Operation::MakeOperation( oKind, newExpr1, newExpr2,
													 NULL );
				}
				else {
					return NULL;
				}
			}
		}
		else if( oKind == Operation::TERNARY_OP ) {
			ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
			ExprTree *newExpr3 = AddExplicitConditionals( expr3 );
			if( newExpr2 != NULL || newExpr3 != NULL ) {
				if( newExpr2 == NULL ) {
					newExpr2 = expr2->Copy( );
				}
				if( newExpr3 == NULL ) {
					newExpr3 = expr3->Copy( );
				}
				return Operation::MakeOperation( oKind, expr1->Copy( ), 
												 newExpr2, newExpr3 );
			}
			else {
				return NULL;
			}
		}
		return NULL;
	}
	default: {
		return NULL;
	}
	}
		
	return NULL;
}

ExprTree *ClassAd::
AddExplicitTargetRefs( ExprTree *tree, set<string,CaseIgnLTStr> &definedAttrs )
{
	if( tree == NULL ) {
		return NULL;
	}
	ExprTree::NodeKind nKind = tree->GetKind( );
	switch( nKind ) {
	case ExprTree::ATTRREF_NODE: {
		ExprTree *expr = NULL;
		string attr = "";
		bool abs = false;
		( ( AttributeReference * )tree )->GetComponents(expr,attr,abs);
		if( abs || expr != NULL ) {
			return tree->Copy( );
		}
		else {
			if( definedAttrs.find( attr ) == definedAttrs.end( ) ) {
					// attribute is not defined, so insert "target"
				AttributeReference *target = NULL;
				target = AttributeReference::MakeAttributeReference(NULL,
																	"target");
				return AttributeReference::MakeAttributeReference(target,attr);
			}
			else {
				return tree->Copy( );
			}
		}
	}
	case ExprTree::OP_NODE: {
		Operation::OpKind oKind;
		ExprTree * expr1 = NULL;
		ExprTree * expr2 = NULL;
		ExprTree * expr3 = NULL;
		ExprTree * newExpr1 = NULL;
		ExprTree * newExpr2 = NULL;
		ExprTree * newExpr3 = NULL;
		( ( Operation * )tree )->GetComponents( oKind, expr1, expr2, expr3 );
		if( expr1 != NULL ) {
			newExpr1 = AddExplicitTargetRefs( expr1, definedAttrs );
		}
		if( expr2 != NULL ) {
			newExpr2 = AddExplicitTargetRefs( expr2, definedAttrs );
		}
		if( expr3 != NULL ) {
			newExpr3 = AddExplicitTargetRefs( expr3, definedAttrs );
		}
		return Operation::MakeOperation( oKind, newExpr1, newExpr2, newExpr3 );
	}
	default: {
 			// old ClassAds have no function calls, nested ClassAds or lists
			// literals have no attrrefs in them
		return tree->Copy( );
	}
	}
}

