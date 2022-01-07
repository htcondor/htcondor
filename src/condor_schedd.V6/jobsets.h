
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

	JobSets() : next_setid_num(1) {};

	~JobSets() = default;

	//enum class garbagePolicyEnum { immediateAfterEmpty, delayedAferEmpty };

	bool addJobToSet(JobQueueJob & job, std::vector<unsigned int> & new_ids);

	void status_change(JobQueueJob & job, int new_status);

	bool removeJobFromSet(JobQueueJob & job);

	void setNextSetId(int id);

	void reconfig();

	bool restoreJobSet(JobQueueJobSet *ad);

	bool garbageCollectSet(JobQueueJobSet* & ad);

	int getOrCreateSet(const std::string & setName, const std::string & user, std::vector<unsigned int> & new_ids);

	size_t count() { return mapAliasToId.size(); };

	static std::string makeAlias(const std::string name, const std::string owner) {
		std::string ret = name + "/" + owner;
		lower_case(ret);
		return ret;
	}
	
private:

	unsigned int next_setid_num;
	std::unordered_map<std::string, unsigned int> mapAliasToId;

	// helper implementation methods
	JobQueueJobSet* getSet(JobQueueJob & job);
	bool removeSet(JobQueueJobSet* & jobset);
};

#endif

