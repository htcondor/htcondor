#ifndef __OPERATORS_H__
#define __OPERATORS_H__

#include "exprTree.h"

class Sink;

// kinds of operators
enum OpKind 
{
	__NO_OP__,				// convenience

	__FIRST_OP__,

    __COMPARISON_START__	= __FIRST_OP__,
    LESS_THAN_OP 			= __COMPARISON_START__,
    LESS_OR_EQUAL_OP,		// (comparison)
    NOT_EQUAL_OP,			// (comparison)
    EQUAL_OP,				// (comparison)
	META_EQUAL_OP,			// (comparison)
	META_NOT_EQUAL_OP,		// (comparison)
    GREATER_OR_EQUAL_OP,	// (comparison)
    GREATER_THAN_OP,		// (comparison)
	__COMPARISON_END__ 		= GREATER_THAN_OP,

    __ARITHMETIC_START__,     
	UNARY_PLUS_OP 			= __ARITHMETIC_START__,
	UNARY_MINUS_OP,			// (arithmetic)
    ADDITION_OP,			// (arithmetic)
    SUBTRACTION_OP,			// (arithmetic)
    MULTIPLICATION_OP,		// (arithmetic)
    DIVISION_OP,			// (arithmetic)
    MODULUS_OP,				// (arithmetic)
	__ARITHMETIC_END__ 		= MODULUS_OP,
	
	__LOGIC_START__,
	LOGICAL_NOT_OP 			= __LOGIC_START__,
    LOGICAL_OR_OP,			// (logical)
    LOGICAL_AND_OP,			// (logical)
    __LOGIC_END__ 			= LOGICAL_AND_OP,

	__BITWISE_START__,
	BITWISE_NOT_OP 			= __BITWISE_START__,
	BITWISE_OR_OP,			// (bitwise)
	BITWISE_XOR_OP,			// (bitwise)
	BITWISE_AND_OP,			// (bitwise)
	LEFT_SHIFT_OP,			// (bitwise)
	LOGICAL_RIGHT_SHIFT_OP,	// (bitwise) -- unsigned shift
	ARITH_RIGHT_SHIFT_OP,	// (bitwise) -- signed shift
	__BITWISE_END__ 		= ARITH_RIGHT_SHIFT_OP,	

	__MISC_START__,
	PARENTHESES_OP 			= __MISC_START__,
	SUBSCRIPT_OP,			// (misc) -- subscripting elements from lists
	TERNARY_OP,				// (misc)
	__MISC_END__ 			= TERNARY_OP,

	__LAST_OP__				= __MISC_END__
};


class Operation : public ExprTree
{
  	public:
		// ctors/dtor
		Operation ();
		~Operation ();

		// override methods
		virtual ExprTree *copy (CopyMode);
		virtual bool toSink (Sink &);

		// specific methods (allow for unary, binary and ternary ops) 
		void setOperation (OpKind, ExprTree* = NULL, ExprTree* = NULL,
				ExprTree* = NULL);

		// table of string representations of operators; indexed by OpKind
		static char *opString[];

		// public access to operation function
		static void operate (OpKind, Value &, Value &, Value &);
		static void operate (OpKind, Value &, Value &, Value &, Value &);

	protected:
		virtual void setParentScope( ClassAd * );

  	private:
		virtual void _evaluate (EvalState &, EvalValue &);

		// auxillary functionms
		static ValueType  coerceToNumber (EvalValue&, EvalValue &);
		static void _doOperation(OpKind,EvalValue&,EvalValue&,EvalValue&,
								bool,bool,bool, EvalValue&);
		static void doComparison(OpKind, EvalValue&, EvalValue&, EvalValue&);
		static void doArithmetic(OpKind, EvalValue&, EvalValue&, EvalValue&);
		static void doLogical 	(OpKind, EvalValue&, EvalValue&, EvalValue&);
		static void doBitwise 	(OpKind, EvalValue&, EvalValue&, EvalValue&); 
		static void compareStrings(OpKind, EvalValue&, EvalValue&, EvalValue&);
		static void compareReals(OpKind, EvalValue&, EvalValue&, EvalValue&);
		static void compareIntegers(OpKind, EvalValue&, EvalValue&, EvalValue&);
		static void doRealArithmetic(OpKind,EvalValue&, EvalValue&, EvalValue&);

		// operation specific information
		OpKind		operation;
		ExprTree 	*child1;
		ExprTree	*child2;
		ExprTree	*child3;
};


#endif//__OPERATORS_H__
