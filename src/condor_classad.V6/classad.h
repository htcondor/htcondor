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

#ifndef __CLASSAD_H__
#define __CLASSAD_H__


#include <set>
#include <map>
#include <vector>
#include "classad_stl.h"
#include "exprTree.h"

BEGIN_NAMESPACE( classad )

typedef std::set<std::string, CaseIgnLTStr> References;
typedef std::map<const ClassAd*, References> PortReferences;

#if defined( EXPERIMENTAL )
#include "rectangle.h"
#endif

#ifdef CLASSAD_DEPRECATED
#include "stream.h"
#endif

typedef classad_hash_map<std::string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr> AttrList;
typedef std::set<std::string, CaseIgnLTStr> DirtyAttrList;

void ClassAdLibraryVersion(int &major, int &minor, int &patch);
void ClassAdLibraryVersion(std::string &version_string);

/// An internal node of an expression which represents a ClassAd. 
class ClassAd : public ExprTree
{
  	public:
		/**@name Constructors/Destructor */
		//@{
		/// Default constructor 
		ClassAd ();

		/** Copy constructor */
		ClassAd (const ClassAd &);

		/** Destructor */
		~ClassAd ();
		//@}

		/**@name Inherited virtual methods */
		//@{
		// override methods
        /** Makes a deep copy of the expression tree
           	@return A deep copy of the expression, or NULL on failure.
         */
		virtual ExprTree* Copy( ) const;
		//@}

		/** Factory method to make a classad
		 * 	@param vec A vector of (name,expression) pairs to make a classad
		 * 	@return The constructed classad
		 */
		static ClassAd *MakeClassAd( std::vector< std::pair< std::string, ExprTree* > > &vec );

		/** Deconstructor to get the components of a classad
		 * 	@param vec A vector of (name,expression) pairs which are the
		 * 		attributes of the classad
		 */
		void GetComponents( std::vector< std::pair< std::string, ExprTree *> > &vec ) const;

		/**@name Insertion Methods */
		//@{	
		/** Inserts an attribute into the ClassAd.  The setParentScope() method
				is invoked on the inserted expression.
			@param attrName The name of the attribute.  
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool Insert( const std::string &attrName, ExprTree *expr );

		/** Inserts an attribute into a nested classAd.  The scope expression is
		 		evaluated to obtain a nested classad, and the attribute is 
				inserted into this subclassad.  The setParentScope() method
				is invoked on the inserted expression.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool DeepInsert( ExprTree *scopeExpr, const std::string &attrName, 
				ExprTree *expr );

		/** Inserts an attribute into the ClassAd.  The integer value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The integer value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see Value::NumberFactor
e		*/
		bool InsertAttr( const std::string &attrName,int value, 
				Value::NumberFactor f=Value::NO_FACTOR );

