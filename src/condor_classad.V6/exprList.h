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

#include "list.h"

namespace classad {

/** 
	Expression node which represents a list of expressions 
*/
class ExprList : public ExprTree
{
	public:
		/// Constructor
		ExprList();

		/// Destructor
		~ExprList();

        /** Sends the expression list object to a Sink.
            @param s The Sink object.
            @return false if the expression list could not be successfully
                unparsed into the sink.
            @see Sink 
        */
		virtual bool ToSink( Sink &s );

		/** Appends an expression to the list
			@param expr The expression to be appended
		*/
		void AppendExpression( ExprTree *expr );

		/** Gets the number of expressions in the list
			@return The number of expressions in the list
		*/
		int  Number();

		/** Rewinds the internal iterator for the list
		*/
		void Rewind();

		/** Winds the internal iterator
			@return The next expression in the list, or NULL if the end of
				list has been reached.
		*/
		ExprTree* Next();

		virtual ExprList* Copy( );

	private:
		friend class ExprListIterator;

		List<ExprTree> exprList;

		void Clear (void);
		virtual void _SetParentScope( ClassAd* p );
		virtual bool _Evaluate (EvalState &, Value &);
		virtual bool _Evaluate (EvalState &, Value &, ExprTree *&);
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* );
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
		ExprListIterator( ExprList* l );

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

		/// Positions the iterator to the "before first" position.
    	void ToBeforeFirst( );

		/// Positions the iterator to the "after last" position
    	void ToAfterLast( );

		/** Positions the iterator at the n'th expression of the list (assuming 
				0-based index.  {\em {\bf WARNING:}  Do not use this method to 
				``sequentially search'' the list, or your algorithm will be an
				an n^2 search.  Use the next() and prev() methods instead.}
			@param n The index of the expression to retrieve.
			@return true if the iterator was successfully positioned at the
				n'th element, and false otherwise.
		*/
		bool ToNth( int n );

		/** Gets the next expression in the list.
			@return The next expression in the list, or NULL if the iterator
				has crossed the last expression in the list.
		*/
    	ExprTree* NextExpr( );

		/** Gets the expression currently pointed to.
			@return The expression currently pointed to.
		*/
    	ExprTree* CurrentExpr( );

		/** Gets the previous expression in the list.
			@return The previous expression in the list, or NULL if the iterator
				has crossed the first expression in the list.
		*/
    	ExprTree* PrevExpr( );
    
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
    	bool IsBeforeFirst( );

		/** Predicate to check the position of the iterator.
			@return true iff the iterator is after the last element.
		*/
    	bool IsAfterLast( );

	private:
		EvalState	state;
		ListIterator<ExprTree> iter;
};	

} // namespace classad

#endif//__EXPR_LIST_H__
