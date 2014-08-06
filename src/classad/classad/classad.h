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


#ifndef __CLASSAD_CLASSAD_H__
#define __CLASSAD_CLASSAD_H__


#include <set>
#include <map>
#include <vector>
#include <sstream>
#include "classad/classad_stl.h"
#include "classad/exprTree.h"

namespace classad {

typedef std::set<std::string, CaseIgnLTStr> References;
typedef std::map<const ClassAd*, References> PortReferences;

#if defined( EXPERIMENTAL )
#include "classad/rectangle.h"
#endif

typedef classad_unordered<std::string, ExprTree*, ClassadAttrNameHash, CaseIgnEqStr> AttrList;
typedef std::set<std::string, CaseIgnLTStr> DirtyAttrList;

void ClassAdLibraryVersion(int &major, int &minor, int &patch);
void ClassAdLibraryVersion(std::string &version_string);

// Should parsed expressions be cached and shared between multiple ads.
// The default is false.
void ClassAdSetExpressionCaching(bool do_caching);
bool ClassAdGetExpressionCaching();

// This flag is only meant for use in Condor, which is transitioning
// from an older version of ClassAds with slightly different evaluation
// semantics. It will be removed without warning in a future release.
extern bool _useOldClassAdSemantics;

template <class T>
void val_str(std::string & szOut, const T & tValue)
{
  std::stringstream foo;
  foo<<tValue;
  szOut = foo.str();
}
template<bool>
void val_str(std::string & szOut, const bool & tValue)
{
  std::stringstream foo;
  foo <<(tValue?"true":"false");
  szOut = foo.str();
}

/// The ClassAd object represents a parsed %ClassAd.
class ClassAd : public ExprTree
{
    /** \mainpage C++ ClassAd API Documentation
     *  Welcome to the C++ ClassAd API Documentation. 
     *  Use the links at the top to navigate. A good link to start with
     *  is the "Class List" link.
     *
     *  If you are unfamiliar with the ClassAd library, look at the 
     *  sample.C file that comes with the ClassAd library. Also check
     *  out the online ClassAd tutorial at: 
     *  http://www.cs.wisc.edu/condor/classad/c++tut.html
     */

  	public:
		/**@name Constructors/Destructor */
		//@{
		/// Default constructor 
		ClassAd ();

		/** Copy constructor
            @param ad The ClassAd to copy
        */
		ClassAd (const ClassAd &ad);

		/// Destructor
		virtual ~ClassAd ();
		//@}

		/// node type
		virtual NodeKind GetKind (void) const { return CLASSAD_NODE; }

		/**@name Insertion Methods */
		//@{	
		/** Inserts an attribute into the ClassAd.  The setParentScope() method
				is invoked on the inserted expression.
			@param attrName The name of the attribute.  
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool Insert( const std::string& attrName, ExprTree *& pRef, bool cache=true);
		bool Insert( const std::string& attrName, ClassAd *& expr, bool cache=true );
		bool Insert( const std::string& serialized_nvp);


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
		bool InsertAttr( const std::string &attrName,long value, 
				Value::NumberFactor f=Value::NO_FACTOR );
		bool InsertAttr( const std::string &attrName,long long value, 
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
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				long value, Value::NumberFactor f=Value::NO_FACTOR );
		bool DeepInsertAttr( ExprTree *scopeExpr, const std::string &attrName,
				long long value, Value::NumberFactor f=Value::NO_FACTOR );

		/** Inserts an attribute into the ClassAd.  The real value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The real value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see Value::NumberFactor
            @return true on success, false otherwise
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
            @param f A multipler for the number.
			@see Value::NumberFactor
            @return true on success, false otherwise
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
			@param attr The name of the attribute.
			@param intValue The value of the attribute.
			If the type of intValue is smaller than a long long, the
			value may be truncated.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool EvaluateAttrInt( const std::string &attr, int& intValue ) const;
		bool EvaluateAttrInt( const std::string &attr, long& intValue ) const;
		bool EvaluateAttrInt( const std::string &attr, long long& intValue ) const;

		/** Evaluates an attribute to a real.
			@param attr The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a real, false otherwise.
		*/
		bool EvaluateAttrReal( const std::string &attr, double& realValue )const;