		/** Inserts an attribute into a nested classad.  The scope expression 
		 		is evaluated to obtain a nested classad, and the attribute is
		        inserted into this subclassad.  The integer value is
				converted into a Literal expression, and then inserted into
				the nested classad.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The integer value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see Value::NumberFactor
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				int value, Value::NumberFactor f=Value::NO_FACTOR );

		/** Inserts an attribute into the ClassAd.  The real value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The real value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see Value::NumberFactor
		*/
		bool InsertAttr( const std::string &attrName,double value, 
				Value::NumberFactor f=Value::NO_FACTOR);

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The double value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr String representation of the scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				double value, Value::NumberFactor f=Value::NO_FACTOR);

		/** Inserts an attribute into the ClassAd.  The boolean value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute. 
			@param value The boolean value of the attribute.
		*/
		bool InsertAttr( const std::string &attrName, bool value );

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The boolean value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.  This string is
				always duplicated internally.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName, 
				bool value );

		/** Inserts an attribute into the ClassAd.  The string value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool InsertAttr( const std::string &attrName, const char *value );

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The string value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName, 
				const char *value );

		/** Inserts an attribute into the ClassAd.  The string value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool InsertAttr( const std::string &attrName, const std::string &value );

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The string value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName, 
				const std::string &value );
		//@}

		/**@name Lookup Methods */
		//@{
		/** Finds the expression bound to an attribute name.  The lookup only
				involves this ClassAd; scoping information is ignored.
			@param attrName The name of the attribute.  
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *Lookup( const std::string &attrName ) const;

		/** Finds the expression bound to an attribute name.  The lookup uses
				the scoping structure (including <tt>super</tt> attributes) to 
				determine the expression bound to the given attribute name in
				the closest enclosing scope.  The closest enclosing scope is
				also returned.
			@param attrName The name of the attribute.
			@param ad The closest enclosing scope of the returned expression,
				or NULL if no expression was found.
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *LookupInScope(const std::string &attrName,const ClassAd *&ad)const;
		//@}

		/**@name Attribute Deletion Methods */
		//@{
		/** Clears the ClassAd of all attributes. */
		void Clear( );

		/** Deletes the named attribute from the ClassAd.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool Delete( const std::string &attrName );

		/** Deletes the named attribute from a nested classAd.  The scope
		 		expression is evaluated to obtain a nested classad, and the
				attribute is then deleted from this ad.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param scopeExpr String representation of the scope expression.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool DeepDelete( const std::string &scopeExpr, const std::string &attrName );

		/** Deletes the named attribute from a nested classAd.  The scope
		 		expression is evaluated to obtain a nested classad, and the
				attribute is then deleted from this ad.  Only attributes from
				the local ClassAd are considered; scoping information is 
				ignored.  The expression bound to the attribute is deleted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute to be delete.
			@return true if the attribute previously existed and was 
				successfully removed, false otherwise.
		*/
		bool DeepDelete( ExprTree *scopeExpr, const std::string &attrName );
	
		/** Similar to Delete, but the expression is returned rather than 
		  		deleted from the classad.
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *Remove( const std::string &attrName );

		/** Similar to DeepDelete, but the expression is returned rather than 
		  		deleted from the classad.
			@param scopeExpr String representation of the scope expression
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *DeepRemove( const std::string &scopeExpr, const std::string &attrName );

		/** Similar to DeepDelete, but the expression is returned rather than 
		  		deleted from the classad.
			@param scopeExpr The scope expression
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *DeepRemove( ExprTree *scopeExpr, const std::string &attrName );
		//@}

		/**@name Evaluation Methods */
		//@{
		/** Evaluates expression bound to an attribute.
			@param attrName The name of the attribute in the ClassAd.
			@param result The result of the evaluation.
		*/
		bool EvaluateAttr( const std::string& attrName, Value &result ) const;

		/** Evaluates an expression.
			@param buf Buffer containing the external representation of the
				expression.  This buffer is parsed to yield the expression to
				be evaluated.
			@param result The result of the evaluation.
			@return true if the operation succeeded, false otherwise.
		*/
		bool EvaluateExpr( const std::string& buf, Value &result ) const;

		/** Evaluates an expression.  If the expression doesn't already live in
				this ClassAd, the setParentScope() method must be called
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
		*/
		bool EvaluateExpr( const ExprTree* expr, Value &result ) const;	// eval'n

		/** Evaluates an expression, and returns the significant subexpressions
				encountered during the evaluation.  If the expression doesn't 
				already live in this ClassAd, call the setParentScope() method 
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
			@param sig The significant subexpressions of the evaluation.
		*/
		bool EvaluateExpr( const ExprTree* expr, Value &result, ExprTree *&sig) const;

		/** Evaluates an attribute to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool EvaluateAttrInt( const std::string &attrName, int& intValue ) const;

		/** Evaluates an attribute to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a real, false otherwise.
		*/
		bool EvaluateAttrReal( const std::string &attrName, double& realValue )const;

		/** Evaluates an attribute to an integer. If the attribute evaluated to 
				a real, it is truncated to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an number, false otherwise.
		*/
		bool EvaluateAttrNumber( const std::string &attrName, int& intValue ) const;

		/** Evaluates an attribute to a real.  If the attribute evaluated to an 
				integer, it is promoted to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a number, false otherwise.
		*/
		bool EvaluateAttrNumber(const std::string &attrName,double& realValue) const;

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attrName The name of the attribute.
			@param buf The buffer for the string value.
			@param len Size of buffer
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const std::string &attrName, char *buf, int len) 
				const;

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attrName The name of the attribute.
			@param buf The buffer for the string value.
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const std::string &attrName, std::string &buf ) const;

		/** Evaluates an attribute to a boolean.  A pointer to the string is 
				returned.
			@param attrName The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
		bool EvaluateAttrBool( const std::string &attrName, bool& boolValue ) const;

		/** Evaluates an attribute to a boolean.  A pointer to the ClassAd is 
				returned. You do not own the ClassAd--do not free it.
			@param attrName The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
        bool EvaluateAttrClassAd( const std::string &attr, ClassAd *&classad ) const;

		//@}

		/**@name STL-like Iterators */
		//@{

		/** Define an iterator we can use on a ClassAd */
		typedef AttrList::iterator iterator;

		/** Define a constatnt iterator we can use on a ClassAd */
		typedef AttrList::const_iterator const_iterator;

		/** Returns an iterator pointing to the beginning of the
			attribute/value pairs in the ClassAd */
		iterator begin() { return attrList.begin(); }

		/** Returns a constant iterator pointing to the beginning of the
			attribute/value pairs in the ClassAd */
		const_iterator begin() const { return attrList.begin(); }

		/** Returns aniterator pointing past the end of the
			attribute/value pairs in the ClassAd */
		iterator end() { return attrList.end(); }

		/** Returns a constant iterator pointing past the end of the
			attribute/value pairs in the ClassAd */
		const_iterator end() const { return attrList.end(); }
		//@}

		// STL-like functions
		iterator find(std::string const& attrName);
		const_iterator find(std::string const& attrName) const;

        int size(void) const { return attrList.size(); }

		/**@name Miscellaneous */
		//@{
		void Update( const ClassAd& ad );	

		void Modify( ClassAd& ad );

		ClassAd &operator=(const ClassAd &rhs);

		bool CopyFrom( const ClassAd &ad );

        virtual bool SameAs(const ExprTree *tree) const;

        friend bool operator==(ClassAd &list1, ClassAd &list2);

		/** Flattens (a partial evaluation operation) the given expression in 
		  		the context of the classad.
			@param expr The expression to be flattened.
			@param val The value after flattening, if the expression was 
				completely flattened.  This value is valid if and only if	
				fexpr is NULL.
			@param fexpr The flattened expression tree if the expression did
				not flatten to a single value, and NULL otherwise.
			@return true if the flattening was successful, and false otherwise.
		*/
		bool Flatten( const ExprTree* expr, Value& val, ExprTree *&fexpr )const;

		bool FlattenAndInline( const ExprTree* expr, Value& val,	// NAC
							   ExprTree *&fexpr )const;				// NAC
		
		bool GetExternalReferences( const ExprTree *tree, References &refs, bool fullNames );

		bool GetExternalReferences(const ExprTree *tree, PortReferences &refs);
