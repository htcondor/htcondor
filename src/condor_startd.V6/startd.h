/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _CONDOR_STARTD_H
#define _CONDOR_STARTD_H

#include "condor_common.h"

#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Condor includes
#include "daemon.h"
#include "condor_classad.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_state.h"
#include "string_list.h"
#include "get_full_hostname.h"
#include "condor_random_num.h"
#include "killfamily.h"
#include "../condor_procapi/procapi.h"
#include "misc_utils.h"
#include "get_daemon_addr.h"


#if !defined(WIN32)
// Unix specific stuff
#include "sig_install.h"
#else
// Windoze specific stuff
extern "C" int WINAPI KBShutdown(void);	/* in the Kbdd DLL */
#endif

// Startd includes
class Resource;
#include "LoadQueue.h"
#include "ResAttributes.h"
#include "AvailStats.h"
#include "Match.h"
#include "Starter.h"
#include "Reqexp.h"
#include "ResState.h"
#include "Resource.h"
#include "ResMgr.h"
#include "command.h"
#include "util.h"

static const int MAX_STARTERS = 10;

#ifndef _STARTD_NO_DECLARE_GLOBALS

// Define external functions
extern int finish_main_config( void );

// Define external global variables

// Resource manager
extern	ResMgr*	resmgr;		// Pointer to the resource manager object

// Polling variables
extern	int		polling_interval;	// Interval for polling when
									// running a job
extern	int		update_interval;	// Interval to update CM

// Paths
extern	char*	exec_path;

// String Lists
extern	StringList* console_devices;
extern	StringList* startd_job_exprs;

// Hosts
extern	Daemon*	Collector;
extern	Daemon*	View_Collector;
extern	char*	condor_view_host;
extern	char*	accountant_host;

// Others
extern	int		match_timeout;	// How long you're willing to be
								// matched before claimed 
extern	int		killing_timeout;  // How long you're willing to be
	                              // in preempting/killing before you
								  // drop the hammer on the starter
extern	int		last_x_event;		// Time of the last x event
extern  time_t	startd_startup;		// Time when the startd started up

extern	int		console_vms;	// # of virtual machines in an SMP
								// machine that care when there's
								// console activity 
extern	int		keyboard_vms;	// # of virtual machines in an SMP
								// machine that care when there's
								// keyboard activity  

extern	int		disconnected_keyboard_boost;	
    // # of seconds before when we started up that we advertise as the
	// last key press for resources that aren't connected to anything.

extern	int		startd_noclaim_shutdown;	
    // # of seconds we can go without being claimed before we "pull
    // the plug" and tell the master to shutdown.

extern	char*	Name;			// The startd's name

extern	bool	compute_avail_stats;
	// should the startd compute vm availability statistics; currently 
	// false by default

extern	int		pid_snapshot_interval;	
    // How often do we take snapshots of the pid families? 

extern  int main_reaper;

#endif /* _STARTD_NO_DECLARE_GLOBALS */

#endif /* _CONDOR_STARTD_H */
