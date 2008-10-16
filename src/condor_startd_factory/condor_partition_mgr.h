#ifndef CONDOR_PARTITION_MGR_H
#define CONDOR_PARTITION_MGR_H

#include "condor_common.h"
#include "extArray.h"
#include "HashTable.h"
#include "condor_workload.h"
#include "condor_workload_mgr.h"


/* Hold knowledge about partitions and what states they are in. Additionally,
	schedule when partitions should state change.
*/
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
			char *generate_script,
			char *boot_script,
			char *back_script, 
			char *shutdown_script,
			char *destroy_script);

		void partition_realization(int &tot_smp, int &tot_dual, int &tot_vn);

		void dump(int flags);

	private:
		//////////////////////////////////////////////////////////
		// Variables
		//////////////////////////////////////////////////////////
		// Each time STARTD_FACTORY_SCRIPT_AVAILABLE_PARTITIONS is invoked,
		// this array is rebuilt with the current state of the partitions.
		ExtArray<Partition> m_parts;

		// which partitions have I labeled as assigned but haven't gone to
		// backed yet?  This data must be preserved over the rebuilding of the
		// above array backing of partitions in order that a partition doesn't
		// get backed more than once. If the daemon dies while a partition is
		// assigned, then a race condition happens and an overcommit could
		// happen.
		HashTable<MyString, bool> m_assigned;

		//////////////////////////////////////////////////////////
		// Functions
		//////////////////////////////////////////////////////////
		void read_partitions(FILE *fin);

};

#endif
