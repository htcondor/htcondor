#include "condor_common.h"
#include "operators.h"

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
	">>",
	">>>",

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
	if( child1 ) delete child1;
	if( child2 ) delete child2;
	if( child3 ) delete child3;
}


ExprTree *Operation::
_copy( CopyMode cm )
{
	if( cm == EXPR_DEEP_COPY ) {
		Operation *newTree = new Operation ();
		if (newTree == 0) return NULL;

		if( child1 && (newTree->child1=child1->copy( EXPR_DEEP_COPY ))==NULL ){
			delete newTree;
			return NULL;
		}

		if( child2 && (newTree->child2=child2->copy( EXPR_DEEP_COPY ))==NULL ){
			delete newTree;
			return NULL;
		}

		if( child3 && (newTree->child3=child3->copy( EXPR_DEEP_COPY ))==NULL ){
			delete newTree;
			return NULL;
		}

		newTree->operation = operation;
		newTree->nodeKind = nodeKind;
		return newTree;
	} else if( cm == EXPR_REF_COUNT ) {
		if( child1 ) child1->copy( EXPR_REF_COUNT );
		if( child2 ) child2->copy( EXPR_REF_COUNT );
		if( child3 ) child3->copy( EXPR_REF_COUNT );
		return this;
	}

	// will not reach here
	return 0;
}

void Operation::
_setParentScope( ClassAd* parent ) 
{
	if( child1 ) child1->setParentScope( parent );
	if( child2 ) child2->setParentScope( parent );
	if( child3 ) child3->setParentScope( parent );
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
    	case LEFT_SHIFT_OP:		flag = s.sendToSink ((void*)" << ", 4); break;
    	case RIGHT_SHIFT_OP:	flag = s.sendToSink ((void*)" >> ", 5); break;
    	case URIGHT_SHIFT_OP:	flag = s.sendToSink ((void*)" >>> ",4); break;

		default:
			return false;
	}
	
	// ensure operator was written ok; dump right hand operand of expression
	return (flag && child2 && child2->toSink(s));
}


void Operation::
operate (OpKind op, Value &op1, Value &op2, Value &result)
{
	Value dummy;
	_doOperation(op, op1, op2, dummy, true, true, false, result);
}


void Operation::
operate (OpKind op, Value &op1, Value &op2, Value &op3, Value &result)
{
	_doOperation(op, op1, op2, op3, true, true, true, result);
}


