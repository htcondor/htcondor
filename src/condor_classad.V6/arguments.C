#include "arguments.h"

ArgumentList::
ArgumentList ()
{
	nodeKind = ARGUMENT_LIST_NODE;
	numberOfArguments = 0;
}


ArgumentList::
~ArgumentList ()
{
	clear();
}


ExprTree *ArgumentList::
copy (void)
{
	ArgumentList *newList = new ArgumentList;
    ExprTree *newTree, *tree;
    
    if (newList == 0) return 0;
    
    exprList.Rewind();
    while ((tree = exprList.Next()))
    {
        newTree = tree->copy();
        if (newTree)
        {
            newList->appendExpression(newTree);
        }
        else
        {
            delete newList;
            return 0;
        }
    }

	newList->numberOfArguments = numberOfArguments;

    return newList;
}



bool ArgumentList::
toSink (Sink &s)
{
    ExprTree *tree;

    if (!s.sendToSink ((void*)" ( ", 3)) return false;
    exprList.Rewind();
    while ((tree = exprList.Next()))
    {
        if (!tree->toSink(s)) return false;
        if (!exprList.AtEnd())
        {
            if (!s.sendToSink ((void*)" , ", 3)) return false;
        }
    }
    if (!s.sendToSink ((void*)" ) ", 3)) return false;

    return true;
}


void ArgumentList::
appendArgument (ExprTree *tree)
{
	appendExpression (tree);
	numberOfArguments++;
}


void ArgumentList::
evalArgument (EvalState &state, int argNum, Value &result)
{
	int 	  i = 0;
	ExprTree *tree;

	// traverse the arg list and evaluate arguments
	exprList.Rewind();
	while ((tree = exprList.Next()) && (i < argNum))
	{
		i++;
	}

	// evaluate the argument 
	if (tree)
	{
		tree->evaluate (state, result);
	}
	else
	{
		result.setUndefinedValue ();
	}
}
