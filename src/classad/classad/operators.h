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


#ifndef __CLASSAD_OPERATORS_H__
#define __CLASSAD_OPERATORS_H__

#include "classad/exprTree.h"

namespace classad {

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
			/** Elvis  i.e A?:B     */  ELVIS_OP,               // (misc)
			/** Conditional op      */  TERNARY_OP,             // (misc)
			//@}
			__MISC_END__            = TERNARY_OP,

			__LAST_OP__             = __MISC_END__
		};

		/// Destructor
		virtual ~Operation ();

		/// node type
		virtual NodeKind GetKind (void) const { return OP_NODE; }
		virtual OpKind GetOpKind(void) const { return __NO_OP__; }

		/** Factory method to create an operation expression node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
			@return The constructed operation
		*/
		static Operation *MakeOperation(OpKind kind,ExprTree*e1=NULL,
					ExprTree*e2=NULL, ExprTree*e3=NULL);

#ifdef TJ_PICKLE
		static Operation * Make(ExprStream & stm);
		// validate and skip over the next expression in the stream if it is a valid Literal (or Value)
		// returns the number of bytes read from the stream, or 0 on failure.
		static unsigned int Scan(ExprStream & stm);
		unsigned int Pickle(ExprStreamMaker & stm) const;
		static unsigned char codeToOpKind(unsigned char tag);
		static unsigned char OpKindTocode(OpKind op);
