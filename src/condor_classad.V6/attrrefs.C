#include "condor_common.h"
#include "classad.h"
#include "evalContext.h"


AttributeReference::
AttributeReference()
{
	nodeKind = ATTRREF_NODE;
	attributeStr = NULL;
	scopeStr = NULL;
}


AttributeReference::
~AttributeReference()
{
	if (attributeStr) free (attributeStr);
	if (scopeStr) 	  free (scopeStr);
}


ExprTree *AttributeReference::
copy (void)
{
	AttributeReference *newTree = new AttributeReference ();
	if (newTree == 0) return NULL;

	if (attributeStr) 
		newTree->attributeStr = strdup (attributeStr);

	newTree->attributeName = attributeName;

	if (scopeStr)
		newTree->scopeStr = strdup (scopeStr);
	
	newTree->scopeName = scopeName;
	newTree->nodeKind = nodeKind;

	return newTree;
}


bool AttributeReference::
toSink (Sink &s)
{
	if (scopeStr) 
	{
		if (!s.sendToSink ((void*) scopeStr, strlen(scopeStr))	||
			!s.sendToSink ((void*) ".", 1))
				return false;
	}
		
	if (attributeStr)
	{
		return (s.sendToSink ((void*) attributeStr, strlen(attributeStr)));
	}

	// should not reach here
	return false;
}


void AttributeReference::
_evaluate (EvalState &state, Value &value)
{
	Layer	 	layer;
    EvalContext *evalContext;
	Closure		*closure;	
    Closure     *newClosure;
    ExprTree    *tree;

    // get the current evaluation state
    evalContext = state.evalContext;
	layer   = state.layer;
    closure = state.closure;

    // check if there is a specific scope resolution
    if (scopeStr)
    {
		// obtain expression from the specific scope named
		tree = closure->obtainExpr (scopeStr, attributeStr, newClosure, layer);
    }
    else
	{
		// go through each of the scopes in the context
		for (unsigned int i = 0; i < closure->afterLast; i++)
		{
			if ((tree=closure->obtainExpr (i, attributeStr, newClosure, layer)))
				break;
		}
	}
	
	// check if we obtained a tree
	if (tree)
	{
		// yes ... go ahead and evaluate it; first change context
		state.closure = newClosure;
		tree->evaluate (state, value);

		// restore context
		state.closure = closure;	
	}
	else
	{
		// no such attribute found; result of evaluation is UNDEFINED
		value.setUndefinedValue();
	}
		
	return;
}


void AttributeReference::
setReference (char *refScope, char *attrStr)
{
	if (scopeStr) free (scopeStr);
	if (attributeStr) free (attributeStr);

	scopeStr 		= refScope ? strdup (refScope) : NULL;
	attributeStr 	= attrStr  ? strdup (attrStr)  : NULL;
}
