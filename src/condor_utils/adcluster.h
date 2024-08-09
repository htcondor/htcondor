
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

#ifndef _adcluster_H_
#define _adcluster_H_

#define ALLOW_ON_THE_FLY_AGGREGATION

#include "condor_classad.h"
#include <generic_stats.h>

template <class K> class AdAggregationResults;
template <class K> class AdKeySet;

// cluster (i.e. group-by) of a set of ads, templatized on the key used to lookup the ads.
// (note, the key can be the classad pointer itself provided some other data structure *owns* the pointer)
template <class K> class AdCluster {
public:
	AdCluster();
	~AdCluster();

	// add to the significant attribute set for this ad cluster. 
	bool setSigAttrs(const char* new_sig_attrs, bool replace_attrs);

	// store a function for making a key from an ad, when non NULL, this enables the cluster_use map
	void keepAdKeys(K (*make_key)(ClassAd &ad)) { get_ad_key = make_key; }

	// calculate the cluster id for this ad, storing the key of the ad in the cluster_use map if that is enabled.
	int getClusterid(ClassAd &ad, bool expand_refs, std::string * final_list);
	int size();
	void clear();

protected:
	friend class AdAggregationResults<K>;
	typedef std::map<std::string,int> AdSigidMap;
	AdSigidMap cluster_map;  // map of signature to a cluster id

	// this map is used only when keep_ad_keys is no-null.
	typedef std::map<int, AdKeySet<K> > AdKeySetMap;
	AdKeySetMap cluster_use; // map clusterId to a set of jobIds

	int next_id;
	classad::References significant_attrs;
	K (*get_ad_key)(ClassAd &ad);
};

// use the AdCluster class with the folling pattern
// a string or int can be used instead of ClassAd* as the key type
//
// AdCluster<ClassAd*> ac;
// std::string attrs;
// as.setSigAttrs(projection, false, true);
// as.keyAdKeys(true);
// foreach (ad in adlist) {
//    if (constraint && ! EvalBool(ad, constraint)) continue;
//    int id = ac.getClusterId(ad, true, attrs);
//    // optionally store the id and/or attrs for this ad somewhere...
// }
//
// access the results using this pattern
//
// AdAggregationResults<ClassAd*> res(ac,NULL,0)


// this class is used to iterate an AdCluster in a way that allows for pause()/resume() semantics (i.e. for async socket replies)
// and also formats the resulting items as ClassAds that are constructed from the significant attribute sets
template <class K> class AdAggregationResults {
public:
	AdAggregationResults(AdCluster<K>& ac_, bool take_ac=false, const char * proj_=NULL, int limit_=INT_MAX, classad::ExprTree * constraint_=NULL)
		: ac(ac_), attrId("Id"), attrCount("Count"), attrMembers("Members")
		, projection(proj_?proj_:""), constraint(NULL)
		, owns_ac(take_ac), return_key_limit(INT_MAX), result_limit(limit_), results_returned(0)
	{
		if (constraint_) constraint = constraint_->Copy();
	}
	~AdAggregationResults()
	{
		delete constraint; constraint = NULL;
		if (owns_ac) delete &ac;
	}
	ClassAd * next();
	bool rewind();
	int  set_key_limit(int lim) { int old = return_key_limit; return_key_limit=lim; return old; }
	void set_attrs(const char * id, const char* count, const char * members) {
		attrId = id; attrCount = count; attrMembers = members;
	}
	void pause(); // remember the iterator location, but don't hold an actual iterator because it may go invalid while paused.

protected:
	AdCluster<K> & ac;
	std::string attrId; // attribute name to use for Id e.g. ATTR_AUTO_CLUSTER_ID
	std::string attrCount; // attribute name to use for Id e.g. JobCount
	std::string attrMembers; // attribute name to use for key list, e.g. JobIds
	std::string projection;
	classad::ExprTree * constraint;
	bool owns_ac;
	int  return_key_limit;
	int  result_limit;
	int  results_returned;
	ClassAd ad; // temporary ad that we populate and then return with the next() method.
	std::map<std::string,int>::iterator it;
	std::string pause_position; // holds the key that the iterator was pointing to before we paused.
};

AdAggregationResults<ClassAd*> * GroupAdsByProjection(ClassAdList & adlist, const char * projection, int result_limit, classad::ExprTree * constraint);



#endif	// _adcluster_H_

