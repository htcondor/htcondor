#include "exprTree.h"

ExprList::
ExprList()
{
	nodeKind = EXPR_LIST_NODE;
}


ExprList::
~ExprList()
{
	clear( );
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

	
ExprTree *ExprList::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {
		ExprList *newList = new ExprList;
		ExprTree *newTree, *tree;
	
		if (newList == 0) return 0;
	
		exprList.Rewind();
		while ((tree = exprList.Next())) {
			newTree = tree->copy( EXPR_DEEP_COPY );
			if (newTree) {
				newList->appendExpression(newTree);
			} else {
				delete newList;
				return 0;
			}
		}
		return newList;
	} else if( cm == EXPR_REF_COUNT ) {
		ExprTree *tree;
		exprList.Rewind();
		while( ( tree = exprList.Next() ) ) {
			tree->copy( EXPR_REF_COUNT );
		}
		return this;
	}

	// will not reach here
	return 0;
}


void ExprList::
setParentScope( ClassAd *parent )
{
	ExprTree *tree;
	exprList.Rewind( );
	while( ( tree = exprList.Next( ) ) ) {
		tree->setParentScope( parent );
	}
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
number()
{
	return exprList.Number();
}


void ExprList::
rewind( )
{
	exprList.Rewind( );
}

ExprTree *ExprList::
next( )
{
	return( exprList.Next( ) );
}

void ExprList::
appendExpression (ExprTree *tree)
{
	exprList.Append (tree);
}


void ExprList::
_evaluate (EvalState &, Value &val)
{
	val.setListValue (this);
}

void ExprList::
_evaluate( EvalState &, Value &val, ExprTree *&sig )
{
	val.setListValue( this );
	sig = copy( );
}

bool ExprList::
_flatten( EvalState &state, Value &, ExprTree *&tree, OpKind* )
{
	ExprTree	*expr, *nexpr;
	Value		tempVal;
	ExprList	*newList;

	exprList.Rewind();
	while( ( expr = exprList.Next() ) ) {
		// flatten the constituent expression
		if( !expr->flatten( state, tempVal, nexpr ) ) {
			delete newList;
			tree = NULL;
			return false;
		}

		// if only a value was obtained, convert to an expression
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

	tree = newList;

	return true;
}
