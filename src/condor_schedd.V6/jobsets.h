
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

	JobSets() {};

	~JobSets() = default;

	// called when we commit a transaction to add the cluster to the jobset
	// and on startup to initialize the jobset membership counters and the ids of the jobs
	bool addToSet(JobQueueJob & job);

	void status_change(JobQueueJob & job, int new_status);

	bool removeJobFromSet(JobQueueJob & job);

	void reconfig();

	bool restoreJobSet(JobQueueJobSet *ad);

	bool garbageCollectSet(JobQueueJobSet* & ad);

	size_t count() { return mapAliasToId.size(); };

	int find(const std::string & alias) {
		auto it = mapAliasToId.find(alias);
		if (it != mapAliasToId.end()) return it->second;
		return 0;
	}

	static std::string makeAlias(const std::string name, const std::string owner) {
		std::string ret = name + "/" + owner;
		lower_case(ret);
		return ret;
	}
	
private:

	std::unordered_map<std::string, unsigned int> mapAliasToId;

	// helper implementation methods
	bool removeSet(JobQueueJobSet* & jobset);
};

#endif

