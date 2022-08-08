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

	// This query should perform only a location lookup.
	bool setLocationLookup(const std::string &location, bool want_one_result=true);

	// fetch from collector
	QueryResult fetchAds (ClassAdList &adList, const char * pool, CondorError* errstack = NULL);
	// fetch ads from the collector, handing each to 'callback'
	// callback will return 'false' if it took ownership of the ad.
	QueryResult processAds (bool (*callback)(void*, ClassAd *), void* pv, const char * pool, CondorError* errstack = NULL);


	// filter list of ads; arg1 is 'in', arg2 is 'out'
	QueryResult filterAds (ClassAdList &, ClassAdList &);

	// get the query filter ad --- useful for debugging
	QueryResult getQueryAd (ClassAd &);
	QueryResult getRequirements (std::string & req) { return (QueryResult) query.makeQuery (req); }
	
	// set the type for the next generic query
	void setGenericQueryType(const char*);

	// Add a non-requirements attribute to send along with the
	// query.  The server will decide what, if anything to do with it
	int addExtraAttribute(const char *name, const char *expression)          { return extraAttrs.AssignExpr(name, expression); }
	int addExtraAttributeString(const char *name, const std::string & value) { return extraAttrs.Assign(name, value); }
	int addExtraAttributeNumber(const char *name, long long value)           { return extraAttrs.Assign(name, value); }
	int addExtraAttributeBool(const char *name, bool value)                  { return extraAttrs.Assign(name, value); }

		// Set the list of desired attributes
		// to be returned in the queried ads.
		// If not set, all attributes are returned.
	void setDesiredAttrs(char const * const *attrs);
	void setDesiredAttrs(const std::vector<std::string> &attrs);
	void setDesiredAttrs(const classad::References & attrs);
		// set list of desired attributes as a space-separated string
	void setDesiredAttrs(const std::string &attrs) { extraAttrs.Assign(ATTR_PROJECTION, attrs); }
	void setDesiredAttrsExpr(const char *expr);

	void setResultLimit(int limit) { resultLimit = limit; }
	int  getResultLimit() const { return resultLimit; }

  private:
		// These are unimplemented, so make them private so that they
		// can't be used.
	CondorQuery (const CondorQuery &);
	CondorQuery &operator= (const CondorQuery &);				// assignment

	int         command;
	AdTypes     queryType;
	GenericQuery query;
	char*		genericQueryType;
	int         resultLimit; // limit on number of desired results. collectors prior to 8.7.1 will ignore this.

 // Stores extra attributes other than reqs to send to server
	ClassAd		extraAttrs;
};

#endif
