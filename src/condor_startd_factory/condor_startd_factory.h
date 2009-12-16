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

#ifndef CONDOR_STARTD_FACTORY_H
#define CONDOR_STARTD_FACTORY_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "extArray.h"
#include "MyString.h"
#include "condor_partition.h"
#include "condor_partition_mgr.h"
#include "condor_workload.h"
#include "condor_workload_mgr.h"
#include "XXX_startd_factory_attrs.h"

class StartdFactory : public Service
{
	public:
		StartdFactory();
		~StartdFactory();
		
		// TRUE for good init, FALSE for bad.
		int init(int argc, char *argv[]);

		// Start the daemon's work up.
		void register_timers(void);

		// The timer which initiates the daemon to try and alter the
		// BG partitions to match the workload required.
		void adjust_partitions(void);

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

extern StartdFactory g_bgd;

#endif