int Operation::
_doOperation (OpKind op, Value &val1, Value &val2, Value &val3, 
			bool valid1, bool valid2, bool valid3, Value &result, EvalState*es)
{
	ValueType	vt1,  vt2,  vt3;

	// get the types of the values
	vt1 = val1.getType ();
	vt2 = val2.getType ();
	vt3 = val3.getType ();

	// take care of the easy cases
	if (op == __NO_OP__ || op == PARENTHESES_OP) {
		result.copyFrom( val1 );
		return SIG_LEFT;
	} else if (op == UNARY_PLUS_OP) {
		if (vt1 == BOOLEAN_VALUE || vt1 == STRING_VALUE || 
			vt1 == LIST_VALUE || vt1 == CLASSAD_VALUE) {
			result.setErrorValue();
		} else {
			// applies for ERROR, UNDEFINED and numbers
			result.copyFrom( val1 );
		}
		return SIG_LEFT;
	}

	// test for cases when evaluation is strict
	if( isStrictOperator( op ) ) {
		// check for error values
		if( vt1==ERROR_VALUE ) {
			result.setErrorValue ();
			return SIG_LEFT;
		}
		if( valid2 && vt2==ERROR_VALUE ) {
			result.setErrorValue ();
			return( !valid3 ? SIG_RIGHT : SIG_MIDDLE );
		}
		if( valid3 && vt3==ERROR_VALUE ) {
			result.setErrorValue ();
			return SIG_RIGHT;
		}

		// check for undefined values.  we need to check if the corresponding
		// tree exists, because these values would be undefined" anyway then.
		if( valid1 && vt1==UNDEFINED_VALUE ) {
			result.setUndefinedValue();
			return SIG_LEFT;
		}
		if( valid2 && vt2==UNDEFINED_VALUE ) {
			result.setUndefinedValue();
			return( !valid3 ? SIG_RIGHT :  SIG_MIDDLE );
		}
		if( valid3 && vt3==UNDEFINED_VALUE ) {
			result.setUndefinedValue();
			return SIG_RIGHT;
		}
	}

	// comparison operations (binary, one unary)
	if (op >= __COMPARISON_START__ && op <= __COMPARISON_END__) {
		return( doComparison( op, val1, val2, result ) );
	}

	// arithmetic operations (binary)
	if (op >= __ARITHMETIC_START__ && op <= __ARITHMETIC_END__) {
		return( doArithmetic (op, val1, val2, result) );
	}

	// logical operators (binary, one unary)
	if (op >= __LOGIC_START__ && op <= __LOGIC_END__) {
		return( doLogical (op, val1, val2, result) );
	}

	// bitwise operators (binary, one unary)
	if (op >= __BITWISE_START__ && op <= __BITWISE_END__) {
		return( doBitwise (op, val1, val2, result) );
	}
	
	// misc.
	if (op == TERNARY_OP) {
		// ternary (if-operator)
		bool b;

		// if the selector is UNDEFINED, the result is undefined
		if (vt1==UNDEFINED_VALUE) {
			result.setUndefinedValue();
			return SIG_LEFT;
		}

		// otherwise, if the selector is not boolean propagate error
		if( vt1 != BOOLEAN_VALUE ) {
			result.setErrorValue();	
			return SIG_LEFT;
		}

		if( val1.isBooleanValue(b) && b ) {
			result.copyFrom(val2 );
			return( SIG_LEFT | SIG_MIDDLE );
		} else {
			result.copyFrom(val3 );
			return( SIG_LEFT | SIG_RIGHT );
		}
	} else if( op == SUBSCRIPT_OP ) {
		int			index;
		ExprTree	*tree;
		ExprList	*elist;

		// subscripting from a list (strict)
		if( vt1 != LIST_VALUE || vt2 != INTEGER_VALUE) {
			result.setErrorValue();
			return( SIG_LEFT | SIG_RIGHT );
		}

		val1.isListValue( elist );		
		val2.isIntegerValue( index );

		// check bounds
		if( elist->number() <= index || index < 0 ) {
			result.setUndefinedValue();
			return SIG_RIGHT;
		}

		// access the index'th element (0-based)
		elist->rewind();
		index++;
		while( index && ( tree = elist->next() ) ) {
			index --;
		}

		if( es ) {
			// if an EvalState has been provided, use it
			if( !tree->evaluate( *es, result ) ) {
				result.setErrorValue( );
				return( SIG_NONE );
			}
		} else {
			// this needs to be changed
			result.setUndefinedValue( );
		}

		return( SIG_LEFT | SIG_RIGHT );
	}	
	
	// should not reach here
	EXCEPT ("Should not get here");
	return SIG_NONE;
}


bool Operation::
_evaluate (EvalState &state, Value &result)
{
	Value	val1, val2, val3;
	bool	valid1, valid2, valid3;
	int		rval;

	valid1 = false;
	valid2 = false;
	valid3 = false;

	// evaluate all valid children
	if (child1) { 
		if( !child1->evaluate (state, val1) ) {
			result.setErrorValue( );
			return( false );
		}
		valid1 = true;
	}

	if (child2) {
		if( !child2->evaluate (state, val2) ) {
			result.setErrorValue( );
			return( false );
		}
		valid2 = true;
	}
	if (child3) {
		if( !child3->evaluate (state, val3) ) {
			result.setErrorValue( );
			return( false );
		}
		valid3 = true;
	}

	rval = _doOperation (operation, val1, val2, val3, valid1, valid2, valid3,
				result, &state );

	return( rval != SIG_NONE );
}


