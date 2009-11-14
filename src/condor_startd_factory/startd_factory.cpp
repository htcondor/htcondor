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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_startd_factory.h"
#include "condor_partition.h"
#include "condor_partition_mgr.h"
#include "condor_workload.h"
#include "condor_workload_mgr.h"


StartdFactory::StartdFactory()
{
	// what partitions are available and what state are they in?
	m_script_available_partitions = NULL;

	// who has work that I need to worry about?
	m_script_query_work_loads = NULL;

	// for now, these are unused
	m_script_generate_partition = NULL;
	m_script_destroy_partition = NULL;

	// boot/shutdown a partition
	m_script_boot_partition = NULL;
	m_script_shutdown_partition = NULL;

	// make a startd back a known booted partition.
	m_script_back_partition = NULL;

	// How often I should adjust the partitions
	// defaults to 7 minutes (if init() isn't called).
	m_adjustment_interval = 60*7;
}

StartdFactory::~StartdFactory()
{
	if (m_script_available_partitions != NULL) {
		free(m_script_available_partitions);
		m_script_available_partitions = NULL;
	}

	if (m_script_query_work_loads != NULL) {
		free(m_script_query_work_loads);
		m_script_query_work_loads = NULL;
	}

	if (m_script_generate_partition != NULL) {
		free(m_script_generate_partition);
		m_script_generate_partition = NULL;
	}

	if (m_script_destroy_partition != NULL) {
		free(m_script_destroy_partition);
		m_script_destroy_partition = NULL;
	}

	if (m_script_boot_partition != NULL) {
		free(m_script_boot_partition);
		m_script_boot_partition = NULL;
	}

	if (m_script_shutdown_partition != NULL) {
		free(m_script_shutdown_partition);
		m_script_shutdown_partition = NULL;
	}

	if (m_script_back_partition != NULL) {
		free(m_script_back_partition);
		m_script_back_partition = NULL;
	}

}

int
StartdFactory::init(int argc, char *argv[])
{
	// load the config file attributes I need
	reconfig();

	dprintf(D_ALWAYS, "Startd Factory Initialized\n");

	return TRUE;
}

void
StartdFactory::reconfig(void)
{
	if (m_script_available_partitions != NULL) {
		free(m_script_available_partitions);
	}
	m_script_available_partitions =
		param_or_except("STARTD_FACTORY_SCRIPT_AVAILABLE_PARTITIONS");

	if (m_script_query_work_loads != NULL) {
		free(m_script_query_work_loads);
	}
	m_script_query_work_loads = 
		param_or_except("STARTD_FACTORY_SCRIPT_QUERY_WORK_LOADS");
	
	if (m_script_generate_partition != NULL) {
		free(m_script_generate_partition);
	}
	m_script_generate_partition =
		param_or_except("STARTD_FACTORY_SCRIPT_GENERATE_PARTITION");

	if (m_script_destroy_partition != NULL) {
		free(m_script_destroy_partition);
	}
	m_script_destroy_partition =
		param_or_except("STARTD_FACTORY_SCRIPT_DESTROY_PARTITION");

	if (m_script_boot_partition != NULL) {
		free(m_script_boot_partition);
	}
	m_script_boot_partition =
		param_or_except("STARTD_FACTORY_SCRIPT_BOOT_PARTITION");
	
	if (m_script_shutdown_partition != NULL) {
		free(m_script_shutdown_partition);
	}
	m_script_shutdown_partition =
		param_or_except("STARTD_FACTORY_SCRIPT_SHUTDOWN_PARTITION");

	if (m_script_back_partition != NULL) {
		free(m_script_back_partition);
	}
	m_script_back_partition =
		param_or_except("STARTD_FACTORY_SCRIPT_BACK_PARTITION");
	
	// default to 7 minutes if not specified
	m_adjustment_interval = 
		param_integer("STARTD_FACTORY_ADJUSTMENT_INTERVAL", 60*7);
	if (m_adjustment_interval < 20) {
		// don't let it go smaller than 20 seconds.
		m_adjustment_interval = 20;
	}
}

void
StartdFactory::register_timers(void)
{
	// This is the main workhorse of the daemon
	daemonCore->Register_Timer( 0, m_adjustment_interval,
		(TimerHandlercpp)&StartdFactory::adjust_partitions,
			"StartdFactory::adjust_partitions", this );
}


/* This timer queries the schedds, figures out how many partitions of what
	kind it needs, then starts them up (and possibly tears them down if
	there are too many partitions available for the workload already being
	done).
*/
void
StartdFactory::adjust_partitions(void)
{
	dprintf(D_ALWAYS, "=================================\n");
	dprintf(D_ALWAYS, "Adjusting partitions to workload!\n");
	dprintf(D_ALWAYS, "=================================\n");

	dprintf(D_ALWAYS, "-----------------------------\n");
	dprintf(D_ALWAYS, "0) Query Available Partitions\n");
	dprintf(D_ALWAYS, "-----------------------------\n");

	// Soak up the available partitions into a structure
	m_part_mgr.query_available_partitions(m_script_available_partitions);

	m_part_mgr.dump(D_ALWAYS);

	dprintf(D_ALWAYS, "-------------------------\n");
	dprintf(D_ALWAYS, "1) Querying for Workloads\n");
	dprintf(D_ALWAYS, "-------------------------\n");

	// get a snapshot of what the workloads are
	m_wklds_mgr.query_workloads(m_script_query_work_loads);

	m_wklds_mgr.dump(D_ALWAYS);

	dprintf(D_ALWAYS, "------------------------\n");
	dprintf(D_ALWAYS, "2) Scheduling Partitions\n");
	dprintf(D_ALWAYS, "------------------------\n");

	m_part_mgr.schedule_partitions(m_wklds_mgr,
		m_script_generate_partition,
		m_script_boot_partition,
		m_script_back_partition,
		m_script_shutdown_partition,
		m_script_destroy_partition);

	dprintf(D_ALWAYS, "-------------------\n");
	dprintf(D_ALWAYS, "Finished adjustment\n");
	dprintf(D_ALWAYS, "-------------------\n");
}








