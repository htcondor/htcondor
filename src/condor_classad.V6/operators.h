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
		virtual bool _flatten( EvalState&, EvalValue&, ExprTree*&, OpKind* );

		// auxillary functionms
		bool combine( OpKind&, EvalValue&, ExprTree*&, 
				OpKind, EvalValue&, ExprTree*, OpKind, EvalValue&, ExprTree* );
		bool flattenSpecials( EvalState &, EvalValue &, ExprTree *& );

		static Operation* makeOperation( OpKind, EvalValue&, ExprTree* );
		static Operation* makeOperation( OpKind, ExprTree*, EvalValue& );
		static Operation* makeOperation( OpKind, ExprTree*, ExprTree* );
		static Operation* makeOperation( OpKind,ExprTree*,ExprTree*,ExprTree* );
		static Operation* makeOperation( OpKind, ExprTree* );
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
