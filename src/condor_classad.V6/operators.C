#include <math.h>				// for HUGE_VAL
#include <limits.h>				// for WORD_BIT
#include "condor_common.h"
#include "operators.h"
#include "caseSensitivity.h"

static char *_FileName_ = __FILE__;

char *Operation::opString[] = 
{
	"",		// no-op

	"<",	// comparisons
	"<=",
	"!=",
	"==",
	"=?=",
	"=!=",
	">=",
	">",

	"+",	// arithmetic
	"-",
	"+",
	"-",
	"*",
	"/",
	"%",

	"!",	// logical
	"||",
	"&&",

	"~",	// bitwise
	"|",
	"^",
	"&",
	"<<",
	">>>",
	">>",

	"()",	//misc --- no "single token" representation
	"[]",
	"?:"
};


Operation::
Operation ()
{
	nodeKind = OP_NODE;
	operation = __NO_OP__;
	child1    = NULL;
	child2    = NULL;
	child3    = NULL;
}


Operation::
~Operation ()
{
	if (child1) delete child1;
	if (child2) delete child2;
	if (child3) delete child3;
}


ExprTree *Operation::
copy (CopyMode)
{
	Operation *newTree = new Operation ();
	if (newTree == 0) 
		return NULL;

	if (child1 && (newTree->child1 = child1->copy()) == NULL)
	{
		return NULL;
	}

	if (child2 && (newTree->child2 = child2->copy()) == NULL)
	{
		delete newTree->child1;
		return NULL;
	}

	if (child3 && (newTree->child3 = child3->copy()) == NULL)
	{
		delete newTree->child1;
		delete newTree->child2;
		return NULL;
	}

	newTree->operation = operation;
	newTree->nodeKind = nodeKind;

	return newTree;
}


bool Operation::
toSink (Sink &s)
{
	bool flag;

	// trivial case
	if (operation == __NO_OP__) return true;

	// parentheses
	if (operation == PARENTHESES_OP)
	{
		if ((!s.sendToSink((void*)"(", 1)) 	||
			(!child1 || !child1->toSink(s))	||
			(!s.sendToSink((void*)")", 1)))
				return false;
		else
			return true;
	}

	// unary operators
	if ((operation == UNARY_PLUS_OP  && !s.sendToSink((void*)"+", 1)) 	||
		(operation == UNARY_MINUS_OP && !s.sendToSink((void*)"-", 1))	||
		(operation == LOGICAL_NOT_OP && !s.sendToSink((void*)"!", 1))	||
		(operation == BITWISE_NOT_OP && !s.sendToSink((void*)"~", 1)))
			return false;

	// there must be at least one child
	if (!child1 || !child1->toSink (s))	return false;

	// ternary operator
	if (operation == TERNARY_OP)
	{
		if ((!s.sendToSink ((void*)" ? ", 3))	||
			(!child2 || !child2->toSink(s))		||
			(!s.sendToSink ((void*)" : ", 3))	||
			(!child3 || !child3->toSink(s)))
				return false;
		else
			return true;
	}
	else
	if( operation == SUBSCRIPT_OP )
	{
		if( ( !s.sendToSink( (void*)"[", 1 ) )	||	
			( !child2 || !child2->toSink( s ) ) ||
			( !s.sendToSink( (void*)"]", 1 ) ) )
				return false;
		else
			return true;
	}

	// the rest are infix binary operators
	switch (operation)
	{
		case LESS_THAN_OP:		flag = s.sendToSink ((void*)" < ",  3);	break;
    	case LESS_OR_EQUAL_OP:	flag = s.sendToSink ((void*)" <= ", 4); break;
    	case NOT_EQUAL_OP:		flag = s.sendToSink ((void*)" != ", 4); break;
    	case EQUAL_OP:			flag = s.sendToSink ((void*)" == ", 4); break;
    	case META_EQUAL_OP:		flag = s.sendToSink ((void*)" =?= ",5); break;
    	case META_NOT_EQUAL_OP:	flag = s.sendToSink ((void*)" =!= ",5); break;
    	case GREATER_OR_EQUAL_OP:flag= s.sendToSink ((void*)" >= ", 4); break;
    	case GREATER_THAN_OP:	flag = s.sendToSink ((void*)" > ",  3); break;
    	case ADDITION_OP:		flag = s.sendToSink ((void*)" + ",  3); break;
    	case SUBTRACTION_OP:	flag = s.sendToSink ((void*)" - ",  3); break;
    	case MULTIPLICATION_OP:	flag = s.sendToSink ((void*)" * ",  3); break;
    	case DIVISION_OP:		flag = s.sendToSink ((void*)" / ",  3); break;
    	case MODULUS_OP:		flag = s.sendToSink ((void*)" % ",  3); break;
    	case LOGICAL_OR_OP:		flag = s.sendToSink ((void*)" || ", 4); break;
    	case LOGICAL_AND_OP:	flag = s.sendToSink ((void*)" && ", 4); break;
    	case BITWISE_OR_OP:		flag = s.sendToSink ((void*)" | ",  3); break;
    	case BITWISE_XOR_OP:	flag = s.sendToSink ((void*)" ^ ",  3); break;
    	case BITWISE_AND_OP:	flag = s.sendToSink ((void*)" & ",  3); break;
    	case LEFT_SHIFT_OP:		flag = s.sendToSink ((void*)" >> ", 4); break;
    	case ARITH_RIGHT_SHIFT_OP:flag=s.sendToSink((void*)" >>> ", 5); break;
    	case LOGICAL_RIGHT_SHIFT_OP:flag=s.sendToSink((void*)" >> ",4); break;

		default:
			return false;
	}
	
	// ensure operator was written ok; dump right hand operand of expression
	return (flag && child2 && child2->toSink(s));
}


