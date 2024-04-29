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


#ifndef __CLASSAD_EXPR_TREE_H__
#define __CLASSAD_EXPR_TREE_H__

#include <vector>
#include "classad/classad_containers.h"
#include "classad/common.h"
#include "classad/value.h"

namespace classad {


// forward declarations
class ExprTree;
class ClassAd;
class MatchClassAd;


class EvalState {
	public:
		const int MAX_CLASSAD_RECURSION = 400;

		EvalState ()
			: depth_remaining(MAX_CLASSAD_RECURSION)
			, rootAd(nullptr)
			, curAd(nullptr)
			, flattenAndInline(false)
			, debug(false)
			, inAttrRefScope(false)
		{}

		~EvalState( );

		void SetRootScope( );
		void SetScopes( const ClassAd* );

		int depth_remaining; // max recursion depth - current depth

		// Normally, rootAd will be the ClassAd at the root of the tree
		// of ExprTrees in the current evaluation. That is, the parent
		// scope whose parent scope is NULL.
		// It can be set to a closer parent scope. Then that ClassAd is
		// treated like it has no parent scope for LookupInScope() and
		// Evaluate().
		const ClassAd *rootAd;
		const ClassAd *curAd;

		bool		flattenAndInline;	// NAC
		bool		debug;
		bool		inAttrRefScope;

		// Cache_to_free are the things in the cache that must be
		// freed when this gets deleted.
		std::vector<ExprTree*> cache_to_delete;

		// add an exprtree to the evaluation cache so that it is deleted after evaluation is complete
		void AddToDeletionCache(ExprTree * tree);

		// check to see if the pointer is in the cache, if so remove it and return true.
		// otherwise return false
		bool TakeFromDeletionCache(ExprTree * tree);

		bool GivePtrToValue(Value & val);
};

/** A node of the expression tree, which may be a literal, attribute reference,
	function call, classad, expression list, or an operator applied to other
	ExprTree operands.
	@see NodeKind
*/
class ExprTree 
{
  	public:
			/// The kinds of nodes in expression trees
		enum NodeKind {
			/// Attribute reference node (attr, .attr, expr.attr) 
			ATTRREF_NODE,
			/// Expression operation node (unary, binary, ternary)/
			OP_NODE,
			/// Function call node 
	   		FN_CALL_NODE,
			/// ClassAd node 
			CLASSAD_NODE,
			/// Expression list node 
			EXPR_LIST_NODE,
			/// Expression envelope.
			EXPR_ENVELOPE,
			ERROR_LITERAL,
			UNDEFINED_LITERAL,
			BOOLEAN_LITERAL,
			INTEGER_LITERAL,
			REAL_LITERAL,
			RELTIME_LITERAL,
			ABSTIME_LITERAL,
			STRING_LITERAL
		};

		/// Virtual destructor
		virtual ~ExprTree () {};

		/** Sets the lexical parent scope of the expression, which is used to 
				determine the lexical scoping structure for resolving attribute
				references. (However, the semantic parent may be different from 
				the lexical parent if a <tt>super</tt> attribute is specified.) 
				This method is automatically called when expressions are 
				inserted into ClassAds, and should thus be called explicitly 
				only when evaluating expressions which haven't been inserted 
				into a ClassAd.
			@param p The parent ClassAd.
		*/
		void SetParentScope( const ClassAd* p );

		/** Gets the parent scope of the expression.
			Some expressions (e.g. literals) don't know their parent
			scope and always return NULL.
		 	@return The parent scope of the expression.
		*/
		virtual const ClassAd *GetParentScope( ) const = 0;

		/** Makes a deep copy of the expression tree
		 * 	@return A deep copy of the expression, or NULL on failure.
		*/
		virtual ExprTree *Copy( ) const = 0;

		/** Gets the node kind of this expression node.
			@return The node kind. Child nodes MUST implement this.
			@see NodeKind
		*/
		virtual NodeKind GetKind (void) const = 0;
		
		/**
		 * To eliminate the mass of external checks to see if the ExprTree is 
		 * a classad.
		 */ 
		virtual bool isClassad(ClassAd ** ptr=0) const;
		
		/**
		 * Return a ptr to the raw exprtree below the interface
		 */
		SAL_Ret_notnull
		virtual const ExprTree* self() const;

		/* This version is for shared-library compatibility.
		 * Remove it the next time we have to bump the ClassAds SO version.
		 */
		SAL_Ret_notnull
		virtual const ExprTree* self();

		/// A debugging method; send expression to stdout
		void Puke( ) const;

        /** Evaluate this tree
         *  @param state The current state
         *  @param val   The result of the evaluation
         *  @return true on success, false on failure
         */
		bool Evaluate( EvalState &state, Value &val ) const; 
		
        /** Evaluate this tree.
         *  This only works if the expression is currently part of a ClassAd.
         *  @param val   The result of the evaluation
         *  @return true on success, false on failure
         */
		bool Evaluate( Value& v ) const;

        /** Is this ExprTree the same as the tree?
         *  @return true if it is the same, false otherwise
         */
        virtual bool SameAs(const ExprTree *tree) const = 0;

		// Pass in a pointer to a function taking a const char *, which will
		// print it out somewhere useful, when the classad debug() function
		// is called

		static void set_user_debug_function(void (*dbf)(const char *));

  	protected:
		void debug_print(const char *message) const;
		void debug_format_value(Value &value, double time=0) const;
		ExprTree () {};

		/** Fill in this ExprTree with the contents of the other ExprTree.
		*  @return true if the copy succeeded, false otherwise.
		*/
		void CopyFrom(const ExprTree &) { }

		bool Evaluate( Value& v, ExprTree*& t ) const;
		bool Flatten( Value& val, ExprTree*& tree) const;

		bool Flatten( EvalState&, Value&, ExprTree*&, int* = NULL ) const;
		bool Evaluate( EvalState &, Value &, ExprTree *& ) const;

		enum {
			EVAL_FAIL,
			EVAL_OK,
			EVAL_UNDEF,
			EVAL_ERROR
		};


  	private: 
		friend class Operation;
		friend class Operation1;
		friend class Operation2;
		friend class Operation3;
		friend class OperationParens;
		friend class AttributeReference;
		friend class FunctionCall;
		friend class FunctionTable;
		friend class ExprList;
		friend class ExprListIterator;
		friend class ClassAd;
		friend class CachedExprEnvelope;
		friend class Literal;

		/// Copy constructor
        ExprTree(const ExprTree &tree);
        ExprTree &operator=(const ExprTree &literal);

		virtual void _SetParentScope( const ClassAd* )=0;
		virtual bool _Evaluate( EvalState&, Value& ) const=0;
		virtual bool _Evaluate( EvalState&, Value&, ExprTree*& ) const=0;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* )const=0;

        friend bool operator==(const ExprTree &tree1, const ExprTree &tree2);

		// To avoid making classads depend on a condor debug function,
		// have the user set a function to call to debug classads
		static void (*user_debug_function)(const char *);
};

} // classad

#include "classad/literals.h"
#include "classad/attrrefs.h"
#include "classad/operators.h"
#include "classad/exprList.h"
#include "classad/classad.h"
#include "classad/fnCall.h"

#endif//__CLASSAD_EXPR_TREE_H__
