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
copy (CopyMode cm)
{
	if( cm == EXPR_DEEP_COPY ) {	
		Literal *newTree = new Literal;

		if (newTree == 0) return 0;
		newTree->value.copyFrom (value);
		newTree->nodeKind = nodeKind;

		return newTree;
	}

	return this;
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
makeLiteral( EvalValue& val ) 
{
	Literal* lit = new Literal();
	if( !lit ) return NULL;
	lit->value.copyFrom( val );
	return lit;
}


void Literal::
_evaluate (EvalState &, EvalValue &val)
{
	int		i;
	double	r;
	Value	v;

	v.copyFrom (value);
	val.copyFrom( v );

	// if integer or real, multiply by the factor
	if (val.isIntegerValue(i)) {
		val.setIntegerValue (i*factor);
	} else if (val.isRealValue(r)) {
		val.setRealValue (r*(double)factor);
	}
}


bool Literal::
_flatten( EvalState &state, EvalValue &val, ExprTree *&tree, OpKind*)
{
	tree = NULL;
	_evaluate( state, val );
	return true;
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
