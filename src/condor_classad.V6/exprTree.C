#include "stringSpace.h"
#include "exprTree.h"
#include "evalContext.h"
#include "source.h"


ExprTree::
ExprTree ()
{
	for (int i = 0; i < NumLayers; i++) 
		evalFlags[i]=false;
}


void ExprTree::
evaluate (EvalState &state, Value &val)
{
	int layer = state.layer;

	if (evalFlags[layer])				// check layer'th flag
	{
		val.setErrorValue ();
		evalFlags[layer] = false;  		// clear layer'th flag
		return;
	}

	evalFlags[layer] = true; 			// set layer'th flag
	_evaluate (state, val);
	evalFlags[layer] = false; 			// clear layer'th flag

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