void Operation::
operate (OpKind op, Value &op1, Value &op2, Value &result)
{
	Value dummy, answer;

	_doOperation (op, op1, op2, dummy, true, true, false, answer);
	result.copyFrom( answer );
}


void Operation::
operate (OpKind op, Value &op1, Value &op2, Value &op3, Value &result)
{
	Value answer;
	_doOperation (op, op1, op2, op3, true, true, true, answer);
	result.copyFrom( answer );
}


void Operation::
_doOperation (OpKind op, EvalValue &val1, EvalValue &val2, EvalValue &val3, 
			bool valid1, bool valid2, bool valid3, EvalValue &result)
{
	ValueType	vt1,  vt2,  vt3;

	// get the types of the values
	vt1 = val1.getType ();
	vt2 = val2.getType ();
	vt3 = val3.getType ();

	// take care of the easy cases
	if (op == __NO_OP__ 	|| op == PARENTHESES_OP)
	{
		result.copyFrom (val1);
		return;
	}
	else
	if (op == UNARY_PLUS_OP)
	{
		if (vt1 == STRING_VALUE || vt1 == LIST_VALUE || vt1 == CLASSAD_VALUE)
			result.setErrorValue();
		else
			result.copyFrom (val1);
		return;
	}

	// test for cases when evaluation is strict
	if (op != META_EQUAL_OP && op != META_NOT_EQUAL_OP 	&&
		op != LOGICAL_OR_OP && op != LOGICAL_AND_OP		&&
		op != TERNARY_OP)
	{
		// check for error values
		if (vt1==ERROR_VALUE || vt2==ERROR_VALUE || vt3==ERROR_VALUE)
		{
			result.setErrorValue ();
			return;
		}

		// check for undefined values.  we need to check if the corresponding
		// tree exists, because these values would be undefined" anyway then.
		if (valid1 && vt1==UNDEFINED_VALUE	||
			valid2 && vt2==UNDEFINED_VALUE 	||
			valid3 && vt3==UNDEFINED_VALUE)
		{
			result.setUndefinedValue();
			return;
		}
	}

	// comparison operations (binary, one unary)
	if (op >= __COMPARISON_START__ && op <= __COMPARISON_END__)
	{
		doComparison (op, val1, val2, result);
		return;
	}

	// arithmetic operations (binary)
	if (op >= __ARITHMETIC_START__ && op <= __ARITHMETIC_END__)
	{
		doArithmetic (op, val1, val2, result);
		return;
	}

	// logical operators (binary, one unary)
	if (op >= __LOGIC_START__ && op <= __LOGIC_END__)
	{
		doLogical (op, val1, val2, result);
		return;
	}

	// bitwise operators (binary, one unary)
	if (op >= __BITWISE_START__ && op <= __BITWISE_END__)
	{
		doBitwise (op, val1, val2, result);
		return;
	}
	
	// misc. -- ternary op
	if (op == TERNARY_OP)
	{
		int   i; 
		double r;

		// if the selector is error, classad or list, propagate error
		if (vt1==ERROR_VALUE || vt1==CLASSAD_VALUE || vt1==LIST_VALUE)
		{
			result.setErrorValue();	
			return;
		}

		// if the selector is UNDEFINED, the result is undefined
		if (vt1==UNDEFINED_VALUE) 
		{
			result.setUndefinedValue();
			return;
		}

		// val1 is either a real or an integer
		if ((val1.isIntegerValue(i)&&(i!=0)) || (val1.isRealValue(r)&&(r!=0)))
				result.copyFrom (val2);
		else
				result.copyFrom (val3);

		return;
	}

	// should not reach here
	EXCEPT ("Should not get here");
}


