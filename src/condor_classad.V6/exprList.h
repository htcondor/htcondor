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

#ifndef __EXPR_LIST_H__
#define __EXPR_LIST_H__

#include <vector>

BEGIN_NAMESPACE( classad );

class ExprListIterator;

/** 
	Expression node which represents a list of expressions 
*/
class ExprList : public ExprTree
{
	public:
		/// Destructor
		~ExprList();

		/** Factory to make an expression list expression
		 * 	@param	list A vector of the expressions to be contained in the
		 * 		list
		 * 	@return The constructed expression list
		 */
		static ExprList *MakeExprList( const vector< ExprTree* > &list );

		/** Deconstructor to obtain the components of an expression list
		 * 	@param list The list of expressions
		 */
		void GetComponents( vector<ExprTree*>& list) const;

		/// Make a deep copy of the expression
		virtual ExprList* Copy( ) const;

	protected:
		/// Constructor
		ExprList();

	private:
		friend class ExprListIterator;

		vector<ExprTree*> exprList;

		void Clear (void);
		virtual void _SetParentScope( const ClassAd* p );
		virtual bool _Evaluate (EvalState &, Value &) const;
		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
};


/** 
	Expression List iterator
*/
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
		const ExprList				*l;
		EvalState					state;
		vector<ExprTree*>::const_iterator itr;

    	bool GetValue( Value& v, const ExprTree *tree, EvalState *es=NULL );
		bool GetValue( Value& v, ExprTree*& t, const ExprTree *tree, EvalState *es=NULL );

};	


END_NAMESPACE // classad

#endif//__EXPR_LIST_H__