bool Operation::
_evaluate( EvalState &state, Value &result, ExprTree *& tree )
{
	int			sig;
	Value		val1, val2, val3;
	ExprTree 	*tl=NULL, *tm=NULL, *tr=NULL;
	bool		valid1=false, valid2=false, valid3=false;

	// evaluate all valid children
	tree = NULL;
	if (child1) { 
		if( !child1->evaluate (state, val1) ) {
			result.setErrorValue( );
			return( false );
		}
		valid1 = true;
	}

	if (child2) {
		if( !child2->evaluate (state, val2) ) {
			result.setErrorValue( );
			return( false );
		}
		valid2 = true;
	}
	if (child3) {
		if( !child3->evaluate (state, val3) ) {
			result.setErrorValue( );
			return( false );
		}
		valid3 = true;
	}

	// do evaluation
	sig = _doOperation( operation,val1,val2,val3,valid1,valid2,valid3,result,
			&state );

	// delete trees which were not significant
	if( valid1 && !( sig & SIG_LEFT ) ) { delete tl; tl = NULL; }
	if( valid2 && !( sig & SIG_RIGHT ))	{ delete tr; tr = NULL; }
	if( valid3 && !( sig & SIG_MIDDLE )){ delete tm; tm = NULL; }

	if( sig == SIG_NONE ) {
		result.setErrorValue( );
		tree = NULL;
		return( false );
	}

	// in case of strict operators, if a subexpression is significant and the
	// corresponding value is UNDEFINED or ERROR, propagate only that tree
	if( isStrictOperator( operation ) ) {
		// strict unary operators:  unary -, unary +, !, ~, ()
		if( operation == UNARY_MINUS_OP || operation == UNARY_PLUS_OP ||
		  	operation == LOGICAL_NOT_OP || operation == BITWISE_NOT_OP ||
			operation == PARENTHESES_OP ) {
			if( val1.isExceptional() ) {
				// the operator is only propagating the value;  only the
				// subexpression is significant
				tree = tl;
			} else {
				// the node operated on the value; the operator is also
				// significant
				tree = makeOperation( operation, tl );
			}
			return( true );	
		} else {
			// strict binary operators
			if( val1.isExceptional() || val2.isExceptional() ) {
				// exceptional values are only being propagated
				if( sig & SIG_LEFT ) {
					tree = tl;
					return( true );
				} else if( sig & SIG_RIGHT ) {
					tree = tr;
					return( true );
				} 
				EXCEPT( "Should not reach here" );
			} else {
				// the node is also significant
				tree = makeOperation( operation, tl, tr );
				return( true );
			}
		}
	} else {
		// non-strict operators
		if( operation == IS_OP || operation == ISNT_OP ) {
			// the operation is *always* significant for IS and ISNT
			tree = makeOperation( operation, tl, tr );
			return( true );
		}
		// other non-strict binary operators
		if( operation == LOGICAL_AND_OP || operation == LOGICAL_OR_OP ) {
			if( ( sig & SIG_LEFT ) && ( sig & SIG_RIGHT ) ) {
				tree = makeOperation( operation, tl, tr );
				return( true );
			} else if( sig & SIG_LEFT ) {
				tree = tl;
				return( true );
			} else if( sig & SIG_RIGHT ) {
				tree = tr;
				return( true );
			} else {
				EXCEPT( "Shouldn't reach here" );
			}
		}
		// non-strict ternary operator (conditional operator) s ? t : f
		// selector is *always* significant
		if( operation == TERNARY_OP ) {
			Value tmpVal;
			tmpVal.setUndefinedValue( );
			tree = Literal::makeLiteral( tmpVal );

			// "true" consequent taken
			if( sig & SIG_MIDDLE ) {
				tree = makeOperation( TERNARY_OP, tl, tm, tree );
				return( true );
			} else if( sig & SIG_RIGHT ) {
				tree = makeOperation( TERNARY_OP, tl, tree, tr );
				return( true );
			}
			// neither consequent; selector was exceptional; return ( s )
			tree = tl;
			return( true );
		}
	}

	EXCEPT( "Should not reach here" );
	return( false );
}