void Operation::
_evaluate (EvalState &state, EvalValue &result)
{
	EvalValue	val1, val2, val3;
	bool	valid1, valid2, valid3;

	valid1 = false;
	valid2 = false;
	valid3 = false;

	// evaluate all valid children
	if (child1) 
	{
		child1->evaluate (state, val1);
		valid1 = true;
	}

	if (child2) 
	{
		child2->evaluate (state, val2);
		valid2 = true;
	}


	if (child3) 
	{
		child3->evaluate (state, val3);
		valid3 = true;
	}

	_doOperation (operation, val1, val2, val3, valid1, valid2, valid3, result);

	return;
}


void Operation::
doComparison (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	ValueType vt1, vt2, coerceResult;

	// do numerical type promotions --- other types/values are unchanged
	coerceResult = coerceToNumber(v1, v2);
	vt1 = v1.getType();
	vt2 = v2.getType();

	// perform comparison for =?= ; true iff same types and same values
	if (op == META_EQUAL_OP)
	{
		if (vt1 != vt2)
		{
			result.setIntegerValue (0);
			return;
		}
		
		// undefined or error
		if (vt1 == UNDEFINED_VALUE || vt1 == ERROR_VALUE)
		{
			result.setIntegerValue (1);
			return;
		}

		// if not the above cases, =?= is just like ==
		op = EQUAL_OP;
	}
	// perform comparison for =!= ; negation of =?=
	if (op == META_NOT_EQUAL_OP)
	{
		if (vt1 != vt2)
		{
			result.setIntegerValue (1);
			return;
		}

		// undefined or error
		if (vt1 == UNDEFINED_VALUE || vt1 == ERROR_VALUE ||
			vt2 == UNDEFINED_VALUE || vt2 == ERROR_VALUE)
		{
			result.setIntegerValue (0);
			return;
		}

		// if not the above cases, =!= is just like !=
		op = NOT_EQUAL_OP;
	}

	switch (coerceResult)
	{
		// at least one of v1, v2 is a string
		case STRING_VALUE:
			// check if both are strings
			if (vt1 != STRING_VALUE || vt2 != STRING_VALUE)
			{
				// comparison between strings and non-exceptional non-string 
				// values is undefined
				result.setUndefinedValue();
				return;
			}
			compareStrings (op, v1, v2, result);
			return;

		case INTEGER_VALUE:
			compareIntegers (op, v1, v2, result);
			return;

		case REAL_VALUE:
			compareReals (op, v1, v2, result);
            return;
	
		case LIST_VALUE:
		case CLASSAD_VALUE:
			result.setErrorValue();
			return;

		default:
			// should not get here
			EXCEPT ("Should not get here");
			return;
	}
}


