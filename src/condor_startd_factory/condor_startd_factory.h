#ifndef CONDOR_BGD_H
#define CONDOR_BGD_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "extArray.h"
#include "MyString.h"
#include "condor_partition.h"
#include "condor_partition_mgr.h"
#include "condor_workload.h"
#include "condor_workload_mgr.h"
#include "XXX_startd_factory_attrs.h"

class BGD : public Service
{
	public:
		BGD();
		~BGD();
		
		// TRUE for good init, FALSE for bad.
		int init(int argc, char *argv[]);

		// Start the daemon's work up.
		void register_timers(void);

		// The timer which initiates the daemon to try and alter the
		// BG partitions to match the workload required.
		int adjust_partitions(void);

		// pull in all of the config file variables.
		void reconfig(void);

	private:
		////////////////////////////////////////////////////////////////
		// Functions
		////////////////////////////////////////////////////////////////
		

		////////////////////////////////////////////////////////////////
		// Variables
		////////////////////////////////////////////////////////////////

		// what partitions are available and what state are they in?
		char *m_script_available_partitions;

		// who has work that I need to worry about?
		char *m_script_query_work_loads;

		// for now, these are unused
		char *m_script_generate_partition;
		char *m_script_destroy_partition;

		// boot/shutdown a partition
		char *m_script_boot_partition;
		char *m_script_shutdown_partition;

		// make a startd back a known booted partition.
		char *m_script_back_partition;

		// how often the partition adjustments should happen in seconds.
		int m_adjustment_interval;

		// Holds the partition data of what is available on the BG/P
		PartitionManager	m_part_mgr;

		// Holds the current snapshot of the schedd's work loads and
		// computes how to alter the partition manager to satisfy the
		// entire workload set.
		WorkloadManager		m_wklds_mgr;
};

extern BGD g_bgd;

#endif
