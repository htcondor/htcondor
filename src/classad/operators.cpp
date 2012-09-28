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

#include "classad/common.h"
#include "classad/operators.h"
#include "classad/sink.h"
#include "classad/util.h"

using namespace std;

namespace classad {

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
Operation(const Operation &op)
{
    CopyFrom(op);
    return;
}

Operation::
~Operation ()
{
	if( child1 ) delete child1;
	if( child2 ) delete child2;
	if( child3 ) delete child3;
}

Operation &Operation::
operator=(const Operation &op)
{
    if (this != &op) {
        CopyFrom(op);
    }
    return *this;
}

ExprTree *Operation::
Copy( ) const
{
	Operation *newTree = new Operation ();
	if (newTree != NULL) {
        if (!newTree->CopyFrom(*this)) {
            delete newTree;
            newTree = NULL;
        }
    }

	return newTree;
}

bool Operation::
CopyFrom(const Operation &op)
{
    bool success;

    success = true;

	if( op.child1 && (child1 = op.child1->Copy()) == NULL){
        success = false;
    } else if( op.child2 && (child2 = op.child2->Copy()) == NULL ){
        success = false;
    } else if( op.child3 && (child3 = op.child3->Copy()) == NULL ){
        success = false;
	} else {
        operation   = op.operation;
        ExprTree::CopyFrom(op);
    }

    if (success == false) {
        CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
    }

    return success;
}

bool Operation::
SameAs(const ExprTree *tree) const
{
    bool is_same;
    const Operation *other_op;

    if (tree->GetKind() != OP_NODE) {
        is_same = false;
    } else {
        other_op = (const Operation *) tree;
        
        if (   operation == other_op->operation
            && SameChild(child1, other_op->child1)
            && SameChild(child2, other_op->child2)
            && SameChild(child3, other_op->child3)) {
            is_same = true;
        } else {
            is_same = false;
        }
    }

    return is_same;
}

bool operator==(const Operation &op1, const Operation &op2)
{
    return op1.SameAs(&op2);
}

bool Operation::
SameChild(const ExprTree *tree1, const ExprTree *tree2) const
{
    bool is_same; 

    if (tree1 == NULL) {
        if (tree2 == NULL) is_same = true;
        else               is_same = false;
    } else if (tree2 == NULL) {
        is_same = false;
    } else {
        is_same = tree1->SameAs(tree2);
    }
    return is_same;
}

void Operation::
_SetParentScope( const ClassAd* parent ) 
{
	if( child1 ) child1->SetParentScope( parent );
	if( child2 ) child2->SetParentScope( parent );
	if( child3 ) child3->SetParentScope( parent );
}


void Operation::
Operate (OpKind op, Value &op1, Value &op2, Value &result)
{
	Value dummy;
	_doOperation(op, op1, op2, dummy, true, true, false, result);
}


void Operation::
Operate (OpKind op, Value &op1, Value &op2, Value &op3, Value &result)
{
	_doOperation(op, op1, op2, op3, true, true, true, result);
}


int Operation::
_doOperation (OpKind op, Value &val1, Value &val2, Value &val3, 
			bool valid1, bool valid2, bool valid3, Value &result, EvalState*es)
{
	Value::ValueType	vt1,  vt2,  vt3;

	// get the types of the values
	vt1 = val1.GetType ();
	vt2 = val2.GetType ();
	vt3 = val3.GetType ();

	// take care of the easy cases
	if (op == __NO_OP__ || op == PARENTHESES_OP) {
		result.CopyFrom( val1 );
		return SIG_CHLD1;
	} else if (op == UNARY_PLUS_OP) {
		if (vt1 == Value::BOOLEAN_VALUE || vt1 == Value::STRING_VALUE || 
			val1.IsListValue() || vt1 == Value::CLASSAD_VALUE || 
			vt1 == Value::ABSOLUTE_TIME_VALUE) {
			result.SetErrorValue();
		} else {
			// applies for ERROR, UNDEFINED and Numbers
			result.CopyFrom( val1 );
		}
		return SIG_CHLD1;
	}

	// test for cases when evaluation is strict
	if( IsStrictOperator( op ) ) {
		// check for error values
		if( vt1==Value::ERROR_VALUE ) {
			result.SetErrorValue ();
			return SIG_CHLD1;
		}
		if( valid2 && vt2==Value::ERROR_VALUE ) {
			result.SetErrorValue ();
			return SIG_CHLD2;
		}
		if( valid3 && vt3==Value::ERROR_VALUE ) {
			result.SetErrorValue ();
			return SIG_CHLD3;
		}

		// check for undefined values.  we need to check if the corresponding
		// tree exists, because these values would be undefined" anyway then.
		if( valid1 && vt1==Value::UNDEFINED_VALUE ) {
			result.SetUndefinedValue();
			return SIG_CHLD1;
		}
		if( valid2 && vt2==Value::UNDEFINED_VALUE ) {
			result.SetUndefinedValue();
			return SIG_CHLD2;
		}
		if( valid3 && vt3==Value::UNDEFINED_VALUE ) {
			result.SetUndefinedValue();
			return SIG_CHLD3;
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
		if (vt1==Value::UNDEFINED_VALUE) {
			result.SetUndefinedValue();
			return SIG_CHLD1;
		}

		if( !val1.IsBooleanValueEquiv(b) ) {
			result.SetErrorValue();	
			return SIG_CHLD1;
		} else if( b ) {
			result.CopyFrom( val2 );
			return( SIG_CHLD2 );
		} else {
			result.CopyFrom( val3 );
			return( SIG_CHLD3 );
		}
	} else if( op == SUBSCRIPT_OP ) {
		// subscripting from a list (strict)

		if (vt1 == Value::CLASSAD_VALUE && vt2 == Value::STRING_VALUE) {
			ClassAd  *classad = NULL;
			string   index;
			
			val1.IsClassAdValue(classad);
			val2.IsStringValue(index);
			
            if (!classad->Lookup(index)) {
				result.SetErrorValue();
				return SIG_CHLD2;
            }
			if (!classad->EvaluateAttr(index, result)) {
				result.SetErrorValue();
				return SIG_CHLD2;
			}

			return( SIG_CHLD1 | SIG_CHLD2 );
		} else if ( val1.IsListValue() && vt2 == Value::INTEGER_VALUE) {
				// TODO index should to changed to a long long
				//   (and ExprListIterator::ToNth() converted)
				//    or the value from val2 needs to be capped
				//    at INT_MAX (rather than truncated).
			int            index;
			const ExprList *elist = NULL;

			val1.IsListValue( elist );		
			val2.IsIntegerValue( index );
			
			// check bounds
			ExprListIterator itr( elist );
			if( index < 0 || !itr.ToNth( index ) ) {
				result.SetErrorValue();
				return SIG_CHLD2;
			}
			
			// get value
			if( !itr.CurrentValue( result, es ) ) {
				result.SetErrorValue( );
			}
			return( SIG_CHLD1 | SIG_CHLD2 );
		} else {
			result.SetErrorValue();
			return( SIG_CHLD1 | SIG_CHLD2 );
		}
	}	
	
	// should not reach here
	CLASSAD_EXCEPT ("Should not get here");
	return SIG_NONE;
}

bool Operation::
_Evaluate (EvalState &state, Value &result) const
{
	Value	val1, val2, val3;
	bool	valid1, valid2, valid3;
	int		rval;

	valid1 = false;
	valid2 = false;
	valid3 = false;

	// Evaluate all valid children
	if (child1) { 
		if( !child1->Evaluate (state, val1) ) {
			result.SetErrorValue( );
			return( false );
		}
		valid1 = true;

		if( shortCircuit( state, val1, result ) ) {
			return true;
		}
	}

	if (child2) {
		if( !child2->Evaluate (state, val2) ) {
			result.SetErrorValue( );
			return( false );
		}
		valid2 = true;
	}
	if (child3) {
		if( !child3->Evaluate (state, val3) ) {
			result.SetErrorValue( );
			return( false );
		}
		valid3 = true;
	}

	rval = _doOperation (operation, val1, val2, val3, valid1, valid2, valid3,
				result, &state );

	return( rval != SIG_NONE );
}

bool Operation::
shortCircuit( EvalState &state, Value const &arg1, Value &result ) const
{
	bool arg1_bool;

	switch( operation ) {
	case LOGICAL_OR_OP:
		if( arg1.IsBooleanValueEquiv(arg1_bool) && arg1_bool ) {
			result.SetBooleanValue( true );
			return true;
		}
		break;
	case LOGICAL_AND_OP:
		if( arg1.IsBooleanValueEquiv(arg1_bool) && !arg1_bool ) {
			result.SetBooleanValue( false );
			return true;
		}
		break;
	case TERNARY_OP:
		if( arg1.IsBooleanValueEquiv(arg1_bool) ) {
			if( arg1_bool ) {
				if( child2 ) {
					return child2->Evaluate(state,result);
				}
			}
			else {
				if( child3 ) {
					return child3->Evaluate(state,result);
				}
			}
		}
		break;
	default:
		// no-op
		break;
	}
	return false;
}

bool Operation::
_Evaluate( EvalState &state, Value &result, ExprTree *& tree ) const
{
	int			sig;
	Value		val1, val2, val3;
	ExprTree 	*t1=NULL, *t2=NULL, *t3=NULL;
	bool		valid1=false, valid2=false, valid3=false;

	// Evaluate all valid children
	tree = NULL;
	if (child1) { 
		if( !child1->Evaluate (state, val1, t1) ) {
			result.SetErrorValue( );
			return( false );
		}
		valid1 = true;
	}

	if (child2) {
		if( !child2->Evaluate (state, val2, t2) ) {
			result.SetErrorValue( );
			return( false );
		}
		valid2 = true;
	}
	if (child3) {
		if( !child3->Evaluate (state, val3, t3) ) {
			result.SetErrorValue( );
			return( false );
		}
		valid3 = true;
	}

	// do evaluation
	sig = _doOperation( operation,val1,val2,val3,valid1,valid2,valid3,result,
			&state );

	// delete trees which were not significant
	if( valid1 && !( sig & SIG_CHLD1 )) { delete t1; t1 = NULL; }
	if( valid2 && !( sig & SIG_CHLD2 ))	{ delete t2; t2 = NULL; }
	if( valid3 && !( sig & SIG_CHLD3 )) { delete t3; t3 = NULL; }

	if( sig == SIG_NONE ) {
		result.SetErrorValue( );
		tree = NULL;
		return( false );
	}

	// in case of strict operators, if a subexpression is significant and the
	// corresponding value is UNDEFINED or ERROR, propagate only that tree
	if( IsStrictOperator( operation ) ) {
		// strict unary operators:  unary -, unary +, !, ~, ()
		if( operation == UNARY_MINUS_OP || operation == UNARY_PLUS_OP ||
		  	operation == LOGICAL_NOT_OP || operation == BITWISE_NOT_OP ||
			operation == PARENTHESES_OP ) {
			if( val1.IsExceptional() ) {
				// the operator is only propagating the value;  only the
				// subexpression is significant
				tree = t1;
			} else {
				// the node operated on the value; the operator is also
				// significant
				tree = MakeOperation( operation, t1 );
			}
			return( true );	
		} else {
			// strict binary operators
			if( val1.IsExceptional() || val2.IsExceptional() ) {
				// exceptional values are only being propagated
				if( sig & SIG_CHLD1 ) {
					tree = t1;
					return( true );
				} else if( sig & SIG_CHLD2 ) {
					tree = t2;
					return( true );
				} 
				CLASSAD_EXCEPT( "Should not reach here" );
			} else {
				// the node is also significant
				tree = MakeOperation( operation, t1, t2 );
				return( true );
			}
		}
	} else {
		// non-strict operators
		if( operation == IS_OP || operation == ISNT_OP ) {
			// the operation is *always* significant for IS and ISNT
			tree = MakeOperation( operation, t1, t2 );
			return( true );
		}
		// other non-strict binary operators
		if( operation == LOGICAL_AND_OP || operation == LOGICAL_OR_OP ) {
			if( ( sig & SIG_CHLD1 ) && ( sig & SIG_CHLD2 ) ) {
				tree = MakeOperation( operation, t1, t2 );
				return( true );
			} else if( sig & SIG_CHLD1 ) {
				tree = t1;
				return( true );
			} else if( sig & SIG_CHLD2 ) {
				tree = t2;
				return( true );
			} else {
				CLASSAD_EXCEPT( "Shouldn't reach here" );
			}
		}
		// non-strict ternary operator (conditional operator) s ? t : f
		// selector is always significant (???)
		if( operation == TERNARY_OP ) {
			Value tmpVal;
			tmpVal.SetUndefinedValue( );
			tree = Literal::MakeLiteral( tmpVal );

			// "true" consequent taken
			if( sig & SIG_CHLD2 ) {
				tree = t2;
				delete t1;
				delete t3;
				return( true );
			} else if( sig & SIG_CHLD3 ) {
				tree = t3;
				delete t1;
				delete t2;
				return( true );
			}
			// neither consequent; selector was exceptional; return ( s )
			tree = t1;
			delete tree;
			return( true );
		}
	}

	CLASSAD_EXCEPT( "Should not reach here" );
	return( false );
}


bool Operation::
_Flatten( EvalState &state, Value &val, ExprTree *&tree, int *opPtr ) const
{
	int		childOp1=__NO_OP__, childOp2=__NO_OP__;
	ExprTree	*fChild1=NULL, *fChild2=NULL;
	Value		val1, val2, val3;
	OpKind		newOp = operation, op = operation;

	tree = NULL; // Just to be safe...  wenger 2003-12-11.
	
	// if op is binary, but not associative or commutative, disallow splitting 
	if( ( op >= __COMPARISON_START__ && op <= __COMPARISON_END__ ) ||
		op == SUBTRACTION_OP || op == DIVISION_OP || op == MODULUS_OP ||
		op == LEFT_SHIFT_OP  || op == RIGHT_SHIFT_OP || op == URIGHT_SHIFT_OP) 
	{
		if( opPtr ) *opPtr = __NO_OP__;
		if( child1->Flatten( state, val1, fChild1 ) &&
			child2->Flatten( state, val2, fChild2 ) ) {
			if( !fChild1 && !fChild2 ) {
				_doOperation(op, val1, val2, val3, true, true, false, val);	
				tree = NULL;
				return true;
			} else if( fChild1 && fChild2 ) {
				tree = Operation::MakeOperation( op, fChild1, fChild2 );
				return true;
			} else if( fChild1 ) {
				tree = Operation::MakeOperation( op, fChild1, val2 );
				return true;
			} else if( fChild2 ) {
				tree = Operation::MakeOperation( op, val1, fChild2 );
				return true;
			}
		} else {
			if( fChild1 ) delete fChild1;
			if( fChild2 ) delete fChild2;
			tree = NULL;
			return false;
		}
	} else 
		// now catch all non-binary operators
	if( op == TERNARY_OP || op == SUBSCRIPT_OP || op ==  UNARY_PLUS_OP ||
				op == UNARY_MINUS_OP || op == PARENTHESES_OP || 
				op == LOGICAL_NOT_OP || op == BITWISE_NOT_OP ) {
		return flattenSpecials( state, val, tree );
	}
					
	// any op that got past the above is binary, commutative and associative
	// Flatten sub expressions
	if( ( child1 && !child1->Flatten( state, val1, fChild1, &childOp1 ) ) ||
		( child2 && !child2->Flatten( state, val2, fChild2, &childOp2 ) ) ) {
		delete fChild1;
		delete fChild2;
		tree = NULL;
		return false;
	}

		// NOTE: combine() deletes fChild1 and/or fChild2 if they are not
		// included in tree
	if( !combine( newOp, val, tree, childOp1, val1, fChild1, 
						childOp2, val2, fChild2 ) ) {
		tree = NULL;
		if( opPtr ) *opPtr = __NO_OP__;
		return false;
	}

	// if splitting is disallowed, fold the value and tree into a tree
	if( !opPtr && newOp != __NO_OP__ ) {
		tree = Operation::MakeOperation( newOp, val, tree );
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
			int op1, Value &val1, ExprTree *tree1,
			int op2, Value &val2, ExprTree *tree2 ) const
{
	Operation 	*newOp;
	Value		dummy; 	// undefined

	// special don't care cases for logical operators with exactly one value
	if( (!tree1 || !tree2) && (tree1 || tree2) && 
		( op==LOGICAL_OR_OP || op==LOGICAL_AND_OP ) ) {
		_doOperation( op, !tree1?val1:dummy , !tree2?val2:dummy , dummy , 
						true, true, false, val );
		if( val.IsBooleanValue() ) {
			tree = NULL;
			delete tree1;
			delete tree2;
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
		val.CopyFrom( val1 );
		return true;
	} else if( !tree2 && (tree1 && op1 == __NO_OP__ ) ) {
		// rightson is a value, leftson is a tree
		tree = tree1;
		val.CopyFrom( val2 );
		return true;
	} else if( ( tree1 && op1 == __NO_OP__ ) && ( tree2 && op2 == __NO_OP__ ) ){
		// left and rightsons are trees only
		if( !( newOp = MakeOperation( op, tree1, tree2 )) ) {
			delete tree1;
			delete tree2;
			return false;
		}
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
			newOp1 = Operation::MakeOperation( (OpKind)op1, val1, tree1 );
		} else if( tree1 ) {
			newOp1 = tree1;
		} else {
			newOp1 = Literal::MakeLiteral( val1 );
		}
		
		if( op2 != __NO_OP__ ) {
			newOp2 = Operation::MakeOperation( (OpKind)op2, val2, tree2 );
		} else if( tree2 ) {
			newOp2 = tree2;
		} else {
			newOp2 = Literal::MakeLiteral( val2 );
		}

		if( !newOp1 || !newOp2 ) {
			if( newOp1 ) delete newOp1;
			if( newOp2 ) delete newOp2;
			tree = NULL; op = __NO_OP__;
			return false;
		}

		if( !( newOp = MakeOperation( op, newOp1, newOp2 ) ) ) {
			delete newOp1; delete newOp2;
			tree = NULL; op = __NO_OP__;
			return false;
		}
		op = __NO_OP__;
		tree = newOp;
		return true;
	}

	if( op == op1 && op == op2 ) {
		// same operators on both children . since op!=NO_OP, neither are op1, 
		// op2.  so they both make tree and value contributions
		if( !( newOp = MakeOperation( op, tree1, tree2 ) ) ) {
			delete tree1;
			delete tree2;
			return false;
		}
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
			Operation *local_newOp = MakeOperation( op, tree1, tree2 );
			if( !local_newOp ) {
				delete tree1;
				delete tree2;
				tree = NULL; op = __NO_OP__;	
				return false;
			}
			val.CopyFrom( val1 );
			tree = local_newOp;	// NAC - BUG FIX
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
			Operation *local_newOp = MakeOperation( op, tree1, tree2 );
			if( !local_newOp ) {
				delete tree1;
				delete tree2;
				tree = NULL; op = __NO_OP__;	
				return false;
			}
			tree = local_newOp;	// NAC BUG FIX
			val.CopyFrom( val2 );
			return true;
		}
	}

	CLASSAD_EXCEPT( "Should not reach here" );
	return false;
}


		
int Operation::
doComparison (OpKind op, Value &v1, Value &v2, Value &result)
{
	Value::ValueType 	vt1, vt2, coerceResult;
	bool		exact = false;	

	// do numerical type promotions --- other types/values are unchanged
	coerceResult = coerceToNumber(v1, v2);
	vt1 = v1.GetType();
	vt2 = v2.GetType();

	// perform comparison for =?= ; true iff same types and same values
	if (op == META_EQUAL_OP) {
		if (vt1 != vt2) {
			result.SetBooleanValue( false );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}
		
		// undefined or error
		if (vt1 == Value::UNDEFINED_VALUE || vt1 == Value::ERROR_VALUE) {
			result.SetBooleanValue( true );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		// if not the above cases, =?= is like == ; but remember =?=
		exact = true;
		op = EQUAL_OP;
	}
	// perform comparison for =!= ; negation of =?=
	if (op == META_NOT_EQUAL_OP) {
		if (vt1 != vt2) {
			result.SetBooleanValue( true );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		// undefined or error
		if (vt1 == Value::UNDEFINED_VALUE || vt1 == Value::ERROR_VALUE ||
			vt2 == Value::UNDEFINED_VALUE || vt2 == Value::ERROR_VALUE) {
			result.SetBooleanValue( false );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		// if not the above cases, =!= is just like !=; but remember =?=
		exact = true;
		op = NOT_EQUAL_OP;
	}

	switch (coerceResult) {
		// at least one of v1, v2 is a string
		case Value::STRING_VALUE:
			// check if both are strings
			if (vt1 != Value::STRING_VALUE || vt2 != Value::STRING_VALUE) {
				// comparison between strings and non-exceptional non-string 
				// values is error
				result.SetErrorValue();
				return( SIG_CHLD1 | SIG_CHLD2 );
			}
			compareStrings (op, v1, v2, result, exact);
			return( SIG_CHLD1 | SIG_CHLD2 );

		case Value::INTEGER_VALUE:
			compareIntegers (op, v1, v2, result);
			return( SIG_CHLD1 | SIG_CHLD2 );

		case Value::REAL_VALUE:
			compareReals (op, v1, v2, result);
			return( SIG_CHLD1 | SIG_CHLD2 );
	
		case Value::BOOLEAN_VALUE:
			// check if both are bools
			if( !v1.IsBooleanValue( ) || !v2.IsBooleanValue( ) ) {
				result.SetErrorValue();
				return( SIG_CHLD1 | SIG_CHLD2 );
			}
			compareBools( op, v1, v2, result );
			return( SIG_CHLD1 | SIG_CHLD2 );

		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
		case Value::CLASSAD_VALUE:
			result.SetErrorValue();
			return( SIG_CHLD1 | SIG_CHLD2 );

		case Value::ABSOLUTE_TIME_VALUE:
			if( !v1.IsAbsoluteTimeValue( ) || !v2.IsAbsoluteTimeValue( ) ) {
				result.SetErrorValue( );
				return( SIG_CHLD1 | SIG_CHLD2 );
			}
			compareAbsoluteTimes( op, v1, v2, result );
			return( SIG_CHLD1 | SIG_CHLD2 );

		case Value::RELATIVE_TIME_VALUE:
			if( !v1.IsRelativeTimeValue( ) || !v2.IsRelativeTimeValue( ) ) {
				result.SetErrorValue( );
				return( SIG_CHLD1 | SIG_CHLD2 );
			}
			compareRelativeTimes( op, v1, v2, result );
			return( SIG_CHLD1 | SIG_CHLD2 );

		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
			return( SIG_CHLD1 | SIG_CHLD2 );
	}
}


int Operation::
doArithmetic (OpKind op, Value &v1, Value &v2, Value &result)
{
	long long	i1, i2;
	double	t1;
	double 	r1;
    bool    b1;

	// ensure the operands have arithmetic types
	if( ( !v1.IsIntegerValue() && !v1.IsRealValue() && !v1.IsAbsoluteTimeValue()
			&& !v1.IsRelativeTimeValue() && !v1.IsBooleanValue()) 
        || ( op!=UNARY_MINUS_OP 
            && !v2.IsBooleanValue() && !v2.IsIntegerValue() && !v2.IsRealValue()
			&& !v2.IsAbsoluteTimeValue() && !v2.IsRelativeTimeValue() ) ) {
		result.SetErrorValue ();
		return( SIG_CHLD1 | SIG_CHLD2 );
	}

	// take care of the unary arithmetic operators
	if (op == UNARY_MINUS_OP) {
		if (v1.IsIntegerValue (i1)) {
			result.SetIntegerValue (-i1);
			return SIG_CHLD1;
		} else	if (v1.IsRealValue (r1)) {
			result.SetRealValue (-r1);
			return SIG_CHLD1;
		} else if( v1.IsRelativeTimeValue( t1 ) ) {
			result.SetRelativeTimeValue( -t1 );
			return( SIG_CHLD1 );
        } else if (v1.IsBooleanValue( b1 ) ) {
            result.SetBooleanValue( !b1 );
		} else if( v1.IsExceptional() ) {
			// undefined or error --- same as operand
			result.CopyFrom( v1 );
			return SIG_CHLD1;
		}
		// unary minus not defined on any other operand type
		result.SetErrorValue( );
		return( SIG_CHLD1 );
	}

	// perform type promotions and proceed with arithmetic
	switch (coerceToNumber (v1, v2)) {
		case Value::INTEGER_VALUE:
			v1.IsIntegerValue (i1);
			v2.IsIntegerValue (i2);
			switch (op) {
				case ADDITION_OP:		
					result.SetIntegerValue(i1+i2);	
					return( SIG_CHLD1 | SIG_CHLD2 );

				case SUBTRACTION_OP:	
					result.SetIntegerValue(i1-i2);	
					return( SIG_CHLD1 | SIG_CHLD2 );

				case MULTIPLICATION_OP:	
					result.SetIntegerValue(i1*i2);	
					return( SIG_CHLD1 | SIG_CHLD2 );

				case DIVISION_OP:		
					if (i2 != 0) {
						result.SetIntegerValue(i1/i2);
					} else {
						result.SetErrorValue ();
					}
					return( SIG_CHLD1 | SIG_CHLD2 );
					
				case MODULUS_OP:
					if (i2 != 0) {
						result.SetIntegerValue(i1%i2);
					} else {
						result.SetErrorValue ();
					}
					return( SIG_CHLD1 | SIG_CHLD2 );
							
				default:
					// should not reach here
					CLASSAD_EXCEPT ("Should not get here");
					return( SIG_CHLD1 | SIG_CHLD2 );
			}

		case Value::REAL_VALUE:
			return( doRealArithmetic (op, v1, v2, result) );

		case Value::ABSOLUTE_TIME_VALUE:
		case Value::RELATIVE_TIME_VALUE:
			return( doTimeArithmetic( op, v1, v2, result ) );

		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
	}

	return( SIG_NONE );
}


int Operation::
doLogical (OpKind op, Value &v1, Value &v2, Value &result)
{
	bool		b1, b2;

		// first coerece inputs to boolean if they are considered equivalent
	if( !v1.IsBooleanValue( b1 ) && v1.IsBooleanValueEquiv( b1 ) ) {
		v1.SetBooleanValue( b1 );
	}
	if( !v2.IsBooleanValue( b2 ) && v2.IsBooleanValueEquiv( b2 ) ) {
		v2.SetBooleanValue( b2 );
	}

	Value::ValueType	vt1 = v1.GetType();
	Value::ValueType	vt2 = v2.GetType();

	if( vt1!=Value::UNDEFINED_VALUE && vt1!=Value::ERROR_VALUE && 
			vt1!=Value::BOOLEAN_VALUE ) {
		result.SetErrorValue();
		return SIG_CHLD1;
	}
	if( vt2!=Value::UNDEFINED_VALUE && vt2!=Value::ERROR_VALUE && 
			vt2!=Value::BOOLEAN_VALUE ) { 
		result.SetErrorValue();
		return SIG_CHLD2;
	}
	
	// handle unary operator
	if (op == LOGICAL_NOT_OP) {
		if( vt1 == Value::BOOLEAN_VALUE ) {
			result.SetBooleanValue( !b1 );
		} else {
			result.CopyFrom( v1 );
		}
		return SIG_CHLD1;
	}

	if (op == LOGICAL_OR_OP) {
		if( vt1 == Value::BOOLEAN_VALUE && b1 ) {
			result.SetBooleanValue( true );
			return SIG_CHLD1;
		} else if( vt1 == Value::ERROR_VALUE ) {
			result.SetErrorValue( );
			return SIG_CHLD1;
		} else if( vt1 == Value::BOOLEAN_VALUE && !b1 ) {
			result.CopyFrom( v2 );
		} else if( vt2 != Value::BOOLEAN_VALUE ) {
			result.CopyFrom( v2 );
		} else if( b2 ) {
			result.SetBooleanValue( true );
		} else {
			result.SetUndefinedValue( );
		}
		return( SIG_CHLD1 | SIG_CHLD2 );
	} else if (op == LOGICAL_AND_OP) {
        if( vt1 == Value::BOOLEAN_VALUE && !b1 ) {
            result.SetBooleanValue( false );
			return SIG_CHLD1;
		} else if( vt1 == Value::ERROR_VALUE ) {
			result.SetErrorValue( );
			return SIG_CHLD1;
		} else if( vt1 == Value::BOOLEAN_VALUE && b1 ) {
			result.CopyFrom( v2 );
		} else if( vt2 != Value::BOOLEAN_VALUE ) {
			result.CopyFrom( v2 );
		} else if( !b2 ) {
			result.SetBooleanValue( false );
		} else {
			result.SetUndefinedValue( );
		}
		return( SIG_CHLD1 | SIG_CHLD2 );
	}

	CLASSAD_EXCEPT( "Shouldn't reach here" );
	return( SIG_NONE );
}



int Operation::
doBitwise (OpKind op, Value &v1, Value &v2, Value &result)
{
	long long i1, i2;
	long long signMask = ~LLONG_MAX;	// now at the position of the sign bit
	long long val;

	// bitwise operations are defined only on integers
	if (op == BITWISE_NOT_OP) {
		if (!v1.IsIntegerValue(i1)) {
			result.SetErrorValue();
			return SIG_CHLD1;
		}
	} else if (!v1.IsIntegerValue(i1) || !v2.IsIntegerValue(i2)) {
		result.SetErrorValue();
		return( SIG_CHLD1 | SIG_CHLD2 );
	}

	switch (op) {
		case BITWISE_NOT_OP:	result.SetIntegerValue(~i1);	break;
		case BITWISE_OR_OP:		result.SetIntegerValue(i1|i2);	break;
		case BITWISE_AND_OP:	result.SetIntegerValue(i1&i2);	break;
		case BITWISE_XOR_OP:	result.SetIntegerValue(i1^i2);	break;
		case LEFT_SHIFT_OP:		result.SetIntegerValue(i1<<i2);	break;

		case URIGHT_SHIFT_OP:
			if (i1 >= 0) {
				// sign bit is not on;  >> will work fine
				result.SetIntegerValue (i1 >> i2);
				break;
			} else {
				// sign bit is on
				val = i1 >> 1;	    // shift right 1; the sign bit *may* be on
				val &= (~signMask);	// Clear the sign bit for sure
				val >>= (i2 - 1);	// shift remaining Number of positions
				result.SetIntegerValue (val);
				break;
			}
			// will not reach here
			break;

		case RIGHT_SHIFT_OP:
			if (i1 >= 0) {
				// sign bit is off;  >> will work fine
				result.SetIntegerValue (i1 >> i2);
				break;
			} else {
				// sign bit is on; >> *may* not set the sign
				// TODO Cap i2 to 0..64
				val = i1;
				for (long long i = 0; i < i2; i++)
					val = (val >> 1) | signMask;	// make sure that it does
				result.SetIntegerValue (val);
				break;
			}
			// will not reach here
			break;

		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
	}

	if( op == BITWISE_NOT_OP ) {
		return SIG_CHLD1;
	}

	return( SIG_CHLD1 | SIG_CHLD2 );
}


static volatile bool ClassAdExprFPE = false;
#ifndef WIN32
void ClassAd_SIGFPE_handler (int) { ClassAdExprFPE = true; }
#endif

int Operation::
doRealArithmetic (OpKind op, Value &v1, Value &v2, Value &result)
{
	double r1, r2;	
	double comp=0;

	// we want to prevent FPE and set the ERROR value on the result; on Unix
	// trap sigfpe and set the ClassAdExprFPE flag to true; on NT check the 
	// result against HUGE_VAL.  check errno for EDOM and ERANGE for kicks.

	v1.IsRealValue (r1);
	v2.IsRealValue (r2);

#if 0
#ifndef WIN32
    struct sigaction sa1, sa2;
    sa1.sa_handler = ClassAd_SIGFPE_handler;
    sigemptyset (&(sa1.sa_mask));
    sa1.sa_flags = 0;
    if (sigaction (SIGFPE, &sa1, &sa2)) {
       CLASSAD_EXCEPT("Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
			errno);
    }
#endif
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
			CLASSAD_EXCEPT ("Should not get here");
			return( SIG_NONE );
	}

	// check if anything bad happened
	if (ClassAdExprFPE==true || errno==EDOM || errno==ERANGE || comp==HUGE_VAL)
		result.SetErrorValue ();
	else
		result.SetRealValue (comp);

	// restore the state
#if 0
#ifndef WIN32 
    if (sigaction (SIGFPE, &sa2, &sa1)) {
        CLASSAD_EXCEPT( "Warning! ClassAd: Failed sigaction for SIGFPE (errno=%d)\n",
			errno);
    }
#endif
#endif
	return( SIG_CHLD1 | SIG_CHLD2 );
}


int Operation::
doTimeArithmetic( OpKind op, Value &v1, Value &v2, Value &result )
{
  abstime_t asecs1,asecs2;
  asecs1.secs = 0;
  asecs1.offset =0;
asecs2.secs = 0;
  asecs2.offset =0;
	double rsecs1=0;
	double rsecs2=0;
	Value::ValueType vt1=v1.GetType( ), vt2=v2.GetType( );

		// addition
	if( op==ADDITION_OP ) {
		if( vt1==Value::ABSOLUTE_TIME_VALUE && 
				vt2==Value::RELATIVE_TIME_VALUE ) {
			v1.IsAbsoluteTimeValue( asecs1 );
			v2.IsRelativeTimeValue( rsecs2 );
			asecs1.secs += (int) rsecs2;
			result.SetAbsoluteTimeValue( asecs1 );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt1==Value::RELATIVE_TIME_VALUE && 
				vt2==Value::ABSOLUTE_TIME_VALUE ) {
			v1.IsRelativeTimeValue( rsecs1 );
			v2.IsAbsoluteTimeValue( asecs2 );
			asecs2.secs += (int) rsecs1;
			result.SetAbsoluteTimeValue( asecs2 );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt1==Value::RELATIVE_TIME_VALUE && 
				vt2==Value::RELATIVE_TIME_VALUE ) {
			v1.IsRelativeTimeValue( rsecs1 );
			v2.IsRelativeTimeValue( rsecs2 );
			result.SetRelativeTimeValue( rsecs1 + rsecs2 );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}
	}

	if( op == SUBTRACTION_OP ) {
		if( vt1==Value::ABSOLUTE_TIME_VALUE && 
				vt2==Value::ABSOLUTE_TIME_VALUE ) {
			v1.IsAbsoluteTimeValue( asecs1 );
			v2.IsAbsoluteTimeValue( asecs2 );
			result.SetRelativeTimeValue( asecs1.secs - asecs2.secs );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt1==Value::ABSOLUTE_TIME_VALUE && 
				vt2==Value::RELATIVE_TIME_VALUE ) {
			v1.IsAbsoluteTimeValue( asecs1 );
			v2.IsRelativeTimeValue( rsecs2 );
			asecs1.secs = asecs1.secs - (int) rsecs2;
			result.SetAbsoluteTimeValue( asecs1 );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt1==Value::RELATIVE_TIME_VALUE && 
				vt2==Value::RELATIVE_TIME_VALUE ) {
			v1.IsRelativeTimeValue( rsecs1 );
			v2.IsRelativeTimeValue( rsecs2 );
			result.SetRelativeTimeValue( rsecs1 - rsecs2 );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}
	}

	if( op == MULTIPLICATION_OP || op == DIVISION_OP ) {
		if( vt1==Value::RELATIVE_TIME_VALUE && vt2==Value::INTEGER_VALUE ) {
			long long     num;
            double  msecs;
			v1.IsRelativeTimeValue( rsecs1 );
			v2.IsIntegerValue( num );
			if( op == MULTIPLICATION_OP ) {
				msecs = rsecs1 * num;
			} else {
				msecs = rsecs1 / num;
			}
			result.SetRelativeTimeValue( msecs );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt1==Value::RELATIVE_TIME_VALUE &&  vt2==Value::REAL_VALUE ) {
			double  num;
			double  msecs;
			v1.IsRelativeTimeValue( rsecs1 );
			v2.IsRealValue( num );
			if( op == MULTIPLICATION_OP ) {
				msecs = rsecs1 * num;
			} else {
				msecs = rsecs1 / num;
			}
			result.SetRelativeTimeValue( msecs );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt1==Value::INTEGER_VALUE && vt2==Value::RELATIVE_TIME_VALUE && 
				op==MULTIPLICATION_OP ) {
			long long num;
			v1.IsIntegerValue( num );
			v2.IsRelativeTimeValue( rsecs1 );
			result.SetRelativeTimeValue( num * rsecs1 );
			return( SIG_CHLD1 | SIG_CHLD2 );
		}

		if( vt2==Value::RELATIVE_TIME_VALUE &&  vt1==Value::REAL_VALUE &&
				op==MULTIPLICATION_OP ) {
			double 	num;
			v1.IsRelativeTimeValue( rsecs1 );
			v2.IsRealValue( num );
			result.SetRelativeTimeValue(rsecs1 * num);
			return( SIG_CHLD1 | SIG_CHLD2 );
		}
	}

	// no other operations are supported on times
	result.SetErrorValue( );
	return( SIG_CHLD1 | SIG_CHLD2 );
}


void Operation::
compareStrings (OpKind op, Value &v1, Value &v2, Value &result, bool exact)
{
	const char *s1 = NULL, *s2 = NULL;
	int  cmp;
	
	v1.IsStringValue (s1);
	v2.IsStringValue (s2);

	result.SetBooleanValue( false );
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
			result.SetBooleanValue( true );
		}
	} else if (cmp == 0) {
		// s1 == s2
		if (op == LESS_OR_EQUAL_OP 	|| 
			op == EQUAL_OP			||
			op == GREATER_OR_EQUAL_OP) {
			result.SetBooleanValue( true );
		}
	} else {
		// s1 > s2
		if (op == GREATER_THAN_OP	||
			op == GREATER_OR_EQUAL_OP	||
			op == NOT_EQUAL_OP) {
			result.SetBooleanValue( true );
		}
	}
}


void Operation::
compareAbsoluteTimes( OpKind op, Value &v1, Value &v2, Value &result )
{
	abstime_t asecs1, asecs2;
	bool compResult;

	v1.IsAbsoluteTimeValue( asecs1 );
	v2.IsAbsoluteTimeValue( asecs2 );

	switch( op ) {
		case LESS_THAN_OP: 			compResult = (asecs1.secs < asecs2.secs); 	break;
		case LESS_OR_EQUAL_OP: 		compResult = (asecs1.secs <= asecs2.secs); 	break;
		case EQUAL_OP: 				compResult = (asecs1.secs == asecs2.secs); 	break;
		case NOT_EQUAL_OP: 			compResult = (asecs1.secs != asecs2.secs); 	break;
		case GREATER_THAN_OP: 		compResult = (asecs1.secs > asecs2.secs); 	break;
		case GREATER_OR_EQUAL_OP: 	compResult = (asecs1.secs >= asecs2.secs); 	break;
		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
			return;
	}

	result.SetBooleanValue( compResult );
}

void Operation::
compareRelativeTimes( OpKind op, Value &v1, Value &v2, Value &result )
{
	double rsecs1, rsecs2;
	bool compResult;

	v1.IsRelativeTimeValue( rsecs1 );
	v2.IsRelativeTimeValue( rsecs2 );

	switch( op ) {
		case LESS_THAN_OP: 			
			compResult = ( rsecs1 < rsecs2 );
			break;

		case LESS_OR_EQUAL_OP:
			compResult = ( rsecs1 <= rsecs2 );
			break;

		case EQUAL_OP:
			compResult = ( rsecs1 == rsecs2 );
			break;

		case NOT_EQUAL_OP:
			compResult = ( rsecs1 != rsecs2 );
			break;

		case GREATER_THAN_OP:
			compResult = ( rsecs1 > rsecs2 );
			break;

		case GREATER_OR_EQUAL_OP:
			compResult = ( rsecs1 >= rsecs2 );
			break;

		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
			return;
	}

	result.SetBooleanValue( compResult );
}


void Operation::
compareBools( OpKind op, Value &v1, Value &v2, Value &result )
{
	bool b1, b2, compResult;

	v1.IsBooleanValue( b1 );
	v2.IsBooleanValue( b2 );

	switch( op ) {
		case LESS_THAN_OP: 			compResult = (b1 < b2); 	break;
		case LESS_OR_EQUAL_OP: 		compResult = (b1 <= b2); 	break;
		case EQUAL_OP: 				compResult = (b1 == b2); 	break;
		case NOT_EQUAL_OP: 			compResult = (b1 != b2); 	break;
		case GREATER_THAN_OP: 		compResult = (b1 > b2); 	break;
		case GREATER_OR_EQUAL_OP: 	compResult = (b1 >= b2); 	break;
		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
			return;
	}

	result.SetBooleanValue( compResult );
}


void Operation::
compareIntegers (OpKind op, Value &v1, Value &v2, Value &result)
{
	long long 	i1, i2; 
	bool	compResult;

	v1.IsIntegerValue (i1); 
	v2.IsIntegerValue (i2);

	switch (op) {
		case LESS_THAN_OP: 			compResult = (i1 < i2); 	break;
		case LESS_OR_EQUAL_OP: 		compResult = (i1 <= i2); 	break;
		case EQUAL_OP: 				compResult = (i1 == i2); 	break;
		case NOT_EQUAL_OP: 			compResult = (i1 != i2); 	break;
		case GREATER_THAN_OP: 		compResult = (i1 > i2); 	break;
		case GREATER_OR_EQUAL_OP: 	compResult = (i1 >= i2); 	break;
		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
			return;
	}

	result.SetBooleanValue( compResult );
}


void Operation::
compareReals (OpKind op, Value &v1, Value &v2, Value &result)
{
	double 	r1, r2;
	bool	compResult;

	v1.IsRealValue (r1);
	v2.IsRealValue (r2);

	switch (op) {
		case LESS_THAN_OP:          compResult = (r1 < r2);     break;
		case LESS_OR_EQUAL_OP:      compResult = (r1 <= r2);    break;
		case EQUAL_OP:              compResult = (r1 == r2);    break;
		case NOT_EQUAL_OP:          compResult = (r1 != r2);    break;
		case GREATER_THAN_OP:       compResult = (r1 > r2);     break;
		case GREATER_OR_EQUAL_OP:   compResult = (r1 >= r2);    break;
		default:
			// should not get here
			CLASSAD_EXCEPT ("Should not get here");
			return;
	}

	result.SetBooleanValue( compResult );
}


// This function performs type promotions so that both v1 and v2 are of the
// same numerical type: (v1 and v2 are not ERROR or UNDEFINED)
//  + if both v1 and v2 are Numbers and of the same type, return type
//  + if v1 is an int and v2 is a real, convert v1 to real; return REAL_VALUE
//  + if v1 is a real and v2 is an int, convert v2 to real; return REAL_VALUE
Value::ValueType Operation::
coerceToNumber (Value &v1, Value &v2)
{
	long long i;
	double 	r;
    bool    b;

	// either of v1, v2 not numerical?
	if (v1.IsClassAdValue()   || v2.IsClassAdValue())   
		return Value::CLASSAD_VALUE;
	if (v1.IsListValue()      || v2.IsListValue())   	
		return Value::LIST_VALUE;
	if (v1.IsStringValue ()   || v2.IsStringValue ())  
		return Value::STRING_VALUE;
	if (v1.IsUndefinedValue() || v2.IsUndefinedValue()) 
		return Value::UNDEFINED_VALUE;
	if (v1.IsErrorValue ()    || v2.IsErrorValue ())    
		return Value::ERROR_VALUE;
	if (v1.IsAbsoluteTimeValue()||v2.IsAbsoluteTimeValue()) 
		return Value::ABSOLUTE_TIME_VALUE;
	if( v1.IsRelativeTimeValue() || v2.IsRelativeTimeValue() )
		return Value::RELATIVE_TIME_VALUE;

    // promote booleans to integers
    if (v1.IsBooleanValue(b)) {
        if (b) {
            v1.SetIntegerValue(1);
        } else {
            v1.SetIntegerValue(0);
        }
    }

    if (v2.IsBooleanValue(b)) {
        if (b) {
            v2.SetIntegerValue(1);
        } else {
            v2.SetIntegerValue(0);
        }
    }

	// both v1 and v2 of same numerical type
	if(v1.IsIntegerValue(i) && v2.IsIntegerValue(i))return Value::INTEGER_VALUE;
	if(v1.IsRealValue(r) && v2.IsRealValue(r)) return Value::REAL_VALUE;

	// type promotions required
	if (v1.IsIntegerValue(i) && v2.IsRealValue(r))
		v1.SetRealValue ((double)i);
	else
	if (v1.IsRealValue(r) && v2.IsIntegerValue(i))
		v2.SetRealValue ((double)i);

	return Value::REAL_VALUE;
}

 
Operation *Operation::
MakeOperation (OpKind op, ExprTree *e1, ExprTree *e2, ExprTree *e3)
{
	Operation *opnode = new Operation;
	if( !opnode ) {
		CondorErrno = ERR_MEM_ALLOC_FAILED;
		CondorErrMsg = "";
		return( NULL );
	}
	opnode->operation = op;
	opnode->child1    = e1;
	opnode->child2    = e2;
	opnode->child3    = e3;
	return( opnode );
}


void Operation::
GetComponents( OpKind &op, ExprTree *&e1, ExprTree *&e2, ExprTree *&e3 ) const
{
	op = operation;
	e1 = child1;
	e2 = child2;
	e3 = child3;
}


Operation *Operation::
MakeOperation( OpKind op, Value &val, ExprTree *tree ) 
{
	Operation  	*newOp;
	Literal		*lit;
	
	if( !tree ) return NULL;

	if( !( lit = Literal::MakeLiteral( val ) ) ) {
		return NULL;
	}
	if( !( newOp = MakeOperation( op, lit, tree ) ) ) {
		delete lit;
		return( NULL );
	}
	return newOp;
}
	
	
Operation *Operation::
MakeOperation( OpKind op, ExprTree *tree, Value &val ) 
{
	Operation  	*newOp;
	Literal		*lit;

	if( tree == NULL ) return NULL;

	if( !( lit = Literal::MakeLiteral( val ) ) ) {
		return NULL;
	}
	if( !( newOp = MakeOperation( op, tree, lit ) ) ) {
		delete lit;
		return( NULL );
	}
	return newOp;
}


bool Operation::
flattenSpecials( EvalState &state, Value &val, ExprTree *&tree ) const
{
	ExprTree 	*fChild1 = NULL, *fChild2 = NULL, *fChild3 = NULL;
	Value		eval1, eval2, eval3, dummy;

	switch( operation ) {
		case UNARY_PLUS_OP:
		case UNARY_MINUS_OP:
		case PARENTHESES_OP:
		case LOGICAL_NOT_OP:
		case BITWISE_NOT_OP:
			if( !child1->Flatten( state, eval1, fChild1 ) ) {
				tree = NULL;
				return false;
			} 
			if( fChild1 ) {
				tree = Operation::MakeOperation( operation, fChild1 );
				return( tree != NULL );
			} else {
				_doOperation( operation, eval1, dummy, dummy, true, false, 
					false, val );
				tree = NULL;
				eval1.Clear();
				return true;
			}
			break;


		case TERNARY_OP:
			// Flatten the selector expression
			if( !child1->Flatten( state, eval1, fChild1 ) ) {
				tree = NULL;
				return false;
			}

			// check if selector expression collapsed to a non-undefined value
			if( !fChild1 && !eval1.IsUndefinedValue() ) {
				bool   		b; 
				Value::ValueType	vt = eval1.GetType();

				// if the selector is not boolean, propagate error
				if( vt!=Value::BOOLEAN_VALUE ) {
					val.SetErrorValue();	
					eval1.Clear();
					tree = NULL;
					return true;
				}

				// eval1 is either a real or an integer
				if(	eval1.IsBooleanValue(b) && b!=0 ) {
					return child2->Flatten( state, val, tree );	
				} else {
					return child3->Flatten( state, val, tree );	
				}
			} else {
				// Flatten arms of the if expression
				if( !child2->Flatten( state, eval2, fChild2 ) ||
					!child3->Flatten( state, eval3, fChild3 ) ) {
					// clean up
					if( fChild1 ) delete fChild1;
					if( fChild2 ) delete fChild2;
					if( fChild3 ) delete fChild3;
					tree = NULL;
					return false;
				}

				// if any arm collapsed into a value, make it a Literal
				if( !fChild2 ) fChild2 = Literal::MakeLiteral( eval2 );
				if( !fChild3 ) fChild3 = Literal::MakeLiteral( eval3 );
				if( !fChild2 || !fChild3 ) {
					// clean up
					if( fChild1 ) delete fChild1;
					if( fChild2 ) delete fChild2;
					if( fChild3 ) delete fChild3;
					tree = NULL;
					return false;
				}
	
				// fChild1 may be NULL if child1 Flattened to UNDEFINED
				if( !fChild1 ) {
					fChild1 = child1->Copy();
				}

				tree = Operation::MakeOperation( operation, fChild1, fChild2, 
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
			// Flatten both arguments
			if( !child1->Flatten( state, eval1, fChild1 ) ||
				!child2->Flatten( state, eval2, fChild2 ) ) {
				if( fChild1 ) delete fChild1;
				if( fChild2 ) delete fChild2;
				tree = NULL;
				return false;
			}

			// if both arguments Flattened to values, Evaluate now
			if( !fChild1 && !fChild2 ) {
				_doOperation( operation, eval1, eval2, dummy, true, true, false,
						val );
				tree = NULL;
				return true;
			} 

			// otherwise convert Flattened values into literals
			if( !fChild1 ) fChild1 = Literal::MakeLiteral( eval1 );
			if( !fChild2 ) fChild2 = Literal::MakeLiteral( eval2 );
			if( !fChild1 || !fChild2 ) {
				if( fChild1 ) delete fChild1;
				if( fChild2 ) delete fChild2;
				tree = NULL;
				return false;
			}
				
			tree = Operation::MakeOperation( operation, fChild1, fChild2 );
			if( !tree ) {
				if( fChild1 ) delete fChild1;
				if( fChild2 ) delete fChild2;
				tree = NULL;
				return false;
			}
			return true;

		default:
			CLASSAD_EXCEPT( "Should not get here" );
	}

	return true;
}

bool Operation::
IsStrictOperator( OpKind op ) 
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


	// get precedence levels for operators (see K&R p.53 )
int Operation::
PrecedenceLevel( OpKind op )
{
	switch( op ) {
		case SUBSCRIPT_OP: 
			return( 12 );

		case LOGICAL_NOT_OP: case BITWISE_NOT_OP: case UNARY_PLUS_OP: 
		case UNARY_MINUS_OP: 
			return( 11 );

		case MULTIPLICATION_OP: case DIVISION_OP: case MODULUS_OP:
			return( 10 );

		case ADDITION_OP: case SUBTRACTION_OP:
			return( 9 );

		case LEFT_SHIFT_OP: case RIGHT_SHIFT_OP: case URIGHT_SHIFT_OP:
			return( 8 );

		case LESS_THAN_OP: case LESS_OR_EQUAL_OP: case GREATER_OR_EQUAL_OP:
		case GREATER_THAN_OP:
			return( 7 );

		case NOT_EQUAL_OP: case EQUAL_OP: case IS_OP: case ISNT_OP: 
			return( 6 );

		case BITWISE_AND_OP:
			return( 5 );

		case BITWISE_XOR_OP:
			return( 4 );

		case BITWISE_OR_OP:
			return( 3 );

		case LOGICAL_AND_OP:
			return( 2 );

		case LOGICAL_OR_OP:
			return( 1 );

		case TERNARY_OP:
			return( 0 );

		default:
			return( -1 );
	}
}

} // classad
