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

#ifndef __GENERIC_QUERY_H__
#define __GENERIC_QUERY_H__

#include "condor_classad.h"
#include "query_result_type.h"

class GenericQuery
{
  public:
	// ctor/dtor
	GenericQuery () {}
	GenericQuery (const GenericQuery &qq) { copyQueryObject(qq); }
	~GenericQuery () { clearQueryObject(); }

	int addCustomOR (const char *);
	int addCustomAND( const char * );

	bool hasCustomOR(const char * value) { return hasItem(customORConstraints, value); }
	bool hasCustomAND(const char * value) { return hasItem(customANDConstraints, value); }

	// make the query expression, returns a QueryResult (Q_OK which is 0 for success)
	int makeQuery (ExprTree *&tree, const char * expr_if_empty="TRUE");
	int makeQuery (std::string &expr);

	void clear() { clearQueryObject(); }
	bool empty() { return customORConstraints.empty() && customANDConstraints.empty(); }

  protected:
	std::vector<char *> customORConstraints;
	std::vector<char *> customANDConstraints;

	// helper functions
	void clearQueryObject() {
		clearStringCategory (customANDConstraints);
		clearStringCategory (customORConstraints);
	}
	void copyQueryObject(const GenericQuery & from) {
		copyStringCategory (customANDConstraints, from.customANDConstraints);
		copyStringCategory (customORConstraints,  from.customORConstraints);
	}
	bool hasItem(std::vector<char *>& lst, const char * value) {
		for (auto *item : lst) {
			if (YourString(item) == value) {
				return true;
			}
		}
		return false;
	}
	void clearStringCategory  (std::vector<char *> & str_category) {
		for (auto *s: str_category) { free(s); }
		str_category.clear();
	}
	void copyStringCategory   (std::vector<char *>& to, const std::vector<char *> & from) {
		clearStringCategory (to);
		for (auto *item: from) {
			to.emplace_back(strdup(item));
		}
	}
};

#endif







