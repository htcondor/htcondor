#ifndef __OPERATORS_H__
#define __OPERATORS_H__

#include "exprTree.h"

class Sink;

class Operation : public ExprTree
{
  	public:
		// ctors/dtor
		Operation ();
		~Operation ();

		// override methods
		virtual bool toSink (Sink &);
		virtual void setParentScope( ClassAd* );

		// specific methods (allow for unary, binary and ternary ops) 
		void setOperation (OpKind, ExprTree* = NULL, ExprTree* = NULL,
				ExprTree* = NULL);

		// table of string representations of operators; indexed by OpKind
		static char *opString[];

		// public access to operation function
		static void operate (OpKind, Value &, Value &, Value &);
		static void operate (OpKind, Value &, Value &, Value &, Value &);
		static bool isStrictOperator( OpKind );

  	private:
		virtual ExprTree* _copy( CopyMode );
		virtual void _evaluate( EvalState &, Value &);
		virtual void _evaluate( EvalState &, Value &, ExprTree*& );
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );

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

		enum SigValues { SIG_NONE=0 , SIG_LEFT=1 , SIG_MIDDLE=2 , SIG_RIGHT=4 };

		static int _doOperation(OpKind,Value&,Value&,Value&,
								bool,bool,bool, Value&, EvalState* = NULL);
		static int doComparison(OpKind, Value&, Value&, Value&);
		static int doArithmetic(OpKind, Value&, Value&, Value&);
		static int doLogical 	(OpKind, Value&, Value&, Value&);
		static int doBitwise 	(OpKind, Value&, Value&, Value&); 
		static int doRealArithmetic(OpKind,Value&, Value&, Value&);
		static void compareStrings(OpKind, Value&, Value&, Value&);
		static void compareReals(OpKind, Value&, Value&, Value&);
		static void compareIntegers(OpKind, Value&, Value&, Value&);

		// operation specific information
		OpKind		operation;
		ExprTree 	*child1;
		ExprTree	*child2;
		ExprTree	*child3;
};


#endif//__OPERATORS_H__
