#include "evalContext.h"
#include "caseSensitivity.h"

void Closure::
makeEntry (unsigned int i, const char *name, Closure *ctx)
{
    if (name) 
    {
        if (closureTable[i].scopeName) free (closureTable[i].scopeName);
        closureTable[i].scopeName = strdup (name);
        closureTable[i].scope = ctx;
        if (i >= afterLast) afterLast = i + 1;
    }
}


ExprTree *Closure::
obtainExpr (char *scope, char *name, Closure *&ctxt, Layer layer)
{
	for (unsigned int i = 0; i < afterLast; i++)
	{
		if (CLASSAD_SCOPE_NAMES_STRCMP (closureTable[i].scopeName, scope) == 0)
		{
			// follow the scope pointer into the closure, 
			ctxt = closureTable[i].scope;

			// sanity checks
			if (!ctxt || !ctxt->ads[layer]) return NULL;

			// pickout the layer'th ad in the closure and perform the lookup 
			return (ctxt->ads[layer]->lookup(name));
		}
	}

	return NULL;
}


ExprTree *Closure::
obtainExpr (unsigned int i, char *name, Closure *&ctxt, Layer layer)
{
	// sanity check
	if (i >= afterLast) return NULL;

	// follow the scope pointer into the closure, 
	ctxt = closureTable[i].scope;

	// sanity checks
	if (!ctxt || !ctxt->ads[layer]) return NULL;

	// pickout the layer'th ad in the closure and perform the lookup on it
	return (closureTable[i].scope->ads[layer]->lookup(name));
}


bool EvalContext::
isWellFormed ()
{
	unsigned int	i, j;

	for (i = 0; i < afterLast; i++)
	{
		for (j = 0; j < closures[i].afterLast; j++)
		{
			if (!(closures[i].closureTable[j].scope)) return false;
		}
	}
			
	return true;
}


void EvalContext::
evaluate (ExprTree *tree, Value &result, Closure *start, Layer layer)
{
	EvalState 	state;

	// initialize the state of the evluation
	state.evalContext = this;
	state.layer = layer;
	state.closure = start;
	
	// perform the evaluation
	if (tree) 
		tree->evaluate (state, result);
	else
		result.setUndefinedValue();
}
