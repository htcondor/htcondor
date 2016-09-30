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

#include "classad/unparsedExpr.h"
#include "classad/source.h"
#include <assert.h>
#include <stdio.h>
#include <list>

using namespace classad;
using namespace std;

ExprTree *
UnparsedExpr::Copy( ) const
{
	return new UnparsedExpr(this->strValue);
}

void UnparsedExpr::_SetParentScope( const ClassAd* )
{
	// Think we don't need anything here???
}

bool UnparsedExpr::SameAs(const ExprTree* tree) const
{
	if (this == tree) {
		return true;
	}

	if (tree->GetKind() != UNPARSED_EXPR) {
		return false;
	}

	UnparsedExpr *rhs = (UnparsedExpr *) tree;
	return this->strValue == rhs->strValue;
}

ExprTree * UnparsedExpr::getParseTree() const
{
	if (!child1) {
		ClassAdParser parser;
		child1 = parser.ParseExpression(strValue);
		if (child1) child1->SetParentScope(this->GetParentScope());
	}
	return child1;
}

bool UnparsedExpr::_Evaluate( EvalState& st, Value& v ) const
{
	ExprTree *et = getParseTree();
	if (et) {
		return et->Evaluate(st, v);
	} else {
		return false;
	}
}

bool UnparsedExpr::_Evaluate( EvalState& st, Value& v, ExprTree*& t) const
{
	ExprTree *et = getParseTree();
	if (et) {
		return et->Evaluate(st, v, t);
	} else {
		return false;
	}
}

bool UnparsedExpr::_Flatten( EvalState& state, Value& val, ExprTree*& tree, int*i ) const
{
	ExprTree *et = getParseTree();
	if (et) {
		return et->Flatten(state, val, tree, i);
	} else {
		return false;
	}
}
