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
	ExprTree	*tree; 
	ExprTree	*dummy;
	ClassAd		*curAd;
	bool		rval;
	Value		cv;
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
				state.curAd = curAd;		// NAC (fixed a bug)
				return true;
			} 

			// temporarily cache value as undef, so any circular refs
			// in Evaluate() will eval to undef rather than loop

			cv.SetUndefinedValue( );
			state.cache[ tree ] = cv;

			rval = tree->Evaluate( state , val );

			// replace cached undef with actual evaluation
			state.cache[ tree ] = val;

			state.curAd = curAd;

			return rval;

		default:  EXCEPT( "ClassAd:  Should not reach here" );
	}
	return false;
}


bool AttributeReference::
_Evaluate (EvalState &state, Value &val, ExprTree *&sig ) const
{
	ExprTree	*tree;
	ExprTree	*exprSig;
	ClassAd		*curAd;
	bool		rval;
	Value 		cv;
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
				state.curAd = curAd;		// NAC (fixed a bug)
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
_Flatten( EvalState &state, Value &val, ExprTree*&ntree, int*) const
{
	ExprTree	*tree;
	ExprTree	*dummy;
	ClassAd		*curAd;
	bool		rval;
	Value		cv;

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
	ClassAd	*current=NULL;
	const ExprList *adList = NULL;
	Value	val;
	bool	rval;

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
		
		if( !val.IsClassAdValue( current ) && !val.IsListValue( adList ) ) {
			return( EVAL_ERROR );
		}
	}

	if( val.IsListValue( ) ) {
		vector< ExprTree *> eVector;
		const ExprTree *currExpr;
			// iterate through exprList and apply attribute reference
			// to each exprTree
		for(ExprListIterator itr(adList);!itr.IsAfterLast( );itr.NextExpr( )){
 			currExpr = itr.CurrentExpr( );
			if( currExpr == NULL ) {
				return( EVAL_FAIL );
			} else {
				AttributeReference *attrRef = NULL;
				attrRef = MakeAttributeReference( currExpr->Copy( ),
												  attributeStr,
												  false );
				attrRef->SetParentScope( currExpr->GetParentScope( ) );
				val.Clear( );
				rval = wantSig ? attrRef->Evaluate( state, val, sig )
					: attrRef->Evaluate( state, val );
				if( !rval ) {
					return( EVAL_FAIL );
				}
				
				ClassAd *evaledAd = NULL;
				const ExprList *evaledList = NULL;
				if( val.IsClassAdValue( evaledAd ) ) {
					eVector.push_back( evaledAd );
					continue;
				}
				else if( val.IsListValue( evaledList ) ) {
					eVector.push_back( evaledList->Copy( ) );
					continue;
				}
				else {
					eVector.push_back( Literal::MakeLiteral( val ) );
				}
			}
		}
		tree = ExprList::MakeExprList( eVector );
		tree->SetParentScope( adList->GetParentScope( ) );
		return EVAL_OK;
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
