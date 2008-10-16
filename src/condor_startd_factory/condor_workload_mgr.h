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