#endif

		/** Deconstructor to obtain the components of an operation node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		virtual void GetComponents( OpKind&, ExprTree*&, ExprTree*&, ExprTree *& )const;

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

        virtual bool SameAs(const ExprTree *tree) const;

        friend bool operator==(const Operation &op1, const Operation &op2);

		virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

	protected:
		/// Constructor
		Operation() : parentScope(NULL) {};

  	private:
        static bool SameChild(const ExprTree *tree1, const ExprTree *tree2);
        static bool SameChildren(const Operation * op1, const Operation * op2);

		virtual void _SetParentScope( const ClassAd* );
		virtual bool _Evaluate( EvalState &, Value &) const;
		virtual bool _Evaluate( EvalState &, Value &, ExprTree*& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;

			// returns true if result is determined for this operation
			// based on the evaluated arg1
		bool shortCircuit( EvalState &state, Value const &arg1, Value &result ) const;

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
		static void compareStrings		(OpKind, Value&, Value&, Value&);
		static void compareReals		(OpKind, Value&, Value&, Value&);
		static void compareBools		(OpKind, Value&, Value&, Value&);
		static void compareIntegers		(OpKind, Value&, Value&, Value&);
		static void compareAbsoluteTimes(OpKind, Value&, Value&, Value&);
		static void compareRelativeTimes(OpKind, Value&, Value&, Value&);

		const ClassAd *parentScope;

		// No Operation-specific data members.
		// Everything is in child classes

		friend class Operation1;
		friend class OperationParens;
		friend class Operation2;
		friend class Operation3;
};


class Operation1 : public Operation
{
	public:
        /// Copy Constructor
		Operation1(const Operation1 &op);

		/// Destructor
		virtual ~Operation1 ();

        /// Assignment operator
        Operation1 &operator=(const Operation1 &op);

		virtual OpKind GetOpKind(void) const { return operation; }

		/** Deconstructor to obtain the components of an operation node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		virtual void GetComponents( OpKind&, ExprTree*&, ExprTree*&, ExprTree *& )const;

		/// Make a deep copy of the expression
		virtual ExprTree* Copy( ) const;

	protected:
		/// Constructor
		Operation1 (OpKind op=__NO_OP__, ExprTree *c1=NULL) : child1(c1), operation(op) {};
		friend class Operation;

	private:
		bool flatten( EvalState &, Value &, ExprTree *& ) const;
		ExprTree	*child1;
		OpKind		operation;
};

class OperationParens : public Operation
{
	public:
        /// Copy Constructor
		OperationParens(const Operation1 &op);

		/// Destructor
		virtual ~OperationParens ();

        /// Assignment operator
        OperationParens &operator=(const OperationParens &op);

		virtual OpKind GetOpKind(void) const { return PARENTHESES_OP; }

		/** Deconstructor to obtain the components of an operation node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		virtual void GetComponents( OpKind&, ExprTree*&, ExprTree*&, ExprTree *& )const;

		/// Make a deep copy of the expression
		virtual ExprTree* Copy( ) const;
		virtual bool _Evaluate( EvalState &, Value &) const;

	protected:
		/// Constructor
		OperationParens (ExprTree *c1=NULL) : child1(c1) {};
		friend class Operation;

	private:
		bool flatten( EvalState &, Value &, ExprTree *& ) const;
		ExprTree	*child1;
};

class Operation2 : public Operation
{
	public:
        /// Copy Constructor
		Operation2(const Operation2 &op);

		/// Destructor
		virtual ~Operation2 ();

        /// Assignment operator
        Operation2 &operator=(const Operation2 &op);

		virtual OpKind GetOpKind(void) const { return operation; }

		/** Deconstructor to obtain the components of an operation node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		virtual void GetComponents( OpKind&, ExprTree*&, ExprTree*&, ExprTree *& )const;

		/// Make a deep copy of the expression
		virtual ExprTree* Copy( ) const;

	protected:
		/// Constructor
		Operation2 (OpKind op=__NO_OP__, ExprTree *c1=NULL, ExprTree *c2=NULL) : child1(c1), child2(c2), operation(op) {};
		friend class Operation;

	private:
		//virtual bool _Evaluate( EvalState &, Value &) const;
		//virtual bool _Evaluate( EvalState &, Value &, ExprTree*& ) const;
		//virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
		bool flatten( EvalState &, Value &, ExprTree *& ) const;

			// returns true if result is determined for this operation
			// based on the evaluated arg1
		bool shortCircuit( EvalState &state, Value const &arg1, Value &result ) const;

		// auxillary functionms
		bool combine( OpKind&, Value&, ExprTree*&,
				int, Value&, ExprTree*, int, Value&, ExprTree* ) const;
		bool flattenSpecials( EvalState &, Value &, ExprTree *& ) const;

		ExprTree	*child1;
		ExprTree	*child2;
		OpKind		operation;
};

class Operation3 : public Operation
{
	public:
        /// Copy Constructor
		Operation3(const Operation3 &op);

		/// Destructor
		virtual ~Operation3 ();

        /// Assignment operator
        Operation3 &operator=(const Operation3 &op);

		virtual OpKind GetOpKind(void) const { return TERNARY_OP; }

		/** Deconstructor to obtain the components of an operation node
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		virtual void GetComponents( OpKind&, ExprTree*&, ExprTree*&, ExprTree *& )const;

		/// Make a deep copy of the expression
		virtual ExprTree* Copy( ) const;

			// returns true if result is determined for this operation
			// based on the evaluated arg1
		bool shortCircuit( EvalState &state, Value const &arg1, Value &result ) const;

	protected:
		/// Constructor
		Operation3 (ExprTree *c1=NULL, ExprTree* c2 = NULL, ExprTree *c3 = NULL) : child1(c1), child2(c2), child3(c3) {};
		friend class Operation;

	private:
		//virtual bool _Evaluate( EvalState &, Value &) const;
		//virtual bool _Evaluate( EvalState &, Value &, ExprTree*& ) const;
		//virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
		bool flatten( EvalState &, Value &, ExprTree *& ) const;

		// auxillary functionms
		bool combine( OpKind&, Value&, ExprTree*&,
				int, Value&, ExprTree*, int, Value&, ExprTree* ) const;
		bool flattenSpecials( EvalState &, Value &, ExprTree *& ) const;

		ExprTree	*child1;
		ExprTree	*child2;
		ExprTree	*child3;
};

} // classad

#endif//__CLASSAD_OPERATORS_H__
