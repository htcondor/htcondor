/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __OPERATORS_H__
#define __OPERATORS_H__

#include "exprTree.h"

BEGIN_NAMESPACE( classad )

class Sink;

/** Represents a node of the expression tree which is an operation applied to
	expression operands
*/
class Operation : public ExprTree
{
  	public:
		/// Constructor
		Operation ();

		/// Destructor
		~Operation ();

        /** Sends the function call object to a Sink.
            @param s The Sink object.
            @return false if the expression could not be successfully
                unparsed into the sink.
            @see Sink
        */
		virtual bool ToSink( Sink &s );

		/** Sets the type and children of the expression node.
			@param kind The kind of operation.
			@param e1 The first sub-expression child of the node.
			@param e2 The second sub-expression child of the node (if any).
			@param e3 The third sub-expression child of the node (if any).
		*/
		void SetOperation (OpKind, ExprTree* = NULL, ExprTree* = NULL,
				ExprTree* = NULL);

		/// table of string representations of operators; indexed by OpKind
		static char *opString[];

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

		virtual Operation* Copy( );

  	private:
		virtual void _SetParentScope( ClassAd* );
		virtual bool _Evaluate( EvalState &, Value &);
		virtual bool _Evaluate( EvalState &, Value &, ExprTree*& );
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* );

		// auxillary functionms
		bool combine( OpKind&, Value&, ExprTree*&, 
				OpKind, Value&, ExprTree*, OpKind, Value&, ExprTree* );
		bool flattenSpecials( EvalState &, Value &, ExprTree *& );

		static Operation* makeOperation( OpKind, Value&, ExprTree* );
		static Operation* makeOperation( OpKind, ExprTree*, Value& );
		static Operation* makeOperation( OpKind, ExprTree*, ExprTree* );
		static Operation* makeOperation( OpKind,ExprTree*,ExprTree*,ExprTree* );
		static Operation* makeOperation( OpKind, ExprTree* );
		static ValueType  coerceToNumber (Value&, Value &);

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
