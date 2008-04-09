#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_bgd.h"


BGD::BGD()
{
	m_script_query_work_loads = NULL;
	m_script_generate_partition = NULL;
	m_script_destroy_partition = NULL;
	m_script_available_partitions = NULL;
	m_script_activate_a_partition = NULL;
	m_script_deactivate_a_partition = NULL;
	m_script_activated_partitions = NULL;
	m_script_glidein_to_partition = NULL;
}

BGD::~BGD()
{
}

/* soak up the various scripts I need to do my work */
int
BGD::init(int argc, char *argv[])
{

	m_script_query_work_loads = 
		param_or_except("BGD_SCRIPT_QUERY_WORK_LOADS");
	
	m_script_generate_partition =
		param_or_except("BGD_SCRIPT_GENERATE_PARTITION");

	m_script_destroy_partition =
		param_or_except("BGD_SCRIPT_DESTROY_PARTITION");

	m_script_available_partitions =
		param_or_except("BGD_SCRIPT_AVAILABLE_PARTITIONS");

	m_script_activate_a_partition =
		param_or_except("BGD_SCRIPT_ACTIVATE_A_PARTITION");
	
	m_script_deactivate_a_partition =
		param_or_except("BGD_SCRIPT_DEACTIVATE_A_PARTITION");

	m_script_activated_partitions =
		param_or_except("BGD_SCRIPT_ACTIVATED_PARTITIONS");

	m_script_glidein_to_partition =
		param_or_except("BGD_SCRIPT_GLIDEIN_TO_PARTITION");

	dprintf(D_ALWAYS, "BGD Initialized\n");
}

/* This timer queries the schedds, figures out how many partitions of what
	kind it needs, then starts them up  (and possibly tears them down if
	there are too many paritions available for the workload already being
	done).
*/
int
BGD::adjust_partitions(void)
{
}