		/** Evaluates an attribute to an integer. If the attribute evaluated to 
				a real, it is truncated to an integer.
				If the value is a boolean, it is converted to 0 (for False)
				or 1 (for True).
			@param attr The name of the attribute.
			@param intValue The value of the attribute.
			If the type of intValue is smaller than a long long, the
			value may be truncated.
			@return true if attrName evaluated to an number, false otherwise.
		*/
		bool EvaluateAttrNumber( const std::string &attr, int& intValue ) const;
		bool EvaluateAttrNumber( const std::string &attr, long& intValue ) const;
		bool EvaluateAttrNumber( const std::string &attr, long long& intValue ) const;

		/** Evaluates an attribute to a real.  If the attribute evaluated to an 
				integer, it is promoted to a real.
				If the value is a boolean, it is converted to 0.0 (for False)
				or 1.0 (for True).
			@param attr The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a number, false otherwise.
		*/
		bool EvaluateAttrNumber(const std::string &attr,double& realValue) const;

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attr The name of the attribute.
			@param buf The buffer for the string value.
			@param len Size of buffer
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const std::string &attr, char *buf, int len) 
				const;

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attr The name of the attribute.
			@param buf The buffer for the string value.
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const std::string &attr, std::string &buf ) const;

		/** Evaluates an attribute to a boolean.  A pointer to the string is 
				returned.
			@param attr The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
		bool EvaluateAttrBool( const std::string &attr, bool& boolValue ) const;

		/** Evaluates an attribute to a ClassAd.  A pointer to the ClassAd is 
				returned. You do not own the ClassAd--do not free it.
			@param attr The name of the attribute.
			@param classad The value of the attribute.
			@return true if attrName evaluated to a ClassAd, false 
				otherwise.
		*/
		// This interface is disabled, because it cannot support dynamically allocated
		// classad values (in case such a thing is ever added, similar to SLIST_VALUE).
		// Instead, use EvaluateAttr().
		// Waiting to hear if anybody cares ...
		// If anybody does, we can make this set a shared_ptr instead, but that
		// has performance implications that depend on whether all ClassAds
		// are managed via shared_ptr or only ones dynamically created
		// during evaluation (because a fresh copy has to be made for
		// objects not already managed via shared_ptr).  So let's avoid depending
		// on this interface until we need it.
        //bool EvaluateAttrClassAd( const std::string &attr, ClassAd *&classad ) const;

		/** Evaluates an attribute to an ExprList.  A pointer to the ExprList is 
				returned. You do not own the ExprList--do not free it.
			@param attr The name of the attribute.
			@param l The value of the attribute.
			@return true if attrName evaluated to a ExprList, false 
				otherwise.
		*/
		// This interface is disabled, because it cannot support dynamically allocated
		// list values (SLIST_VALUE).  Instead, use EvaluateAttr().
		// Waiting to hear if anybody cares ...
		// If anybody does, we can make this set a shared_ptr instead, but that
		// has performance implications that depend on whether all ExprLists
		// in ClassAds are managed via shared_ptr or only ones dynamically created
		// during evaluation  (because a fresh copy has to be made for
		// objects not already managed via shared_ptr).  So let's avoid depending
		// on this interface until we need it.
        //bool EvaluateAttrList( const std::string &attr, ExprList *&l ) const;
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

        /** Return an interator pointing to the attribute with a particular name.
         */
		iterator find(std::string const& attrName);

        /** Return a constant interator pointing to the attribute with a particular name.
         */
		const_iterator find(std::string const& attrName) const;

        /** Return the number of attributes at the root level of this ClassAd.
         */
        int size(void) const { return attrList.size(); }
		//@}

		/** Deconstructor to get the components of a classad
		 * 	@param vec A vector of (name,expression) pairs which are the
		 * 		attributes of the classad
		 */
		void GetComponents( std::vector< std::pair< std::string, ExprTree *> > &vec ) const;

        /** Make sure everything in the ad is in this ClassAd.
         *  This is different than CopyFrom() because we may have many 
         *  things that the ad doesn't have: we just ensure that everything we 
         *  import everything from the other ad. This could be called Merge().
         *  @param ad The ad to copy attributes from.
         */
		bool Update( const ClassAd& ad );	

        /** Modify this ClassAd in a specific way
         *  Ad is a ClassAd that looks like:
         *  [ Context = expr;    // Sub-ClassAd to operate on
         *    Replace = classad; // ClassAd to Update() replace context
         *    Updates = classad; // ClassAd to merge into context (via Update())
         *    Deletes = {a1, a2};  // A list of attribute names to delete from the context
         *  @param ad is a description of how to modify this ClassAd
         */
		void Modify( ClassAd& ad );

        /** Makes a deep copy of the ClassAd.
           	@return A deep copy of the ClassAd, or NULL on failure.
         */
		virtual ExprTree* Copy( ) const;

        /** Make a deep copy of the ClassAd, via the == operator. 
         */
		ClassAd &operator=(const ClassAd &rhs);

        /** Fill in this ClassAd with the contents of the other ClassAd.
         *  This ClassAd is cleared of its contents before the copy happens.
         *  @return true if the copy succeeded, false otherwise.
         */
		bool CopyFrom( const ClassAd &ad );

        /** Is this ClassAd the same as the tree?
         *  Two ClassAds are identical if they have the same
         *  number of elements, and each is the SameAs() the other. 
         *  This is a deep comparison.
         *  @return true if it is the same, false otherwise
         */
        virtual bool SameAs(const ExprTree *tree) const;

        /** Are the two ClassAds the same?
         *  Uses SameAs() to decide if they are the same. 
         *  @return true if they are, false otherwise.
         */
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
		
        /** Return a list of attribute references in the expression that are not 
         *  contained within this ClassAd.
         *  @param tree The ExprTree for the expression that has references that you are
         *  wish to know about. 
         *  @param refs The list of references
         *  @param fullNames true if you want full names (like other.foo)
         *  @return true on success, false on failure. 
         */
		bool GetExternalReferences( const ExprTree *tree, References &refs, bool fullNames );

        /** Return a list of attribute references in the expression that are not 
         *  contained within this ClassAd.
         *  @param tree The ExprTree for the expression that has references that you are
         *  wish to know about. 
         *  @param refs The list of references
         *  @return true on success, false on failure. 
         */
		bool GetExternalReferences(const ExprTree *tree, PortReferences &refs);
		//@}


        /** Return a list of attribute references in the expression that are
         *  contained within this ClassAd.
         *  @param tree The ExprTree for the expression that has references 
         *      that you wish to know about. 
         *  @param refs The list of references
         *  @param fullNames true if you want full names (like other.foo)
         *  @return true on success, false on failure. 
         */
        bool GetInternalReferences( const ExprTree *tree, References &refs, bool fullNames);

