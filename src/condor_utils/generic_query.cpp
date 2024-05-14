/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "generic_query.h"
#include "condor_attributes.h"
#include "condor_classad.h"
//#include <algorithm>
//#include <numeric>
//#include <iterator>

int GenericQuery::
addCustomOR (const char *value)
{
	if (hasCustomOR(value)) return Q_OK;
	char *x = strdup (value);
	if (!x) return Q_MEMORY_ERROR;
	customORConstraints.emplace_back(x);
	return Q_OK;
}

int GenericQuery::
addCustomAND (const char *value)
{
	if (hasCustomAND(value)) return Q_OK;
	char *x = strdup (value);
	if (!x) return Q_MEMORY_ERROR;
	customANDConstraints.emplace_back(x);
	return Q_OK;
}


// make query
int GenericQuery::
makeQuery (std::string &req)
{
	req.clear();

	// construct query requirement expression
	bool firstCategory = true;

	// add custom AND constraints
	if (!customANDConstraints.empty())
	{
		bool firstTime = true;
		req += firstCategory ? "(" : " && (";
		for (auto *item: customANDConstraints)
		{
			formatstr_cat (req, "%s(%s)", firstTime ? " " : " && ", item);
			firstTime = false;
			firstCategory = false;
		}
		req += " )";
	}

	// add custom OR constraints
	if (!customORConstraints.empty())
	{
		bool firstTime = true;
		req += firstCategory ? "(" : " && (";
		for (auto *item: customORConstraints)
		{
			formatstr_cat (req, "%s(%s)", firstTime ? " " : " || ", item);
			firstTime = false;
			firstCategory = false;
		}
		req += " )";
	}

	return Q_OK;
}

int GenericQuery::
makeQuery (ExprTree *&tree, const char * expr_if_empty)
{
	std::string req;
	int status = makeQuery(req);
	if (status != Q_OK) return status;

	// If there are no constraints, then we match everything.
	if (req.empty()) {
		if ( ! expr_if_empty) {
			tree = nullptr;
			return Q_OK;
		}
		req = expr_if_empty;
	}

	// parse constraints and insert into query ad
	if (ParseClassAdRvalExpr (req.c_str(), tree) > 0) return Q_PARSE_ERROR;

	return Q_OK;
}

