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
#include "condor_classad.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_state.h"
#include "string_list.h"

// Unix specific stuff
#if !defined(WIN32)
#include "afs.h"
#include "sig_install.h"
#endif

// Startd includes
#include "Match.h"
#include "Starter.h"
#include "Reqexp.h"
class Resource;
#include "ResState.h"
#include "ResAttributes.h"
#include "Resource.h"
#include "ResMgr.h"
#include "calc.h"
#include "command.h"
#include "util.h"

static const int MAX_STARTERS = 10;

#ifndef _STARTD_NO_DECLARE_GLOBALS
// Define external global variables

// Resource manager
extern	ResMgr*	resmgr;		// Pointer to the resource manager object

// Polling variables
extern	int		polling_interval;	// Interval for polling when
									// running a job
extern	int		update_interval;	// Interval to update CM

// Paths
extern	char*	exec_path;

// Sting Lists
extern	StringList* console_devices;
extern	StringList* startd_job_exprs;

// Starter paths
extern	char*	PrimaryStarter;
extern	char*	AlternateStarter[10];
extern	char*	RealStarter;

// Hosts
extern	char*	negotiator_host;
extern	char*	collector_host;
extern	char*	condor_view_host;
extern	char*	accountant_host;

// Others
extern	int		match_timeout;	// How long you're willing to be
								// matched before claimed 
extern	int		killing_timeout;  // How long you're willing to be
	                              // in preempting/killing before you
								  // drop the hammer on the starter
extern	int		last_x_event;	// Time of the last x event

#endif _STARTD_NO_DECLARE_GLOBALS

#endif _CONDOR_STARTD_H