void Operation::
doArithmetic (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	int		i1, i2;
	double 	r1;

	// ensure the operands have arithmetic types
	if( ( !v1.isIntegerValue() && !v1.isRealValue() ) ||
	    ( !v2.isIntegerValue() && !v2.isRealValue() ) )
	{
		result.setErrorValue ();
		return;
	}

	// take care of the one unary arithmetic operator
	if (op == UNARY_MINUS_OP)
	{
		if (v1.isIntegerValue (i1))
		{
			result.setIntegerValue (-i1);
			return;
		}
		else	
		if (v1.isRealValue (r1))
		{
			result.setRealValue (-r1);
			return;
		}

		// v1 is either UNDEFINED or ERROR ... the result is the same as v1
		result.copyFrom (v1);
		return;
	}

	// ensure none of the operands are strings
	switch (coerceToNumber (v1, v2))
	{
		case INTEGER_VALUE:
			v1.isIntegerValue (i1);
			v2.isIntegerValue (i2);
			switch (op)
			{
				case ADDITION_OP:		result.setIntegerValue(i1+i2);	return;
				case SUBTRACTION_OP:	result.setIntegerValue(i1-i2);	return;
				case MULTIPLICATION_OP:	result.setIntegerValue(i1*i2);	return;
				case DIVISION_OP:		
					if (i2 != 0)
						result.setIntegerValue(i1/i2);
					else
						result.setErrorValue ();
					return;
				case MODULUS_OP:
					if (i2 != 0)
						result.setIntegerValue(i1%i2);
					else
						result.setErrorValue ();
					return;
							
				default:
					// should not reach here
					EXCEPT ("Should not get here");
					return;
			}

		case REAL_VALUE:
			doRealArithmetic (op, v1, v2, result);
			return;	

		default:
			// should not get here
			EXCEPT ("Should not get here");
	}

	return;
}


void Operation::
doLogical (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	int		i1, i2;
	double	r1, r2;
	ValueType vt1 = v1.getType();
	ValueType vt2 = v2.getType();

	// stash i1, i2; r1 and r2 --- some of them will be invalid
	v1.isIntegerValue (i1);
	v1.isRealValue (r1);
	v2.isIntegerValue (i2);
	v2.isRealValue (r2);

	// no logic on strings, classads or lists 
	if (vt1==STRING_VALUE || vt1==LIST_VALUE || vt1==CLASSAD_VALUE) 
		vt1 = ERROR_VALUE;
	if (vt2==STRING_VALUE || vt2==LIST_VALUE || vt2==CLASSAD_VALUE) 
		vt2 = ERROR_VALUE;

	// handle unary operator
	if (op == LOGICAL_NOT_OP)
	{
		if (vt1 == INTEGER_VALUE)
			result.setIntegerValue(!i1);
		else
		if (vt1 == REAL_VALUE)
			result.setIntegerValue((int)(!r1));
		else
			// v1 is either UNDEFINED or ERROR
			result.copyFrom(v1);

		return;
	}

	// logic with UNDEFINED and ERROR
	if (op == LOGICAL_OR_OP)
	{
		// short circuiting case ...
		if ((vt1==INTEGER_VALUE && i1) || (vt1==REAL_VALUE && r1))
			result.setIntegerValue(1);
		else
		// v1 is not true --- check v2
		if ((vt2==INTEGER_VALUE && i2) || (vt2==REAL_VALUE && r2))
			result.setIntegerValue(1);
		else
		// v1 is not true and v2 is not true; if either of them is ERROR,
		// the result of the computation is ERROR
		if (vt1==ERROR_VALUE || vt2==ERROR_VALUE) 
			result.setErrorValue();
		else
		// if either of them is UNDEFINED, the result is UNDEFINED
		if (vt1==UNDEFINED_VALUE || vt2==UNDEFINED_VALUE)
			result.setUndefinedValue();
		else
			// must be false
			result.setIntegerValue(0);
	}
	else
	if (op == LOGICAL_AND_OP)
	{
        // short circuiting case ...
        if ((vt1==INTEGER_VALUE && !i1) || (vt1==REAL_VALUE && !r1))
            result.setIntegerValue(0);
		else
        // v1 is not false --- check v2
        if ((vt2==INTEGER_VALUE && !i2) || (vt2==REAL_VALUE && !r2))
            result.setIntegerValue(0);
		else
        // v1 is not false and v2 is not false; if either of them is ERROR,
        // the result of the computation is ERROR
        if (vt1==ERROR_VALUE || vt2==ERROR_VALUE) result.setErrorValue();
		else
        // if either of them is UNDEFINED, the result is UNDEFINED
        if (vt1==UNDEFINED_VALUE || vt2==UNDEFINED_VALUE)
            result.setUndefinedValue();
		else
        	// must be true
        	result.setIntegerValue(1);
	}

	// done
	return;
}



