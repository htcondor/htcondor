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
#include "common.h"
#include "exprTree.h"

BEGIN_NAMESPACE( classad )

ExprList::
ExprList()
{
	nodeKind = EXPR_LIST_NODE;
}


ExprList::
~ExprList()
{
	Clear( );
}


void ExprList::
Clear ()
{
	vector<ExprTree*>::iterator itr;
	for( itr = exprList.begin( ); itr != exprList.end( ); itr++ ) {
		delete *itr;
	}
	exprList.clear( );
}

	
ExprList *ExprList::
Copy( ) const
{
	ExprList *newList = new ExprList;
	ExprTree *newTree;

	if (newList == 0) return 0;
	newList->parentScope = parentScope;

	vector<ExprTree*>::const_iterator itr;
	for( itr = exprList.begin( ); itr != exprList.end( ); itr++ ) {
		if( !( newTree = (*itr)->Copy( ) ) ) {
			delete newList;
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
			return((ExprList*)NULL);
		}
		newList->exprList.push_back( newTree );
	}
	return newList;
}


void ExprList::
_SetParentScope( const ClassAd *parent )
{
	vector<ExprTree*>::iterator	itr;
	for( itr = exprList.begin( ); itr != exprList.end( ); itr++ ) {
		(*itr)->SetParentScope( parent );
	}
}


ExprList *ExprList::
MakeExprList( const vector<ExprTree*> &exprs )
{
	vector<ExprTree*>::const_iterator itr;
	ExprList *el = new ExprList;
	if( !el ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( NULL );
	}
	for( itr=exprs.begin( ) ; itr!=exprs.end( ); itr++ ) {	
		el->exprList.push_back( *itr );
	}
	return( el );
}

void ExprList::
GetComponents( vector<ExprTree*> &exprs ) const
{
	vector<ExprTree*>::const_iterator itr;
	exprs.clear( );
	for( itr=exprList.begin( ); itr!=exprList.end( ); itr++ ) {
		exprs.push_back( *itr );
	}
}


bool ExprList::
_Evaluate (EvalState &, Value &val) const
{
	val.SetListValue (this);
	return( true );
}

bool ExprList::
_Evaluate( EvalState &, Value &val, ExprTree *&sig ) const
{
	val.SetListValue( this );
	return( ( sig = Copy( ) ) );
}

bool ExprList::
_Flatten( EvalState &state, Value &, ExprTree *&tree, int* ) const
{
	vector<ExprTree*>::const_iterator	itr;
	ExprTree	*expr, *nexpr;
	Value		tempVal;
	ExprList	*newList;

	if( ( newList = new ExprList( ) ) == NULL ) return false;

	for( itr = exprList.begin( ); itr != exprList.end( ); itr++ ) {
		expr = *itr;
		// flatten the constituent expression
		if( !expr->Flatten( state, tempVal, nexpr ) ) {
			delete newList;
			tree = NULL;
			return false;
		}

		// if only a value was obtained, convert to an expression
		if( !nexpr ) {
			nexpr = Literal::MakeLiteral( tempVal );
			if( !nexpr ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				delete newList;
				return false;
			}
		}
		// add the new expression to the flattened list
		newList->exprList.push_back( nexpr );
	}

	tree = newList;

	return true;
}


// Iterator implementation
ExprListIterator::
ExprListIterator( )
{
}


ExprListIterator::
ExprListIterator( const ExprList* l )
{
	Initialize( l );
}


ExprListIterator::
~ExprListIterator( )
{
}


void ExprListIterator::
Initialize( const ExprList *el )
{
	// initialize eval state
	l = el;
	state.cache.clear( );
	state.curAd = (ClassAd*)l->parentScope;
	state.SetRootScope( );

	// initialize list iterator
	itr = l->exprList.begin( );
}


void ExprListIterator::
ToFirst( )
{
	if( l ) itr = l->exprList.begin( );
}


