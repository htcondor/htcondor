/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
	int addString (const int, char *);
	int addFloat (const int, float);
	int addCustomOR (char *);
	int addCustomAND( char * );

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
	
	// make the query Attrlist (N.B. types *not* set)
	int makeQuery (ClassAd &);

	// overloaded operators
    // friend ostream &operator<< (ostream &, GenericQuery &);  // display
    // GenericQuery   &operator=  (GenericQuery &);             // assignment

  private:
	// to store the number of categories of each type
	int integerThreshold;
	int stringThreshold;
	int floatThreshold;

	// keyword lists
	char **integerKeywordList;
	char **stringKeywordList;
	char **floatKeywordList;

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
    void copyQueryObject      (GenericQuery &);
    void copyStringCategory   (List<char> &, List<char> &);
    void copyIntegerCategory  (SimpleList<int> &, SimpleList<int> &);
    void copyFloatCategory    (SimpleList<float>&, SimpleList<float>&);
};

#endif







