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

#ifndef __CLASSAD_H__
#define __CLASSAD_H__

#include "exprTree.h"
#include "extArray.h"
#include "stringSpace.h"
#include "sink.h"
#include "source.h"


// each attribute in the classad
class Attribute
{
	public:
		Attribute ();

		bool		valid;
		const char 	*attrName;
		ExprTree	*expression;
		SSString	canonicalAttrName;
};


/// An internal node of an expression which represents a ClassAd. 
class ClassAd : public ExprTree
{
  	public:
		/// Default constructor 
		ClassAd ();
		/** Instantiates ClassAd in a specified domain.  This feature is
			experimental; use of domains is discouraged.
		 	@param d The domain that the ClassAd belongs to.
		*/
		ClassAd (char *d);			// instantiate in domain
		/** Copy constructor */
		ClassAd (const ClassAd &);
		/** Destructor */
		~ClassAd ();

		// override methods
		/** Sends the ClassAd object to a sink.
			@param s The sink object.
			@return false if the ClassAd could not be successfully unparsed
				into the sink.
			@see Sink
		*/
		virtual bool ToSink( Sink &s );	

		/** Update a class-ad with another ClassAd. The attributes from the specified
            ClassAd are inserted into this ClassAd, overwriting any existing attributes
            with the same name.
			@param ad the class-ad that represents the update.
			@return false if the ClassAd could not be successfully updated.
		*/
		virtual void Update( ClassAd& ad );	

		// Accessors/modifiers
		/** Clears the ClassAd of all attributes.
		*/
		void Clear( );

		/** Inserts an attribute into the ClassAd.  The setParentScope() method
				is invoked on the inserted expression.
			@param attrName The name of the attribute.  This string is
				duplicated internally.
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool Insert( const char *attrName, ExprTree *expr );

		/** Inserts an attribute into the ClassAd.  The contents of the buffer 
				is parsed into an expression and then inserted into the 
				ClassAd. The setParentScope() method is invoked on the 
				inserted expression.
			@param attrName The name of the attribute.  This string is
				duplicated internally.
			@param buf The character representation of the expression.
			@param len The length of the buffer.  If this paramter is not
				supplied, the length is inferred from the string through
				strlen().
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool Insert( const char *attrName, const char *buf, int len=-1 );

		/** Inserts an attribute into the ClassAd.  The integer value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.  This string is
				duplicated internally.
			@param value The integer value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see NumberFactor
		*/
		bool InsertAttr( const char *attrName,int value, 
			NumberFactor f=NO_FACTOR );

		/** Inserts an attribute into the ClassAd.  The real value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.  This string is
				duplicated internally.
			@param value The real value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see NumberFactor
		*/
		bool InsertAttr( const char *attrName,double value, 
			NumberFactor f=NO_FACTOR);

		/** Inserts an attribute into the ClassAd.  The boolean value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.  This string is
				duplicated internally.
			@param value The boolean value of the attribute.
		*/
		bool InsertAttr( const char *attrName, bool value );

		/** Inserts an attribute into the ClassAd.  The string value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.  This string is
				always duplicated internally.
			@param value The string attribute
			@param dup If dup is true, the value is duplicated internally.
				Otherwise, the string is assumed to have been created with new[]
				and the classad assumes responsibility for freeing the storage.
		*/
		bool InsertAttr( const char *attrName, const char *value, 
				bool dup=true );

		/** Finds the expression bound to an attribute name.  The lookup only
				involves this ClassAd; scoping information is ignored.
			@param attrName The name of the attribute.  
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *Lookup( const char *attrName );

		/** Finds the expression bound to an attribute name.  The lookup uses
				the scoping structure (including {\tt super} attributes) to 
				determine the expression bound to the given attribute name in
				the closest enclosing scope.  The closest enclosing scope is
				also returned.
			@param attrName The name of the attribute.
			@param ad The closest enclosing scope of the returned expression,
				or NULL if no expression was found.
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *LookupInScope( const char *attrName, ClassAd *&ad );

		/** Deletes the named attribute from the ClassAd.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool Delete( const char *attrName );
	
		/** Similar to Delete, but the expression is returned rather than 
		  		deleted from the classad.
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
		*/
		ExprTree *Remove( const char* attrName );

		// evaluation methods
		/** Evaluates expression bound to an attribute.
			@param attrName The name of the attribute in the ClassAd.
			@param result The result of the evaluation.
		*/
		bool EvaluateAttr( const char* attrName, Value &result );

		/** Evaluates an expression.
			@param buf Buffer containing the external representation of the
				expression.  This buffer is parsed to yield the expression to
				be evaluated.
			@param result The result of the evaluation.
			@param len The length of the buffer.  If this parameter is not
				specified, it is inferred via a strlen(buf).
			@return true if the operation succeeded, false otherwise.
		*/
		bool EvaluateExpr( const char* buf, Value &result, int len=-1);

		/** Evaluates an expression.  If the expression doesn't already live in
				this ClassAd, the setParentScope() method must be called
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
		*/
		bool EvaluateExpr( ExprTree* expr, Value &result );		// eval'n

		/** Evaluates an expression, and returns the significant subexpressions
				encountered during the evaluation.  If the expression doesn't 
				already live in this ClassAd, call the setParentScope() method 
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
			@param sig The significant subexpressions of the evaluation.
		*/
		bool EvaluateExpr( ExprTree* expr, Value &result, ExprTree *&sig );

