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


#ifndef __CLASSAD_XMLSINK_H__
#define __CLASSAD_XMLSINK_H__

#include "classad/common.h"
#include "classad/exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

namespace classad {

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
	void Unparse(std::string &buffer, const ExprTree *expr);
	void Unparse(std::string &buffer, const ClassAd *ad, const References &whitelist);
	/* This version is provided for backwards SO compatibility.
	 * It should be removed the next time we have to bump the
	 * SO version.
	 */
	void Unparse(std::string &buffer, ExprTree *expr)
	{ Unparse(buffer, (const ExprTree *)expr); }

 private:
	/** Unparse a value 
	 * 	@param buffer The string to unparse to
	 * 	@param val The value to unparse
	 */
	void Unparse(std::string &buffer, const ExprTree *expr, int indent);

	void Unparse(std::string &buffer, const Value &val, int indent);
	virtual void UnparseAux(std::string &buffer, 
							std::vector< std::pair< std::string, ExprTree*> >& attrlist,
							int indent);
	virtual void UnparseAux(std::string &buffer, std::vector<ExprTree*>&, 
							int indent);

	bool compact_spacing;

};


} // classad

#endif /*__CLASSAD_XMLSINK_H__ */
