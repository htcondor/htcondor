#include "condor_common.h"
#include "stringSpace.h"
#include "exprTree.h"

namespace classad {

extern int exprHash( ExprTree* const&, int );

ExprTree::
ExprTree ()
{
	flattenFlag = false;
	parentScope = NULL;
}


ExprTree::
~ExprTree()
{
}


bool ExprTree::
Evaluate (EvalState &state, Value &val)
{
	Value 	cv;
	bool	rval;

	if( state.cache.lookup( this, cv ) == 0 ) {
		// found in cache; return cached value
		val.CopyFrom( cv );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.SetUndefinedValue( );
	if( state.cache.insert( this, cv ) < 0 ) {
		val.SetErrorValue( );
		return false;
	}

	// evaluate the expression
	rval = _Evaluate( state, val );

	// remove old value and cache actual value 
	if( state.cache.remove( this )<0 || state.cache.insert( this, val )<0 ) {
		val.SetErrorValue( );
		return false;
	}

	return( rval );
}

bool ExprTree::
Evaluate( EvalState &state, Value &val, ExprTree *&sig )
{
	Value	cv;
	bool	rval;

	if( state.cache.lookup( this, cv ) == 0 ) {
		// found in cache; return cached value
		val.CopyFrom( cv );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.SetUndefinedValue( );
	if( state.cache.insert( this, cv ) < 0 ) {
		val.SetErrorValue( );
		return false;
	}

	// evaluate the expression
	rval = _Evaluate( state, val, sig );

	// cache the value
	if( state.cache.remove( this )<0 || state.cache.insert( this, val )<0 ) {
		val.SetErrorValue( );
		return false;
	}

	return( rval );
}


void ExprTree::
SetParentScope( ClassAd* scope ) 
{
	parentScope = scope;
	_SetParentScope( scope );
}


bool ExprTree::
Evaluate( Value& val )
{
	EvalState 	state;

	state.SetScopes( parentScope );
	return( Evaluate( state, val ) );
}


bool ExprTree::
Evaluate( Value& val, ExprTree*& sig )
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
Flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind* op)
{
	bool rval;

	if(flattenFlag) {
		val.SetErrorValue ();
		flattenFlag = false; 
		return true;
	}

	flattenFlag = true; 	
	rval = _Flatten( state, val, tree, op );
	flattenFlag = false; 

	return rval;
}

ExprTree *ExprTree::
FromSource (Source &s)
{
	ExprTree *tree = NULL;
	if (!s.ParseExpression(tree) && tree) {
		delete tree;
		return (ExprTree*)NULL;
	}
	return tree;
}


int 
exprHash( ExprTree* const& expr, int numBkts ) 
{
	unsigned char *ptr = (unsigned char*) &expr;
	int	result = 0;
	
	for( int i = 0 ; (unsigned)i < sizeof( expr ) ; i++ ) result += ptr[i];
	return( result % numBkts );
}



EvalState::
EvalState( ) : cache( 128, &exprHash )
{
	rootAd = NULL;
	curAd  = NULL;
	strings.fill( NULL );
}

EvalState::
~EvalState( )
{
	for( int i = 0 ; i <= strings.getlast() ; i++ ) {
		delete [] strings[i];
	}
}

void EvalState::
SetScopes( ClassAd *curScope )
{
	curAd = curScope;
	SetRootScope( );
}


void EvalState::
SetRootScope( )
{
	ClassAd *prevScope = curAd, *curScope = curAd->parentScope;

	while( curScope ) {
		prevScope = curScope;
		curScope  = curScope->parentScope;
	}

	rootAd = prevScope;
}

} // namespace classad
