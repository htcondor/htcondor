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
		char 		*attrName;
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
		*/
		virtual bool toSink (Sink &s);	

		// Accessors/modifiers
		/** Clears the ClassAd of all attributes.
		*/
		void clear( );

		/** Inserts an attribute into the ClassAd.  The setParentScope() method
				is invoked on the inserted expression.
			@param attrName The name of the attribute.  This string is
				duplicated internally.
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool insert( char *attrName, ExprTree *expr );

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
		bool insert( char *attrName, char *buf, int len=-1 );

		/** Finds the expression bound to an attribute name.  The lookup only
				involves this ClassAd; scoping information is ignored.
			@param attrName The name of the attribute.  
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *lookup( char *attrName );

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
		ExprTree *lookupInScope( char *attrName, ClassAd *&ad );

		/** Removes the named attribute from the ClassAd.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.
			@param attrName The name of the attribute to be removed.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool remove( char *attrName );
	
		// evaluation methods
		/** Evaluates expression bound to an attribute.
			@param attrName The name of the attribute in the ClassAd.
			@param result The result of the evaluation.
		*/
		bool evaluateAttr( char* attrName , Value &result );// lookup+eval'n

		/** Evaluates an expression.
			@param buf Buffer containing the character representation of the
				expression.
			@param result The result of the evaluation.
			@param len The length of the buffer.  If this parameter is not
				specified, it is inferred via a strlen(buf).
			@return true if the operation succeeded, false otherwise.
		*/
		bool evaluateExpr( char* buf, Value &result, int len=-1);// parse+eval'n

		/** Evaluates an expression.  If the expression doesn't already live in
				this ClassAd, call the setParentScope() method on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
		*/
		bool evaluateExpr( ExprTree* expr, Value &result );		// eval'n

		/** Evaluates an expression, and returns the significant subexpressions
				encountered during the evaluation.  If the expression doesn't 
				already live in this ClassAd, call the setParentScope() method 
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
			@param sig The significant subexpressions of the evaluation.
		*/
		bool evaluateExpr( ExprTree* expr, Value &result, ExprTree *&sig );

		/** Evaluates an attribute to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool evaluateAttrInt( char* attrName, int& intValue );

		/** Evaluates an attribute to a real..
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to an real, false otherwise.
		*/
		bool evaluateAttrReal( char* attrName, double& realValue );

		/** Evaluates an attribute to an integer. If the attribute evaluated to 
				a real, it is truncated to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool evaluateAttrNumber( char* attrName, int& intValue );

		/** Evaluates an attribute to a real.  If the attribute evaluated to an 
				integer, it is promoted to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a real, false otherwise.
		*/
		bool evaluateAttrNumber( char* attrName, double& realValue );

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attrName The name of the attribute.
			@param buf The buffer for the string value.
			@param len The size of the buffer.
			@return true if attrName evaluated to a string which can be fit
				into the buffer, false otherwise.
		*/
		bool evaluateAttrString( char* attrName, char* buf, int len );

		/** Evaluates an attribute to a string.  A pointer to the string is 
				returned.  This memory is NOT to be deallocated by the user.
			@param attrName The name of the attribute.
			@param strValue The value of the attribute.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool evaluateAttrString( char* attrName, char*& strValue );

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
		bool flatten( ExprTree* expr, Value& val, ExprTree *&fexpr );
		
		// factory methods to procure classads
		/** Factory method to create a ClassAd.
			@param s The source object.
			@return The ClassAd if the parse was successful, and NULL otherwise.
		*/
		static ClassAd *fromSource (Source &s);

		/** Factory method to augment a ClassAd with more expressions.  
				Expressions from the source overwrite similarly named 
				expressions in the ClassAd c.
			@param s The source object.
			@param c The ClassAd which is being augmented.
			@return The ClassAd if the parse was successful, and NULL otherwise.
		*/
		static ClassAd *augmentFromSource( Source &s, ClassAd &c);

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class EvalState;
		friend 	class ClassAdIterator;

		virtual ExprTree* _copy( CopyMode );
		virtual void _setParentScope( ClassAd* p );
		virtual bool _evaluate( EvalState& , Value& );
		virtual bool _evaluate( EvalState&, Value&, ExprTree*& );
		virtual bool _flatten( EvalState&, Value&, ExprTree*&, OpKind* );
	
		int lookupInScope( char* , ExprTree*&, EvalState& );
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
		~ClassAdIterator();

        /** Initializes the object to iterate over a ClassAd; the iterator 
				begins at the "before first" position.  This method must be 
				called before the iterator is usable.  (The iteration methods 
				return false if the iterator has not been initialized.)  This 
				method may be called any number of times, with different 
				ClassAds as arguments.
            @param l The expression list to iterate over (i.e., the iteratee).
        */
		inline void initialize (const ClassAd &ca) { ad = &ca ; index = -1; }

        /// Positions the iterator to the "before first" position.
		inline void toBeforeFirst () { index = -1; }

        /// Positions the iterator to the "after last" position
		inline void toAfterLast ()	{ index = ad->last; }

		/** Gets the next attribute in the ClassAd.
			@param attr The name of the next attribute in the ClassAd.
			@param expr The expression of the next attribute in the ClassAd.
            @return false if the iterator has crossed the last attribute in the 
				ClassAd, or true otherwise.
        */
		bool nextAttribute( char*& attr, ExprTree*& expr );

		/** Gets the attribute currently referred to by the iterator.
            @param attr The name of the next attribute in the ClassAd.
            @param expr The expression of the next attribute in the ClassAd.
            @return false if the operation failed, true otherwise.
		*/
		bool currentAttribute( char*& attr, ExprTree*& expr );

		/** Gets the previous attribute in the ClassAd.
			@param attr The name of the previous attribute in the ClassAd.
			@param expr The expression of the previous attribute in the ClassAd.
            @return false if the iterator has crossed the first attribute in 
				the ClassAd, or true otherwise.
        */
		bool previousAttribute( char *&attr, ExprTree *&expr );

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is before the first element.
        */
		inline bool isBeforeFirst () { return (index == -1); }

        /** Predicate to check the position of the iterator.
            @return true iff the iterator is after the last element.
        */
		inline bool isAfterLast () { return (ad ? (index==ad->last) : false); }

	private:
		int		index;
		const ClassAd	*ad;
};


#endif//__CLASSAD_H__
