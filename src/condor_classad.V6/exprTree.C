#include "stringSpace.h"
#include "exprTree.h"


ExprTree::
ExprTree ()
{
	refCount = 0;
	evalFlag = false;
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
