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
#ifndef __CONDOR_QUERY_H__
#define __CONDOR_QUERY_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "list.h"
#include "simplelist.h"
#include "condor_collector.h"
#include "condor_attributes.h"
#include "query_result_type.h"
#include "generic_query.h"
#include "CondorError.h"
#include "daemon.h"
#include "daemon_list.h"

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
	QueryResult fetchAds (ClassAdList &adList, const char * pool, CondorError* errstack = NULL);


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
