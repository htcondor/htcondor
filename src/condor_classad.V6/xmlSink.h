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

#ifndef __XMLSINK_H__
#define __XMLSINK_H__

#include "common.h"
#include "exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

BEGIN_NAMESPACE( classad )

/// This converts a ClassAd into an XML string representing the %ClassAd
class ClassAdXMLUnParser
{
 public:
	/// Constructor
	ClassAdXMLUnParser( );

	/// Destructor
	virtual ~ClassAdXMLUnParser( );

	/** Set if compact spacing is desired or note */
	void SetCompactSpacing(bool use_compact_spacing);

	/** Unparse an expression 
	 * 	@param buffer The string to unparse to
	 * 	@param expr The expression to unparse
		 */
	void Unparse(std::string &buffer, ExprTree *expr);

 private:
	/** Unparse a value 
	 * 	@param buffer The string to unparse to
	 * 	@param val The value to unparse
	 */
	void Unparse(std::string &buffer, ExprTree *expr, int indent);

	void Unparse(std::string &buffer, Value &val, int indent);
	virtual void UnparseAux(std::string &buffer, 
							std::vector< std::pair< std::string, ExprTree*> >& attrlist,
							int indent);
	virtual void UnparseAux(std::string &buffer, std::vector<ExprTree*>&, 
							int indent);

	bool compact_spacing;

};


END_NAMESPACE // classad

#endif /*__XMLSINK_H__ */
