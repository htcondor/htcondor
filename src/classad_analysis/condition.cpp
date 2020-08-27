/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "condition.h"

using namespace std;

Condition::
Condition( )
{
	myTree = NULL;
	pos = ATTR_POS_LEFT;
	op = classad::Operation::__NO_OP__;
	multiAttr = false;
	isComplex = false;
	op2 = op;
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
GetAttr( string& attribute )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex && multiAttr) {
		return false;
	}
	attribute = this->attr;
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
GetVal( classad::Value &value )
{
	if( !initialized ) {
		return false;
	}
	if( isComplex && multiAttr ) {
		return false;
	}
	value.CopyFrom( this->val );
	return true;
}

bool Condition::
GetVal2( classad::Value &value )
{
	if( !initialized ) {
		return false;
	}
	if( !isComplex || multiAttr ) {
		return false;
	}
	value.CopyFrom( this->val2 );
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
IsComplex( ) const
{
	return isComplex;
}

bool Condition::
HasMultipleAttrs( ) const
{
	return multiAttr;
}
