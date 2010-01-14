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


#ifndef __OPERATORS_H__
#define __OPERATORS_H__

#include "classad/exprTree.h"

BEGIN_NAMESPACE( classad )

/** Represents a node of the expression tree which is an operation applied to
	expression operands, like 3 + 2
*/
class Operation : public ExprTree
{
  	public:
		/// List of supported operators 
		enum OpKind
		{
			/** No op */ __NO_OP__,              // convenience

			__FIRST_OP__,

			__COMPARISON_START__    = __FIRST_OP__,
			/** @name Strict comparison operators */
			//@{
			/** Less than operator  */ LESS_THAN_OP = __COMPARISON_START__,
			/** Less or equal       */  LESS_OR_EQUAL_OP,       // (comparison)
			/** Not equal           */  NOT_EQUAL_OP,           // (comparison)
			/** Equal               */  EQUAL_OP,               // (comparison)
			/** Greater or equal    */  GREATER_OR_EQUAL_OP,    // (comparison)
			/** Greater than        */  GREATER_THAN_OP,        // (comparison)
			//@}

			/** @name Non-strict comparison operators */
			//@{
			/** Meta-equal (same as IS)*/ META_EQUAL_OP,        // (comparison)
			/** Is                  */  IS_OP= META_EQUAL_OP,   // (comparison)
			/** Meta-not-equal (same as ISNT) */META_NOT_EQUAL_OP,//(comparison)
			/** Isnt                */  ISNT_OP=META_NOT_EQUAL_OP,//(comparison)
			//@}
			__COMPARISON_END__      = ISNT_OP,

			__ARITHMETIC_START__,
			/** @name Arithmetic operators */
			//@{
			/** Unary plus          */  UNARY_PLUS_OP = __ARITHMETIC_START__,
			/** Unary minus         */  UNARY_MINUS_OP,         // (arithmetic)
			/** Addition            */  ADDITION_OP,            // (arithmetic)
			/** Subtraction         */  SUBTRACTION_OP,         // (arithmetic)
			/** Multiplication      */  MULTIPLICATION_OP,      // (arithmetic)
			/** Division            */  DIVISION_OP,            // (arithmetic)
			/** Modulus             */  MODULUS_OP,             // (arithmetic)
			//@}
			__ARITHMETIC_END__      = MODULUS_OP,

			__LOGIC_START__,
			/** @name Logical operators */
			//@{
			/** Logical not         */  LOGICAL_NOT_OP = __LOGIC_START__,
			/** Logical or          */  LOGICAL_OR_OP,          // (logical)
			/** Logical and         */  LOGICAL_AND_OP,         // (logical)
			//@}
			__LOGIC_END__           = LOGICAL_AND_OP,

			__BITWISE_START__,
			/** @name Bitwise operators */
			//@{
			/** Bitwise not         */  BITWISE_NOT_OP = __BITWISE_START__,
			/** Bitwise or          */  BITWISE_OR_OP,          // (bitwise)
			/** Bitwise xor         */  BITWISE_XOR_OP,         // (bitwise)
			/** Bitwise and         */  BITWISE_AND_OP,         // (bitwise)
			/** Left shift          */  LEFT_SHIFT_OP,          // (bitwise)
			/** Right shift         */  RIGHT_SHIFT_OP,         // (bitwise)
			/** Unsigned right shift */ URIGHT_SHIFT_OP,        // (bitwise)
			//@}
			__BITWISE_END__         = URIGHT_SHIFT_OP,

			__MISC_START__,
			/** @name Miscellaneous operators */
			//@{
			/** Parentheses         */  PARENTHESES_OP = __MISC_START__,
			/** Subscript           */  SUBSCRIPT_OP,           // (misc)
			/** Conditional op      */  TERNARY_OP,             // (misc)
			//@}
			__MISC_END__            = TERNARY_OP,

			__LAST_OP__             = __MISC_END__
		};

        /// Copy Constructor
        Operation(const Operation &op);

		/// Destructor
		virtual ~Operation ();