#if defined( EXPERIMENTAL )
		bool AddRectangle( const ExprTree *tree, Rectangles &r, 
					const std::string &allowed, const References &imported );
#endif
		//@}

#if defined( CLASSAD_DEPRECATED )
// AttrList methods

		// insert expressions into the ad
        int        	Insert(const char*);
		int			InsertOrUpdate(const char *expr) { return Insert(expr); }

		// for iteration through expressions
//		void		ResetExpr();
//		ExprTree*	NextExpr();

		// for iteration through names (i.e., lhs of the expressions)
//		void		ResetName() { this->ptrName = exprList; }
//		const char* NextNameOriginal();

		// lookup values in classads  (for simple assignments)
//		ExprTree*   Lookup(char *) const;  		// for convenience
//      ExprTree*	Lookup(const char*) const;	// look up an expression

		int         LookupString(const char *, char *) const; 
		int         LookupString(const char *, char *, int) const; //uses strncpy
		int         LookupString (const char *name, char **value) const;
        int         LookupInteger(const char *, int &) const;
        int         LookupFloat(const char *, float &) const;
        int         LookupBool(const char *, int &) const;
        int         LookupBool(const char *, bool &) const;

		// evaluate values in classads
		int         EvalString (const char *, class ClassAd *, char *);
		int         EvalInteger (const char *, class ClassAd *, int &);
		int         EvalFloat (const char *, class ClassAd *, float &);
		int         EvalBool  (const char *, class ClassAd *, int &);

		// chaining
