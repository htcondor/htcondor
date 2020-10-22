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
#include "analysis.h"

static bool GetOpName( classad::Operation::OpKind, std::string & );
static bool GetTypeName( classad::Value::ValueType, std::string & );

int main( ) {
	classad::PrettyPrint pp;
	classad::ClassAdParser parser;
	std::string buffer = "";
	std::string condString = "( ( MemoryRequirements < 234 ) || ( MemoryRequirements =?= undefined ) )";

	std::cout << "-------------" << std::endl;
	std::cout << "BOOLEXPR TEST" << std::endl;
	std::cout << "-------------" << std::endl;
	std::cout << "condString = " << condString << std::endl;
	std::cout << std::endl;

	ExprTree *condTree = NULL;
	if( !( condTree =  parser.ParseExpression( condString ) ) ) {
		std::cerr << "error parsing expression" << std::endl;
	}

	Condition *cond = new Condition( );
	if( !( BoolExpr::ExprToCondition( condTree, cond ) ) ) {
		std::cerr << "error with ExprToCondition" << std::endl;
	}

	cond->ToString( buffer );
	std::cout << "cond.ToString( ) = " << buffer << std::endl;
	buffer = "";
	std::cout << std::endl;

	std::string attr;
	classad::Operation::OpKind op1 = classad::Operation::__NO_OP__;
	classad::Operation::OpKind op2 = classad::Operation::__NO_OP__;
	classad::Value val1, val2;
	classad::Value::ValueType type;

	cond->GetAttr( attr );
	std::cout << "attr = " << attr << std::endl;

	cond->GetOp( op1 );
	GetOpName( op1, buffer );
	std::cout << "op1 = " << buffer << std::endl;
	std::cout << "op1 is op number " << (int)op1 << std::endl;
	buffer = "";

	cond->GetOp2( op2 );
	GetOpName( op2, buffer );
	std::cout << "op2 = " << buffer << std::endl;
	buffer = "";

	cond->GetVal( val1 );
	pp.Unparse( buffer, val1 );
	std::cout << "val1 = " << buffer << std::endl;
	buffer = "";

	cond->GetVal2( val2 );
	pp.Unparse( buffer, val2 );
	std::cout << "val2 = " << buffer << std::endl;
	buffer = "";

	cond->GetType( type );
	GetTypeName( type, buffer );
	std::cout << "type = " << buffer << std::endl;
	buffer = "";
	
}

static bool
GetOpName( classad::Operation::OpKind op, std::string &result )
{
	switch( op ) {
	case classad::Operation::__NO_OP__: { result = "NO_OP"; return true; }
	case classad::Operation::LESS_THAN_OP: { result = "<"; return true; }
	case classad::Operation::LESS_OR_EQUAL_OP: { result = "<="; return true; }
	case classad::Operation::NOT_EQUAL_OP: { result = "!="; return true; }
	case classad::Operation::EQUAL_OP: { result = "=="; return true; }
	case classad::Operation::GREATER_OR_EQUAL_OP: { result = ">="; return true; }
	case classad::Operation::GREATER_THAN_OP: { result = ">"; return true; }
	case classad::Operation::IS_OP: { result = "is"; return true; }
	case classad::Operation::ISNT_OP: { result = "isnt"; return true; }
	default: { result = "non-comp"; return true; }
	}
}
	
static bool
GetTypeName( classad::Value::ValueType type, std::string &result )
{
	switch( type ) {
	case classad::Value::NULL_VALUE: { result = "NULL"; return true; }
	case classad::Value::ERROR_VALUE: { result = "Error"; return true; }
	case classad::Value::UNDEFINED_VALUE: { result = "Undefined"; return true; }
	case classad::Value::BOOLEAN_VALUE: { result = "Boolean"; return true; }
	case classad::Value::INTEGER_VALUE: { result = "Integer"; return true; }
	case classad::Value::REAL_VALUE: { result = "Real"; return true; }
	case classad::Value::RELATIVE_TIME_VALUE: { result = "Relative Time"; return true; }
	case classad::Value::ABSOLUTE_TIME_VALUE: { result = "Absolute Time"; return true; }
	case classad::Value::STRING_VALUE: { result = "String"; return true; }
	case classad::Value::SCLASSAD_VALUE:
	case classad::Value::CLASSAD_VALUE: { result = "ClassAd"; return true; }
	case classad::Value::SLIST_VALUE:
	case classad::Value::LIST_VALUE: { result = "List"; return true; }
	default:  { result = "Unknown"; return true; }
	}
}
