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
#ifndef __CONDOR_QUERY_H__
#define __CONDOR_QUERY_H__

#include "condor_classad.h"
#include "list.h"
#include "simplelist.h"
#include "condor_collector.h"
#include "condor_attributes.h"
#include "query_result_type.h"
#include "generic_query.h"

// Please read the documentation for the API before you use it (RTFM :-)  --RR

// the following arrays can be indexed by the enumerated constants to get
// the required keyword eg ScheddIntegerKeywords[SCHEDD_IDLE_JOBS]=="IdleJobs"
extern const char *ScheddStringKeywords [];
extern const char *ScheddIntegerKeywords[];
extern const char *ScheddFloatKeywords	[];
extern const char *StartdStringKeywords	[];
extern const char *StartdIntegerKeywords[];
extern const char *StartdFloatKeywords	[];

char *getStrQueryResult(QueryResult);

enum ScheddStringCategory
{
	SCHEDD_NAME,

	SCHEDD_STRING_THRESHOLD
};

enum ScheddIntCategory
{
	SCHEDD_USERS,
	SCHEDD_IDLE_JOBS,
	SCHEDD_RUNNING_JOBS,

	SCHEDD_INT_THRESHOLD
};

enum ScheddFloatCategory
{
	SCHEDD_FLOAT_THRESHOLD
};

enum ScheddCustomCategory
{
	SCHEDD_CUSTOM,
	
	SCHEDD_CUSTOM_THRESHOLD
};

enum StartdStringCategory
{
	STARTD_NAME,
	STARTD_MACHINE,
	STARTD_ARCH,
	STARTD_OPSYS,

	STARTD_STRING_THRESHOLD
};

enum StartdIntCategory
{
	STARTD_MEMORY,
	STARTD_DISK,

	STARTD_INT_THRESHOLD
};

enum StartdFloatCategory
{
	STARTD_FLOAT_THRESHOLD
};

enum StartdCustomCategory
{
	STARTD_CUSTOM,

	STARTD_CUSTOM_THRESHOLD
};


class CondorQuery
{
  public:
	// ctor/dtor
	CondorQuery (AdTypes);
	CondorQuery (const CondorQuery &);
	~CondorQuery ();

	// clear constraints
	QueryResult clearStringConstraints  (const int);
	QueryResult clearIntegerConstraints (const int);
	QueryResult clearFloatConstraints   (const int);
	void 		clearORCustomConstraints(void);
	void 		clearANDCustomConstraints(void);

	// add constraints
	QueryResult addConstraint (const int, const char *); // string constraints
	QueryResult addConstraint (const int, const int);	 // integer constraints
	QueryResult addConstraint (const int, const float);	 // float constraints
	QueryResult addORConstraint (const char *);			 // custom constraints
	QueryResult addANDConstraint (const char *);		 // custom constraints

	// fetch from collector
	QueryResult fetchAds (ClassAdList &, const char [] = "");

	// filter list of ads; arg1 is 'in', arg2 is 'out'
	QueryResult filterAds (ClassAdList &, ClassAdList &);

	// get the query filter ad --- useful for debugging
	QueryResult getQueryAd (ClassAd &);

	// overloaded operators
	friend ostream &operator<< (ostream &, CondorQuery &); 	// display
	CondorQuery    &operator=  (CondorQuery &);				// assignment

  private:
	int         command;
	AdTypes     queryType;
	GenericQuery query;
};

#endif
