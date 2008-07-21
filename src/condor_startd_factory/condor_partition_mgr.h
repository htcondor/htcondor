#ifndef CONDOR_PARTITION_MGR_H
#define CONDOR_PARTITION_MGR_H

#include "condor_common.h"
#include "extArray.h"
#include "HashTable.h"
#include "condor_workload.h"
#include "condor_workload_mgr.h"


/* This holds the partitions and knows which ones are active and in what
	order to activate them. */
class PartitionManager
{
	public:
		PartitionManager();
		~PartitionManager();

		void query_available_partitions(char *script);

		bool partition_exists(MyString name);
		void set_partition_backer(MyString part_name, MyString backer);
		MyString get_partition_backer(MyString part_name);

		void schedule_partitions(WorkloadManager &wkld_mgr, 
			char *boot_script, char *back_script, char *shutdown_script);

		void partition_realization(int &tot_smp, int &tot_dual, int &tot_vn);

		void dump(int flags);

	private:
		//////////////////////////////////////////////////////////
		// Variables
		//////////////////////////////////////////////////////////
		// partitions are activated from 0 to end of array and deactivated in
		// reverse manner.
		ExtArray<Partition> m_parts;

		// which partitions have I labeled as assigned but haven't gone
		// to backed yet?  This data must be preserved over the backing
		// of partitions in order that a partition doesn't get backed more
		// than once. If the daemon dies while a partition is assigned, then
		// a race condition happens and an overcommit could happen.
		HashTable<MyString, bool> m_assigned;

		//////////////////////////////////////////////////////////
		// Functions
		//////////////////////////////////////////////////////////
		void read_partitions(FILE *fin);

};

#endif
