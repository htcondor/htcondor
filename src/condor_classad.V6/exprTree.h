#ifndef __EXPR_TREE_H__
#define __EXPR_TREE_H__

#include "condor_common.h"
#include "values.h"

// forward declarations
class ClassAd;
class EvalState;
class Source;
class Sink;

// structures pertaining to the classad domain
#include "domain.h"

// kinds of nodes in the expression tree
enum NodeKind 
{
	CONSTANT_NODE,
	ATTRREF_NODE,
	OP_NODE,
	FN_CALL_NODE,
	ARGUMENT_LIST_NODE
};


// upto 'NumLayers' evaluations can be performed concurrently in a closure
typedef unsigned char Layer;
const   int NumLayers = 1;


// abstract base class for expression tree node class
class ExprTree 
{
  	public:
		// dtor
		virtual ~ExprTree () {};

		// factory method to parse expressions
		static ExprTree *fromSource (Source &s);
		
		// operations defined on expressions
		virtual bool toSink (Sink &)  = 0;
		virtual ExprTree *copy (void) = 0;

		// general purpose methods
		NodeKind getKind (void) { return nodeKind; }

  	protected:
		ExprTree ();
		void evaluate (EvalState &, Value &); 

		NodeKind	nodeKind;

  	private:
		friend class EvalContext;
		friend class Operation;
		friend class AttributeReference;
		friend class ArgumentList;
		friend class ExprList;

		virtual void _evaluate (EvalState &, Value &) = 0;

		bool evalFlags[NumLayers];	// for cycle detection
};


#include "constants.h"
#include "attrrefs.h"
#include "operators.h"
#include "fnCall.h"
#include "exprList.h"

#endif//__EXPR_TREE_H__
