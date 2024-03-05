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
	GenericQuery ();
	GenericQuery (const GenericQuery &);
	~GenericQuery ();

#if 0
	// set number of categories
	int setNumIntegerCats (const int);
	int setNumStringCats (const int);
	int setNumFloatCats (const int);
	
	// add constraints
	int addInteger (const int, int);
	int addString (const int, const char *);
	int addFloat (const int, float);
#endif
	int addCustomOR (const char *);
	int addCustomAND( const char * );

	bool hasCustomOR(const char * value) { return hasItem(customORConstraints, value); }
	bool hasCustomAND(const char * value) { return hasItem(customANDConstraints, value); }
#if 0
	bool hasString(const int cat, const char * value);
	bool hasStringNoCase(const int cat, const char * value);

	// clear constraints
	int clearInteger (const int);
	int clearString (const int);
	int clearFloat (const int);
	int clearCustomOR (void);
	int clearCustomAND(void);

	// set keyword lists
	void setIntegerKwList (char **);
	void setStringKwList (char **);
	void setFloatKwList (char **);
#endif
	// make the query expression
	int makeQuery (ExprTree *&tree, const char * expr_if_empty="TRUE");
	int makeQuery (std::string &expr);

	void clear() { clearQueryObject(); }

	// overloaded operators
    // friend ostream &operator<< (ostream &, GenericQuery &);  // display
    // GenericQuery   &operator=  (GenericQuery &);             // assignment

  private:
#if 0
	// to store the number of categories of each type
	int integerThreshold;
	int stringThreshold;
	int floatThreshold;

	// keyword lists
	char * const *integerKeywordList;
	char * const *stringKeywordList;
	char * const *floatKeywordList;

	// pointers to store the arrays of Lists neessary to store the constraints
	std::vector<int>   *integerConstraints;
	std::vector<float> *floatConstraints;
	List<char> 		  *stringConstraints;
#endif
	std::vector<char *> 		  customORConstraints;
	std::vector<char *> 		  customANDConstraints;

	// helper functions
	void clearQueryObject     ();
	void copyQueryObject      (const GenericQuery &);
	bool hasItem(std::vector<char *>& lst, const char * value) {
		for (auto *item : lst) {
			if (YourString(item) == value) {
				return true;
			}
		}
		return false;
	}
	void clearStringCategory  (std::vector<char *> &);
	void copyStringCategory   (std::vector<char *>&, std::vector<char *> &);
#if 0
    void clearIntegerCategory (std::vector<int> &);
    void clearFloatCategory   (std::vector<float> &);
    void copyIntegerCategory  (std::vector<int> &, std::vector<int> &);
    void copyFloatCategory    (std::vector<float>&, std::vector<float>&);
	bool hasItemNoCase(List<char>& lst, const char * value) {
		for (YourStringNoCase item = lst.First(); ! item.empty(); item = lst.Next()) {
			if (item == value) return true;
		}
		return false;
	}
#endif
};

#endif







