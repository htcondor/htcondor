#include "exprTree.h"
#include "domain.h"

Literal::
Literal ()
{
	nodeKind = LITERAL_NODE;
	factor = NO_FACTOR;
	string = NULL;
}


Literal::
~Literal ()
{
	if( string ) free( string );
}


ExprTree *Literal::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {	
		Literal *newTree = new Literal;

		if (newTree == 0) return 0;
		newTree->value.copyFrom (value);
		newTree->nodeKind = nodeKind;
		if( ( newTree->string = strdup( string ) ) == NULL ) {
			delete newTree;
			return 0;
		}
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
	char *s;

	Literal* lit = new Literal();
	if( !lit ) return NULL;
	lit->value.copyFrom( val );
	if( val.isStringValue( s ) ) {
		lit->string = s;
	}

	return lit;
}


bool Literal::
_evaluate (EvalState &, Value &val)
{
	int		i;
	double	r;

	val.copyFrom( value );

	// if integer or real, multiply by the factor
	if (val.isIntegerValue(i)) {
		val.setIntegerValue (i*factor);
	} else if (val.isRealValue(r)) {
		val.setRealValue (r*(double)factor);
	}

	return( true );
}


bool Literal::
_evaluate( EvalState &state, Value &val, ExprTree *&tree )
{
	_evaluate( state, val );
	return( !( tree = copy() ) );
}


bool Literal::
_flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind*)
{
	tree = NULL;
	return( _evaluate( state, val ) );
}


void Literal::
setBooleanValue( bool b )
{
	value.setBooleanValue( b );
	if( string ) { 
		free( string );
		string = NULL;
	}
	factor = NO_FACTOR;
}


void Literal::
setIntegerValue (int i, NumberFactor f)
{
	value.setIntegerValue (i);
	if( string ) { 
		free( string );
		string = NULL;
	}
	factor = f;
}


void Literal::
setRealValue (double d, NumberFactor f)
{
	value.setRealValue (d);
	if( string ) { 
		free( string );
		string = NULL;
	}
	factor = f;
}


void Literal::
adoptStringValue (char *str)
{
	value.adoptStringValue (str, SS_ADOPT_C_STRING);
	if( string ) { 
		free( string );
		string = NULL;
	}
	factor = NO_FACTOR;
}


void Literal::
setStringValue (char *str)
{
	if( string ) { 
		free( string );
		string = NULL;
	}
	string = strdup( str );
	value.setStringValue( string );
	factor = NO_FACTOR;
}


void Literal::
setUndefinedValue (void)
{
	value.setUndefinedValue( );
	if( string ) { 
		free( string );
		string = NULL;
	}
	factor = NO_FACTOR;
}


void Literal::
setErrorValue (void)
{
	value.setErrorValue( );
	if( string ) { 
		free( string );
		string = NULL;
	}
	factor = NO_FACTOR;
}
