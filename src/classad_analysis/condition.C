/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condition.h"

Condition::
Condition( )
{
	myTree = NULL;
	pos = ATTR_POS_LEFT;
	op = classad::Operation::__NO_OP__;
}

Condition::
~Condition( )
{
}

bool Condition::
Init( const string &_attr, classad::ExprTree *_tree, bool bVal )
{
	if( !( BoolExpr::Init( _tree ) ) ) {
		return false;
	}
	attr = _attr;
	op = classad::Operation::EQUAL_OP;
	val.SetBooleanValue( bVal );
	isComplex = false;
	initialized = true;
	return true;
}


bool Condition::
Init( const string &_attr, classad::Operation::OpKind _op,
	  const classad::Value &_val, 
	  classad::ExprTree *_tree, AttrPos _pos )
{
	if( _op < classad::Operation::__COMPARISON_START__ || 
		_op > classad::Operation::__COMPARISON_END__ ) {
		return false;
	}
	if( !( BoolExpr::Init( _tree ) ) ) {
		return false;
	}
	attr = _attr;
	op = _op;
	val.CopyFrom( _val );
	pos = _pos;
	isComplex = false;
	initialized = true;
	return true;
}

bool Condition::
InitComplex( classad::ExprTree *_tree )
{	
	if( !( BoolExpr::Init( _tree ) ) ) {
		return false;
	}
	isComplex = true;	
	multiAttr = true;
	initialized = true;
	return true;
}

bool Condition::
InitComplex( const string &_attr,
			 classad::Operation::OpKind _op1, const classad::Value &_val1,
			 classad::Operation::OpKind _op2, const classad::Value &_val2,
			 classad::ExprTree *_tree )
{
	if( !( BoolExpr::Init( _tree ) ) ) {
		return false;
	}
	attr = _attr;
	op = _op1;
	val.CopyFrom( _val1 );
	op2 = _op2;
	val2.CopyFrom( _val2 );	
	isComplex = true;
	multiAttr = false;
	initialized = true;
	return true;
}

bool Condition::
GetAttr( string& attr )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex && multiAttr) {
		return false;
	}
	attr = this->attr;
	return true;
}

bool Condition::
GetOp( classad::Operation::OpKind &result )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex && multiAttr) {
		return false;
	}
	result = op;
	return true;
}

bool Condition::
GetOp2( classad::Operation::OpKind &result )
{
	if( !initialized ) {
		return false;
	}
	if( !isComplex || multiAttr ) {
		return false;
	}
	result = op2;
	return true;
}

bool Condition::
GetVal( classad::Value &val )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex && multiAttr ) {
		return false;
	}
	val.CopyFrom( this->val );
	return true;
}

bool Condition::
GetVal2( classad::Value &val )
{
	if( !initialized ) {
		return false;
	}
	if( !isComplex || multiAttr ) {
		return false;
	}
	val.CopyFrom( this->val2 );
	return true;
}

bool Condition::
GetType( classad::Value::ValueType &result )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex ) {
		if( multiAttr ) {
			return false;
		}
		else {
			if( val.IsUndefinedValue( ) ) {
				result = val2.GetType( );
			}
		}
	}
	result = val.GetType( );
	return true;
}

bool Condition::
GetAttrPos( Condition::AttrPos &result )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex ) {
		return false;
	}
	result = pos;
	return true;
}

bool Condition::
IsComplex( )
{
	return isComplex;
}

bool Condition::
HasMultipleAttrs( )
{
	return multiAttr;
}
