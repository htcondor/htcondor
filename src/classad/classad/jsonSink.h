/***************************************************************
 *
 * Copyright (C) 1990-2016, Condor Team, Computer Sciences Department,
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


#ifndef __CLASSAD_JSON_SINK_H__
#define __CLASSAD_JSON_SINK_H__

#include "classad/common.h"
#include "classad/exprTree.h"
#include <vector>
#include <utility>	// for pair template
#include <string>

namespace classad {

/// This converts a ClassAd into a JSON string representing the ClassAd
class ClassAdJsonUnParser
{
 public:
	/// Constructor
	ClassAdJsonUnParser( );

	ClassAdJsonUnParser(bool oneline);

	/// Destructor
	virtual ~ClassAdJsonUnParser( );

	/** Unparse a value 
	 * 	@param buffer The string to unparse to
	 * 	@param val The value to unparse
	 */
	void Unparse( std::string &buffer, const Value &val );

	/** Unparse an expression 
	 * 	@param buffer The string to unparse to
	 * 	@param expr The expression to unparse
	 */
	void Unparse( std::string &buffer, const ExprTree *expr );
	void Unparse( std::string &buffer, const ClassAd *ad, const References &whitelist );

	static void UnparseAuxEscapeString( std::string &buffer, const std::string &value );

 protected:
	void UnparseAuxQuoteExpr( std::string &buffer, const ExprTree *expr );

	void UnparseAuxQuoteExpr( std::string &buffer, const std::string &expr );

	void UnparseAuxClassAd( std::string &buffer,
			const std::vector< std::pair< std::string, ExprTree*> >& attrs );

	int m_indentLevel;
	int m_indentIncrement;
	bool m_oneline;
};


} // classad

#endif//__CLASSAD_JSON_SINK_H__