bool Operation::
_flatten( EvalState &state, Value &val, ExprTree *&tree, OpKind *opPtr )
{
	OpKind		childOp1=__NO_OP__, childOp2=__NO_OP__;
	ExprTree	*fChild1=NULL, *fChild2=NULL;
	Value		val1, val2, val3;
	OpKind		newOp = operation, op = operation;
	
	// if op is not associative, don't allow splitting 
	if( op == SUBTRACTION_OP || op == DIVISION_OP || op == MODULUS_OP ||
		op == LEFT_SHIFT_OP  || op == RIGHT_SHIFT_OP || op == URIGHT_SHIFT_OP) 
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
combine( OpKind &op, Value &val, ExprTree *&tree,
			OpKind op1, Value &val1, ExprTree *tree1,
			OpKind op2, Value &val2, ExprTree *tree2 )
{
	Operation 	*newOp;
	Value		dummy; 	// undefined

	// special don't care cases for logical operators with exactly one value
	if( (!tree1 || !tree2) && (tree1 || tree2) && 
		( op==LOGICAL_OR_OP || op==LOGICAL_AND_OP ) ) {
		_doOperation( op, !tree1?val1:dummy , !tree2?val2:dummy , dummy , 
						true, true, false, val );
		if( val.isBooleanValue() ) {
			tree = NULL;
			op = __NO_OP__;
			return true;
		}
	}
		
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


		
int Operation::
doComparison (OpKind op, Value &v1, Value &v2, Value &result)
{
	ValueType 	vt1, vt2, coerceResult;
	bool		exact = false;	

	// do numerical type promotions --- other types/values are unchanged
	coerceResult = coerceToNumber(v1, v2);
	vt1 = v1.getType();
	vt2 = v2.getType();

	// perform comparison for =?= ; true iff same types and same values
	if (op == META_EQUAL_OP) {
		if (vt1 != vt2) {
			result.setBooleanValue( false );
			return( SIG_LEFT | SIG_RIGHT );
		}
		
		// undefined or error
		if (vt1 == UNDEFINED_VALUE || vt1 == ERROR_VALUE) {
			result.setBooleanValue( true );
			return( SIG_LEFT | SIG_RIGHT );
		}

		// if not the above cases, =?= is just like == ; but remember =?=
		exact = true;
		op = EQUAL_OP;
	}
	// perform comparison for =!= ; negation of =?=
	if (op == META_NOT_EQUAL_OP) {
		if (vt1 != vt2) {
			result.setBooleanValue( true );
			return( SIG_LEFT | SIG_RIGHT );
		}

		// undefined or error
		if (vt1 == UNDEFINED_VALUE || vt1 == ERROR_VALUE ||
			vt2 == UNDEFINED_VALUE || vt2 == ERROR_VALUE) {
			result.setBooleanValue( false );
			return( SIG_LEFT | SIG_RIGHT );
		}

		// if not the above cases, =!= is just like !=; but remember =?=
		exact = true;
		op = NOT_EQUAL_OP;
	}

	switch (coerceResult) {
		// at least one of v1, v2 is a string
		case STRING_VALUE:
			// check if both are strings
			if (vt1 != STRING_VALUE || vt2 != STRING_VALUE) {
				// comparison between strings and non-exceptional non-string 
				// values is error
				result.setErrorValue();
				return( SIG_LEFT | SIG_RIGHT );
			}
			compareStrings (op, v1, v2, result, exact);
			return( SIG_LEFT | SIG_RIGHT );

		case INTEGER_VALUE:
			compareIntegers (op, v1, v2, result);
			return( SIG_LEFT | SIG_RIGHT );

		case REAL_VALUE:
			compareReals (op, v1, v2, result);
            return( SIG_LEFT | SIG_RIGHT );
	
		case BOOLEAN_VALUE:
			{
				bool b1, b2;

				// check if both are bools
				if( !v1.isBooleanValue( b1 ) || !v2.isBooleanValue( b2 ) ) {
					result.setErrorValue();
					return( SIG_LEFT | SIG_RIGHT );
				}
				result.setBooleanValue( b1 == b2 );
				return( SIG_LEFT | SIG_RIGHT );
			}

		case LIST_VALUE:
		case CLASSAD_VALUE:
			result.setErrorValue();
			return( SIG_LEFT | SIG_RIGHT );

		default:
			// should not get here
			EXCEPT ("Should not get here");
			return( SIG_LEFT | SIG_RIGHT );
	}
}


int Operation::
doArithmetic (OpKind op, Value &v1, Value &v2, Value &result)
{
	int		i1, i2;
	double 	r1;

	// ensure the operands have arithmetic types
	if( ( !v1.isIntegerValue() && !v1.isRealValue() ) || ( op!=UNARY_MINUS_OP 
			&& !v2.isIntegerValue() && !v2.isRealValue() ) ) {
		result.setErrorValue ();
		return( SIG_LEFT | SIG_RIGHT );
	}

	// take care of the unary arithmetic operators
	if (op == UNARY_MINUS_OP) {
		if (v1.isIntegerValue (i1)) {
			result.setIntegerValue (-i1);
			return SIG_LEFT;
		} else	if (v1.isRealValue (r1)) {
			result.setRealValue (-r1);
			return SIG_LEFT;
		}

		// v1 is either UNDEFINED or ERROR ... the result is the same as v1
		result.copyFrom (v1);
		return SIG_LEFT;
	}

	// perform type promotions and proceed with arithmetic
	switch (coerceToNumber (v1, v2)) {
		case INTEGER_VALUE:
			v1.isIntegerValue (i1);
			v2.isIntegerValue (i2);
			switch (op) {
				case ADDITION_OP:		
					result.setIntegerValue(i1+i2);	
					return( SIG_LEFT | SIG_RIGHT );

				case SUBTRACTION_OP:	
					result.setIntegerValue(i1-i2);	
					return( SIG_LEFT | SIG_RIGHT );

				case MULTIPLICATION_OP:	
					result.setIntegerValue(i1*i2);	
					return( SIG_LEFT | SIG_RIGHT );

				case DIVISION_OP:		
					if (i2 != 0) {
						result.setIntegerValue(i1/i2);
					} else {
						result.setErrorValue ();
					}
					return( SIG_LEFT | SIG_RIGHT );
					
				case MODULUS_OP:
					if (i2 != 0) {
						result.setIntegerValue(i1%i2);
					} else {
						result.setErrorValue ();
					}
					return( SIG_LEFT | SIG_RIGHT );
							
				default:
					// should not reach here
					EXCEPT ("Should not get here");
					return( SIG_LEFT | SIG_RIGHT );
			}

		case REAL_VALUE:
			return( doRealArithmetic (op, v1, v2, result) );

		default:
			// should not get here
			EXCEPT ("Should not get here");
	}

	return( SIG_NONE );
}


int Operation::
doLogical (OpKind op, Value &v1, Value &v2, Value &result)
{
	ValueType	vt1 = v1.getType();
	ValueType	vt2 = v2.getType();
	bool		b1, b2;

	if( vt1!=UNDEFINED_VALUE && vt1!=ERROR_VALUE && vt1!=BOOLEAN_VALUE ) {
		result.setErrorValue();
		return SIG_LEFT;
	}
	if( vt2!=UNDEFINED_VALUE && vt2!=ERROR_VALUE && vt2!=BOOLEAN_VALUE ) { 
		result.setErrorValue();
		return SIG_RIGHT;
	}
	
	v1.isBooleanValue( b1 );
	v2.isBooleanValue( b2 );

	// handle unary operator
	if (op == LOGICAL_NOT_OP) {
		if( vt1 == BOOLEAN_VALUE ) {
			result.setBooleanValue( !b1 );
		} else {
			result.copyFrom( v1 );
		}
		return SIG_LEFT;
	}

	if (op == LOGICAL_OR_OP) {
		if( vt1 == BOOLEAN_VALUE && b1 ) {
			result.setBooleanValue( true );
			return SIG_LEFT;
		} else if( vt1 == ERROR_VALUE ) {
			result.setErrorValue( );
			return SIG_LEFT;
		} else if( vt1 == BOOLEAN_VALUE && !b1 ) {
			result.copyFrom( v2 );
		} else if( vt2 != BOOLEAN_VALUE ) {
			result.copyFrom( v2 );
		} else if( b2 ) {
			result.setBooleanValue( true );
		} else {
			result.setUndefinedValue( );
		}
		return( SIG_LEFT | SIG_RIGHT );
	} else if (op == LOGICAL_AND_OP) {
        if( vt1 == BOOLEAN_VALUE && !b1 ) {
            result.setBooleanValue( false );
			return SIG_LEFT;
		} else if( vt1 == ERROR_VALUE ) {
			result.setErrorValue( );
			return SIG_LEFT;
		} else if( vt1 == BOOLEAN_VALUE && b1 ) {
			result.copyFrom( v2 );
		} else if( vt2 != BOOLEAN_VALUE ) {
			result.copyFrom( v2 );
		} else if( !b2 ) {
			result.setBooleanValue( false );
		} else {
			result.setUndefinedValue( );
		}
		return( SIG_LEFT | SIG_RIGHT );
	}

	EXCEPT( "Shouldn't reach here" );
	return( SIG_NONE );
}



int Operation::
doBitwise (OpKind op, Value &v1, Value &v2, Value &result)
{
	int	i1, i2;
	int signMask = 1 << (WORD_BIT-1);	// now at the position of the sign bit
	int val;

	// bitwise operations are defined only on integers
	if (op == BITWISE_NOT_OP) {
		if (!v1.isIntegerValue(i1)) {
			result.setErrorValue();
			return SIG_LEFT;
		}
	} else if (!v1.isIntegerValue(i1) || !v2.isIntegerValue(i2)) {
		result.setErrorValue();
		return( SIG_LEFT | SIG_RIGHT );
	}

	switch (op) {
		case BITWISE_NOT_OP:	result.setIntegerValue(~i1);	break;
		case BITWISE_OR_OP:		result.setIntegerValue(i1|i2);	break;
		case BITWISE_AND_OP:	result.setIntegerValue(i1&i2);	break;
		case BITWISE_XOR_OP:	result.setIntegerValue(i1^i2);	break;
		case LEFT_SHIFT_OP:		result.setIntegerValue(i1<<i2);	break;

		case URIGHT_SHIFT_OP:
			if (i1 >= 0) {
				// sign bit is not on;  >> will work fine
				result.setIntegerValue (i1 >> i2);
				break;
			} else {
				// sign bit is on
				val = i1 >> 1;	    // shift right 1; the sign bit *may* be on
				val &= (~signMask);	// clear the sign bit for sure
				val >>= (i2 - 1);	// shift remaining number of positions
				result.setIntegerValue (val);
				break;
			}
			// will not reach here
			break;

		case RIGHT_SHIFT_OP:
			if (i1 >= 0) {
				// sign bit is off;  >> will work fine
				result.setIntegerValue (i1 >> i2);
				break;
			} else {
				// sign bit is on; >> *may* not set the sign
				val = i1;
				for (int i = 0; i < i2; i++)
					val = (val >> 1) | signMask;	// make sure that it does
				result.setIntegerValue (val);
				break;
			}
			// will not reach here
			break;

		default:
			// should not get here
			EXCEPT ("Should not get here");
	}

	if( op == BITWISE_NOT_OP ) {
		return SIG_LEFT;
	}

	return( SIG_LEFT | SIG_RIGHT );
}


static volatile bool ClassAdExprFPE = false;
#ifndef WIN32
void ClassAd_SIGFPE_handler (int) { ClassAdExprFPE = true; }
#endif

int Operation::
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
    if (sigaction (SIGFPE, &sa1, &sa2)) {
       EXCEPT("Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
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
			return( SIG_NONE );
	}

	// check if anything bad happened
	if (ClassAdExprFPE==true || errno==EDOM || errno==ERANGE || comp==HUGE_VAL)
		result.setErrorValue ();
	else
		result.setRealValue (comp);

	// restore the state
#ifndef WIN32 
    if (sigaction (SIGFPE, &sa2, &sa1)) {
        EXCEPT( "Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
			errno);
    }
#endif
	return( SIG_LEFT | SIG_RIGHT );
}


void Operation::
compareStrings (OpKind op, Value &v1, Value &v2, Value &result, bool exact)
{
	char *s1, *s2;
	int  cmp;
	
	v1.isStringValue (s1);
	v2.isStringValue (s2);

	result.setBooleanValue( false );
	if( exact ) {
		cmp = strcmp( s1, s2 );
	} else {
		cmp = strcasecmp( s1, s2 );
	}
	if (cmp < 0) {
		// s1 < s2
		if (op == LESS_THAN_OP 		|| 
			op == LESS_OR_EQUAL_OP 	|| 
			op == NOT_EQUAL_OP) {
			result.setBooleanValue( true );
		}
	} else if (cmp == 0) {
		// s1 == s2
		if (op == LESS_OR_EQUAL_OP 	|| 
			op == EQUAL_OP			||
			op == GREATER_OR_EQUAL_OP) {
			result.setBooleanValue( true );
		}
	} else {
		// s1 > s2
		if (op == GREATER_THAN_OP	||
			op == GREATER_OR_EQUAL_OP	||
			op == NOT_EQUAL_OP) {
			result.setBooleanValue( true );
		}
	}
}


void Operation::
compareIntegers (OpKind op, Value &v1, Value &v2, Value &result)
{
	int 	i1, i2; 
	bool	compResult;

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

	result.setBooleanValue( compResult );
}


void Operation::
compareReals (OpKind op, Value &v1, Value &v2, Value &result)
{
	double 	r1, r2;
	bool	compResult;

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

	result.setBooleanValue( compResult );
}


// This function performs type promotions so that both v1 and v2 are of the
// same numerical type: (v1 and v2 are not ERROR or UNDEFINED)
//  + if both v1 and v2 are numbers and of the same type, return type
//  + if v1 is an int and v2 is a real, convert v1 to real; return REAL_VALUE
//  + if v1 is a real and v2 is an int, convert v2 to real; return REAL_VALUE
ValueType Operation::
coerceToNumber (Value &v1, Value &v2)
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
	if (v1.isBooleanValue()	  || v2.isBooleanValue())	return BOOLEAN_VALUE;

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


Operation *Operation::
makeOperation( OpKind op, Value &val, ExprTree *tree ) 
{
	Operation  	*newOp;
	Literal		*lit;
	
	if( !tree ) return NULL;

	if( !( newOp = new Operation() ) ) return NULL;
	if( !( lit = Literal::makeLiteral( val ) ) ) {
		delete newOp;
		return NULL;
	}

	newOp->setOperation( op, lit, tree );
	return newOp;
}
	
	
Operation *Operation::
makeOperation( OpKind op, ExprTree *tree, Value &val ) 
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
flattenSpecials( EvalState &state, Value &val, ExprTree *&tree )
{
	ExprTree 	*fChild1, *fChild2, *fChild3;
	Value		eval1, eval2, eval3, dummy;

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
				eval1.clear();
				return true;
			}
			break;


		case TERNARY_OP:
			// flatten the selector expression
			if( !flatten( state, eval1, fChild1 ) ) {
				tree = NULL;
				return false;
			}

			// check if selector expression collapsed to a non-undefined value
			if( !fChild1 && !eval1.isUndefinedValue() ) {
				bool   		b; 
				ValueType	vt = eval1.getType();

				// if the selector is not boolean, propagate error
				if( vt!=BOOLEAN_VALUE ) {
					val.setErrorValue();	
					eval1.clear();
					tree = NULL;
					return true;
				}

				// eval1 is either a real or an integer
				if(	eval1.isBooleanValue(b) && b!=0 ) {
					return child1->flatten( state, val, tree );	
				} else {
					return child2->flatten( state, val, tree );	
				}
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
	
				// fChild1 may be NULL if child1 flattened to UNDEFINED
				if( !fChild1 ) {
					fChild1 = child1->copy();
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

bool Operation::
isStrictOperator( OpKind op ) 
{
	switch( op ) {
		case META_EQUAL_OP:
		case META_NOT_EQUAL_OP:
		case LOGICAL_AND_OP:
		case LOGICAL_OR_OP:
		case TERNARY_OP:
			return false;

		default:
			return true;
	}
	return true;
}