// ClassAd methods

        ClassAd(FILE*,char*,int&,int&,int&);	// Constructor, read from file.

		// Type operations
        void		SetMyTypeName(const char *); /// my type name set.
        const char*	GetMyTypeName();		// my type name returned.
        void 		SetTargetTypeName(const char *);// target type name set.
        const char*	GetTargetTypeName();	// target type name returned.

        // shipping functions
        int put(Stream& s);
		int initFromStream(Stream& s);

		// output functions
        virtual int	fPrint(FILE*);			// print the ClassAd to a file
		void		dPrint( int );			// dprintf to given dprintf level

// Back compatiblity helper methods

		bool AddExplicitConditionals( ExprTree *expr, ExprTree *&newExpr );
		ClassAd *AddExplicitTargetRefs( );
		
#endif

	    void		ChainToAd(ClassAd *new_chain_parent_ad);
		void		Unchain(void);

		void        EnableDirtyTracking(void)  { do_dirty_tracking = true;  }
		void        DisableDirtyTracking(void) { do_dirty_tracking = false; }
		void		ClearAllDirtyFlags(void);
		void        MarkAttributeDirty(const std::string &name);
		void        MarkAttributeClean(const std::string &name);
		bool        IsAttributeDirty(const std::string &name);

		typedef DirtyAttrList::iterator dirtyIterator;
		dirtyIterator dirtyBegin() { return dirtyAttrList.begin(); }
		dirtyIterator dirtyEnd() { return dirtyAttrList.end(); }
		

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class EvalState;
		friend 	class ClassAdIterator;

		bool _GetExternalReferences( const ExprTree *, ClassAd *, 
					EvalState &, References&, bool fullNames );

		bool _GetExternalReferences( const ExprTree *, ClassAd *, 
					EvalState &, PortReferences& );
#if defined( EXPERIMENTAL )
		bool _MakeRectangles(const ExprTree*,const std::string&,Rectangles&, bool);
		bool _CheckRef( ExprTree *, const std::string & );
#endif

#if defined( CLASSAD_DEPRECATED )
		void evalFromEnvironment( const char *name, Value val );
		ExprTree *AddExplicitConditionals( ExprTree * );
		ExprTree *AddExplicitTargetRefs( ExprTree *,
			std::set < std::string, CaseIgnLTStr > & );
#endif

		ClassAd *_GetDeepScope( const std::string& ) const;
		ClassAd *_GetDeepScope( ExprTree * ) const;

		virtual void _SetParentScope( const ClassAd* p );
		virtual bool _Evaluate( EvalState& , Value& ) const;
		virtual bool _Evaluate( EvalState&, Value&, ExprTree*& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
	
		int LookupInScope( const std::string&, ExprTree*&, EvalState& ) const;
		AttrList	  attrList;
		DirtyAttrList dirtyAttrList;
		bool          do_dirty_tracking;
		ClassAd       *chained_parent_ad;
};

END_NAMESPACE // classad

#include "classadItor.h"

#endif//__CLASSAD_H__
