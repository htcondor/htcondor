/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_string.h"
#include "classad.h"
#include "classadItor.h"

extern "C" void to_lower (char *);	// from util_lib (config.c)

BEGIN_NAMESPACE( classad )

static bool isValidIdentifier( const string & );

ClassAd::
ClassAd ()
{
	nodeKind = CLASSAD_NODE;
}


ClassAd::
ClassAd (const ClassAd &ad)
{
	AttrList::const_iterator itr;

	nodeKind = CLASSAD_NODE;
	parentScope = ad.parentScope;
	
	for( itr = ad.attrList.begin( ); itr != ad.attrList.end( ); itr++ ) {
		attrList[itr->first] = itr->second->Copy( );
	}
}	


bool ClassAd::
CopyFrom( const ClassAd &ad )
{
	AttrList::const_iterator	itr;
	ExprTree 					*tree;

	Clear( );
	nodeKind = CLASSAD_NODE;
	parentScope = ad.parentScope;
	
	for( itr = ad.attrList.begin( ); itr != ad.attrList.end( ); itr++ ) {
		if( !( tree = itr->second->Copy( ) ) ) {
			Clear( );
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
			return( false );
		}
		attrList[itr->first] = tree;
	}

	return( true );
}


ClassAd::
~ClassAd ()
{
	Clear( );
}


void ClassAd::
Clear( )
{
	AttrList::iterator	itr;
	for( itr = attrList.begin( ); itr != attrList.end( ); itr++ ) {
		if( itr->second ) delete itr->second;
	}
	attrList.clear( );
}


ClassAd *ClassAd::
MakeClassAd( vector< pair< string, ExprTree* > > &attrs )
{
	vector< pair<string, ExprTree*> >::iterator	itr;
	ClassAd *newAd = new ClassAd( );

	if( !newAd ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( NULL );
	};

	for( itr = attrs.begin( ); itr != attrs.end( ); itr++ ) {
		if( !Insert( itr->first, itr->second ) ) {
			delete newAd;
			return( NULL );
		}
		itr->first = "";
		itr->second = NULL;
	}
	return( newAd );
}


void ClassAd::
GetComponents( vector< pair< string, ExprTree* > > &attrs ) const
{
	attrs.clear( );
	for( AttrList::const_iterator itr=attrList.begin(); itr!=attrList.end(); 
		itr++ ) {
			// make_pair is a STL function
		attrs.push_back( make_pair( itr->first, itr->second ) );
	}
}


// --- begin integer attribute insertion ----
bool ClassAd::
InsertAttr( const string &name, int value, NumberFactor f )
{
	Value val;
	val.SetIntegerValue( value );
	return( Insert( name, Literal::MakeLiteral( val, f ) ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, int value, 
	NumberFactor f )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value, f ) );
}
// --- end integer attribute insertion ---



// --- begin real attribute insertion ---
bool ClassAd::
InsertAttr( const string &name, double value, NumberFactor f )
{
	Value val;
	val.SetRealValue( value );
	return( Insert( name, Literal::MakeLiteral( val, f ) ) );
}
	

bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, double value, 
	NumberFactor f )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value, f ) );
}
// --- end real attribute insertion



// --- begin boolean attribute insertion
bool ClassAd::
InsertAttr( const string &name, bool value )
{
	Value val;
	val.SetBooleanValue( value );
	return( Insert( name, Literal::MakeLiteral( val ) ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, bool value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end boolean attribute insertion



// --- begin string attribute insertion
bool ClassAd::
InsertAttr( const string &name, const string &value )
{
	Value val;
	val.SetStringValue( value );
	return( Insert( name, Literal::MakeLiteral( val ) ) );
}


bool ClassAd::
DeepInsertAttr( ExprTree *scopeExpr, const string &name, const string &value )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->InsertAttr( name, value ) );
}
// --- end string attribute insertion



// --- begin expression insertion 
bool ClassAd::
Insert( const string &name, ExprTree *tree )
{
		// sanity checks
	if( name == "" ) {
		CondorErrno = ERR_MISSING_ATTRNAME;
		CondorErrMsg= "no attribute name when inserting expression in classad";
		return( false );
	}
	if( !tree ) {
		CondorErrno = ERR_BAD_EXPRESSION;
		CondorErrMsg = "no expression when inserting attribute " + name +
				" in classad";
		return( false );
	}
	if( !isValidIdentifier( name ) ) {
		CondorErrno = ERR_INVALID_IDENTIFIER;
		CondorErrMsg = "attribute name " +name+ " is not a valid identifier";
		return false;
	}

	// parent of the expression is this classad
	tree->SetParentScope( this );

		// check if attribute already exists in classad
	AttrList::iterator itr = attrList.find( name );
	if( itr != attrList.end( ) ) {
			// delete old expression and replace
		delete itr->second;
	}
	attrList[name] = tree;
	return( true );
}


bool ClassAd::
DeepInsert( ExprTree *scopeExpr, const string &name, ExprTree *tree )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->Insert( name, tree ) );
}
// --- end expression insertion


