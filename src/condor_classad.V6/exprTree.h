/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#ifndef __EXPR_TREE_H__
#define __EXPR_TREE_H__

#include "classad_stl.h"
#include "common.h"
#include "value.h"

BEGIN_NAMESPACE( classad )

// forward declarations
class ExprTree;
class ClassAd;
class MatchClassAd;

typedef classad_hash_map< const ExprTree*, Value, ExprHash > EvalCache;

class EvalState {
	public:
		EvalState( );
		~EvalState( );

		void SetRootScope( );
		void SetScopes( const ClassAd* );

		EvalCache	cache;
		ClassAd		*rootAd;
		ClassAd 	*curAd;

		bool		flattenAndInline;	// NAC

		// Cache_to_free are the things in the cache that must be
		// freed when this gets deleted. The problem is that we put
		// two kinds of things into the cache: some that must be
		// freed, and some that must not be freed. We keep track of
		// the ones that must be freed separately.  Memory managment
		// is a pain! We should all use languages that do memory
		// management for you.
		//EvalCache   cache_to_delete; 
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
	    	/// Literal node (string, integer, real, boolean, undefined, error)
	    	LITERAL_NODE,
			/// Attribute reference node (attr, .attr, expr.attr) 
			ATTRREF_NODE,
			/// Expression operation node (unary, binary, ternary)/
			OP_NODE,
			/// Function call node 
	   		FN_CALL_NODE,
			/// ClassAd node 
			CLASSAD_NODE,
			/// Expression list node 
			EXPR_LIST_NODE
		};

		/// Virtual destructor
		virtual ~ExprTree ();

		// Assignment operator
        virtual ExprTree& operator=(const ExprTree &tree);

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
		 	@return The parent scope of the expression.
		*/
		const ClassAd *GetParentScope( ) const { return( parentScope ); }

		/** Makes a deep copy of the expression tree
		 * 	@return A deep copy of the expression, or NULL on failure.
		*/
		virtual ExprTree *Copy( ) const = 0;

		/** Gets the node kind of this expression node.
			@return The node kind.
			@see NodeKind
		*/
		NodeKind GetKind (void) const { return nodeKind; }

		/// A debugging method; send expression to stdout
		void Puke( ) const;

		// ajr--made public on 4-nov-2002. Needed for user functions.
		bool Evaluate( EvalState &, Value & ) const; 
		
		// This only works if the exprtree is within a ClassAd.
		bool Evaluate( Value& v ) const;

  	protected:
		ExprTree ();

		bool Evaluate( Value& v, ExprTree*& t ) const;
		bool Flatten( Value& val, ExprTree*& tree) const;

		bool Flatten( EvalState&, Value&, ExprTree*&, int* = NULL ) const;
		bool Evaluate( EvalState &, Value &, ExprTree *& ) const;

		const ClassAd	*parentScope;
		NodeKind		nodeKind;

		enum {
			EVAL_FAIL,
			EVAL_OK,
			EVAL_UNDEF,
			PROP_UNDEF,
			EVAL_ERROR,
			PROP_ERROR
		};


  	private:
		friend class Operation;
		friend class AttributeReference;
		friend class FunctionCall;
		friend class FunctionTable;
		friend class ExprList;
		friend class ExprListIterator;
		friend class ClassAd; 

		/// Copy constructor
        ExprTree(const ExprTree &tree);

		virtual void _SetParentScope( const ClassAd* )=0;
		virtual bool _Evaluate( EvalState&, Value& ) const=0;
		virtual bool _Evaluate( EvalState&, Value&, ExprTree*& ) const=0;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* )const=0;
};

std::ostream& operator<<(std::ostream &os, const ExprTree &expr);

END_NAMESPACE // classad

#include "literals.h"
#include "attrrefs.h"
#include "operators.h"
#include "exprList.h"
#include "classad.h"
#include "fnCall.h"

#endif//__EXPR_TREE_H__