#if defined( EXPERIMENTAL )
		bool AddRectangle( const ExprTree *tree, Rectangles &r, 
					const std::string &allowed, const References &imported );
#endif

		/**@name Chaining functions */
        //@{
        /** Chain this ad to the parent ad.
         *  After chaining, any attribute we look for that is not
         *  in this ad will be looked for in the parent ad. This is
         *  a simple form of compression: many ads can be linked to a 
         *  parent ad that contains common attributes between the ads. 
         *  If an attribute is in both this ad and the parent, a lookup
         *  will only show it in the parent. If we make any modifications to
         *  this ad, it will not affect the parent. 
         *  @param new_chain_parent_ad the parent ad we are chained too.
         */
	    void		ChainToAd(ClassAd *new_chain_parent_ad);
		
		/** If there is a chained parent remove redundant entries.
		 */
		int 		PruneChildAd();
		
        /** If we are chained to a parent ad, remove the chain. 
         */
		void		Unchain(void);

		/** Return a pointer to the parent ad.
		 */
		ClassAd *   GetChainedParentAd(void);
		const ClassAd *   GetChainedParentAd(void) const;

		/** Fill in this ClassAd with the contents of the other ClassAd,
		 *  including any attributes from the other ad's chained parent.
		 *  Any previous contents of this ad are cleared.
		 *  @return true if the copy succeeded, false otherwise.
		 */
		bool CopyFromChain( const ClassAd &ad );

		/** Insert all attributes from the other ad and its chained
		 *  parents into this ad, but do not clear out existing
		 *  contents of this ad before doing so.
		 *  @return true if the operation succeeded, false otherwise.
		 */
		bool UpdateFromChain( const ClassAd &ad );
        //@}

		/**@name Dirty Tracking */
        //@{
		/** enable or disable dirty tracking for this ClassAd
		 *  and return whether dirty track was previously enabled or disabled.
		 */
		bool       SetDirtyTracking(bool enable) { bool was_enabled = do_dirty_tracking; do_dirty_tracking = enable; return was_enabled; }
        /** Turn on dirty tracking for this ClassAd. 
         *  If tracking is on, every insert will label the attribute that was inserted
         *  as dirty. Dirty tracking is always turned off during Copy() and
         *  CopyFrom().
         */ 
		void        EnableDirtyTracking(void)  { do_dirty_tracking = true;  }
        /** Turn off ditry tracking for this ClassAd.
         */
		void        DisableDirtyTracking(void) { do_dirty_tracking = false; }
        /** Mark all attributes in the ClassAd as not dirty
         */
		void		ClearAllDirtyFlags(void);
        /** Mark a particular attribute as dirty
         * @param name The attribute name
         */
		void        MarkAttributeDirty(const std::string &name);
        /** Mark a particular attribute as not dirty 
         * @param name The attribute name
         */
		void        MarkAttributeClean(const std::string &name);
        /** Return true if an attribute is dirty
         *  @param name The attribute name
         *  @return true if the attribute is dirty, false otherwise
         */
		bool        IsAttributeDirty(const std::string &name) const;

		/* Needed for backward compatibility
		 * Remove it the next time we have to bump the ClassAds SO version.
		 */
		bool        IsAttributeDirty(const std::string &name) {return ((const ClassAd*)this)->IsAttributeDirty(name);}

		typedef DirtyAttrList::iterator dirtyIterator;
        /** Return an interator to the first dirty attribute so all dirty attributes 
         * can be iterated through.
         */
		dirtyIterator dirtyBegin() { return dirtyAttrList.begin(); }
        /** Return an iterator past the last dirty attribute
         */
		dirtyIterator dirtyEnd() { return dirtyAttrList.end(); }
        //@}

		/* This data member is intended for transitioning Condor from
		 * old to new ClassAds. It allows unscoped attribute references
		 * in expressions that can't be found in the local scope to be
		 * looked for in an alternate scope. In Condor, the alternate
		 * scope is the Target ad in matchmaking.
		 * Expect alternateScope to be removed from a future release.
		 */
		ClassAd *alternateScope;

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class EvalState;
		friend 	class ClassAdIterator;

		bool _GetExternalReferences( const ExprTree *, ClassAd *, 
					EvalState &, References&, bool fullNames );

		bool _GetExternalReferences( const ExprTree *, ClassAd *, 
					EvalState &, PortReferences& );

        bool _GetInternalReferences(const ExprTree *expr, ClassAd *ad,
            EvalState &state, References& refs, bool fullNames);
#if defined( EXPERIMENTAL )
		bool _MakeRectangles(const ExprTree*,const std::string&,Rectangles&, bool);
		bool _CheckRef( ExprTree *, const std::string & );
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

} // classad

#include "classad/classadItor.h"

#endif//__CLASSAD_CLASSAD_H__
