#include "stringSpace.h"
#include "exprTree.h"


ExprTree::
ExprTree ()
{
	refCount = 0;
	evalFlag = false;
	flattenFlag = false;
}


ExprTree::
~ExprTree()
{
}


void ExprTree::
evaluate( Value& val )
{
	EvalState	state;
	EvalValue	eval;

	state.curAd  = NULL;
	state.rootAd = NULL;
	evaluate( state, eval );

	val.copyFrom( eval );
}


void ExprTree::
evaluate (EvalState &state, EvalValue &val)
{
	if(evalFlag) {
		val.setErrorValue ();
		evalFlag = false; 
		return;
	}

	evalFlag = true; 	
	_evaluate( state, val );
	evalFlag = false; 

	return;
}


bool ExprTree::
flatten( Value& val, ExprTree *&tree )
{
	EvalState 	state;
	EvalValue	eval;
	bool		rval;

	state.curAd  = NULL;
	state.rootAd = NULL;

	rval = flatten( state, eval, tree );

	val.copyFrom( eval );

	return rval;
}


bool ExprTree::
flatten( EvalState &state, EvalValue &val, ExprTree *&tree, OpKind* op)
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


ExprTree *ExprTree::
fromSource (Source &s)
{
	ExprTree *tree = NULL;
	if (!s.parseExpression(tree) && tree)
	{
		delete tree;
		return (ExprTree*)NULL;
	}
	return tree;
}
