/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#include "common.h"
#include "exprTree.h"

BEGIN_NAMESPACE( classad )

Literal::
Literal ()
{
	nodeKind = LITERAL_NODE;
	factor = Value::NO_FACTOR;
}


Literal::
~Literal ()
{
}


Literal *Literal::
Copy( ) const
{
	Literal *newTree = new Literal;

	if (newTree == 0) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return((Literal*)NULL);
	}
	newTree->value.CopyFrom( value );
	newTree->nodeKind = nodeKind;
	newTree->parentScope = parentScope;
	newTree->factor	= factor;
	return newTree;
}

Literal* Literal::
MakeAbsTime( time_t now )
{
	Value val;

	if( now<0 ) time( &now );
	val.SetAbsoluteTimeValue( now );
	return( MakeLiteral( val ) );
}

Literal* Literal::
MakeRelTime( time_t t1, time_t t2 )
{
	Value	val;

	if( t1<0 ) time( &t1 );
	if( t2<0 ) time( &t2 );
	val.SetRelativeTimeValue( t1 - t2 );
	return( MakeLiteral( val ) );
}


Literal* Literal::
MakeRelTime( time_t secs )
{
	Value		val;
	struct	tm 	lt;

	if( secs<0 ) {
		time(&secs );
		localtime_r( &secs, &lt );
	}
	val.SetRelativeTimeValue( lt.tm_hour*3600 + lt.tm_min*60 + lt.tm_sec );
	return( MakeLiteral( val ) );
}


Literal* Literal::
MakeLiteral( const Value& val, Value::NumberFactor f ) 
{
	if(val.GetType()==Value::CLASSAD_VALUE && val.GetType()==Value::LIST_VALUE){
		CondorErrno = ERR_BAD_VALUE;
		CondorErrMsg = "list and classad values are not literals";
		return( NULL );
	}

	Literal* lit = new Literal();
	if( !lit ){
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return NULL;
	}
	lit->value.CopyFrom( val );
	if( !val.IsNumber( ) ) f = Value::NO_FACTOR;
	lit->factor = f;

	return lit;
}


void Literal::
GetValue( Value &val ) const 
{
	int		i;
	double	r;

	val.CopyFrom( value );

	// if integer or real, multiply by the factor
	if (val.IsIntegerValue(i)) {
		if( factor != Value::NO_FACTOR ) {
			val.SetRealValue( ((double)i)*Value::ScaleFactor[factor] );
		}
	} else if (val.IsRealValue(r)) {
		if( factor != Value::NO_FACTOR ) {
			val.SetRealValue (r*Value::ScaleFactor[factor]);
		}
	}
}


void Literal::
GetComponents( Value &val, Value::NumberFactor &f ) const
{
	val = value;
	f = factor;
}


bool Literal::
_Evaluate (EvalState &, Value &val) const
{
	int		i;
	double	r;

	val.CopyFrom( value );

	// if integer or real, multiply by the factor
	if (val.IsIntegerValue(i)) {
		if( factor != Value::NO_FACTOR ) {
			val.SetRealValue( ((double)i)*Value::ScaleFactor[factor] );
		} else {
			val.SetIntegerValue(i);
		}
	} else if (val.IsRealValue(r)) {
		val.SetRealValue (r*Value::ScaleFactor[factor]);
	}

	return( true );
}


bool Literal::
_Evaluate( EvalState &state, Value &val, ExprTree *&tree ) const
{
	_Evaluate( state, val );
	return( ( tree = Copy() ) );
}


bool Literal::
_Flatten( EvalState &state, Value &val, ExprTree *&tree, int*) const
{
	tree = NULL;
	return( _Evaluate( state, val ) );
}

END_NAMESPACE // classad
