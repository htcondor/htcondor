#include "condor_common.h"
#include "condor_classad.h"
#include "operators.h"

static void EvalResultToValue( EvalResult &, Value & );
static OpKind LexemeTypeToOpKind( LexemeType );
static void ValueToEvalResult( Value&, EvalResult& );

int BinaryOpBase::
_EvalTree( AttrList *myScope, EvalResult *result )
{
	return( this->EvalTree( myScope, NULL, result ) );
}


int BinaryOpBase::
_EvalTree( AttrList *myScope, AttrList *targetScope, EvalResult *result )
{
	EvalResult 	lArgResult, rArgResult;
	Value		lValue, rValue, resultValue;
	OpKind		op;

	// evaluate the left and right children of the expression
	if( lArg ) lArg->EvalTree( myScope, targetScope, &lArgResult );
	if( rArg ) rArg->EvalTree( myScope, targetScope, &rArgResult );

	// translate from old types/values to new types/values
	EvalResultToValue( lArgResult, lValue );
	EvalResultToValue( rArgResult, rValue );

	// translate from old operation to new operation
	op = LexemeTypeToOpKind( MyType() );

	// convert overloaded subtraction operator to unary minus
	if( op == SUBTRACTION_OP && lArg == NULL ) {
		operate( UNARY_MINUS_OP, rValue, resultValue );
	} else
	if( op == TERNARY_OP ) {
		// operator was assignment operator
		resultValue = rValue;
	} else {
		// use evaluation function
		operate( op, lValue, rValue, resultValue );
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
		case LX_BOOL:	
			v.setIntegerValue( er.b );	
			break;

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
