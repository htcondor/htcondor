#include "stringSpace.h"
#include "exprTree.h"

extern int exprHash( ExprTree* const&, int );

CopyMode ExprTree::defaultCopyMode = EXPR_DEEP_COPY;

ExprTree::
ExprTree ()
{
	refCount = 1;
	flattenFlag = false;
	parentScope = NULL;
}


ExprTree::
~ExprTree()
{
}


bool ExprTree::
evaluate (EvalState &state, Value &val)
{
	Value 	cv;
	bool	rval;

	if( state.cache.lookup( this, cv ) == 0 ) {
		// found in cache; return cached value
		val.copyFrom( cv );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.setUndefinedValue( );
	if( state.cache.insert( this, cv) < 0 ) {
		val.setErrorValue( );
		return false;
	}

	// evaluate the expression
	rval = _evaluate( state, val );

	// cache the value
	if( state.cache.insert( this, val ) < 0 ) {
		val.setErrorValue( );
		return false;
	}

	return( rval );
}

bool ExprTree::
evaluate( EvalState &state, Value &val, ExprTree *&sig )
{
	Value	cv;
	bool	rval;

	if( state.cache.lookup( this, cv ) == 0 ) {
		// found in cache; return cached value
		val.copyFrom( cv );
		return true;
	} 

	// not found in cache; insert a cache entry
	cv.setUndefinedValue( );
	if( state.cache.insert( this, cv) < 0 ) {
		val.setErrorValue( );
		return false;
	}

	// evaluate the expression
	rval = _evaluate( state, val, sig );

	// cache the value
	if( state.cache.insert( this, val ) < 0 ) {
		val.setErrorValue( );
		return false;
	}

	return( rval );
}


void ExprTree::
setParentScope( ClassAd* scope ) 
{
	parentScope = scope;
	_setParentScope( scope );
}


bool ExprTree::
evaluate( Value& val )
{
	EvalState state;

	state.setScopes( parentScope );
	return( evaluate( state, val ) );
}


bool ExprTree::
evaluate( Value& val, ExprTree*& sig )
{
	EvalState state;

	state.setScopes( parentScope );
	return( evaluate( state, val, sig ) );
}


bool ExprTree::
flatten( Value& val, ExprTree *&tree )
{
	EvalState state;

	state.setScopes( parentScope );
	return( flatten( state, val, tree ) );
}


bool ExprTree::
flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind* op)
{
	bool rval;

	if(flattenFlag) {
		val.setErrorValue ();
		flattenFlag = false; 
		return true;
	}

	flattenFlag = true; 	
	rval = _flatten( state, val, tree, op );
	flattenFlag = false; 

	return rval;
}

void ExprTree::
setDefaultCopyMode( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY || cm == EXPR_REF_COUNT ) {
		defaultCopyMode = cm;
	} else {
		cm = EXPR_DEEP_COPY;
	}
}

ExprTree *ExprTree::
copy( CopyMode cm )
{
	if( cm == EXPR_DEFAULT_COPY ) cm = defaultCopyMode;
	if( cm == EXPR_REF_COUNT ) {
		refCount++;
	}
	return( _copy( cm ) );
}


ExprTree *ExprTree::
fromSource (Source &s)
{
	ExprTree *tree = NULL;
	if (!s.parseExpression(tree) && tree) {
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
}


void EvalState::
setScopes( ClassAd *curScope )
{
	curAd = curScope;
	setRootScope( );
}


void EvalState::
setRootScope( )
{
	ClassAd *prevScope = curAd, *curScope = curAd->parentScope;

	while( curScope ) {
		prevScope = curScope;
		curScope  = curScope->parentScope;
	}

	rootAd = prevScope;
}