// --- begin lookup methods
ExprTree *ClassAd::
Lookup( const string &name ) const
{
	AttrList::const_iterator	itr = attrList.find( name );
	return( itr==attrList.end( ) ? NULL : itr->second );
}


ExprTree *ClassAd::
LookupInScope( const string &name, const ClassAd *&finalScope ) const
{
	EvalState	state;
	ExprTree	*tree;
	int			rval;

	state.SetScopes( this );
	rval = LookupInScope( name, tree, state );
	if( rval == EVAL_OK ) {
		finalScope = state.curAd;
		return( tree );
	}

	finalScope = NULL;
	return( NULL );
}


int ClassAd::
LookupInScope(const string &name, const ExprTree*& expr, EvalState &state)const
{
	extern int exprHash( const ExprTree* const&, int );
	HashTable<const ExprTree*,bool> superChase( 17, &exprHash );
	const ClassAd 	*current = this, *superScope;
	ExprTree		*super;
	bool			visited = false;
	Value			val;

	expr = NULL;

	while( !expr && current ) {
		// lookups/eval's being done in the 'current' ad
		state.curAd = current;

		// have we already visited this scope?
		if( superChase.lookup( current, visited ) < 0 ) {
			// not yet visited until now --- mark as visited
			if( superChase.insert( current, true ) < 0 ) {
				return( EVAL_ERROR );
			}
		} else if( visited ) {
			// have already visited this scope
			return( EVAL_UNDEF );
		}

		// lookup in current scope
		if( ( expr = current->Lookup( name ) ) ) {
			return( EVAL_OK );
		}

		// not in current scope; try superScope
		if( !( super = current->Lookup( "super" ) ) ) {
			// no explicit super attribute; get lexical parent
			superScope = current->parentScope;
		} else {
			// explicit super attribute
			if( !super->Evaluate( state, val ) ) {
				return( EVAL_FAIL );
			}

			if( !val.IsClassAdValue( superScope ) ) {
				return( val.IsUndefinedValue( ) ? EVAL_UNDEF : EVAL_ERROR );
			}
		}

		// Case insensitive to "reserved keywords"
		if( strcasecmp( name.c_str( ), "super" ) == 0 ) {
			// if the "super" attribute was requested ...
			expr = superScope;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if(strcasecmp(name.c_str( ),"toplevel")==0 || 
				strcasecmp(name.c_str( ),"root")==0){
			// if the "toplevel" attribute was requested ...
			expr = state.rootAd;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), "self" ) == 0 ) {
			// if the "self" ad was requested
			expr = state.curAd;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else if( strcasecmp( name.c_str( ), "parent" ) == 0 ) {
			// the lexical parent
			expr = state.curAd->parentScope;
			return( expr ? EVAL_OK : EVAL_UNDEF );
		} else {
			// continue searching from the superScope ...
			current = superScope;
		}
	}	

	return( EVAL_UNDEF );
}
// --- end lookup methods



// --- begin deletion methods 
bool ClassAd::
Delete( const string &name )
{
	AttrList::iterator itr = attrList.find( name );
	if( itr == attrList.end( ) ) {
		CondorErrno = ERR_MISSING_ATTRIBUTE;
		CondorErrMsg = "attribute " +name+ " not found to be deleted";
		return( false );
	}
	delete itr->second;
	attrList.erase( itr );
	return( true );
}

bool ClassAd::
DeepDelete( ExprTree *scopeExpr, const string &name )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( false );
	return( ad->Delete( name ) );
}
// --- end deletion methods



// --- begin removal methods
ExprTree *ClassAd::
Remove( const string &name )
{
	AttrList::iterator itr = attrList.find( name );
	if( itr == attrList.end( ) ) {
		return( NULL );
	}
	ExprTree *tree = itr->second;
	attrList.erase( itr );
	return( tree );
}