void Operation::
doBitwise (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	int	i1, i2;
	int signMask = 1 << (WORD_BIT-1);	// now at the position of the sign bit
	int val;

	// bitwise operations are defined only on integers
	if (op == BITWISE_NOT_OP)
	{
		if (!v1.isIntegerValue(i1))
		{
			result.setErrorValue();
			return;
		}
	}
	else
	if (!v1.isIntegerValue(i1) || !v2.isIntegerValue(i2))
	{
		result.setErrorValue();
		return;
	}

	switch (op)
	{
		case BITWISE_NOT_OP:	result.setIntegerValue(~i1);	return;
		case BITWISE_OR_OP:		result.setIntegerValue(i1|i2);	return;
		case BITWISE_AND_OP:	result.setIntegerValue(i1&i2);	return;
		case BITWISE_XOR_OP:	result.setIntegerValue(i1^i2);	return;
		case LEFT_SHIFT_OP:		result.setIntegerValue(i1<<i2);	return;

		case LOGICAL_RIGHT_SHIFT_OP:
			if (i1 >= 0)
			{
				// sign bit is not on;  >> will work fine
				result.setIntegerValue (i1 >> i2);
				return;
			}
			else
			{
				// sign bit is on
				val = i1 >> 1;	    // shift right 1; the sign bit *may* be on
				val &= (~signMask);	// clear the sign bit for sure
				val >>= (i2 - 1);	// shift remaining number of positions
				result.setIntegerValue (val);
				return;
			}
			// will not reach here
			return;

		case ARITH_RIGHT_SHIFT_OP:
			if (i1 >= 0)
			{
				// sign bit is off;  >> will work fine
				result.setIntegerValue (i1 >> i2);
				return;
			}
			else
			{
				// sign bit is on; >> *may* not set the sign
				val = i1;
				for (int i = 0; i < i2; i++)
					val = (val >> 1) | signMask;	// make sure that it does
				result.setIntegerValue (val);
				return;
			}
			// will not reach here
			return;

		default:
			// should not get here
			EXCEPT ("Should not get here");
	}
}


static volatile bool ClassAdExprFPE = false;
#ifndef WIN32
void ClassAd_SIGFPE_handler (int) { ClassAdExprFPE = true; }
#endif

void Operation::
doRealArithmetic (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	double r1, r2;	
	double comp;

	// we want to prevent FPE and set the ERROR value on the result; on Unix
	// trap sigfpe and set the ClassAdExprFPE flag to true; on NT check the 
	// result against HUGE_VAL.  check errno for EDOM and ERANGE for kicks.

	v1.isRealValue (r1);
	v2.isRealValue (r2);

#ifndef WIN32
    struct sigaction sa1, sa2;
    sa1.sa_handler = ClassAd_SIGFPE_handler;
    sigemptyset (&(sa1.sa_mask));
    sa1.sa_flags = 0;
    if (sigaction (SIGFPE, &sa1, &sa2))
    {
        dprintf (D_ALWAYS, 
			"Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
			errno);
    }
#endif

	ClassAdExprFPE = false;
	errno = 0;
	switch (op)
	{
		case ADDITION_OP:       comp = r1+r2;  break;
		case SUBTRACTION_OP:    comp = r1-r2;  break;
		case MULTIPLICATION_OP: comp = r1*r2;  break;
		case DIVISION_OP:		comp = r1/r2;  break;
		case MODULUS_OP:		errno = EDOM;  break;

		default:
			// should not reach here
			EXCEPT ("Should not get here");
			return;
	}

	// check if anything bad happened
	if (ClassAdExprFPE==true || errno==EDOM || errno==ERANGE || comp==HUGE_VAL)
		result.setErrorValue ();
	else
		result.setRealValue (comp);

	// restore the state
#ifndef WIN32 
    if (sigaction (SIGFPE, &sa2, &sa1))
    {
        dprintf (D_ALWAYS, 
			"Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
			errno);
    }
#endif
}


void Operation::
compareStrings (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	char *s1, *s2;
	int  cmp;
	
	v1.isStringValue (s1);
	v2.isStringValue (s2);

	result.setIntegerValue (0);
	cmp = CLASSAD_STR_VALUES_STRCMP (s1, s2);
	if (cmp < 0)
	{
		// s1 < s2
		if (op == LESS_THAN_OP 		|| 
			op == LESS_OR_EQUAL_OP 	|| 
			op == NOT_EQUAL_OP)
		{
			result.setIntegerValue (1);
		}
	}
	else
	if (cmp == 0)
	{
		// s1 == s2
		if (op == LESS_OR_EQUAL_OP 	|| 
			op == EQUAL_OP			||
			op == GREATER_OR_EQUAL_OP)
		{
			result.setIntegerValue (1);
		}
	}
	else
	{
		// s1 > s2
		if (op == GREATER_THAN_OP	||
			op == GREATER_OR_EQUAL_OP	||
			op == NOT_EQUAL_OP)
		{
			result.setIntegerValue (1);
		}
	}
}


