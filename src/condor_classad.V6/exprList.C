#include "exprTree.h"

ExprList::
ExprList()
{
	nodeKind = EXPR_LIST_NODE;
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
	while ((tree = exprList.Next())) {
		delete tree;
		exprList.DeleteCurrent ();
	}
}

	
void ExprList::
setParentScope( ClassAd *ad )
{
	ExprTree *tree;
	exprList.Rewind();
	while( (tree = exprList.Next()) )
		tree->setParentScope( ad );
}


ExprTree *ExprList::
copy (CopyMode)
{
	ExprList *newList = new ExprList;
	ExprTree *newTree, *tree;
	
	if (newList == 0) return 0;
	
	exprList.Rewind();
	while ((tree = exprList.Next())) {
		newTree = tree->copy();
		if (newTree) {
			newList->appendExpression(newTree);
		} else {
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
	while ((tree = exprList.Next())) {
		if (!tree->toSink(s)) return false;
		if (!exprList.AtEnd()) {
			if (!s.sendToSink ((void*)" , ", 3)) return false;
		}
	}
	if (!s.sendToSink ((void*)" } ", 3)) return false;

	return true;
}


int ExprList::
getLength()
{
	return exprList.Number();
}


void ExprList::
appendExpression (ExprTree *tree)
{
	exprList.Append (tree);
}


void ExprList::
_evaluate (EvalState &state, EvalValue &val)
{
	ExprTree 	*tree;
	EvalValue	ev;
	Value	 	*v;
	ValueList 	*vl = new ValueList;

	if (vl == NULL) {
		val.setUndefinedValue();
		return;
	}	

	exprList.Rewind();
	while ((tree = exprList.Next())) {
		v = new Value;
		tree->evaluate (state, ev);
		v->copyFrom( ev );
		vl->Append (v);
	}
		
	val.setListValue (vl);
}


bool ExprList::
_flatten( EvalState &state, EvalValue &val, ExprTree *&tree, OpKind* )
{
	ExprTree	*expr, *oexpr, *nexpr;
	Value		*newVal;
	Value		tempVal;
	ValueList	*valList;
	ExprList	*newList;
	bool		allValues = true;

	// first assume all values, and make a value list
	if( !( valList = new ValueList() ) ) return false;
	exprList.Rewind();
	while( ( expr = exprList.Next() ) ) {
		// flatten the constituent expression
		if( !expr->flatten( state, tempVal, nexpr ) ) {
			delete valList;
			tree = NULL;
			return false;
		}

		// is the list still only composed of values?
		if( allValues ) {
			if( !nexpr ) {
				newVal = new Value();
				if( !newVal ) {
					delete valList;
					tree = NULL;
					return false;
				}
				newVal->copyFrom( tempVal );
				valList->Append( newVal );
			} else {
				// expr didn't flatten to a value; make exprList
				allValues = false;
				if( !( newList = new ExprList() ) ) {
					delete valList;
					return false;
				}
				valList->Rewind();
				while( ( newVal = valList->Next() ) ) {
					if( !( oexpr = Literal::makeLiteral( *newVal ) ) ) {
						delete valList;
						delete newList;
						return false;
					}
					newList->appendExpression( oexpr );
				}
				delete valList;
				newList->appendExpression( nexpr );
			}
		} else {
			if( !nexpr ) {
				nexpr = Literal::makeLiteral( tempVal );
				if( !nexpr ) {
					delete newList;
					return false;
				}
			}
			// add the new expression to the flattened list
			newList->appendExpression( nexpr );
		}
	}

	if( allValues ) {
		val.setListValue( valList );
		tree = NULL;
	} else {
		tree = newList;
	}

	return true;
}

