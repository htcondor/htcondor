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

#ifndef __ATTRREFS_H__
#define __ATTRREFS_H__

BEGIN_NAMESPACE( classad )

/** Represents a attribute reference node in the expression tree
*/
class AttributeReference : public ExprTree 
{
  	public:
		///  Destructor
    	~AttributeReference ();

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

		/// Make a deep copy of the expression
        // We should return an AttributeReference, but Visual
        // Studio 6 won't accept that part of the standard. 
		virtual ExprTree* Copy( ) const;

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

		ExprTree	*expr;
		bool		absolute;
    	std::string attributeStr;
};

END_NAMESPACE // classad

#endif//__ATTRREFS_H__
