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

// Please read the documentation for the API before you use it (RTFM :-)  --RR

// the following arrays can be indexed by the enumerated constants to get
// the required keyword eg ScheddIntegerKeywords[SCHEDD_IDLE_JOBS]=="IdleJobs"
extern const char *ScheddStringKeywords      [];
extern const char *ScheddIntegerKeywords     [];
extern const char *ScheddFloatKeywords	     [];
extern const char *StartdStringKeywords	     [];
extern const char *StartdIntegerKeywords     [];
extern const char *StartdFloatKeywords	     [];
extern const char *GridManagerStringKeywords [];
extern const char *GridManagerIntegerKeywords[];
extern const char *GridManagerFloatKeywords	 [];

const char *getStrQueryResult(QueryResult);

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

enum GridManagerStringCategory
{
    GRID_NAME,
    GRID_HASH_NAME,
    GRID_SCHEDD_NAME,
    GRID_OWNER,
    
    GRID_STRING_THRESHOLD
};
enum GridManagerIntCategory
{
    GRID_NUM_JOBS,
    GRID_JOB_LIMIT,
    GRID_SUBMIT_LIMIT,
    GRID_SUBMITS_IN_PROGRESS,
    GRID_SUBMITS_QUEUED,
    GRID_SUBMITS_ALLOWED,
    GRID_SUBMITS_WANTED,

	GRID_INT_THRESHOLD
};

enum GridManagerFloatCategory
{
	GRID_FLOAT_THRESHOLD
};


class CondorQuery
{
  public:
	// ctor/dtor
	CondorQuery (AdTypes);
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
	
	// set the type for the next generic query
	void setGenericQueryType(const char*);

	// Add a non-requirements attribute to send along with the
	// query.  The server will decide what, if anything to do with it
	int addExtraAttribute(const char*);

		// Set the list of desired attributes
		// to be returned in the queried ads.
		// If not set, all attributes are returned.
	void setDesiredAttrs(char const * const *attrs);
	void setDesiredAttrsExpr(const char *expr);

  private:
		// These are unimplemented, so make them private so that they
		// can't be used.
	CondorQuery (const CondorQuery &);
	CondorQuery &operator= (const CondorQuery &);				// assignment

	int         command;
	AdTypes     queryType;
	GenericQuery query;
	char*		genericQueryType;

 // Stores extra attributes other than reqs to send to server
	ClassAd		extraAttrs;
};

#endif