ExprTree *ClassAd::
DeepRemove( ExprTree *scopeExpr, const string &name )
{
	ClassAd *ad = _GetDeepScope( scopeExpr );
	if( !ad ) return( (ExprTree*)NULL );
	return( ad->Remove( name ) );
}
// --- end removal methods


void ClassAd::
_SetParentScope( const ClassAd* )
{
	// already set by base class for this node; we shouldn't propagate 
	// the call to sub-expressions because this is a new scope
}


void ClassAd::
Update( const ClassAd& ad )
{
	AttrList::const_iterator itr;
	for( itr=ad.attrList.begin( ); itr!=ad.attrList.end( ); itr++ ) {
		attrList[itr->first] = itr->second->Copy( );
	}
}

void ClassAd::
Modify( ClassAd& mod )
{
	ClassAd			*ctx;
	const ExprTree	*expr;
	Value			val;

		// Step 0:  Determine Context
	if( ( expr = mod.Lookup( ATTR_CONTEXT ) ) != NULL ) {
		if( ( ctx = _GetDeepScope( (ExprTree*) expr ) ) == NULL ) {
			return;
		}
	} else {
		ctx = this;
	}

		// Step 1:  Process Replace attribute
	if( ( expr = mod.Lookup( ATTR_REPLACE ) ) != NULL ) {
		ClassAd	*ad;
		if( expr->Evaluate( val ) && val.IsClassAdValue( ad ) ) {
			ctx->Clear( );
			ctx->Update( *ad );
		}
	}

		// Step 2:  Process Updates attribute
	if( ( expr = mod.Lookup( ATTR_UPDATES ) ) != NULL ) {
		ClassAd *ad;
		if( expr->Evaluate( val ) && val.IsClassAdValue( ad ) ) {
			ctx->Update( *ad );
		}
	}

		// Step 3:  Process Deletes attribute
	if( ( expr = mod.Lookup( ATTR_DELETES ) ) != NULL ) {
		ExprList 			*list;
		ExprListIterator	itor;
		char				*attrName;

			// make a first pass to check that it is a list of strings ...
		if( !expr->Evaluate( val ) || !val.IsListValue( list ) ) {
			return;
		}
		itor.Initialize( list );
		while( ( expr = itor.NextExpr( ) ) ) {
			if( !expr->Evaluate( val ) || !val.IsStringValue( attrName ) ) {
				return;
			}
		}

			// now go through and delete all the named attributes ...
		itor.Initialize( list );
		while( ( expr = itor.NextExpr( ) ) ) {
			if( expr->Evaluate( val ) && val.IsStringValue( attrName ) ) {
				ctx->Delete( attrName );
			}
		}
	}

	/*
		// Step 4:  Process DeepModifications attribute
	if( ( expr = mod.Lookup( ATTR_DEEP_MODS ) ) != NULL ) {
		ExprList			*list;
		ExprListIterator	itor;
		ClassAd				*ad;

		if( !expr->Evaluate( val ) || !val.IsListValue( list ) ) {
			return( false );
		}
		itor.Initialize( list );
		while( ( expr = itor.NextExpr( ) ) ) {
			if( !expr->Evaluate( val ) 		|| 
				!val.IsClassAdValue( ad ) 	|| 
				!Modify( *ad ) ) {
				return( false );
			}
		}
	}

	*/
}


ClassAd* ClassAd::
Copy( ) const
{
	ExprTree *tree;
	ClassAd	*newAd = new ClassAd();

	if( !newAd ) return NULL;
	newAd->nodeKind = CLASSAD_NODE;
	newAd->parentScope = parentScope;

	AttrList::const_iterator	itr;
	for( itr=attrList.begin( ); itr != attrList.end( ); itr++ ) {
		if( !( tree = itr->second->Copy( ) ) ) {
			delete newAd;
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
			return( NULL );
		}
		newAd->attrList[itr->first] = tree;
	}
	return newAd;
}


bool ClassAd::
_Evaluate( EvalState&, Value& val ) const
{
	val.SetClassAdValue( this );
	return( this );
}


bool ClassAd::
_Evaluate( EvalState&, Value &val, ExprTree *&tree ) const
{
	val.SetClassAdValue( this );
	return( ( tree = Copy( ) ) );
}


