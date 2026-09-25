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
#include <utility>
#include <unordered_map>
#include "classad/classad_containers.h"
#include "classad/common.h"
#include "classad/value.h"

namespace classad {


// forward declarations
class ExprTree;
class ClassAd;
class MatchClassAd;


// Map from a ClassAd to its parent (enclosing) scope, built up during
// evaluation. Lexical scope chains are short -- typically 2-3 deep, even in a
// match -- so a flat vector with linear search beats a hashed map: it avoids
// the per-EvalState heap allocation of hash buckets and the hashing itself,
// both of which showed up in profiles of matchmaking. This exposes just the
// slice of the std::unordered_map interface the evaluator uses
// (find/end/operator[]/copy), so it is a drop-in for the call sites.
class ScopeParentMap {
	public:
		using value_type = std::pair<const ClassAd*, const ClassAd*>;
		using iterator = std::vector<value_type>::iterator;
		using const_iterator = std::vector<value_type>::const_iterator;

		iterator begin() { return m_vec.begin(); }
		iterator end()   { return m_vec.end(); }
		const_iterator begin() const { return m_vec.begin(); }
		const_iterator end()   const { return m_vec.end(); }

		iterator find(const ClassAd* key) {
			for (iterator it = m_vec.begin(); it != m_vec.end(); ++it) {
				if (it->first == key) { return it; }
			}
			return m_vec.end();
		}
		const_iterator find(const ClassAd* key) const {
			for (const_iterator it = m_vec.begin(); it != m_vec.end(); ++it) {
				if (it->first == key) { return it; }
			}
			return m_vec.end();
		}

		// insert-or-assign, matching std::unordered_map::operator[]
		const ClassAd*& operator[](const ClassAd* key) {
			for (value_type& p : m_vec) {
				if (p.first == key) { return p.second; }
			}
			m_vec.emplace_back(key, nullptr);
			return m_vec.back().second;
		}

	private:
		std::vector<value_type> m_vec;
};


class EvalState {
	public:
		const int MAX_CLASSAD_RECURSION = 400;

		// Total number of node evaluations allowed for a single top-level
		// evaluation. Unlike depth_remaining (which is restored on unwind and
		// therefore only bounds stack depth), this is decremented once per
		// node evaluation and never restored, so it bounds the *cumulative*
		// work. This stops exponential re-evaluation of shared sub-expressions
		// (e.g. A=B+B; B=C+C; ...), which is shallow but does 2^N work and
		// would otherwise run effectively unbounded. See ExprTree::Evaluate.
		static constexpr long MAX_CLASSAD_EVAL_STEPS = 10'000'000;

		EvalState ()
			: depth_remaining(MAX_CLASSAD_RECURSION)
			, eval_steps_remaining(MAX_CLASSAD_EVAL_STEPS)
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
		long eval_steps_remaining; // total node-evaluation budget (not restored)

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

		// Map from ClassAd* to its parent scope during evaluation.
		// Populated dynamically as evaluation descends into nested ClassAds.
		ScopeParentMap parentMap;

		// Cache_to_free are the things in the cache that must be
		// freed when this gets deleted.
		std::vector<ExprTree*> cache_to_delete;

		// Attribute-reference target expressions currently being evaluated,
		// used to detect circular references (e.g. a self-referential ad like
		// X = X + 1). Without this, a cycle reachable through more than one
		// child of an operator causes exponential re-evaluation that is only
		// bounded by depth_remaining -- effectively a hang. See
		// AttributeReference::_Evaluate.
		std::vector<const ExprTree*> eval_stack;

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

		/** Sets the lexical parent scope of the expression.
		    This is now a no-op; parent scope is tracked dynamically
		    in EvalState::parentMap during evaluation.
			@param p The parent ClassAd (ignored).
		*/
		void SetParentScope( const ClassAd* p );

		/** Gets the parent scope of the expression.
		    Always returns NULL; parent scope is tracked dynamically
		    in EvalState::parentMap during evaluation.
		 	@return Always NULL.
		*/
		const ClassAd *GetParentScope( ) const { return nullptr; }

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

		/** Evaluate this tree with no enclosing scope.
		 *  Only valid for self-contained (top-level) expressions; an
		 *  expression that references attributes of a containing ClassAd
		 *  will evaluate to UNDEFINED. Use ClassAd::EvaluateExpr() to
		 *  evaluate in the scope of an ad.
		 *  @param v   The result of the evaluation
		 *  @return true on success, false on failure
		 */
		bool Evaluate( Value& v ) const;

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