		/** Evaluates an attribute to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool EvaluateAttrInt( const char* attrName, int& intValue );

		/** Evaluates an attribute to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a real, false otherwise.
		*/
		bool EvaluateAttrReal( const char* attrName, double& realValue );

		/** Evaluates an attribute to an integer. If the attribute evaluated to 
				a real, it is truncated to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an number, false otherwise.
		*/
		bool EvaluateAttrNumber( const char* attrName, int& intValue );

		/** Evaluates an attribute to a real.  If the attribute evaluated to an 
				integer, it is promoted to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a number, false otherwise.
		*/
		bool EvaluateAttrNumber( const char* attrName, double& realValue );

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attrName The name of the attribute.
			@param buf The buffer for the string value.
			@param len The size of the buffer.
			@param alen The actual length of the string value.  Undefined if the
				value was not a string.
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const char* attrName, char* buf, int len, 
				int &alen );

		/** Evaluates an attribute to a string.  A pointer to the string is 
				returned.  This memory is NOT to be deallocated by the user.
			@param attrName The name of the attribute.
			@param strValue The value of the attribute.
			@return true if attrName evaluated to a string, false otherwise.
		*/
		bool EvaluateAttrString( const char* attrName, char*& strValue );

		/** Evaluates an attribute to a boolean.  A pointer to the string is 
				returned.
			@param attrName The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
		bool EvaluateAttrBool( const char* attrName, bool& boolValue );

		// flattening method
		/** Flattens the Classad.
			@param expr The expression to be flattened.
			@param val The value after flattening, if the expression was 
				completely flattened.  This value is valid if and only if	
				fexpr is NULL.
			@param fexpr The flattened expression tree if the expression did
				not flatten to a single value, and NULL otherwise.
			@return true if the flattening was successful, and false otherwise.
		*/
		bool Flatten( ExprTree* expr, Value& val, ExprTree *&fexpr );
		
		// factory methods to procure classads
		/** Factory method to create a ClassAd.
			@param s The source object.
			@return The ClassAd if the parse was successful, and NULL otherwise.
		*/
		static ClassAd *FromSource (Source &s);

		/** Factory method to augment a ClassAd with more expressions.  
				Expressions from the source overwrite similarly named 
				expressions in the ClassAd c.
			@param s The source object.
			@param c The ClassAd which is being augmented.
			@return The ClassAd if the parse was successful, and NULL otherwise.
		*/
		static ClassAd *AugmentFromSource( Source &s, ClassAd &c);

		virtual ClassAd* Copy( );

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class EvalState;
		friend 	class ClassAdIterator;

		virtual void _SetParentScope( ClassAd* p );
		virtual bool _Evaluate( EvalState& , Value& );
		virtual bool _Evaluate( EvalState&, Value&, ExprTree*& );
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, OpKind* );
	
		int LookupInScope( const char* , ExprTree*&, EvalState& );
		static	ClassAdDomainManager 	domMan;
		ExtArray<Attribute> 			attrList;
	
		StringSpace		*schema;
		int				last;
};
	

/** An object for iterating over the attributes of a ClassAd.  Several
	iterators may be active over the same ClassAd at any time, and the same
	iterator object may be used to iterate over other ClassAds as well.
*/
class ClassAdIterator
{
	public:
		/// Constructor
		ClassAdIterator () { ad = NULL ; index = -1; }

		/** Constructor which initializes the iterator to the given ClassAd.
			@param ca The ClassAd to iterate over.
			@see initialize
		*/
		ClassAdIterator (const ClassAd &ca) { ad = &ca; index = -1; }	

		/// Destructor
		~ClassAdIterator(){ }

        /** Initializes the object to iterate over a ClassAd; the iterator 
				begins at the "before first" position.  This method must be 
				called before the iterator is usable.  (The iteration methods 
				return false if the iterator has not been initialized.)  This 
				method may be called any number of times, with different 
				ClassAds as arguments.
            @param l The expression list to iterate over (i.e., the iteratee).
        */
		inline void Initialize (const ClassAd &ca) { ad = &ca ; index = -1; }

        /// Positions the iterator to the "before first" position.
		inline void ToBeforeFirst () { index = -1; }

        /// Positions the iterator to the "after last" position
		inline void ToAfterLast ()	{ index = ad->last; }

		/** Gets the next attribute in the ClassAd.
			@param attr The name of the next attribute in the ClassAd.
			@param expr The expression of the next attribute in the ClassAd.
            @return false if the iterator has crossed the last attribute in the 
				ClassAd, or true otherwise.
        */
		bool NextAttribute( const char*& attr, ExprTree*& expr );

		/** Gets the attribute currently referred to by the iterator.
            @param attr The name of the next attribute in the ClassAd.
            @param expr The expression of the next attribute in the ClassAd.
            @return false if the operation failed, true otherwise.
		*/
		bool CurrentAttribute( const char*& attr, ExprTree*& expr );

		/** Gets the previous attribute in the ClassAd.
			@param attr The name of the previous attribute in the ClassAd.
			@param expr The expression of the previous attribute in the ClassAd.
            @return false if the iterator has crossed the first attribute in 
				the ClassAd, or true otherwise.
        */
		bool PreviousAttribute( const char *&attr, ExprTree *&expr );

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is before the first element.
        */
		inline bool IsBeforeFirst () { return (index == -1); }

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is after the last element.
        */
		inline bool IsAfterLast () { return (ad ? (index==ad->last) : false); }

	private:
		int		index;
		const ClassAd	*ad;
};


#endif//__CLASSAD_H__
