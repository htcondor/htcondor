/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __CLASSAD_H__
#define __CLASSAD_H__

#include <set>
#include <map>
#include <vector>
#include "exprTree.h"

#ifdef __cplusplus
#include <string>
using namespace std;
#endif

BEGIN_NAMESPACE( classad );

#if defined( EXPERIMENTAL )
	typedef set<string, CaseIgnLTStr> References;
	typedef map<const ClassAd*, References> PortReferences;
#include "rectangle.h"
#endif


typedef hash_map<string, ExprTree*, StringCaseIgnHash, CaseIgnEqStr> AttrList;

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
		virtual ClassAd* Copy( ) const;
		//@}

		/** Factory method to make a classad
		 * 	@param vec A vector of (name,expression) pairs to make a classad
		 * 	@return The constructed classad
		 */
		ClassAd *MakeClassAd( vector< pair< string, ExprTree* > > &vec );

		/** Deconstructor to get the components of a classad
		 * 	@param vec A vector of (name,expression) pairs which are the
		 * 		attributes of the classad
		 */
		void GetComponents( vector< pair< string, ExprTree *> > &vec ) const;

		/**@name Insertion Methods */
		//@{	
		/** Inserts an attribute into the ClassAd.  The setParentScope() method
				is invoked on the inserted expression.
			@param attrName The name of the attribute.  
			@param expr The expression bound to the name.
			@return true if the operation succeeded, false otherwise.
			@see ExprTree::setParentScope
		*/
		bool Insert( const string &attrName, ExprTree *expr );

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
		bool DeepInsert( ExprTree *scopeExpr, const string &attrName, 
				ExprTree *expr );

		/** Inserts an attribute into the ClassAd.  The integer value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The integer value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see Value::NumberFactor
		*/
		bool InsertAttr( const string &attrName,int value, 
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
		bool DeepInsertAttr( ExprTree *scopeExpr, const string &attrName,
				int value, Value::NumberFactor f=Value::NO_FACTOR );

		/** Inserts an attribute into the ClassAd.  The real value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The real value of the attribute.
			@param f The multiplicative factor to be attached to value.
			@see Value::NumberFactor
		*/
		bool InsertAttr( const string &attrName,double value, 
				Value::NumberFactor f=Value::NO_FACTOR);

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The double value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr String representation of the scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
			@param dup If dup is true, the value is duplicated internally.
				Otherwise, the string is assumed to have been created with new[]
				and the classad assumes responsibility for freeing the storage.
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const string &attrName,
				double value, Value::NumberFactor f=Value::NO_FACTOR);

		/** Inserts an attribute into the ClassAd.  The boolean value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute. 
			@param value The boolean value of the attribute.
		*/
		bool InsertAttr( const string &attrName, bool value );

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The boolean value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.  This string is
				always duplicated internally.
			@param value The string attribute
			@param dup If dup is true, the value is duplicated internally.
				Otherwise, the string is assumed to have been created with new[]
				and the classad assumes responsibility for freeing the storage.
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const string &attrName, 
				bool value );

		/** Inserts an attribute into the ClassAd.  The string value is
				converted into a Literal expression, and then inserted into
				the classad.
			@param attrName The name of the attribute.
			@param value The string attribute
		*/
		bool InsertAttr( const string &attrName, const string &value );

		/** Inserts an attribute into a nested classad.  The scope expression
		 		is evaluated to obtain a nested classad, and the insertion is
				made in the nested classad.  The string value is
				converted into a Literal expression to yield the expression to
				be inserted.
			@param scopeExpr The scope expression.
			@param attrName The name of the attribute.
			@param value The string attribute
			@param dup If dup is true, the value is duplicated internally.
				Otherwise, the string is assumed to have been created with new[]
				and the classad assumes responsibility for freeing the storage.
		*/
		bool DeepInsertAttr( ExprTree *scopeExpr, const string &attrName, 
				const string &value );
		//@}

		/**@name Lookup Methods */
		//@{
		/** Finds the expression bound to an attribute name.  The lookup only
				involves this ClassAd; scoping information is ignored.
			@param attrName The name of the attribute.  
			@return The expression bound to the name in the ClassAd, or NULL
				otherwise.
		*/
		ExprTree *Lookup( const string &attrName ) const;

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
		ExprTree *LookupInScope(const string &attrName,const ClassAd *&ad)const;
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
		bool Delete( const string &attrName );

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
		bool DeepDelete( const string &scopeExpr, const string &attrName );

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
		bool DeepDelete( ExprTree *scopeExpr, const string &attrName );
	
		/** Similar to Delete, but the expression is returned rather than 
		  		deleted from the classad.
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *Remove( const string &attrName );

		/** Similar to DeepDelete, but the expression is returned rather than 
		  		deleted from the classad.
			@param scopeExpr String representation of the scope expression
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *DeepRemove( const string &scopeExpr, const string &attrName );

		/** Similar to DeepDelete, but the expression is returned rather than 
		  		deleted from the classad.
			@param scopeExpr The scope expression
			@param attrName The name of the attribute to be extricated.
			@return The expression tree of the named attribute, or NULL if
				the attribute could not be found.
			@see Delete
		*/
		ExprTree *DeepRemove( ExprTree *scopeExpr, const string &attrName );
		//@}

		/**@name Evaluation Methods */
		//@{
		/** Evaluates expression bound to an attribute.
			@param attrName The name of the attribute in the ClassAd.
			@param result The result of the evaluation.
		*/
		bool EvaluateAttr( const string& attrName, Value &result ) const;

		/** Evaluates an expression.
			@param buf Buffer containing the external representation of the
				expression.  This buffer is parsed to yield the expression to
				be evaluated.
			@param result The result of the evaluation.
			@return true if the operation succeeded, false otherwise.
		*/
		bool EvaluateExpr( const string& buf, Value &result ) const;

		/** Evaluates an expression.  If the expression doesn't already live in
				this ClassAd, the setParentScope() method must be called
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
		*/
		bool EvaluateExpr( ExprTree* expr, Value &result ) const;	// eval'n

		/** Evaluates an expression, and returns the significant subexpressions
				encountered during the evaluation.  If the expression doesn't 
				already live in this ClassAd, call the setParentScope() method 
				on it first.
			@param expr The expression to be evaluated.
			@param result The result of the evaluation.
			@param sig The significant subexpressions of the evaluation.
		*/
		bool EvaluateExpr( ExprTree* expr, Value &result, ExprTree *&sig) const;

		/** Evaluates an attribute to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an integer, false otherwise.
		*/
		bool EvaluateAttrInt( const string &attrName, int& intValue ) const;

		/** Evaluates an attribute to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a real, false otherwise.
		*/
		bool EvaluateAttrReal( const string &attrName, double& realValue )const;

		/** Evaluates an attribute to an integer. If the attribute evaluated to 
				a real, it is truncated to an integer.
			@param attrName The name of the attribute.
			@param intValue The value of the attribute.
			@return true if attrName evaluated to an number, false otherwise.
		*/
		bool EvaluateAttrNumber( const string &attrName, int& intValue ) const;

		/** Evaluates an attribute to a real.  If the attribute evaluated to an 
				integer, it is promoted to a real.
			@param attrName The name of the attribute.
			@param realValue The value of the attribute.
			@return true if attrName evaluated to a number, false otherwise.
		*/
		bool EvaluateAttrNumber(const string &attrName,double& realValue) const;

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attrName The name of the attribute.
			@param buf The buffer for the string value.
			@param len Size of buffer
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const string &attrName, char *buf, int len) 
				const;

		/** Evaluates an attribute to a string.  If the string value does not 
				fit into the buffer, only the portion that does fit is copied 
				over.
			@param attrName The name of the attribute.
			@param buf The buffer for the string value.
			@return true iff attrName evaluated to a string
		*/
		bool EvaluateAttrString( const string &attrName, string &buf ) const;

		/** Evaluates an attribute to a boolean.  A pointer to the string is 
				returned.
			@param attrName The name of the attribute.
			@param boolValue The value of the attribute.
			@return true if attrName evaluated to a boolean value, false 
				otherwise.
		*/
		bool EvaluateAttrBool( const string &attrName, bool& boolValue ) const;
		//@}

		/**@name Miscellaneous */
		//@{
		void Update( const ClassAd& ad );	

		void Modify( ClassAd& ad );

		bool CopyFrom( const ClassAd &ad );

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
		
