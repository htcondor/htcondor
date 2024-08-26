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

#ifndef __CONDOR_Q_H__
#define __CONDOR_Q_H__

#include "condor_common.h"
#include "generic_query.h"
#include "CondorError.h"

#define MAXOWNERLEN 20
#define MAXSCHEDDLEN 255

// This callback signature is for the getFilterAndProcess function.
// This callback should return false to take ownership of ad, true otherwise.
// we do this to avoid a makeing a copy of the ad in a common use case,
// because the caller will normally delete the ad, but in fact has no more use for it.
typedef bool (*condor_q_process_func)(void*, ClassAd *ad);


enum CondorQStrCategories
{
	CQ_OWNER,
	CQ_SUBMITTER,

	CQ_STR_THRESHOLD
};


#if 0
enum CondorQIntCategories
{
	CQ_CLUSTER_ID,
	CQ_PROC_ID,
	CQ_STATUS,
	CQ_UNIVERSE,

	CQ_INT_THRESHOLD
};

/* a list of all types of direct DB query defined here */
enum CondorQQueryType
{
	AVG_TIME_IN_QUEUE
};


enum CondorQFltCategories
{
	CQ_FLT_THRESHOLD
};

#endif

class DCSChedd; // forward ref

class CondorQ
{
  public:
	// ctor/dtor
	CondorQ ();
	// CondorQ (const CondorQ &);
	~CondorQ ();

	// initialize defaults, etc.
	bool init();  

	// add constraints
	int add (CondorQStrCategories, const char *);
#if 0
	int add (CondorQIntCategories, int);
	int addDBConstraint (CondorQIntCategories, int);
	int add (CondorQFltCategories, float);
#endif
	int addAND (const char *);  // custom
	int addOR (const char *);  // custom

	int addSchedd (const char *);  // what schedd are we querying?
	int addScheddBirthdate (time_t value);  // what 
	// fetch the job ads from the schedd corresponding to the given classad
	// which pass the criterion specified by the constraints; default is
	// from the local schedd
	int fetchQueue (ClassAdList &, const std::vector<std::string> &attrs, ClassAd * = 0, CondorError* errstack = 0);
	int fetchQueueFromHost (ClassAdList &, const std::vector<std::string> &attrs, const char * = 0, char const *schedd_version = 0,CondorError* errstack = 0);
	int fetchQueueFromHostAndProcess (const char * schedd_host, const std::vector<std::string> &attrs, int fetch_opts, int match_limit, condor_q_process_func process_func, void * process_func_data, int useFastPath, CondorError* errstack = 0, ClassAd ** psummary_ad=NULL);

	int initQueryAd(ClassAd & request_ad, const std::vector<std::string> &attrs, int fetch_opts, int match_limit);

	void useDefaultingOperator(bool enable);

	// return the effective query constraint directly to the user.
	// the caller is responsible for deleting the returned ExprTree.
	int  rawQuery(ExprTree * &tree) { return query.makeQuery(tree); }
	int  rawQuery(std::string & str) { return query.makeQuery(str); }

	void requestServerTime(bool request) { requestservertime = request; }

  private:
	GenericQuery query;
	
	// default timeout when talking the schedd (via ConnectQ())
	int connect_timeout;
	
#if 0
	int *clusterarray;
	int *procarray;
	int clusterprocarraysize;
	int numclusters;
	int numprocs;
#endif
	char owner[MAXOWNERLEN];
	char schedd[MAXSCHEDDLEN];
	bool defaulting_operator;
	bool requestservertime;
	time_t scheddBirthdate;
	
	// helper functions
	int fetchQueueFromHostAndProcessV2 ( const char * host, const std::vector<std::string> &attrs, int fetch_opts, int match_limit, condor_q_process_func process_func, void * process_func_data, int connect_timeout, int useFastPath, CondorError* errstack = 0, ClassAd ** psummary_ad=NULL);
	int getAndFilterAds( const char * constraint, const std::vector<std::string> &attrs, int match_limit, ClassAdList &, int useAll );
	int getFilterAndProcessAds( const char * constraint, const std::vector<std::string> &attrs, int match_limit, condor_q_process_func pfn, void * process_func_data, bool useAll );
};

int JobSort(ClassAd *job1, ClassAd *job2, void *data);

char encode_status( int status );

#endif
