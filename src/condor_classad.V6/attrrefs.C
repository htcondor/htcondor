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
#include "classad.h"

BEGIN_NAMESPACE( classad )

AttributeReference::
AttributeReference()
{
	nodeKind = ATTRREF_NODE;
	expr = NULL;
	absolute = false;
}


// a private ctor for use in significant expr identification
AttributeReference::
AttributeReference( ExprTree *tree, const string &attrname, bool absolut )
{
	nodeKind = ATTRREF_NODE;
	attributeStr = attrname;
	expr = tree;
	absolute = absolut;
}


AttributeReference::
~AttributeReference()
{
	if( expr ) delete expr;
}


AttributeReference *AttributeReference::
Copy( ) const
{
	AttributeReference *newTree = new AttributeReference ();
	if (newTree == 0) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return NULL;
	}

	newTree->attributeStr = attributeStr;
	if( expr && ( newTree->expr=expr->Copy( ) ) == NULL ) {
		delete newTree;
		return NULL;
	}

	newTree->nodeKind = nodeKind;
	newTree->parentScope = parentScope;
	newTree->absolute = absolute;

	return newTree;
}


void AttributeReference::
_SetParentScope( const ClassAd *parent ) 
{
	if( expr ) expr->SetParentScope( parent );
}


void AttributeReference::
GetComponents( ExprTree *&tree, string &attr, bool &abs ) const
{
	tree = expr;
	attr = attributeStr;
	abs = absolute;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val) const
{
	ExprTree			*tree, *dummy;
	Value				cv;
	const ClassAd		*curAd;
	bool				rval;
	EvalCache::iterator	itr;

	// find the expression and the evalstate
	curAd = state.curAd;
	switch( FindExpr( state, tree, dummy, false ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
		case PROP_ERROR:
			val.SetErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
		case PROP_UNDEF:
			val.SetUndefinedValue();
			state.curAd = curAd;
			return true;

		case EVAL_OK:
			// lookup in cache
			itr = state.cache.find( tree );

			// if found, return cached value
			if( itr != state.cache.end( ) ) {
				val.CopyFrom( itr->second );
				return true;
			} 

			// temporarily cache value as undef, so any circular refs
			// in Evaluate() will eval to undef rather than loop

			cv.SetUndefinedValue( );
			state.cache[ tree ] = cv;

			rval = tree->Evaluate( state , val );

			// replace cached undef with actual evaluation
			// *** if Evaluate() just returned false, is this still ok? ***
			state.cache[ tree ] = val;

			// *** if Evaluate() just returned false, is this still ok? ***
			state.curAd = curAd;
			return rval;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val, ExprTree *&sig ) const
{
	ExprTree			*tree, *exprSig;
	Value 				cv;
	const ClassAd		*curAd;
	bool				rval;
	EvalCache::iterator	itr;

	curAd = state.curAd;
	exprSig = NULL;
	rval 	= true;

	switch( FindExpr( state , tree , exprSig , true ) ) {
		case EVAL_FAIL:
			rval = false;
			break;

		case EVAL_ERROR:
		case PROP_ERROR:
			val.SetErrorValue( );
			break;

		case EVAL_UNDEF:
		case PROP_UNDEF:
			val.SetUndefinedValue( );
			break;

		case EVAL_OK:

			// lookup in cache
			itr = state.cache.find( tree );

			// if found in cache, return cached value
			if( itr != state.cache.end( ) ) {
				val.CopyFrom( itr->second );
				return true;
			} 

			// temporarily cache value as undef, so any circular refs
			// in Evaluate() will eval to undef rather than loop

			cv.SetUndefinedValue( );
			state.cache[ tree ] = cv;

			rval = tree->Evaluate( state, val );

			// replace undef in cache with actual evaluation
			state.cache[ tree ] = val;

			break;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	if(!rval || !(sig=new AttributeReference(exprSig,attributeStr,absolute))){
		if( rval ) {
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
		}
		delete exprSig;
		sig = NULL;
		return( false );
	}
	state.curAd = curAd;
	return rval;
}


bool AttributeReference::
_Flatten( EvalState &state, Value &val, ExprTree*&ntree, OpKind*) const
{
	ExprTree		*tree, *dummy;
	Value			cv;
	const ClassAd	*curAd;
	bool			rval;

		// find the expression and the evalstate
	curAd = state.curAd;
	switch( FindExpr( state, tree, dummy, false ) ) {
		case EVAL_FAIL:
			return false;

		case EVAL_ERROR:
		case PROP_ERROR:
			val.SetErrorValue();
			state.curAd = curAd;
			return true;

		case EVAL_UNDEF:
		case PROP_UNDEF:
			if( !(ntree = Copy( ) ) ) {
				CondorErrno = ERR_MEM_ALLOC_FAILED;
				CondorErrMsg = "";
				return( false );
			}
			state.curAd = curAd;
			return true;

		case EVAL_OK:

			// temporarily cache value as undef, so any circular refs
			// in Evaluate() will eval to undef rather than loop

			cv.SetUndefinedValue( );
			state.cache[ tree ] = cv;

			rval = tree->Flatten( state, val, ntree );

			// don't inline if it didn't flatten to a value, and clear cache
			if( ntree ) {
				delete ntree;
				ntree = Copy( );
				val.SetUndefinedValue( );
				state.cache.erase( tree );
			} else {
				state.cache[ tree ] = val;
			}

			state.curAd = curAd;
			return rval;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


int AttributeReference::
FindExpr(EvalState &state, ExprTree *&tree, ExprTree *&sig, bool wantSig) const
{
	const ClassAd 	*current=NULL;
	Value			val;
	bool			rval;

	sig = NULL;

	// establish starting point for search
	if( expr == NULL ) {
		// "attr" and ".attr"
		current = absolute ? state.rootAd : state.curAd;
	} else {
		// "expr.attr"
		rval=wantSig?expr->Evaluate(state,val,sig):expr->Evaluate(state,val);
		if( !rval ) {
			return( EVAL_FAIL );
		}

		if( val.IsUndefinedValue( ) ) {
			return( PROP_UNDEF );
		} else if( val.IsErrorValue( ) ) {
			return( PROP_ERROR );
		}

		if( !val.IsClassAdValue( current ) ) {
			return( EVAL_ERROR );
		}
	}

	// lookup with scope; this may side-affect state
	return( current->LookupInScope( attributeStr, tree, state ) );
}


AttributeReference *AttributeReference::
MakeAttributeReference(ExprTree *tree, const string &attrStr, bool absolut)
{
	return( new AttributeReference( tree, attrStr, absolut ) );
}

END_NAMESPACE // classad
