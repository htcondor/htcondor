/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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







