/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#include "common.h"
#include "exprTree.h"

using namespace std;

BEGIN_NAMESPACE( classad )

ExprList::
ExprList()
{
	nodeKind = EXPR_LIST_NODE;
	return;
}

ExprList::
ExprList(std::vector<ExprTree*>& list)
{
    std::copy(list.begin(), list.end(), std::back_inserter(exprList));
	return;
}

ExprList::
ExprList(const ExprList &other_list)
{
    CopyFrom(other_list);
    return;
}

ExprList::
~ExprList()
{
	Clear( );
}

ExprList &ExprList::
operator=(const ExprList &other_list)
{
    if (this != &other_list) {
        CopyFrom(other_list);
    }
    return *this;
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

#ifdef USE_COVARIANT_RETURN_TYPES
ExprList *ExprList::
#else
ExprTree *ExprList::
#endif
Copy( ) const
{
	ExprList *newList = new ExprList;

	if (newList != NULL) {
        if (!newList->CopyFrom(*this)) {
            delete newList;
            newList = NULL;
        }
    }
	return newList;
}


bool ExprList::
CopyFrom(const ExprList &other_list)
{
    bool success;

    success = true;

	parentScope = other_list.parentScope;


	vector<ExprTree*>::const_iterator itr;
	for( itr = other_list.exprList.begin( ); itr != other_list.exprList.end( ); itr++ ) {
        ExprTree *newTree;
		if( !( newTree = (*itr)->Copy( ) ) ) {
            success = false;
			CondorErrno = ERR_MEM_ALLOC_FAILED;
			CondorErrMsg = "";
            break;
		}
		exprList.push_back( newTree );
	}

    return success;

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
	return;
}

void ExprList::
insert(iterator it, ExprTree* t)
{
    exprList.insert(it, t);
	return;
}

void ExprList::
push_back(ExprTree* t)
{
    exprList.push_back(t);
	return;
}

void ExprList::
erase(iterator it)
{
    delete *it;
    exprList.erase(it);
	return;
}

void ExprList::
erase(iterator f, iterator l)
{
    for (iterator it = f; it != l; ++it) {
		delete *it;
    }
	
    exprList.erase(f,l);
	return;
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

	tree = NULL; // Just to be safe...  wenger 2003-12-11.

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
	l = NULL;
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
	
	const ClassAd *tmpScope = currentState->curAd;
	currentState->curAd = (ClassAd*) tree->GetParentScope();
	tree->Evaluate( *currentState, val );
	currentState->curAd = (ClassAd*) tmpScope;

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

	const ClassAd *tmpScope = currentState->curAd;
	currentState->curAd = (ClassAd*) tree->GetParentScope();
	tree->Evaluate( *currentState, val, sig );
	currentState->curAd = (ClassAd*) tmpScope;

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
