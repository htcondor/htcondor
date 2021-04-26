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
#include "list.h"
#include "simplelist.h"
#include "query_result_type.h"	

class GenericQuery
{
  public:
	// ctor/dtor
	GenericQuery ();
	GenericQuery (const GenericQuery &);
	~GenericQuery ();

	// set number of categories
	int setNumIntegerCats (const int);
	int setNumStringCats (const int);
	int setNumFloatCats (const int);
	
	// add constraints
	int addInteger (const int, int);
	int addString (const int, const char *);
	int addFloat (const int, float);
	int addCustomOR (const char *);
	int addCustomAND( const char * );

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
	
	// make the query expression
	int makeQuery (ExprTree *&tree);
	int makeQuery (std::string &expr);

	// overloaded operators
    // friend ostream &operator<< (ostream &, GenericQuery &);  // display
    // GenericQuery   &operator=  (GenericQuery &);             // assignment

  private:
	// to store the number of categories of each type
	int integerThreshold;
	int stringThreshold;
	int floatThreshold;

	// keyword lists
	char * const *integerKeywordList;
	char * const *stringKeywordList;
	char * const *floatKeywordList;

	// pointers to store the arrays of Lists neessary to store the constraints
	SimpleList<int>   *integerConstraints;
	SimpleList<float> *floatConstraints;
	List<char> 		  *stringConstraints;
	List<char> 		  customORConstraints;
	List<char> 		  customANDConstraints;

	// helper functions
	void clearQueryObject     (void);
    void clearStringCategory  (List<char> &);
    void clearIntegerCategory (SimpleList<int> &);
    void clearFloatCategory   (SimpleList<float> &);
    void copyQueryObject      (const GenericQuery &);
    void copyStringCategory   (List<char> &, List<char> &);
    void copyIntegerCategory  (SimpleList<int> &, SimpleList<int> &);
    void copyFloatCategory    (SimpleList<float>&, SimpleList<float>&);
};

#endif







