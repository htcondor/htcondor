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
#include "stringSpace.h"
#include "exprTree.h"

BEGIN_NAMESPACE( classad )

extern int exprHash( const ExprTree* const&, int );

ExprTree::
ExprTree ()
{
	parentScope = NULL;
}


ExprTree::
~ExprTree()
{
}


bool ExprTree::
Evaluate (EvalState &state, Value &val) const
{
	Value 				cv;
	bool				rval;
	EvalCache::iterator	itr = state.cache.find( this );

	if( itr != state.cache.end( ) ) {
		// found in cache; return cached value
		val.CopyFrom( itr->second );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.SetUndefinedValue( );
	state.cache[ this ] = cv;

	// evaluate the expression
	rval = _Evaluate( state, val );

	// replace undefined value with actual value 
	state.cache[ this ] = val;

	return( rval );
}

bool ExprTree::
Evaluate( EvalState &state, Value &val, ExprTree *&sig ) const
{
	Value 				cv;
	bool				rval;
	EvalCache::iterator	itr = state.cache.find( this );

	if( itr != state.cache.end( ) ) {
		// found in cache; return cached value
		val.CopyFrom( itr->second );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.SetUndefinedValue( );
	state.cache[ this ] = cv;

	// evaluate the expression
	rval = _Evaluate( state, val, sig );

	// replace undefined value with actual value 
	state.cache[ this ] = val;

	return( rval );
}


void ExprTree::
SetParentScope( const ClassAd* scope ) 
{
	parentScope = scope;
	_SetParentScope( scope );
}


bool ExprTree::
Evaluate( Value& val ) const
{
	EvalState 	state;

	state.SetScopes( parentScope );
	return( Evaluate( state, val ) );
}


bool ExprTree::
Evaluate( Value& val, ExprTree*& sig ) const
{
	EvalState 	state;

	state.SetScopes( parentScope );
	return( Evaluate( state, val, sig  ) );
}


bool ExprTree::
Flatten( Value& val, ExprTree *&tree )
{
	EvalState state;

	state.SetScopes( parentScope );
	return( Flatten( state, val, tree ) );
}


bool ExprTree::
Flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind* op) const
{
	Value				cv;
	bool				rval;
	EvalCache::iterator	itr = state.cache.find( this );

	if( itr != state.cache.end( ) ) {
		// found in cache; return cached value
		val.CopyFrom( itr->second );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.SetUndefinedValue( );
	state.cache[ this ] = cv;

	// flatten the expression
	rval = _Flatten( state, val, tree, op );

	// cache the value
	state.cache[ this ] = val;

	return( rval );
}


int 
exprHash( const ExprTree* const& expr, int numBkts ) 
{
	unsigned char *ptr = (unsigned char*) &expr;
	int	result = 0;
	
	for( int i = 0 ; (unsigned)i < sizeof( expr ) ; i++ ) result += ptr[i];
	return( result % numBkts );
}



EvalState::
EvalState( )
{
	rootAd = NULL;
	curAd  = NULL;
}

EvalState::
~EvalState( )
{
}

void EvalState::
SetScopes( const ClassAd *curScope )
{
	curAd = curScope;
	SetRootScope( );
}


void EvalState::
SetRootScope( )
{
	const ClassAd *prevScope = curAd, *curScope = curAd->parentScope;

	while( curScope ) {
		prevScope = curScope;
		curScope  = curScope->parentScope;
	}

	rootAd = prevScope;
}

END_NAMESPACE // classad