#if defined( EXPERIMENTAL )
		bool GetExternalReferences( const ExprTree *tree, References &refs );
		bool GetExternalReferences(const ExprTree *tree, PortReferences &refs);
		bool AddRectangle( const ExprTree *tree, Rectangles &r, 
					const string &allowed, const References &imported );
#endif
		//@}

  	private:
		friend 	class AttributeReference;
		friend 	class ExprTree;
		friend 	class EvalState;
		friend 	class ClassAdIterator;

#if defined( EXPERIMENTAL )
		bool _GetExternalReferences( const ExprTree *, ClassAd *, 
					EvalState &, References& );
		bool _GetExternalReferences( const ExprTree *, ClassAd *, 
					EvalState &, PortReferences& );
		bool _MakeRectangles(const ExprTree*,const string&,Rectangles&, bool);
		bool _CheckRef( ExprTree *, const string & );
#endif

		ClassAd *_GetDeepScope( const string& ) const;
		ClassAd *_GetDeepScope( ExprTree * ) const;

		virtual void _SetParentScope( const ClassAd* p );
		virtual bool _Evaluate( EvalState& , Value& ) const;
		virtual bool _Evaluate( EvalState&, Value&, ExprTree*& ) const;
		virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
	
		int LookupInScope( const string&, ExprTree*&, EvalState& ) const;
		AttrList	attrList;
};
	
END_NAMESPACE // classad

#include "classadItor.h"

#endif//__CLASSAD_H__