        /// Assignment operator
        Operation &operator=(const Operation &op);

		/** Factory method to create an operation expression node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
			@return The constructed operation
		*/
		static Operation *MakeOperation(OpKind kind,ExprTree*e1=NULL,
					ExprTree*e2=NULL, ExprTree*e3=NULL);

		/** Deconstructor to obtain the components of an operation node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		void GetComponents( OpKind&, ExprTree*&, ExprTree*&, ExprTree *& )const;

		// public access to operation function
		/** Convenience method which operates on binary operators.
			@param op The kind of operation.
			@param op1 The first operand.
			@param op2 The second operand.
			@param result The result of the operation.
			@see OpKind, Value
		*/
		static void Operate (OpKind op, Value &op1, Value &op2, Value &result);

		/** Convenience method which operates on ternary operators.
			@param op The kind of operation.
			@param op1 The first operand.
			@param op2 The second operand.
			@param op3 The third operand.
			@param result The result of the operation.
			@see OpKind, Value
		*/
		static void Operate (OpKind op, Value &op1, Value &op2, Value &op3, 
			Value &result);

		/** Predicate which tests if an operator is strict.
			@param op The operator to be tested.
			@return true if the operator is strict, false otherwise.
		*/
		static bool IsStrictOperator( OpKind );

		/** Gets the precedence level of an operator.  Higher precedences
		 * 	get higher values.  (See K&R, p.53)
		 * 	@param op The operator to get the precedence of
		 * 	@return The precedence level of the operator.
		 */
		static int PrecedenceLevel( OpKind );

		/// Make a deep copy of the expression
		virtual ExprTree* Copy( ) const;

        bool CopyFrom(const Operation &op);

        virtual bool SameAs(const ExprTree *tree) const;

        friend bool operator==(const Operation &op1, const Operation &op2);

	protected:
		/// Constructor
		Operation ();

  	private:
        bool SameChild(const ExprTree *tree1, const ExprTree *tree2) const;

		virtual void _SetParentScope( const ClassAd* );
		virtual bool _Evaluate( EvalState &, Value &) const;
		virtual bool _Evaluate( EvalState &, Value &, ExprTree*& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;

		// auxillary functionms
		bool combine( OpKind&, Value&, ExprTree*&, 
				int, Value&, ExprTree*, int, Value&, ExprTree* ) const;
		bool flattenSpecials( EvalState &, Value &, ExprTree *& ) const;

		static Operation* MakeOperation( OpKind, Value&, ExprTree* );
		static Operation* MakeOperation( OpKind, ExprTree*, Value& );
		static Value::ValueType coerceToNumber (Value&, Value &);

		enum SigValues { SIG_NONE=0, SIG_CHLD1=1 , SIG_CHLD2=2 , SIG_CHLD3=4 };

		static int _doOperation(OpKind,Value&,Value&,Value&,
								bool,bool,bool,  Value&, EvalState* = NULL);
		static int 	doComparison		(OpKind, Value&, Value&, Value&);
		static int 	doArithmetic		(OpKind, Value&, Value&, Value&);
		static int 	doLogical 			(OpKind, Value&, Value&, Value&);
		static int 	doBitwise 			(OpKind, Value&, Value&, Value&); 
		static int 	doRealArithmetic	(OpKind, Value&, Value&, Value&);
		static int 	doTimeArithmetic	(OpKind, Value&, Value&, Value&);
		static void compareStrings		(OpKind, Value&, Value&, Value&,bool);
		static void compareReals		(OpKind, Value&, Value&, Value&);
		static void compareBools		(OpKind, Value&, Value&, Value&);
		static void compareIntegers		(OpKind, Value&, Value&, Value&);
		static void compareAbsoluteTimes(OpKind, Value&, Value&, Value&);
		static void compareRelativeTimes(OpKind, Value&, Value&, Value&);

		// operation specific information
		OpKind		operation;
		ExprTree 	*child1;
		ExprTree	*child2;
		ExprTree	*child3;
};

END_NAMESPACE // classad

#endif//__OPERATORS_H__