bool ClassAd::
_Flatten( EvalState& state, Value&, ExprTree*& tree, OpKind* )  const
{
	ClassAd 		*newAd = new ClassAd();
	Value			eval;
	ExprTree		*etree;
	const ClassAd	*oldAd;
	AttrList::const_iterator	itr;

	oldAd = state.curAd;
	state.curAd = this;

	for( itr = attrList.begin( ); itr != attrList.end( ); itr++ ) {
		// flatten expression
		if( !itr->second->Flatten( state, eval, etree ) ) {
			delete newAd;
			tree = NULL;
			eval.Clear();
			state.curAd = oldAd;
			return false;
		}

		// if a value was obtained, convert it to a literal
		if( !etree ) {
			etree = Literal::MakeLiteral( eval );
			if( !etree ) {
				delete newAd;
				tree = NULL;
				eval.Clear();
				state.curAd = oldAd;
				return false;
			}
		}
		newAd->attrList[itr->first] = etree;
		eval.Clear( );
	}

	tree = newAd;
	state.curAd = oldAd;
	return true;
}


ClassAd *ClassAd::
_GetDeepScope( ExprTree *tree ) const
{
	ClassAd	*scope;
	Value	val;

	if( !tree ) return( NULL );
	tree->SetParentScope( this );
	if( !tree->Evaluate( val ) || !val.IsClassAdValue( scope ) ) {
		return( NULL );
	}
	return( scope );
}


bool ClassAd::
EvaluateAttr( const string &attr , Value &val ) const
{
	EvalState	state;
	ExprTree	*tree;

	state.SetScopes( this );
	switch( LookupInScope( attr, tree, state ) ) {	
		case EVAL_FAIL:
			return false;

		case EVAL_OK:
			return( tree->Evaluate( state, val ) );

		case EVAL_UNDEF:
			val.SetUndefinedValue( );
			return( true );

		case EVAL_ERROR:
			val.SetErrorValue( );
			return( true );

		default:
			return false;
	}
}


bool ClassAd::
EvaluateExpr( ExprTree *tree , Value &val ) const
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Evaluate( state , val ) );
}

bool ClassAd::
EvaluateExpr( ExprTree *tree , Value &val , ExprTree *&sig ) const
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Evaluate( state , val , sig ) );
}

bool ClassAd::
EvaluateAttrInt( const string &attr, int &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsIntegerValue( i ) );
}

bool ClassAd::
EvaluateAttrReal( const string &attr, double &r )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsRealValue( r ) );
}

bool ClassAd::
EvaluateAttrNumber( const string &attr, int &i )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( i ) );
}

bool ClassAd::
EvaluateAttrNumber( const string &attr, double &r )  const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsNumber( r ) );
}

bool ClassAd::
EvaluateAttrString( const string &attr, string &buf ) const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsStringValue( buf ) );
}


bool ClassAd::
EvaluateAttrBool( const string &attr, bool &b ) const
{
	Value val;
	return( EvaluateAttr( attr, val ) && val.IsBooleanValue( b ) );
}

bool ClassAd::
Flatten( ExprTree *tree , Value &val , ExprTree *&fexpr ) const
{
	EvalState	state;

	state.SetScopes( this );
	return( tree->Flatten( state , val , fexpr ) );
}

bool ClassAdIterator::
NextAttribute( string &attr, const ExprTree *&expr )
{
	if (!ad) return false;

	attr = "";
	expr = NULL;
	if( itr==ad->attrList.end( ) ) return( false );
	itr++;
	if( itr==ad->attrList.end( ) ) return( false );
	attr = itr->first;
	expr = itr->second;
	return( true );
}


bool ClassAdIterator::
PreviousAttribute( string &attr, const ExprTree *&expr )
{
	if (!ad) return false;

	attr = "";
	expr = NULL;
	if( itr == ad->attrList.begin( ) ) return( false );
	itr--;
	attr = itr->first;
	expr = itr->second;
	return( true );
}


bool ClassAdIterator::
CurrentAttribute (string &attr, const ExprTree *&expr) const
{
	if (!ad ) return( false );
	attr = itr->first;
	expr = itr->second;
	return true;	
}

static bool 
isValidIdentifier( const string &str )
{
	const char *ch = str.c_str( );

		// must start with [a-zA-Z_]
	if( !isalpha( *ch ) && *ch != '_' ) {
		return( false );
	}

		// all other characters must be [a-zA-Z0-9_]
	ch++;
	while( isalnum( *ch ) || *ch == '_' ) {
		ch++;
	}

		// valid if terminated at end of string
	return( *ch == '\0' );
}

END_NAMESPACE // classad
