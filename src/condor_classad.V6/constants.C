#include "exprTree.h"
#include "domain.h"

Constant::
Constant ()
{
	nodeKind = CONSTANT_NODE;
	factor = NO_FACTOR;
}


Constant::
~Constant ()
{
}


ExprTree *Constant::
copy (void)
{
	Constant *newTree = new Constant;

	if (newTree == 0) return 0;
	newTree->value.copy (value);
	newTree->nodeKind = nodeKind;

	return newTree;
}


bool Constant::
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


void Constant::
_evaluate (EvalState &, Value &val)
{
	int		i;
	double	r;

	val.copy (value);

	// if integer or real, multiply by the factor
	if (val.isIntegerValue(i))
	{
		val.setIntegerValue (i*factor);
	}
	else
	if (val.isRealValue(r))
	{
		val.setRealValue (r*(double)factor);
	}
}


void Constant::
setIntegerValue (int i, NumberFactor f)
{
	value.setIntegerValue (i);
	factor = f;
}


void Constant::
setRealValue (double d, NumberFactor f)
{
	value.setRealValue (d);
	factor = f;
}


void Constant::
adoptStringValue (char *str)
{
	value.adoptStringValue (str);
	factor = NO_FACTOR;
}


void Constant::
setStringValue (char *str)
{
	value.setStringValue (str);
	factor = NO_FACTOR;
}


void Constant::
setUndefinedValue (void)
{
	value.setUndefinedValue ();
	factor = NO_FACTOR;
}


void Constant::
setErrorValue (void)
{
	value.setErrorValue ();
	factor = NO_FACTOR;
}
