
/***************************************************************
 *
 * Copyright (C) 1990-2019, Condor Team, Computer Sciences Department,
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

#ifndef _jobsets_H_
#define _jobsets_H_

class JobSets {
public:

	JobSets() : next_setid_num(0) {};

	~JobSets();

	enum class garbagePolicyEnum { immediateAfterEmpty, delayedAferEmpty };

	bool update(JobQueueJob & job, int old_status, int new_status);

	bool removeJobFromSet(JobQueueJob & job);

	void setNextSetId(int id);

	void reconfig();

	bool restoreJobSet(ClassAd *ad);

	size_t count() { return mapIdToSet.size(); };
	

private:

	struct jobidkey_hash {
		inline size_t operator()(const JOB_ID_KEY &s) const noexcept {
			return JOB_ID_KEY::hash(s);
		}
	};

	class JobSet {
	public:
		JobSet(int _id, std::string _name, std::string _owner, ClassAd *ad);
		~JobSet() = default;
		bool persistSetInfo();
		unsigned int id;
		std::string name;
		std::string owner;
		bool dirty = false;
		std::unordered_set<JOB_ID_KEY, jobidkey_hash> jobsInSet;
		LiveJobCounters jobStatusAggregates;
		garbagePolicyEnum garbagePolicy = garbagePolicyEnum::immediateAfterEmpty;
		ClassAd *setAd = NULL;  // never need to delete this!
	};

	unsigned int next_setid_num;
	std::unordered_map<unsigned int, JobSet *> mapIdToSet;
	std::unordered_map<std::string, unsigned int> mapAliasToId;

	// helper implementation methods
	JobSet* getOrCreateSet(JobQueueJob & job);
	bool removeSet(JobSet* & set);
};

#endif

