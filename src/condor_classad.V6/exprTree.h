#ifndef __EXPR_TREE_H__
#define __EXPR_TREE_H__

#include "common.h"
#include "values.h"

// forward declarations
class ClassAd;
class Source;
class Sink;

// structures pertaining to the classad domain
#include "domain.h"

// kinds of nodes in the expression tree
enum NodeKind 
{
	LITERAL_NODE,
	ATTRREF_NODE,
	OP_NODE,
	FN_CALL_NODE,
	CLASSAD_NODE,
	EXPR_LIST_NODE
};


// Required info for handling scopes during eval'n
struct EvalState {
	ClassAd	*rootAd;
	ClassAd *curAd;
};


// abstract base class for expression tree node class
class ExprTree 
{
  	public:
		// dtor
		virtual ~ExprTree ();

		// factory method to parse expressions
		static ExprTree *fromSource (Source &s);
		
		// external interface operations defined on expressions
		virtual bool toSink (Sink &)  = 0;
		virtual ExprTree *copy (CopyMode = EXPR_DEEP_COPY) = 0;

		// general purpose methods
		NodeKind getKind (void) { return nodeKind; }

		bool flatten( Value&, ExprTree*& );
		void evaluate( Value& );

  	protected:
		ExprTree ();

		bool flatten( EvalState&, EvalValue&, ExprTree*&, OpKind* = NULL );
		void evaluate( EvalState &, EvalValue & ); 
		virtual void setParentScope( ClassAd* ) = 0;

		NodeKind	nodeKind;
		int			refCount;

  	private:
		friend class Operation;
		friend class AttributeReference;
		friend class FunctionCall;
		friend class FunctionTable;
		friend class ExprList;
		friend class ClassAd; 

		virtual void _evaluate (EvalState &, EvalValue &) = 0;
		virtual bool _flatten( EvalState&, EvalValue&, ExprTree*&, OpKind* )=0;

		bool evalFlag;	// for cycle detection
		bool flattenFlag;
};


#include "literals.h"
#include "attrrefs.h"
#include "operators.h"
#include "fnCall.h"
#include "exprList.h"
#include "classad.h"

#endif//__EXPR_TREE_H__
