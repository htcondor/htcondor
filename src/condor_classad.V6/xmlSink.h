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

#ifndef __XMLSINK_H__
#define __XMLSINK_H__

#include "common.h"
#include "exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

BEGIN_NAMESPACE( classad );

/// The unparser object
class ClassAdXMLUnParser
{
	public:
		/// Constructor
		ClassAdXMLUnParser( );

		/// Destructor
		virtual ~ClassAdXMLUnParser( );

		/** Unparse a value 
		 * 	@param buffer The string to unparse to
		 * 	@param val The value to unparse
		 */
		void Unparse( string &buffer, Value &val );

		/** Unparse an expression 
		 * 	@param buffer The string to unparse to
		 * 	@param expr The expression to unparse
		 */
		void Unparse( string &buffer, ExprTree *expr );

		virtual void UnparseAux( string &buffer, 
					vector< pair< string, ExprTree*> >& attrlist );
		virtual void UnparseAux( string &buffer, vector<ExprTree*>& );
};


END_NAMESPACE // classad

#endif /*__XMLSINK_H__ */
