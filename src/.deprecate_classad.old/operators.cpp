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
#include "condor_debug.h"
#include "operators.h"

static void _doOperation	(OpKind,Value&,Value&,Value&,bool,bool,bool,Value&);
static void doComparison	(OpKind, Value&, Value&, Value&);
static void doArithmetic	(OpKind, Value&, Value&, Value&);
static bool doLogicalShortCircuit (OpKind op, Value &v1, Value &result);
static void doLogical	 	(OpKind, Value&, Value&, Value&);
static void doBitwise	 	(OpKind, Value&, Value&, Value&);
static void doRealArithmetic(OpKind, Value&, Value&, Value&);
static void compareStrings	(OpKind, Value&, Value&, Value&, bool case_sensitive);
static void compareReals	(OpKind, Value&, Value&, Value&);
static void compareIntegers (OpKind, Value&, Value&, Value&);
static ValueType coerceToNumber(Value&, Value&);


const char *opString[] = 
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
	"?:"
};


void 
operate (OpKind op, Value &op1, Value &result)
{
	Value dummy;

	_doOperation (op, op1, dummy, dummy, true, false, false, result);
}

void 
operate (OpKind op, Value &op1, Value &op2, Value &result)
{
	Value dummy;

	_doOperation (op, op1, op2, dummy, true, true, false, result);
}

bool 
operateShortCircuit (OpKind op, Value &op1, Value &result)
{
    bool did_short_circuit;
    if (op == LOGICAL_OR_OP || op == LOGICAL_AND_OP) 
    {
        did_short_circuit = doLogicalShortCircuit(op, op1, result);
    }
    else
    {
        did_short_circuit = false;
    }
    return did_short_circuit;
}


static void 
_doOperation (OpKind op, Value &val1, Value &val2, Value &val3, bool valid1, 
			bool valid2, bool valid3, Value &result)
{
	ValueType	vt1,  vt2,  vt3;

	// get the types of the values
	vt1 = val1.getType ();
	vt2 = val2.getType ();
	vt3 = val3.getType ();

	// take care of the easy cases
	if (op == __NO_OP__ 	|| op == PARENTHESES_OP)
	{
		result = val1;
		return;
	}
	else
	if (op == UNARY_PLUS_OP)
	{
		if (vt1 != INTEGER_VALUE && vt1 != REAL_VALUE)
			result.setErrorValue();
		else
			result = val1;
		return;
	}

	// test for cases when evaluation is strict w.r.t. "error" and "undefined"
	if (op != META_EQUAL_OP && op != META_NOT_EQUAL_OP 	&&
		op != LOGICAL_OR_OP && op != LOGICAL_AND_OP		&&
		op != TERNARY_OP)
	{
		// check for error values
		if (valid1 && vt1==ERROR_VALUE || 
			valid2 && vt2==ERROR_VALUE || 
			valid3 && vt3==ERROR_VALUE)
		{
			result.setErrorValue ();
			return;
		}

		// check for undefined values
		if (valid1 && vt1==UNDEFINED_VALUE	||
			valid2 && vt2==UNDEFINED_VALUE 	||
			valid3 && vt3==UNDEFINED_VALUE)
		{
			result.setUndefinedValue();
			return;
		}
	}

	// comparison operations (binary)
	if (op >= __COMPARISON_START__ && op <= __COMPARISON_END__)
	{
		doComparison (op, val1, val2, result);
		return;
	}

	// arithmetic operations (binary, one unary)
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

		// if the selector expression is error, propagate it
		if (vt1==ERROR_VALUE)
		{
			result.setErrorValue();	
			return;
		}

		// if the selector is a string or UNDEFINED, the value of the
		// expression is undefined
		if (vt1==STRING_VALUE || vt1==UNDEFINED_VALUE) 
		{
			result.setUndefinedValue();
			return;
		}

		// val1 is either a real or an integer
		if ((val1.isIntegerValue(i)&&(i!=0)) || (val1.isRealValue(r)&&(r!=0)))
				result = val2;
		else
				result = val3;

		return;
	}

	// should not reach here
	EXCEPT ("Should not get here");
}


static void 
doComparison (OpKind op, Value &v1, Value &v2, Value &result)
{
	ValueType vt1, vt2, coerceResult;
	bool case_sensitive = false;

	// do numerical type promotions --- other types/values are unchanged
	coerceResult = coerceToNumber (v1, v2);
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

		// if not the above cases, =?= is just like ==, but
		// case-sensitive for strings
		op = EQUAL_OP;
		case_sensitive = true;
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

		// if not the above cases, =!= is just like !=, but
		// case-sensitive for strings
		op = NOT_EQUAL_OP;
		case_sensitive = true;
	}

	switch (coerceResult)
	{
		// at least one of v1, v2 is a string
		case STRING_VALUE:
			// check if both are strings
			if (vt1 != STRING_VALUE || vt2 != STRING_VALUE)
			{
				// comparison between strings and non-exceptional non-string 
				// values is an error
				result.setErrorValue();
				return;
			}
			compareStrings (op, v1, v2, result, case_sensitive);
			return;

		case INTEGER_VALUE:
			compareIntegers (op, v1, v2, result);
			return;

		case REAL_VALUE:
			compareReals (op, v1, v2, result);
            return;
	
		default:
			// should not get here
			EXCEPT ("Should not get here");
			return;
	}
}


