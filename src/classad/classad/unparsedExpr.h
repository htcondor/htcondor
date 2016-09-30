/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
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

#ifndef __UNPARSED_EXPR_H__
#define __UNPARSED_EXPR_H__

#include "classad/exprTree.h"
#include <string>

namespace classad {

class UnparsedExpr : public ExprTree
{
public:
	UnparsedExpr(const std::string &value) : strValue(value), child1(0) {}
	virtual ~UnparsedExpr() {delete child1;}
	virtual NodeKind GetKind (void) const { return UNPARSED_EXPR; }
	std::string getUnparsedStr() { return strValue; }
	ExprTree *getParseTree() const;

protected:
	
	UnparsedExpr() {}
	
	/**
	 * SameAs() - determines if two elements are the same.
	 */
	virtual bool SameAs(const ExprTree* tree) const;
	
	/** Makes a deep copy of the expression tree
	 * 	@return A deep copy of the expression, or NULL on failure.
	 */
	virtual ExprTree *Copy( ) const;
	
	virtual void _SetParentScope( const ClassAd* );
	virtual bool _Evaluate( EvalState& st, Value& v ) const;
	virtual bool _Evaluate( EvalState& st, Value& v , ExprTree*& t) const;
	virtual bool _Flatten( EvalState& st, Value& v, ExprTree*& t, int* i ) const;
	std::string strValue;  // The string holding the unparsed form
	mutable ExprTree *child1;
};
}

#endif 


