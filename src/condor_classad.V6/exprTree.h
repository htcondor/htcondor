#ifndef __EXPR_TREE_H__
#define __EXPR_TREE_H__

#include "common.h"
#include "value.h"

// forward declarations
class ExprTree;
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

class EvalState {
	public:
		EvalState( );

		HashTable<ExprTree*,Value> cache;
		HashTable<ExprTree*, bool> superChase;
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

		// setter for default copy mode ( deep copy or ref count )
		static void setDefaultCopyMode( CopyMode );
		
		// external interface operations defined on expressions
		virtual void setParentScope( ClassAd* ) = 0;
		virtual bool toSink (Sink &)  = 0;
		ExprTree *copy( CopyMode = EXPR_DEFAULT_COPY );

		NodeKind getKind (void) { return nodeKind; }
		bool flatten( Value&, ExprTree*& );
		void evaluate( Value& );
		void evaluate( Value&, ExprTree*& );

  	protected:
		ExprTree ();

		bool flatten( EvalState&, Value&, ExprTree*&, OpKind* = NULL );
		void evaluate( EvalState &, Value & ); 
		void evaluate( EvalState &, Value &, ExprTree *& );

		static CopyMode	defaultCopyMode;
		NodeKind	nodeKind;
		int			refCount;

  	private:
		friend class Operation;
		friend class AttributeReference;
		friend class FunctionCall;
		friend class FunctionTable;
		friend class ExprList;
		friend class ClassAd; 

		virtual ExprTree *_copy( CopyMode = EXPR_DEEP_COPY )=0;
		virtual void _evaluate( EvalState&, Value& )=0;
		virtual void _evaluate( EvalState&, Value&, ExprTree*& )=0;
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* )=0;

		bool flattenFlag;
};


#include "literals.h"
#include "attrrefs.h"
#include "operators.h"
#include "fnCall.h"
#include "exprList.h"
#include "classad.h"

#endif//__EXPR_TREE_H__
