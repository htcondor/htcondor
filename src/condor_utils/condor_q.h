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


class DCSChedd; // forward ref

class CondorQ
{
  public:
	// ctor/dtor
	CondorQ () = default;
	~CondorQ () {};

	// initialize defaults, etc.
	bool init();  

	// add constraints
	int add (CondorQStrCategories, const char *);
	int addAND (const char * value) { return query.addCustomOR (value); }
	int addOR (const char * value) { return query.addCustomAND (value); }

	void addSchedd (const char * value) { schedd = value; }  // what schedd are we querying?

	int fetchQueueFromHostAndProcess (const char * schedd_host, const std::vector<std::string> &attrs, int fetch_opts, int match_limit, condor_q_process_func process_func, void * process_func_data, int useFastPath, CondorError* errstack = 0, ClassAd ** psummary_ad=NULL);

	// returns a QueryResult, 0 (Q_OK) for success
	int initQueryAd(ClassAd & request_ad, const std::vector<std::string> &attrs, int fetch_opts, int match_limit);

	void useDefaultingOperator(bool enable) { defaulting_operator = enable; }

	// return the effective query constraint directly to the user.
	// the caller is responsible for deleting the returned ExprTree.
	int  rawQuery(ExprTree * &tree) { return query.makeQuery(tree); }
	int  rawQuery(std::string & str) { return query.makeQuery(str); }

	void requestServerTime(bool request) { requestservertime = request; }
	void forAnalysis(bool val) { for_analysis = val; }

  private:
	GenericQuery query;
	
	std::string owner;
	std::string schedd;
	bool defaulting_operator{false};
	bool requestservertime{false};
	bool for_analysis{false};
	int connect_timeout{20}; // default timeout when talking the schedd (via ConnectQ())
	time_t scheddBirthdate{0};
	
	// use DCSchedd::queryJobs(QUERY_JOB_ADS or QUERY_JOB_ADS_WITH_AUTH) to iterate
	int fetchQueueFromHostAndProcessV2 ( const char * host, const std::vector<std::string> &attrs, int fetch_opts, int match_limit, condor_q_process_func process_func, void * process_func_data, int connect_timeout, int useFastPath, CondorError* errstack = 0, ClassAd ** psummary_ad=NULL);
	// use QMGMT GetAllJobsByConstraint_Start to iterate
	int getFilterAndProcessAds( const char * constraint, const std::vector<std::string> &attrs, int match_limit, condor_q_process_func pfn, void * process_func_data, bool useAll );
};

int JobSort(ClassAd *job1, ClassAd *job2, void *data);

char encode_status( int status );

#endif