static void 
doArithmetic (OpKind op, Value &v1, Value &v2, Value &result)
{
	int		i1, i2;
	double 	r1;
	char	*s;

	// ensure the operands are not strings
	if (v1.isStringValue (s) || v2.isStringValue (s))
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
		result = v1;
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

static bool
doLogicalShortCircuit (OpKind op, Value &v1, Value &result)
{
    int       i1;
    double    r1;
    ValueType vt1 = v1.getType();
    bool      did_short_circuit;

    did_short_circuit = false;
    v1.isIntegerValue (i1);
    v1.isRealValue (r1);

    if (op == LOGICAL_OR_OP)
    {
        // no logic on strings --- a string is equivalent to the ERROR
        // value. But due to the logic in doLogical, we can't short-circuit
        // on it, because "somestring" || 3 would evaluate to 1. Stupid
        // old ClassAds.
        if (vt1 == STRING_VALUE) 
        {
            did_short_circuit = false;
        }
        else
        if ((vt1 == INTEGER_VALUE && i1 != 0) || (vt1 == REAL_VALUE && r1 != 0)) 
        {
            result.setIntegerValue(1);
            did_short_circuit = true;
        }
        // We don't short-circuit on error because according to the
        // stupid semantics of old ClassAds, error || 3 is 1. (See
        // doLogical.) Ditto for undefined, but that's actually good.
        else
        {
            did_short_circuit = false;
        }
    }
    else
    if (op == LOGICAL_AND_OP)
    {
        // no logic on strings --- a string is equivalent to the ERROR
        // value. 
        if (vt1 == STRING_VALUE) 
        {
            result.setErrorValue();
            did_short_circuit = true;
        }
        else
        if ((vt1 == INTEGER_VALUE && i1 == 0) || (vt1==REAL_VALUE && r1 == 0))
        {
            result.setIntegerValue(0);
            did_short_circuit = true;
        }
        else
        if (vt1 == ERROR_VALUE) 
        {
            result.setErrorValue();
            did_short_circuit = true;
        }
        else
        if (vt1 == UNDEFINED_VALUE)
        {
            result.setUndefinedValue();
            did_short_circuit = true;
        }
        else
        {
            did_short_circuit = false;
        }
    }

    // done
    return did_short_circuit;
}

static void 
doLogical (OpKind op, Value &v1, Value &v2, Value &result)
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

	// no logic on strings --- a string is equivalent to the ERROR value
	if (vt1==STRING_VALUE) vt1 = ERROR_VALUE;
 	if (vt2==STRING_VALUE) vt2 = ERROR_VALUE;

	// handle unary operator
	if (op == LOGICAL_NOT_OP)
	{
		if (vt1 == INTEGER_VALUE)
			result.setIntegerValue(!i1);
		else
		if (vt1 == REAL_VALUE)
			result.setIntegerValue((int)(!r1));
		else
		{
			// either undefined or error
			result = v1;
		}

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



static void 
doBitwise (OpKind op, Value &v1, Value &v2, Value &result)
{
	int	i1, i2;
	int signMask = ~INT_MAX;			// 1 at the position of the sign bit
	int val;

	// bitwise operations are defined only on integers
	if (op == BITWISE_NOT_OP)
	{
		if (!v1.isIntegerValue(i1))
		{
			// make sure that ERROR is propagated
			if (v1.isErrorValue()) 
				result.setErrorValue();
			else
				result.setUndefinedValue();
			return;
		}
	}
	else
	if (!v1.isIntegerValue(i1) || !v2.isIntegerValue(i2))
	{
		// make sure that errors are propagated
		if (v1.isErrorValue() || v2.isErrorValue())
		{
			result.setErrorValue();
			return;
		}
		
		result.setUndefinedValue();
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

static void
doRealArithmetic (OpKind op, Value &v1, Value &v2, Value &result)
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
	if (ClassAdExprFPE==true || errno==EDOM || errno==ERANGE) {
	  result.setErrorValue ();
#ifdef WIN32
	} else if (comp==HUGE_VAL) {
	  result.setErrorValue ();
#endif
	} else {
	  result.setRealValue (comp);
	}

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


static void 
compareStrings (OpKind op, Value &v1, Value &v2, Value &result, bool case_sensitive)
{
	char *s1, *s2;
	int  cmp;
	
	v1.isStringValue (s1);
	v2.isStringValue (s2);

	result.setIntegerValue (0);
	if (case_sensitive) {
		cmp = strcmp(s1, s2);
	} else {
		cmp = strcasecmp(s1, s2);
	}
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


static void
compareIntegers (OpKind op, Value &v1, Value &v2, Value &result)
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


static void 
compareReals (OpKind op, Value &v1, Value &v2, Value &result)
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
static ValueType 
coerceToNumber (Value &v1, Value &v2)
{
	char 	*s;
	int	 	i;
	double 	r;

	// either of v1, v2 not numerical?
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
