#include "exprTree.h"
#include "domain.h"

Literal::
Literal ()
{
	nodeKind = LITERAL_NODE;
	factor = NO_FACTOR;
}


Literal::
~Literal ()
{
}


ExprTree *Literal::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {	
		Literal *newTree = new Literal;

		if (newTree == 0) return 0;
		newTree->value.copyFrom (value);
		newTree->nodeKind = nodeKind;

		return newTree;
	} else if( cm == EXPR_REF_COUNT ) {
		// ref count already updated by ExprTree::copy()
		return this;
	}

	// will not reach here
	return 0;
}

bool Literal::
toSink (Sink &s)
{
	if (!value.toSink (s)) return false;

	// print out any factors --- (factors are set only if 'value' is a
	// number value, so we don't have to check for that case
	if ((factor == K_FACTOR && !s.sendToSink ((void*)"K", 1))	||
		(factor == M_FACTOR && !s.sendToSink ((void*)"M", 1))	||
		(factor == G_FACTOR && !s.sendToSink ((void*)"G", 1)))
			return false;

	return true;
}


Literal* Literal::
makeLiteral( Value& val ) 
{
	Literal* lit = new Literal();
	if( !lit ) return NULL;
	lit->value.copyFrom( val, VALUE_HANDOVER );
	return lit;
}


void Literal::
_evaluate (EvalState &, Value &val)
{
	int		i;
	double	r;

	val.copyFrom( value, VALUE_COPY );

	// if integer or real, multiply by the factor
	if (val.isIntegerValue(i)) {
		val.setIntegerValue (i*factor);
	} else if (val.isRealValue(r)) {
		val.setRealValue (r*(double)factor);
	}
}


void Literal::
_evaluate( EvalState &state, Value &val, ExprTree *&tree )
{
	_evaluate( state, val );
	tree = copy();
}

bool Literal::
_flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind*)
{
	tree = NULL;
	_evaluate( state, val );
	return true;
}


void Literal::
setBooleanValue( bool b )
{
	value.setBooleanValue( b );
	factor = NO_FACTOR;
}


void Literal::
setIntegerValue (int i, NumberFactor f)
{
	value.setIntegerValue (i);
	factor = f;
}


void Literal::
setRealValue (double d, NumberFactor f)
{
	value.setRealValue (d);
	factor = f;
}


void Literal::
adoptStringValue (char *str)
{
	value.adoptStringValue (str, SS_ADOPT_C_STRING);
	factor = NO_FACTOR;
}


void Literal::
setStringValue (char *str)
{
	value.setStringValue (str);
	factor = NO_FACTOR;
}


void Literal::
setUndefinedValue (void)
{
	value.setUndefinedValue ();
	factor = NO_FACTOR;
}


void Literal::
setErrorValue (void)
{
	value.setErrorValue ();
	factor = NO_FACTOR;
}
