/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#ifndef _CONDOR_STARTD_H
#define _CONDOR_STARTD_H

#include "condor_common.h"

#include "condor_daemon_core.h"

// Condor includes
#include "dc_collector.h"
#include "condor_classad.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_state.h"
#include "condor_string.h"
#include "string_list.h"
#include "MyString.h"
#include "get_full_hostname.h"
#include "condor_random_num.h"
#include "../condor_procapi/procapi.h"
//#include "misc_utils.h"
#include "get_daemon_name.h"
#include "enum_utils.h"
#include "condor_version.h"
#include "classad_command_util.h"


#if !defined(WIN32)
// Unix specific stuff
#include "sig_install.h"
#else
#include "CondorSystrayNotifier.windows.h"//for the "birdwatcher" (system tray icon)
extern CondorSystrayNotifier systray_notifier;
#endif

// Startd includes
class Resource;
#include "LoadQueue.h"
#include "ResAttributes.h"
#include "AvailStats.h"
#include "claim.h"
#include "Starter.h"
#include "Reqexp.h"
#include "ResState.h"
#include "Resource.h"
#include "ResMgr.h"
#include "command.h"
#include "util.h"
#include "starter_mgr.h"
#include "cod_mgr.h"
#include "startd_cron_job_mgr.h"
#include "startd_bench_job_mgr.h"

static const int MAX_STARTERS = 10;

#ifndef _STARTD_NO_DECLARE_GLOBALS

// Define external functions
extern void finish_main_config( void );
extern bool authorizedForCOD( const char* owner );

// Define external global variables

// Resource manager
extern	ResMgr*	resmgr;		// Pointer to the resource manager object

// Polling variables
extern	int		polling_interval;	// Interval for polling when
									// running a job
extern	int		update_interval;	// Interval to update CM
extern	int		update_offset;		// Interval offset to update CM

// String Lists
extern	StringList* console_devices;
extern	StringList* startd_job_exprs;
extern	StringList* startd_slot_attrs;

// Hosts
extern	char*	accountant_host;

// Others
extern	int		match_timeout;	// How long you're willing to be
								// matched before claimed 
extern	int		killing_timeout;  // How long you're willing to be
	                              // in preempting/killing before you
								  // drop the hammer on the starter
extern	int		max_claim_alives_missed;  // how many keepalives can we
										  // miss until we timeout the
										  // claim and give up
extern	int		last_x_event;		// Time of the last x event
extern  time_t	startd_startup;		// Time when the startd started up

extern	int		console_slots;	// # of slots in an SMP machine that
								// care when there's console activity
extern	int		keyboard_slots;	// # of slots in an SMP machine that
								// care when there's keyboard activity  

extern	int		disconnected_keyboard_boost;	
    // # of seconds before when we started up that we advertise as the
	// last key press for resources that aren't connected to anything.

extern	int		startd_noclaim_shutdown;	
    // # of seconds we can go without being claimed before we "pull
    // the plug" and tell the master to shutdown.

extern	char*	Name;			// The startd's name

extern	bool	compute_avail_stats;
	// should the startd compute slot availability statistics; currently
	// false by default

extern	int		pid_snapshot_interval;	
    // How often do we take snapshots of the pid families? 

extern  int main_reaper;

extern StartdCronJobMgr		*cron_job_mgr;
extern StartdBenchJobMgr	*bench_job_mgr;

#endif /* _STARTD_NO_DECLARE_GLOBALS */

// Check to see if we're all free
void	startd_check_free();

#endif /* _CONDOR_STARTD_H */