void Operation::
compareIntegers (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	int i1, i2, compResult;

	v1.isIntegerValue (i1); 
	v2.isIntegerValue (i2);

	switch (op)
	{
		case LESS_THAN_OP: 			compResult = (i1 < i2); 	break;
		case LESS_OR_EQUAL_OP: 		compResult = (i1 <= i2); 	break;
		case EQUAL_OP: 				compResult = (i1 == i2); 	break;
		case NOT_EQUAL_OP: 			compResult = (i1 != i2); 	break;
		case GREATER_THAN_OP: 		compResult = (i1 > i2); 	break;
		case GREATER_OR_EQUAL_OP: 	compResult = (i1 >= i2); 	break;
		default:
			// should not get here
			EXCEPT ("Should not get here");
			return;
	}

	result.setIntegerValue (compResult);
}


void Operation::
compareReals (OpKind op, EvalValue &v1, EvalValue &v2, EvalValue &result)
{
	double r1, r2;
	int	compResult;

	v1.isRealValue (r1);
	v2.isRealValue (r2);

	switch (op)
	{
		case LESS_THAN_OP:          compResult = (r1 < r2);     break;
		case LESS_OR_EQUAL_OP:      compResult = (r1 <= r2);    break;
		case EQUAL_OP:              compResult = (r1 == r2);    break;
		case NOT_EQUAL_OP:          compResult = (r1 != r2);    break;
		case GREATER_THAN_OP:       compResult = (r1 > r2);     break;
		case GREATER_OR_EQUAL_OP:   compResult = (r1 >= r2);    break;
		default:
			// should not get here
			EXCEPT ("Should not get here");
			return;
	}

	result.setIntegerValue (compResult);
}


// This function performs type promotions so that both v1 and v2 are of the
// same numerical type: (v1 and v2 are not ERROR or UNDEFINED)
//  + if either of v1 or v2 are strings return STRING_VALUE
//  + if both v1 and v2 are numbers and of the same type, return type
//  + if v1 is an int and v2 is a real, convert v1 to real; return REAL_VALUE
//  + if v1 is a real and v2 is an int, convert v2 to real; return REAL_VALUE
ValueType Operation::
coerceToNumber (EvalValue &v1, EvalValue &v2)
{
	char 	*s;
	int	 	i;
	double 	r;

	// either of v1, v2 not numerical?
	if (v1.isClassAdValue()   || v2.isClassAdValue())   return CLASSAD_VALUE;
	if (v1.isListValue()      || v2.isListValue())   	return LIST_VALUE;
	if (v1.isStringValue (s)  || v2.isStringValue (s))  return STRING_VALUE;
	if (v1.isUndefinedValue() || v2.isUndefinedValue()) return UNDEFINED_VALUE;
	if (v1.isErrorValue ()    || v2.isErrorValue ())    return ERROR_VALUE;

	// both v1 and v2 of same numerical type
	if (v1.isIntegerValue(i) && v2.isIntegerValue(i)) return INTEGER_VALUE;
	if (v1.isRealValue(r) && v2.isRealValue(r)) return REAL_VALUE;

	// type promotions required
	if (v1.isIntegerValue(i) && v2.isRealValue(r))
		v1.setRealValue ((double)i);
	else
	if (v1.isRealValue(r) && v2.isIntegerValue(i))
		v2.setRealValue ((double)i);

	return REAL_VALUE;
}

 
void Operation::
setOperation (OpKind op, ExprTree *e1, ExprTree *e2, ExprTree *e3)
{
	operation = op;
	child1    = e1;
	child2    = e2;
	child3    = e3;
}


void Operation::
setParentScope( ClassAd *ad )
{
	if( child1 ) child1->setParentScope( ad );
	if( child2 ) child2->setParentScope( ad );
	if( child3 ) child3->setParentScope( ad );
}
