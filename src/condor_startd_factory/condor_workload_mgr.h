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

#ifndef CONDOR_WORKLOAD_MGR_H
#define CONDOR_WORKLOAD_MGR_H

#include "condor_common.h"
#include "extArray.h"

class ParitionManager;

// Each of these objects describes a single resource which has a workload for
// the various kinds of partitions the BG can do. Each workload may also be
// controlling partitions assigned to it.

class WorkloadManager
{
	public:
		WorkloadManager();
		~WorkloadManager();

		void query_workloads(char *script);

		void dump(int flags);

		// fill the parameters with number of idle in each catagory.
		void total_idle(int &smp, int &dual, int &vn);

	private:
		// An array of workloads.
		ExtArray<Workload> m_wklds;
};

#endif