void ExprListIterator::
ToAfterLast( )
{
	if( l ) itr = l->exprList.end( );
}


bool ExprListIterator::
ToNth( int n ) 
{
	if( l && n >= 0 && l->exprList.size( ) > (unsigned)n ) {
		itr = l->exprList.begin( ) + n;
		return( true );
	}
	itr = l->exprList.begin( );
	return( false );
}


const ExprTree* ExprListIterator::
NextExpr( )
{
	if( l && itr != l->exprList.end( ) ) {
		itr++;
		return( itr==l->exprList.end() ? NULL : *itr );
	}
	return( NULL );
}


const ExprTree* ExprListIterator::
CurrentExpr( ) const
{
	return( l && itr != l->exprList.end( ) ? *itr : NULL );
}


const ExprTree* ExprListIterator::
PrevExpr( )
{
	if( l && itr != l->exprList.begin( ) ) {
		itr++;
		return( *itr );
	}
	return( NULL );
}


bool ExprListIterator::
GetValue( Value& val, const ExprTree *tree, EvalState *es )
{
	Value				cv;
	EvalState			*currentState;
    EvalCache::iterator	itr;

	if( !tree ) return false;

	// if called from user code, es == NULL so we use &state instead
	currentState = es ? es : &state;

	// lookup in cache
	itr = currentState->cache.find( tree );

	// if found, return cached value
	if( itr != currentState->cache.end( ) ) {
		val.CopyFrom( itr->second );
		return true;
	} 

	// temporarily cache value as undef, so any circular refs in
    // Evaluate() will eval to undef rather than loop

	cv.SetUndefinedValue( );
	currentState->cache[ tree ] = cv;

	tree->Evaluate( *currentState, val );

	// replace temporary cached value (above) with actual evaluation
	currentState->cache[ tree ] = val;

	return true;
}


bool ExprListIterator::
NextValue( Value& val, EvalState *es )
{
	return GetValue( val, NextExpr( ), es );
}


bool ExprListIterator::
CurrentValue( Value& val, EvalState *es )
{
	return GetValue( val, CurrentExpr( ), es );
}


bool ExprListIterator::
PrevValue( Value& val, EvalState *es )
{
	return GetValue( val, PrevExpr( ), es );
}


bool ExprListIterator::
GetValue( Value& val, ExprTree*& sig, const ExprTree *tree, EvalState *es ) 
{
	Value				cv;
	EvalState			*currentState;
    EvalCache::iterator	itr;

	if( !tree ) return false;

	// if called from user code, es == NULL so we use &state instead
	currentState = es ? es : &state;

	// lookup in cache
	itr = currentState->cache.find( tree );

	// if found, return cached value
	if( itr != currentState->cache.end( ) ) {
		val.CopyFrom( itr->second );
		return true;
	} 

	// temporarily cache value as undef, so any circular refs in
    // Evaluate() will eval to undef rather than loop

	cv.SetUndefinedValue( );
	currentState->cache[ tree ] = cv;

	tree->Evaluate( *currentState, val, sig );

	// replace temporary cached value (above) with actual evaluation
	currentState->cache[ tree ] = val;

	return true;
}


bool ExprListIterator::
NextValue( Value& val, ExprTree*& sig, EvalState *es )
{
	return GetValue( val, sig, NextExpr( ), es );
}


bool ExprListIterator::
CurrentValue( Value& val, ExprTree*& sig, EvalState *es )
{
	return GetValue( val, sig, CurrentExpr( ), es );
}


bool ExprListIterator::
PrevValue( Value& val, ExprTree*& sig, EvalState *es )
{
	return GetValue( val, sig, PrevExpr( ), es );
}


bool ExprListIterator::
IsAtFirst( ) const
{
	return( l && itr == l->exprList.begin( ) );
}


bool ExprListIterator::
IsAfterLast( ) const
{
	return( l && itr == l->exprList.end( ) );
}

END_NAMESPACE // classad
