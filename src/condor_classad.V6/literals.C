#include "condor_common.h"
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


Literal *Literal::
Copy( )
{
	Literal *newTree = new Literal;

	if (newTree == 0) return 0;
	newTree->value.CopyFrom( value );
	newTree->nodeKind = nodeKind;
	return newTree;
}

bool Literal::
ToSink (Sink &s)
{
	if (!value.ToSink( s )) return false;

	// print out any factors --- (factors are set only if 'value' is a
	// number value, so we don't have to check for that case
	if ((factor == K_FACTOR && !s.SendToSink ((void*)"K", 1))	||
		(factor == M_FACTOR && !s.SendToSink ((void*)"M", 1))	||
		(factor == G_FACTOR && !s.SendToSink ((void*)"G", 1)))
			return false;

	return true;
}


Literal* Literal::
MakeAbsTime( time_t now )
{
	Literal *lit;
	if( ( now<0 && time( &now )<0 ) || ( ( lit=new Literal() )==NULL ) ) {
		return NULL;
	}
	lit->SetAbsTimeValue( (int) now );
	return( lit );
}

Literal* Literal::
MakeRelTime( time_t t1, time_t t2 )
{
	Literal *lit;
	if( ( t1<0 && time( &t1 )<0 ) || ( t2<0 && time( &t2 )<0 ) ||
		( ( lit = new Literal() )==NULL ) ) {
		return NULL;
	}
	lit->SetRelTimeValue( t1 - t2 );
	return( lit );
}


Literal* Literal::
MakeRelTime( int secs )
{
	Literal *lit = new Literal( );
	if( lit ) {
		lit->SetRelTimeValue( secs );
		return( lit );
	} 
	return( NULL );
}

Literal* Literal::
MakeLiteral( Value& val ) 
{
	Literal* lit = new Literal();
	if( !lit ) return NULL;
	lit->value.CopyFrom( val );

	return lit;
}


bool Literal::
_Evaluate (EvalState &, Value &val)
{
	int		i;
	double	r;

	val.CopyFrom( value );

	// if integer or real, multiply by the factor
	if (val.IsIntegerValue(i)) {
		val.SetIntegerValue (i*factor);
	} else if (val.IsRealValue(r)) {
		val.SetRealValue (r*(double)factor);
	}

	return( true );
}


bool Literal::
_Evaluate( EvalState &state, Value &val, ExprTree *&tree )
{
	_Evaluate( state, val );
	return( !( tree = Copy() ) );
}


bool Literal::
_Flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind*)
{
	tree = NULL;
	return( _Evaluate( state, val ) );
}


void Literal::
SetBooleanValue( bool b )
{
	value.SetBooleanValue( b );
	factor = NO_FACTOR;
}


void Literal::
SetIntegerValue (int i, NumberFactor f)
{
	value.SetIntegerValue (i);
	factor = f;
}


void Literal::
SetRealValue (double d, NumberFactor f)
{
	value.SetRealValue (d);
	factor = f;
}


void Literal::
SetStringValue (const char *str )
{
	value.SetStringValue( str );
	factor = NO_FACTOR;
}


void Literal::
SetUndefinedValue (void)
{
	value.SetUndefinedValue( );
	factor = NO_FACTOR;
}


void Literal::
SetErrorValue (void)
{
	value.SetErrorValue( );
	factor = NO_FACTOR;
}

void Literal::
SetAbsTimeValue( int secs )
{
	value.SetAbsoluteTimeValue( secs );
}

void Literal::
SetRelTimeValue( int secs, int usecs )
{
	value.SetRelativeTimeValue( secs, usecs );
}
