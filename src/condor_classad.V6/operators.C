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

	if (child1 && (newTree->child1 = child1->copy()) == NULL) {
		return NULL;
	}

	if (child2 && (newTree->child2 = child2->copy()) == NULL) {
		delete newTree->child1;
		return NULL;
	}

	if (child3 && (newTree->child3 = child3->copy()) == NULL) {
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
	if (operation == PARENTHESES_OP) {
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
	if (operation == TERNARY_OP) {
		if ((!s.sendToSink ((void*)" ? ", 3))	||
			(!child2 || !child2->toSink(s))		||
			(!s.sendToSink ((void*)" : ", 3))	||
			(!child3 || !child3->toSink(s)))
				return false;
		else
			return true;
	} else if( operation == SUBSCRIPT_OP ) {
		if( ( !s.sendToSink( (void*)"[", 1 ) )	||	
			( !child2 || !child2->toSink( s ) ) ||
			( !s.sendToSink( (void*)"]", 1 ) ) )
				return false;
		else
			return true;
	}

	// the rest are infix binary operators
	switch (operation) {
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
	if (op == __NO_OP__ 	|| op == PARENTHESES_OP) {
		result.copyFrom (val1);
		return;
	} else if (op == UNARY_PLUS_OP) {
		if (vt1 == STRING_VALUE || vt1 == LIST_VALUE || vt1 == CLASSAD_VALUE)
			result.setErrorValue();
		else
			result.copyFrom (val1);
		return;
	}

	// test for cases when evaluation is strict
	if (op != META_EQUAL_OP && op != META_NOT_EQUAL_OP 	&&
		op != LOGICAL_OR_OP && op != LOGICAL_AND_OP		&&
		op != TERNARY_OP) {
		// check for error values
		if (vt1==ERROR_VALUE || vt2==ERROR_VALUE || vt3==ERROR_VALUE) {
			result.setErrorValue ();
			return;
		}

		// check for undefined values.  we need to check if the corresponding
		// tree exists, because these values would be undefined" anyway then.
		if (valid1 && vt1==UNDEFINED_VALUE	||
			valid2 && vt2==UNDEFINED_VALUE 	||
			valid3 && vt3==UNDEFINED_VALUE) {
			result.setUndefinedValue();
			return;
		}
	}

	// comparison operations (binary, one unary)
	if (op >= __COMPARISON_START__ && op <= __COMPARISON_END__) {
		doComparison (op, val1, val2, result);
		return;
	}

	// arithmetic operations (binary)
	if (op >= __ARITHMETIC_START__ && op <= __ARITHMETIC_END__) {
		doArithmetic (op, val1, val2, result);
		return;
	}

	// logical operators (binary, one unary)
	if (op >= __LOGIC_START__ && op <= __LOGIC_END__) {
		doLogical (op, val1, val2, result);
		return;
	}

	// bitwise operators (binary, one unary)
	if (op >= __BITWISE_START__ && op <= __BITWISE_END__) {
		doBitwise (op, val1, val2, result);
		return;
	}
	
	// misc.
	if (op == TERNARY_OP) {
		// ternary (if-operator)
		int   i; 
		double r;

		// if the selector is error, classad or list, propagate error
		if (vt1==ERROR_VALUE || vt1==CLASSAD_VALUE || vt1==LIST_VALUE) {
			result.setErrorValue();	
			return;
		}

		// if the selector is UNDEFINED, the result is undefined
		if (vt1==UNDEFINED_VALUE) {
			result.setUndefinedValue();
			return;
		}

		// val1 is either a real or an integer
		if ((val1.isIntegerValue(i)&&(i!=0)) || (val1.isRealValue(r)&&(r!=0)))
			result.copyFrom (val2);
		else
			result.copyFrom (val3);

		return;
	} else if( op == SUBSCRIPT_OP ) {
		ValueList 	*vl;
		int			index;
		EvalValue	*eval;

		// subscripting from a list (strict)
		if( vt1 != LIST_VALUE || vt2 != INTEGER_VALUE ) {
			result.setErrorValue();
			return;
		}

		val1.isListValue( vl );		
		val2.isIntegerValue( index );

		// check bounds
		if( vl->Number() <= index || index < 0 ) {
			result.setUndefinedValue();
			return;
		}

		// access the index'th element (0-based)
		vl->Rewind();
		index++;
		while( index && ( eval = vl->Next() ) ) {
			index --;
		}
		result.copyFrom( *eval );
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
	if (child1) {
		child1->evaluate (state, val1);
		valid1 = true;
	}

	if (child2) {
		child2->evaluate (state, val2);
		valid2 = true;
	}


	if (child3) {
		child3->evaluate (state, val3);
		valid3 = true;
	}

	_doOperation (operation, val1, val2, val3, valid1, valid2, valid3, result);

	return;
}


bool Operation::
_flatten( EvalState &state, EvalValue &val, ExprTree *&tree, OpKind *opPtr )
{
	OpKind		childOp1=__NO_OP__, childOp2=__NO_OP__;
	ExprTree	*fChild1=NULL, *fChild2=NULL;
	EvalValue	val1, val2, val3, result;
	OpKind		newOp = operation, op = operation;
	
	// if op is not associative, don't allow splitting 
	if( op == SUBTRACTION_OP || op == DIVISION_OP || op == MODULUS_OP ||
		op == LEFT_SHIFT_OP  || op == LOGICAL_RIGHT_SHIFT_OP || 
		op == ARITH_RIGHT_SHIFT_OP ) 
	{
		if( opPtr ) *opPtr = __NO_OP__;
		if( child1->flatten( state, val1, fChild1 ) &&
			child2->flatten( state, val2, fChild2 ) ) {
			if( !fChild1 && !fChild2 ) {
				_doOperation(op, val1, val2, val3, true, true, false, val);	
				tree = NULL;
				return true;
			} else if( fChild1 && fChild2 ) {
				tree = Operation::makeOperation( op, fChild1, fChild2 );
				return true;
			} else if( fChild1 ) {
				tree = Operation::makeOperation( op, fChild1, val2 );
				return true;
			} else if( fChild2 ) {
				tree = Operation::makeOperation( op, val1, fChild2 );
				return true;
			}
		} else {
			if( fChild1 ) delete fChild1;
			if( fChild2 ) delete fChild2;
			tree = NULL;
			return false;
		}
	} else if( op == TERNARY_OP || op == SUBSCRIPT_OP || op ==  UNARY_PLUS_OP ||
				op == UNARY_MINUS_OP || op == PARENTHESES_OP || 
				op == LOGICAL_NOT_OP || op == BITWISE_NOT_OP ) {
		return flattenSpecials( state, val, tree );
	}
					
	// any op that got past the above is binary, commutative and associative

	// flatten sub expressions
	if( child1 && !child1->flatten( state, val1, fChild1, &childOp1 ) ||
		child2 && !child2->flatten( state, val2, fChild2, &childOp2 ) ) {
		tree = NULL;
		return false;
	}

	if( !combine( newOp, val, tree, childOp1, val1, fChild1, 
						childOp2, val2, fChild2 ) ) {
		tree = NULL;
		if( opPtr ) *opPtr = __NO_OP__;
		return false;
	}

	// if splitting is disallowed, fold the value and tree into a tree
	if( !opPtr && newOp != __NO_OP__ ) {
		tree = Operation::makeOperation( newOp, val, tree );
		if( !tree ) {
			if( opPtr ) *opPtr = __NO_OP__;
			return false;
		}
		return true;
	} else if( opPtr ) {
		*opPtr = newOp;
	}

	return true;
}


bool Operation::
combine( OpKind &op, EvalValue &val, ExprTree *&tree,
			OpKind op1, EvalValue &val1, ExprTree *tree1,
			OpKind op2, EvalValue &val2, ExprTree *tree2 )
{
	Operation 	*newOp;
	EvalValue	dummy;

	if( !tree1 && !tree2 ) {
		// left and rightsons are only values
		_doOperation( op, val1, val2, dummy, true, true, false, val );
		tree = NULL;
		op = __NO_OP__;
		return true;
	} else if( !tree1 && (tree2 && op2 == __NO_OP__ ) ) {
		// leftson is a value, rightson is a tree
		tree = tree2;
		val.copyFrom( val1 );
		return true;
	} else if( !tree2 && (tree1 && op1 == __NO_OP__ ) ) {
		// rightson is a value, leftson is a tree
		tree = tree1;
		val.copyFrom( val2 );
		return true;
	} else if( ( tree1 && op1 == __NO_OP__ ) && ( tree2 && op2 == __NO_OP__ ) ){
		// left and rightsons are trees only
		if( !( newOp = new Operation() ) ) return false;
		newOp->setOperation( op, tree1, tree2 );
		tree = newOp;
		op   = __NO_OP__;
		return true;
	}

	// cannot collapse values due to dissimilar ops
	if( ( op1 != __NO_OP__ || op2 != __NO_OP__ ) && op != op1 && op != op2 ) {
		// at least one of them returned a value and a tree, and parent does
		// not share the same operation with either child
		ExprTree	*newOp1 = NULL, *newOp2 = NULL;

		if( op1 != __NO_OP__ ) {
			newOp1 = Operation::makeOperation( op1, val1, tree1 );
		} else if( tree1 ) {
			newOp1 = tree1;
		} else {
			newOp1 = Literal::makeLiteral( val1 );
		}
		
		if( op2 != __NO_OP__ ) {
			newOp2 = Operation::makeOperation( op2, val2, tree2 );
		} else if( tree2 ) {
			newOp2 = tree2;
		} else {
			newOp2 = Literal::makeLiteral( val2 );
		}

		if( !newOp1 || !newOp2 ) {
			if( newOp1 ) delete newOp1;
			if( newOp2 ) delete newOp2;
			tree = NULL; op = __NO_OP__;
			return false;
		}

		if( !( newOp = new Operation() ) ) {
			delete newOp1; delete newOp2;
			tree = NULL; op = __NO_OP__;
			return false;
		}
		newOp->setOperation( op, newOp1, newOp2 );
		op = __NO_OP__;
		tree = newOp;
		return true;
	}

	if( op == op1 && op == op2 ) {
		// same operators on both children . since op!=NO_OP, neither are op1, 
		// op2.  so they both make tree and value contributions
		if( !( newOp = new Operation() ) ) return false;
		newOp->setOperation( op, tree1, tree2 );
		_doOperation( op, val1, val2, dummy, true, true, false, val );
		tree = newOp;
		return true;
	} else if( op == op1 ) {
		// leftson makes a tree,value contribution
		if( !tree2 ) {
			// rightson makes a value contribution
			_doOperation( op, val1, val2, dummy, true, true, false, val );
			tree = tree1;
			return true;
		} else {
			// rightson makes a tree contribution
			Operation *newOp = new Operation();
			if( !newOp ) {
				tree = NULL; op = __NO_OP__;	
				return false;
			}
			newOp->setOperation( op, tree1, tree2 );
			val.copyFrom( val1 );
			return true;
		}
	} else if( op == op2 ) {
		// rightson makes a tree,value contribution
		if( !tree1 ) {
			// leftson makes a value contribution
			_doOperation( op, val1, val2, dummy, true, true, false, val );
			tree = tree2;
			return true;
		} else {
			// leftson makes a tree contribution
			Operation *newOp = new Operation();
			if( !newOp ) {
				tree = NULL; op = __NO_OP__;	
				return false;
			}
			newOp->setOperation( op, tree1, tree2 );
			val.copyFrom( val2 );
			return true;
		}
	}

	EXCEPT( "Should not reach here" );
	return false;
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
	if (op == META_EQUAL_OP) {
		if (vt1 != vt2) {
			result.setIntegerValue (0);
			return;
		}
		
		// undefined or error
		if (vt1 == UNDEFINED_VALUE || vt1 == ERROR_VALUE) {
			result.setIntegerValue (1);
			return;
		}

		// if not the above cases, =?= is just like ==
		op = EQUAL_OP;
	}
	// perform comparison for =!= ; negation of =?=
	if (op == META_NOT_EQUAL_OP) {
		if (vt1 != vt2) {
			result.setIntegerValue (1);
			return;
		}

		// undefined or error
		if (vt1 == UNDEFINED_VALUE || vt1 == ERROR_VALUE ||
			vt2 == UNDEFINED_VALUE || vt2 == ERROR_VALUE) {
			result.setIntegerValue (0);
			return;
		}

		// if not the above cases, =!= is just like !=
		op = NOT_EQUAL_OP;
	}

	switch (coerceResult) {
		// at least one of v1, v2 is a string
		case STRING_VALUE:
			// check if both are strings
			if (vt1 != STRING_VALUE || vt2 != STRING_VALUE) {
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
	    ( !v2.isIntegerValue() && !v2.isRealValue() ) ) {
		result.setErrorValue ();
		return;
	}

	// take care of the one unary arithmetic operator
	if (op == UNARY_MINUS_OP) {
		if (v1.isIntegerValue (i1)) {
			result.setIntegerValue (-i1);
			return;
		} else	if (v1.isRealValue (r1)) {
			result.setRealValue (-r1);
			return;
		}

		// v1 is either UNDEFINED or ERROR ... the result is the same as v1
		result.copyFrom (v1);
		return;
	}

	// ensure none of the operands are strings
	switch (coerceToNumber (v1, v2)) {
		case INTEGER_VALUE:
			v1.isIntegerValue (i1);
			v2.isIntegerValue (i2);
			switch (op) {
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
	if (op == LOGICAL_NOT_OP) {
		if (vt1 == INTEGER_VALUE)
			result.setIntegerValue(!i1);
		else if (vt1 == REAL_VALUE)
			result.setIntegerValue((int)(!r1));
		else
			// v1 is either UNDEFINED or ERROR
			result.copyFrom(v1);

		return;
	}

	// logic with UNDEFINED and ERROR
	if (op == LOGICAL_OR_OP) {
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
	} else if (op == LOGICAL_AND_OP) {
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
        if (vt1==ERROR_VALUE || vt2==ERROR_VALUE) 
			result.setErrorValue();
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
	if (op == BITWISE_NOT_OP) {
		if (!v1.isIntegerValue(i1)) {
			result.setErrorValue();
			return;
		}
	} else if (!v1.isIntegerValue(i1) || !v2.isIntegerValue(i2)) {
		result.setErrorValue();
		return;
	}

	switch (op) {
		case BITWISE_NOT_OP:	result.setIntegerValue(~i1);	return;
		case BITWISE_OR_OP:		result.setIntegerValue(i1|i2);	return;
		case BITWISE_AND_OP:	result.setIntegerValue(i1&i2);	return;
		case BITWISE_XOR_OP:	result.setIntegerValue(i1^i2);	return;
		case LEFT_SHIFT_OP:		result.setIntegerValue(i1<<i2);	return;

		case LOGICAL_RIGHT_SHIFT_OP:
			if (i1 >= 0) {
				// sign bit is not on;  >> will work fine
				result.setIntegerValue (i1 >> i2);
				return;
			} else {
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
			if (i1 >= 0) {
				// sign bit is off;  >> will work fine
				result.setIntegerValue (i1 >> i2);
				return;
			} else {
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
    if (sigaction (SIGFPE, &sa1, &sa2)) {
        dprintf (D_ALWAYS, 
			"Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
			errno);
    }
#endif

	ClassAdExprFPE = false;
	errno = 0;
	switch (op) {
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
    if (sigaction (SIGFPE, &sa2, &sa1)) {
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
	if (cmp < 0) {
		// s1 < s2
		if (op == LESS_THAN_OP 		|| 
			op == LESS_OR_EQUAL_OP 	|| 
			op == NOT_EQUAL_OP) {
			result.setIntegerValue (1);
		}
	} else if (cmp == 0) {
		// s1 == s2
		if (op == LESS_OR_EQUAL_OP 	|| 
			op == EQUAL_OP			||
			op == GREATER_OR_EQUAL_OP) {
			result.setIntegerValue (1);
		}
	} else {
		// s1 > s2
		if (op == GREATER_THAN_OP	||
			op == GREATER_OR_EQUAL_OP	||
			op == NOT_EQUAL_OP) {
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

	switch (op) {
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

	switch (op) {
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

Operation *Operation::
makeOperation( OpKind op, EvalValue &val, ExprTree *tree ) 
{
	Operation  	*newOp;
	Literal		*lit;

	if( !( newOp = new Operation() ) ) return NULL;
	if( !( lit = Literal::makeLiteral( val ) ) ) {
		delete newOp;
		return NULL;
	}

	newOp->setOperation( op, lit, tree );
	return newOp;
}
	
	
Operation *Operation::
makeOperation( OpKind op, ExprTree *tree, EvalValue &val ) 
{
	Operation  	*newOp;
	Literal		*lit;

	if( tree == NULL ) return NULL;

	if( !( newOp = new Operation() ) ) return NULL;
	if( !( lit = Literal::makeLiteral( val ) ) ) {
		delete newOp;
		return NULL;
	}

	newOp->setOperation( op, tree, lit );
	return newOp;
}

Operation *Operation::
makeOperation( OpKind op, ExprTree *tree1 ) 
{
	Operation  	*newOp;

	if( !tree1 ) return NULL;

	if( !( newOp = new Operation() ) ) return NULL;
	newOp->setOperation( op, tree1 );
	return newOp;
}


Operation *Operation::
makeOperation( OpKind op, ExprTree *tree1, ExprTree *tree2 ) 
{
	Operation  	*newOp;

	if( !tree1 || !tree2 ) return NULL;

	if( !( newOp = new Operation() ) ) return NULL;
	newOp->setOperation( op, tree1, tree2 );
	return newOp;
}


Operation *Operation::
makeOperation( OpKind op, ExprTree *tree1, ExprTree *tree2, ExprTree *tree3 ) 
{
	Operation  	*newOp;

	if( !tree1 || !tree2 || !tree3) return NULL;

	if( !( newOp = new Operation() ) ) return NULL;
	newOp->setOperation( op, tree1, tree2, tree3 );
	return newOp;
}


bool Operation::
flattenSpecials( EvalState &state, EvalValue &val, ExprTree *&tree )
{
	ExprTree 	*fChild1, *fChild2, *fChild3;
	EvalValue	eval1, eval2, eval3, dummy;

	switch( operation ) {
		case UNARY_PLUS_OP:
		case UNARY_MINUS_OP:
		case PARENTHESES_OP:
		case LOGICAL_NOT_OP:
		case BITWISE_NOT_OP:
			if( !child1->flatten( state, eval1, fChild1 ) ) {
				tree = NULL;
				return false;
			} 
			if( fChild1 ) {
				tree = Operation::makeOperation( operation, fChild1 );
				return( tree != NULL );
			} else {
				_doOperation( operation, eval1, dummy, dummy, true, false, 
					false, val );
				tree = NULL;
				return true;
			}
			break;


		case TERNARY_OP:
			// flatten the selector expression
			if( !flatten( state, eval1, fChild1 ) ) {
				tree = NULL;
				return false;
			}

			// check if the selector expression collapsed into a value
			if( !fChild1 ) {
				int   		i; 
				double 		r;
				ValueType	vt = eval1.getType();

				// if the selector is error, classad or list, propagate error
				if( vt==ERROR_VALUE || vt==CLASSAD_VALUE || vt==LIST_VALUE ) {
					val.setErrorValue();	
					tree = NULL;
					return true;
				}

				// if the selector is UNDEFINED, the result is undefined
				if( vt == UNDEFINED_VALUE ) {
					val.setUndefinedValue();
					tree = NULL;
					return true;
				}

				// eval1 is either a real or an integer
				if(	(eval1.isIntegerValue(i) && (i!=0)) || 
					(eval1.isRealValue(r)	&& (r!=0)) )
					return child1->flatten( state, val, tree );	
				else
					return child2->flatten( state, val, tree );	
			} else {
				// flatten arms of the if expression
				if( !child2->flatten( state, eval2, fChild2 ) ||
					!child3->flatten( state, eval3, fChild3 ) ) {
					// clean up
					if( fChild1 ) delete fChild1;
					if( fChild2 ) delete fChild2;
					if( fChild3 ) delete fChild3;
			
					tree = NULL;
					return false;
				}

				// if any arm collapsed into a value, make it a Literal
				if( !fChild2 ) fChild2 = Literal::makeLiteral( eval2 );
				if( !fChild3 ) fChild3 = Literal::makeLiteral( eval3 );
				if( !fChild2 || !fChild3 ) {
					// clean up
					if( fChild1 ) delete fChild1;
					if( fChild2 ) delete fChild2;
					if( fChild3 ) delete fChild3;
			
					tree = NULL;
					return false;
				}
	
				tree = Operation::makeOperation( operation, fChild1, fChild2, 
						fChild3 );
				if( !tree ) {
					// clean up
					if( fChild1 ) delete fChild1;
					if( fChild2 ) delete fChild2;
					if( fChild3 ) delete fChild3;
			
					tree = NULL;
					return false;
				}
				return true;
			}	
			// will not get here
			return false;

		case SUBSCRIPT_OP:
			// flatten both arguments
			if( !child1->flatten( state, eval1, fChild1 ) ||
				!child2->flatten( state, eval2, fChild2 ) ) {
				if( fChild1 ) delete fChild1;
				if( fChild2 ) delete fChild2;
				tree = NULL;
				return false;
			}

			// if both arguments flattened to values, evaluate now
			if( !fChild1 && !fChild2 ) {
				_doOperation( operation, eval1, eval2, dummy, true, true, false,
						val );
				tree = NULL;
				return true;
			} 

			// otherwise convert flattened values into literals
			if( !fChild1 ) fChild1 = Literal::makeLiteral( eval1 );
			if( !fChild2 ) fChild2 = Literal::makeLiteral( eval2 );
			if( !fChild1 || !fChild2 ) {
				if( fChild1 ) delete fChild1;
				if( fChild2 ) delete fChild2;
				tree = NULL;
				return false;
			}
				
			tree = Operation::makeOperation( operation, fChild1, fChild2 );
			if( !tree ) {
				if( fChild1 ) delete fChild1;
				if( fChild2 ) delete fChild2;
				tree = NULL;
				return false;
			}
			return true;

		default:
			EXCEPT( "Should not get here" );
	}

	return true;
}
