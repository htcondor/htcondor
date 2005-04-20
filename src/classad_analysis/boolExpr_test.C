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
#include "analysis.h"

static bool GetOpName( Operation::OpKind, string & );
static bool GetTypeName( Value::ValueType, string & );

int main( ) {
	PrettyPrint pp;
	ClassAdParser parser;
	string buffer = "";
	string condString = "( ( MemoryRequirements < 234 ) || ( MemoryRequirements =?= undefined ) )";

	cout << "-------------" << endl;
	cout << "BOOLEXPR TEST" << endl;
	cout << "-------------" << endl;
	cout << "condString = " << condString << endl;
	cout << endl;

	ExprTree *condTree = NULL;
	if( !( condTree =  parser.ParseExpression( condString ) ) ) {
		cerr << "error parsing expression" << endl;
	}

	Condition *cond = new Condition( );
	if( !( BoolExpr::ExprToCondition( condTree, cond ) ) ) {
		cerr << "error with ExprToCondition" << endl;
	}

	cond->ToString( buffer );
	cout << "cond.ToString( ) = " << buffer << endl;
	buffer = "";
	cout << endl;

	string attr;
	Operation::OpKind op1 = Operation::__NO_OP__;
	Operation::OpKind op2 = Operation::__NO_OP__;
	Value val1, val2;
	Value::ValueType type;

	cond->GetAttr( attr );
	cout << "attr = " << attr << endl;

	cond->GetOp( op1 );
	GetOpName( op1, buffer );
	cout << "op1 = " << buffer << endl;
	cout << "op1 is op number " << (int)op1 << endl;
	buffer = "";

	cond->GetOp2( op2 );
	GetOpName( op2, buffer );
	cout << "op2 = " << buffer << endl;
	buffer = "";

	cond->GetVal( val1 );
	pp.Unparse( buffer, val1 );
	cout << "val1 = " << buffer << endl;
	buffer = "";

	cond->GetVal2( val2 );
	pp.Unparse( buffer, val2 );
	cout << "val2 = " << buffer << endl;
	buffer = "";

	cond->GetType( type );
	GetTypeName( type, buffer );
	cout << "type = " << buffer << endl;
	buffer = "";
	
}

static bool
GetOpName( Operation::OpKind op, string &result )
{
	switch( op ) {
	case Operation::__NO_OP__: { result = "NO_OP"; return true; }
	case Operation::LESS_THAN_OP: { result = "<"; return true; }
	case Operation::LESS_OR_EQUAL_OP: { result = "<="; return true; }
	case Operation::NOT_EQUAL_OP: { result = "!="; return true; }
	case Operation::EQUAL_OP: { result = "=="; return true; }
	case Operation::GREATER_OR_EQUAL_OP: { result = ">="; return true; }
	case Operation::GREATER_THAN_OP: { result = ">"; return true; }
	case Operation::IS_OP: { result = "is"; return true; }
	case Operation::ISNT_OP: { result = "isnt"; return true; }
	default: { result = "non-comp"; return true; }
	}
}
	
static bool
GetTypeName( Value::ValueType type, string &result )
{
	switch( type ) {
	case Value::NULL_VALUE: { result = "NULL"; return true; }
	case Value::ERROR_VALUE: { result = "Error"; return true; }
	case Value::UNDEFINED_VALUE: { result = "Undefined"; return true; }
	case Value::BOOLEAN_VALUE: { result = "Boolean"; return true; }
	case Value::INTEGER_VALUE: { result = "Integer"; return true; }
	case Value::REAL_VALUE: { result = "Real"; return true; }
	case Value::RELATIVE_TIME_VALUE: { result = "Relative Time"; return true; }
	case Value::ABSOLUTE_TIME_VALUE: { result = "Absolute Time"; return true; }
	case Value::STRING_VALUE: { result = "String"; return true; }
	case Value::CLASSAD_VALUE: { result = "ClassAd"; return true; }
	case Value::LIST_VALUE: { result = "List"; return true; }
	default:  { result = "Unknown"; return true; }
	}
}
