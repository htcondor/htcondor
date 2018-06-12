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


#ifndef __CLASSAD_EXPR_LIST_H__
#define __CLASSAD_EXPR_LIST_H__

#include <vector>

namespace classad {

class ExprListIterator;

/// Represents a list of expressions, like {1, 2, 3}
class ExprList : public ExprTree
{
	public:
		ExprList();
		ExprList(const std::vector<ExprTree*>& exprs);

        /// Copy Constructor
        ExprList(const ExprList &other_list);

		/// Destructor
		virtual ~ExprList();

        ExprList &operator=(const ExprList &other_list);

		/// node type
		virtual NodeKind GetKind (void) const { return EXPR_LIST_NODE; }

		/** Factory to make an expression list expression
		 * 	@param	list A vector of the expressions to be contained in the
		 * 		list
		 * 	@return The constructed expression list
		 */
		static ExprList *MakeExprList( const std::vector< ExprTree* > &list );

		/** Deconstructor to obtain the components of an expression list
		 * 	@param list The list of expressions
		 */
		void GetComponents( std::vector<ExprTree*>& list) const;

		virtual ExprTree* Copy( ) const;

        bool CopyFrom(const ExprList &other_list);

        virtual bool SameAs(const ExprTree *tree) const;

        friend bool operator==(ExprList &list1, ExprList &list2);

		// STL-like iterators and functions
	    typedef std::vector<ExprTree*>::iterator       iterator;
    	typedef std::vector<ExprTree*>::const_iterator const_iterator;

        int size() const             { return (int)exprList.size();  }
        iterator begin()             { return exprList.begin(); }
		iterator end()               { return exprList.end();   }
		const_iterator begin() const { return exprList.begin(); }
		const_iterator end() const   { return exprList.end();   }

		void insert(iterator it, ExprTree* t);
		void push_back(ExprTree* t);
		void erase(iterator it);
		void erase(iterator f, iterator l);
	
		virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

	private:
		friend class ExprListIterator;

		const ClassAd   *parentScope;

		std::vector<ExprTree*> exprList;

		void Clear (void);
		virtual void _SetParentScope( const ClassAd* p );
		virtual bool _Evaluate (EvalState &, Value &) const;
		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
        void CopyList(const std::vector<ExprTree*> &exprs);
};


/// An iterator for an ExprList--deprecated: you should use the STL-like iterators now
class ExprListIterator
{
	public:
		/// Constructor
		ExprListIterator( );

		/** Constructor which initializes the iterator.
			@param l The list to be iterated over (i.e., the iteratee).
			@see initialize
		*/
		ExprListIterator( const ExprList* l );

		/// Destructor
		~ExprListIterator( );

        /** Initializes the object to iterate over an expression list; the 
				iterator begins at the "before first" position.  This method 
				must be called before the iterator is usable.  (The iteration 
				methods return false if the iterator has not been initialized.)
				This method may be called any number of times, with different 
                expression lists as arguments.
			@param l The expression list to iterate over (i.e., the iteratee).
		*/
	    void Initialize( const ExprList* l );

		/// Positions the iterator to the first element.
    	void ToFirst( );

		/// Positions the iterator to the "after last" position
    	void ToAfterLast( );

		/** Positions the iterator at the n'th expression of the list (assuming 
				0-based index.  
			@param n The index of the expression to retrieve.
			@return true if the iterator was successfully positioned at the
				n'th element, and false otherwise.
		*/
		bool ToNth( int n );

		/** Gets the next expression in the list.
			@return The next expression in the list, or NULL if the iterator
				has crossed the last expression in the list.
		*/
    	const ExprTree* NextExpr( );

		/** Gets the expression currently pointed to.
			@return The expression currently pointed to.
		*/
    	const ExprTree* CurrentExpr( ) const;

		/** Gets the previous expression in the list.
			@return The previous expression in the list, or NULL if the iterator
				has crossed the first expression in the list.
		*/
    	const ExprTree* PrevExpr( );
    
		/** Gets the value of the next expression in the list.
			@param v The value of the expression.
			@param es The EvalState object which caches values of expressions.
				Ordinarily, this parameter will not be supplied by the user, and
				an internal EvalState object will be used. 
			@return false if the iterator has crossed the last expression in the
				list, or true otherwise.
		*/
    	bool NextValue( Value& v, EvalState *es=NULL );

		/** Gets the value of the expression currently pointed to.
			@param v The value of the expression.
			@param es The EvalState object which caches values of expressions.
				Ordinarily, this parameter will not be supplied by the user, and
				an internal EvalState object will be used. 
			@return true if the operation succeeded, false otherwise.
		*/
    	bool CurrentValue( Value& v, EvalState *es=NULL );

		/** Gets the value of the previous expression in the list.
			@param v The value of the expression.
			@param es The EvalState object which caches values of expressions.
				Ordinarily, this parameter will not be supplied by the user, and
				an internal EvalState object will be used. 
			@return false if the iterator has crossed the first expression in 
				the list, or true otherwise.
		*/
    	bool PrevValue( Value& v, EvalState *es=NULL  );

		/** Gets the value of the next expression in the list, and identifies 
				sub-expressions that caused the value.
			@param v The value of the expression.
			@param t The expression composed of the significant sub-expressions.
			@param es The EvalState object which caches values of expressions.
				Ordinarily, this parameter will not be supplied by the user, and
				an internal EvalState object will be used. 
			@return false if the iterator has crossed the last expression in the
				list, or true otherwise.
		*/
    	bool NextValue( Value& v, ExprTree*& t, EvalState *es=NULL );

		/** Gets the value of the expression currently pointed to, and 
				identifies sub-expressions that caused the value.
			@param v The value of the expression.
			@param t The expression composed of the significant sub-expressions.
			@param es The EvalState object which caches values of expressions.
				Ordinarily, this parameter will not be supplied by the user, and
				an internal EvalState object will be used. 
			@return true if the operation succeeded, false otherwise.
		*/
    	bool CurrentValue( Value& v, ExprTree*& t, EvalState *es=NULL );

		/** Gets the value of the previous expression in the list, and 
				identifies sub-expressions that caused that value.
			@param v The value of the expression.
			@param t The expression composed of the significant sub-expressions.
			@param es The EvalState object which caches values of expressions.
				Ordinarily, this parameter will not be supplied by the user, and
				an internal EvalState object will be used. 
			@return false if the iterator has crossed the first expression in 
				the list, or true otherwise.
		*/
    	bool PrevValue( Value& v, ExprTree*& t, EvalState *es=NULL  );

		/** Predicate to check the position of the iterator.
			@return true iff the iterator is before the first element.
		*/
    	bool IsAtFirst( ) const;

		/** Predicate to check the position of the iterator.
			@return true iff the iterator is after the last element.
		*/
    	bool IsAfterLast( ) const;

	private:
		const ExprList                          *l;
		EvalState                               state;
		std::vector<ExprTree*>::const_iterator  itr;

    	bool GetValue( Value& v, const ExprTree *tree, EvalState *es=NULL );
		bool GetValue( Value& v, ExprTree*& t, const ExprTree *tree, EvalState *es=NULL );

};	


} // classad

#endif//__CLASSAD_EXPR_LIST_H__
