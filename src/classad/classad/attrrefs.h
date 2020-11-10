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


#ifndef __CLASSAD_ATTRREFS_H__
#define __CLASSAD_ATTRREFS_H__

namespace classad {

/// Represents a attribute reference node (like .b) in the expression tree
class AttributeReference : public ExprTree 
{
  	public:

        /// Copy Constructor
        AttributeReference(const AttributeReference &ref);

		///  Destructor
    	virtual ~AttributeReference ();

        /// Assignment operator
        AttributeReference &operator=(const AttributeReference &ref);

		/// node type
		virtual NodeKind GetKind (void) const { return ATTRREF_NODE; }

		/** Factory method to create attribute reference nodes.
			@param expr The expression part of the reference (i.e., in
				case of expr.attr).  This parameter is NULL if the reference
				is absolute (i.e., .attr) or simple (i.e., attr).
			@param attrName The name of the reference.  This string is
				duplicated internally.
			@param absolute True if the reference is an absolute reference
				(i.e., in case of .attr).  This parameter cannot be true if
				expr is not NULL
		*/
    	static AttributeReference *MakeAttributeReference(ExprTree *expr, 
					const std::string &attrName, bool absolute=false);

		/** Deconstructor to get the components of an attribute reference
		 * 	@param expr The expression part of the reference (NULL for
		 * 		absolute or simple references)
		 * 	@param attr The name of the attribute being referred to
		 * 	@param abs  true iff the reference is absolute (i.e., .attr)
		 */
		void GetComponents( ExprTree *&expr,std::string &attr, bool &abs ) const;

		/** Overwrite the components of an attribute reference
		 * 	@param expr The expression part of the reference (NULL for
		 * 		absolute or simple references)
		 * 	@param attr The name of the attribute being referred to
		 * 	@param abs  true iff the reference is absolute (i.e., .attr)
		 */
		bool SetComponents( ExprTree *expr, const std::string &attr, bool abs );

        /** Return a copy of this attribute reference.
         */
		virtual ExprTree* Copy( ) const;

        /** Copy from the given reference into this reference.
         *  @param ref The reference to copy from.
         *  @return true if the copy succeeded, false otherwise.
         */
        bool CopyFrom(const AttributeReference &ref);

        /** Is this attribute reference the same as another?
         *  @param tree The reference to compare with
         *  @return true if they are the same, false otherwise.
         */
        virtual bool SameAs(const ExprTree *tree) const;

        /** Are the two attribute references the same?
         *  @param ref1 An attribute reference
         *  @param ref2 Another attribute reference
         *  @return true if they are the same, false otherwise.
         */
        friend bool operator==(const AttributeReference &ref1, const AttributeReference &ref2);

		virtual const ClassAd *GetParentScope( ) const { return( parentScope ); }

		static int Deref(const AttributeReference & ref, EvalState &, ExprTree*&);

	protected:
		/// Constructor
    	AttributeReference ();

  	private:
		// private ctor for internal use
		AttributeReference( ExprTree*, const std::string &, bool );
		virtual void _SetParentScope( const ClassAd* p );
    	virtual bool _Evaluate( EvalState & , Value & ) const;
    	virtual bool _Evaluate( EvalState & , Value &, ExprTree*& ) const;
    	virtual bool _Flatten( EvalState&, Value&, ExprTree*&, int* ) const;
		int	FindExpr( EvalState&, ExprTree*&, ExprTree*&, bool ) const;

		const ClassAd *parentScope;

		ExprTree	*expr;
		bool		absolute;
    	std::string attributeStr;
};

} // classad

#endif//__CLASSAD_ATTRREFS_H__
