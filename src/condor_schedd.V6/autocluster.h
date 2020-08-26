
/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef _autocluster_H_
#define _autocluster_H_

#define USE_AUTOCLUSTER_TO_JOBID_MAP 1
#define ALLOW_ON_THE_FLY_AGGREGATION

#include "condor_classad.h"
#include <generic_stats.h>

class JobIdSet;
class JobAggregationResults;
class JobQueueJob;

class JobCluster {
public:
	JobCluster();
	~JobCluster();

	bool setSigAttrs(const char* new_sig_attrs, bool free_input_attrs, bool replace_attrs);
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	void keepJobIds(bool keep) { keep_job_ids = keep; }
#endif
	int getClusterid(JobQueueJob &job, bool expand_refs, std::string * final_list);
	int size();
	void clear();
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	bool hasJobIds() const { return keep_job_ids; }
	void collect_garbage(bool brute_force); // free the deleted clusters
#endif

protected:
	friend class JobAggregationResults;
	typedef std::map<std::string,int> JobSigidMap;
	JobSigidMap cluster_map;  // map of signature to a cluster id
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	typedef std::map<int, JobIdSet> JobIdSetMap;
	JobIdSetMap cluster_use; // map clusterId to a set of jobIds
	std::set<int> cluster_gone; // holds the set of deleted clusters until the next garbage collect pass
	JobIdSetMap::iterator find_job_id_set(JobQueueJob & job); // lookup the autocluster for a job (assumes job.autocluster_id is valid)
#endif
	int next_id;
	const char *significant_attrs;
#ifdef USE_AUTOCLUSTER_TO_JOBID_MAP
	bool keep_job_ids;
#endif
};

/** This class manages the computation auto cluster ids for jobs based
	upon a list of significant attributes.  Any jobs with an identical
	auto cluster id can be considered identical for purposes of matchmaking.
	The significant attributes list is a list of all job classad attributes
	that will be examined during negotiation/matchmaking -- this in effect
	means it is a union of all job attributes referenced by any startd
	requirements or rank expression, plus any job attributes referenced by
	the negotiation such as the PREEMPTION_REQUIREMENTS expression, plus
	any attributes referenced by the job ad's own Requirements / Rank
	expressions.  In the schedd, the assumption is this class will be
	configured with the significant attributes passed to the schedd by
	the negotiator.
*/
class AutoCluster : public JobCluster {

public:

	/// Ctor
	AutoCluster();

	/// Dtor
	~AutoCluster();

	/** Configure the class; must be called before any other methods
		are invoked.  May be called as often as desired.
		@param significant_target_attrs A string of attributes delimited
		by commas.  These attributes will be added to a list of attributes
		considered to be significant by the class.  This parameter is
		ignored if the config file explicitly specifies a list of
		significant attributes.
		@return True if the list significant attributes has changed, and
		thus all previously returned auto cluster ids are invalid and need
		to be recomputed; false if the list of significant attributes has
		not changed, and thus previous returned auto cluster ids are still
		valid.   In practice in the schedd, if config() returns True, the
		ATTR_AUTO_CLUSTER_ID attribute should be cleared from all job ads.
	*/
	bool config(const classad::References &basic_attrs, const char* significant_target_attrs = NULL);
	
	/** Given a job classad, return the value of the attribute
		ATTR_AUTO_CLUSTER_ID, or if this attribute is not present,
		compute the autocluster id and store it in the ad.  Also,
		the deletion flag for this autocluster entry will be cleared.
		@param job A job classad
		@return The autocluster id for this job, or -1 if it cannot
		be computed.
	*/
	int getAutoClusterid(JobQueueJob *job);

		// garbage collection methods...

	/** Set the deletion flag for all autocluster id entries in the class.
		After calling mark(), calls to getAutoClusterid() will clear the deletion
		flag for the autocluster returned.  The idea here is to call mark(),
		then call getAutoClusterid() on all job classads still active, then
		call sweep() to remove data structures assicoated with autoclusters
		no longer being used.
		@see sweep
	*/
	void mark();

	/** Deallocate and purge from memory the data structures associated with
		all autoclusters that have their corresponding deletion flag set.
		@see mark
	*/
	void sweep();

	/** Create (or find) and aggregation on the given projection
	  */
	JobAggregationResults * aggregateOn(bool use_default, const char * projection, int result_limit, classad::ExprTree * constraint);

	/** called just before setAttribute sets an attribute value so that we can decide whether
	  * or not to invalidate the autocluster
	  * Returns true if job was removed from its autocluster.
	  */
	bool preSetAttribute(JobQueueJob & job, const char * attr, const char * value, int flags);

	/** Unconditionally remove a job from its assigned autocluster.
	 */
	void removeFromAutocluster(JobQueueJob &job);

	/** Return number of active autoclusters
	  */
	int getNumAutoclusters() const { return (int)cluster_in_use.size(); }


protected:
	bool sig_attrs_came_from_config_file;
	typedef std::set<int> JobClusterIDs;
	JobClusterIDs cluster_in_use; // used by mark & sweep code. id in list if in use

	// used by the aggregateOn option
	// std::map<std::string, JobCluster> current_aggregations;
};

// this class is use to hold job-aggregation results
class JobAggregationResults {
public:
	JobAggregationResults(JobCluster& jc_, const char * proj_, int limit_, classad::ExprTree * constraint_=NULL, bool is_def_=false)
		: jc(jc_), projection(proj_?proj_:""), constraint(NULL), is_def_autocluster(is_def_), return_jobid_limit(0), result_limit(limit_), results_returned(0)
	{
		if (constraint_) constraint = constraint_->Copy();
	}
	~JobAggregationResults()
	{
		delete constraint; constraint = NULL;
	#ifdef ALLOW_ON_THE_FLY_AGGREGATION
		if (!is_def_autocluster) delete &jc;
	#endif
	}
	bool compute(); // populate the job cluster set.
	ClassAd * next();
	bool rewind();
	int  set_return_jobid_limit(int lim) { int old = return_jobid_limit; return_jobid_limit=lim; return old; }
	void pause(); // remember the iterator location, but don't hold an actual iterator because it may go invalid while paused.

protected:
	JobCluster & jc;
	std::string projection;
	classad::ExprTree * constraint;
	bool is_def_autocluster;
	int  return_jobid_limit;
	int  result_limit;
	int  results_returned;
	ClassAd ad;
	JobCluster::JobSigidMap::iterator it;
	std::string pause_position; // holds the key that the iterator was pointing to before we paused.
};



#endif

