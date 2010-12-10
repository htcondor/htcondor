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
#include "condor_classad.h"
#include "operators.h"

static void EvalResultToValue( EvalResult &, Value & );
static OpKind LexemeTypeToOpKind( LexemeType );
static void ValueToEvalResult( Value&, EvalResult& );

int BinaryOpBase::
_EvalTree( const AttrList *myScope, EvalResult *result )
{
	return( this->EvalTree( myScope, NULL, result ) );
}


int BinaryOpBase::
_EvalTree( const AttrList *myScope, const AttrList *targetScope, EvalResult *result )
{
	EvalResult 	lArgResult, rArgResult;
	Value		lValue, rValue, resultValue;
	OpKind		op;

	// translate from the lexer's name for the operation to a reasonable operation name
	op = LexemeTypeToOpKind( MyType() );

	lArgResult.debug = rArgResult.debug = result->debug;

	// evaluate the left side
	if( lArg ) lArg->EvalTree( myScope, targetScope, &lArgResult );
    EvalResultToValue( lArgResult, lValue );
    
    if (!operateShortCircuit(op, lValue, resultValue)) {

        // Evaluating just the left side was insufficient, so
        // we now evaluate the right-hand side. 
        if( rArg ) rArg->EvalTree( myScope, targetScope, &rArgResult );
        EvalResultToValue( rArgResult, rValue );

        // convert overloaded subtraction operator to unary minus
        if( op == SUBTRACTION_OP && lArg == NULL ) {
            operate( UNARY_MINUS_OP, rValue, resultValue );
        } else
        // convert overloaded addition operator to parenthesis operator
        if( op == ADDITION_OP && lArg == NULL ) {
            resultValue = rValue;
        } else
        if( op == TERNARY_OP ) {
            // operator was assignment operator
            resultValue = rValue;
        } else {
            // use evaluation function
            operate( op, lValue, rValue, resultValue );
        }
    }

    // translate from new types/values to old types/values
    ValueToEvalResult( resultValue, (*result) );

	// this version of evaluation never fails
	return TRUE;
}


static void 
EvalResultToValue( EvalResult &er, Value &v )
{
	switch( er.type )
	{
		case LX_INTEGER:
			v.setIntegerValue( er.i );
			break;

		case LX_FLOAT:
			v.setRealValue( er.f );
			break;

		case LX_STRING:
			v.setStringValue( er.s );
			break;

		case LX_UNDEFINED:
			v.setUndefinedValue();
			break;

		case LX_ERROR:
		default:
			v.setErrorValue();
			break;
	}
}


static OpKind
LexemeTypeToOpKind( LexemeType lx )
{
	switch( lx )
	{
		// Ugly hack.  Since new classads do not have an "assignment operator",
		// we use the TERNARY_OP to signal this condition.  --RR
		case LX_ASSIGN:	return TERNARY_OP;

		case LX_AND:	return LOGICAL_AND_OP;
		case LX_OR:		return LOGICAL_OR_OP;
		case LX_EQ:		return EQUAL_OP;
		case LX_NEQ:	return NOT_EQUAL_OP;
		case LX_LT:		return LESS_THAN_OP;
		case LX_LE:		return LESS_OR_EQUAL_OP;
		case LX_GT:		return GREATER_THAN_OP;
		case LX_GE:		return GREATER_OR_EQUAL_OP;
		case LX_ADD:	return ADDITION_OP;
		case LX_SUB:	return SUBTRACTION_OP;
		case LX_MULT:	return MULTIPLICATION_OP;
		case LX_DIV:	return DIVISION_OP;
		case LX_META_EQ:return META_EQUAL_OP;
		case LX_META_NEQ:return META_NOT_EQUAL_OP;

		default:		return __NO_OP__;
	}

	return __NO_OP__;
}


static void
ValueToEvalResult( Value &v, EvalResult &er )
{
	int 	i;
	double	d;
	char	*c;

	if( v.isUndefinedValue() ) {
		er.type = LX_UNDEFINED;
	} else
	if( v.isErrorValue() ) {
		er.type = LX_ERROR;
	} else
	if( v.isIntegerValue(i) ) {
		er.type = LX_INTEGER;
		er.i = i;
	} else
	if( v.isRealValue(d) ) {
		er.type = LX_FLOAT;
		er.f = (float) d;
	} else
	if( v.isStringValue(c) ) {
		er.type = LX_STRING;
		er.s = new char[strlen(c)+1];
		strcpy( er.s, c );
	}
}
