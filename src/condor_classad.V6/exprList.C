#include "exprTree.h"

ExprList::
ExprList()
{
}


ExprList::
~ExprList()
{
	clear ();
}


void ExprList::
clear ()
{
	ExprTree *tree;

	exprList.Rewind();
	while ((tree = exprList.Next()))
	{
		delete tree;
		exprList.DeleteCurrent ();
	}
}

	
ExprTree *ExprList::
copy (void)
{
	ExprList *newList = new ExprList;
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

	return newList;
}


bool ExprList::
toSink (Sink &s)
{
	ExprTree *tree;

	if (!s.sendToSink ((void*)" { ", 3)) return false;
	exprList.Rewind();
	while ((tree = exprList.Next()))
	{
		if (!tree->toSink(s)) return false;
		if (!exprList.AtEnd())
		{
			if (!s.sendToSink ((void*)" , ", 3)) return false;
		}
	}
	if (!s.sendToSink ((void*)" } ", 3)) return false;

	return true;
}


void ExprList::
appendExpression (ExprTree *tree)
{
	exprList.Append (tree);
}


void ExprList::
_evaluate (EvalState &state, Value &val)
{
	ExprTree *tree;
	Value	 *v;
	ValueList *vl = new ValueList;

	if (vl == NULL)
	{
		val.setUndefinedValue();
		return;
	}	

	val.clear();
	exprList.Rewind();
	while ((tree = exprList.Next()))
	{
		v = new Value;
		tree->evaluate (state, *v);
		vl->Append (v);
	}
		

	val.setListValue (vl, VALUE_LIST_ADOPT);
}
