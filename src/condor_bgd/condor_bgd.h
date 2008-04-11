#ifndef CONDOR_BGD_H
#define CONDOR_BGD_H

#include "extArray.h"
#include "MyString.h"
#include "file_transfer.h"
#include "condor_transfer_request.h"
#include "exit.h"

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

		void reconfig(void);

	private:
		// Various scripts which do the work.
		char *m_script_query_work_loads;
		char *m_script_generate_partition;
		char *m_script_destroy_partition;
		char *m_script_available_partitions;
		char *m_script_activate_a_partition;
		char *m_script_deactivate_a_partition;
		char *m_script_activated_partitions;
		char *m_script_glidein_to_partition;
};

/*
    daemonCore->Register_Timer( 0, 20,
	        (TimerHandlercpp)&TransferD::exit_due_to_inactivity_timer,
			        "TransferD::exit_due_to_inactivity_timer", this );
int
TransferD::exit_due_to_inactivity_timer(void)
*/

extern BGD g_bgd;

#endif
