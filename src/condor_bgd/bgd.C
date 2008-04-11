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
	this->reconfig();



	dprintf(D_ALWAYS, "BGD Initialized\n");

	return TRUE;
}

void
BGD::reconfig(void)
{
	if (m_script_query_work_loads != NULL) {
		free(m_script_query_work_loads);
	}
	m_script_query_work_loads = 
		param_or_except("BGD_SCRIPT_QUERY_WORK_LOADS");
	
	if (m_script_generate_partition != NULL) {
		free(m_script_generate_partition);
	}
	m_script_generate_partition =
		param_or_except("BGD_SCRIPT_GENERATE_PARTITION");

	if (m_script_destroy_partition != NULL) {
		free(m_script_destroy_partition);
	}
	m_script_destroy_partition =
		param_or_except("BGD_SCRIPT_DESTROY_PARTITION");

	if (m_script_available_partitions != NULL) {
		free(m_script_available_partitions);
	}
	m_script_available_partitions =
		param_or_except("BGD_SCRIPT_AVAILABLE_PARTITIONS");

	if (m_script_activate_a_partition != NULL) {
		free(m_script_activate_a_partition);
	}
	m_script_activate_a_partition =
		param_or_except("BGD_SCRIPT_ACTIVATE_A_PARTITION");
	
	if (m_script_deactivate_a_partition != NULL) {
		free(m_script_deactivate_a_partition);
	}
	m_script_deactivate_a_partition =
		param_or_except("BGD_SCRIPT_DEACTIVATE_A_PARTITION");

	if (m_script_activated_partitions != NULL) {
		free(m_script_activated_partitions);
	}
	m_script_activated_partitions =
		param_or_except("BGD_SCRIPT_ACTIVATED_PARTITIONS");

	if (m_script_glidein_to_partition != NULL) {
		free(m_script_glidein_to_partition);
	}
	m_script_glidein_to_partition =
		param_or_except("BGD_SCRIPT_GLIDEIN_TO_PARTITION");
}

void
BGD::register_timers(void)
{
	daemonCore->Register_Timer( 0, 20,
		(TimerHandlercpp)&BGD::adjust_partitions,
			"BGD::adjust_partitions", this );
}

/* This timer queries the schedds, figures out how many partitions of what
	kind it needs, then starts them up  (and possibly tears them down if
	there are too many paritions available for the workload already being
	done).
*/
int
BGD::adjust_partitions(void)
{
	return TRUE;
}




