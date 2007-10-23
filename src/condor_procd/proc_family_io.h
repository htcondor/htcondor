/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef _PROC_FAMILY_IO_H
#define _PROC_FAMILY_IO_H

#include "condor_common.h"

// command identifiers for communication with the ProcD
//
enum proc_family_command_t {
	PROC_FAMILY_REGISTER_SUBFAMILY,
	PROC_FAMILY_TRACK_FAMILY_VIA_ENVIRONMENT,
	PROC_FAMILY_TRACK_FAMILY_VIA_LOGIN,
	PROC_FAMILY_TRACK_FAMILY_VIA_SUPPLEMENTARY_GROUP,
	PROC_FAMILY_SIGNAL_PROCESS,
	PROC_FAMILY_SUSPEND_FAMILY,
	PROC_FAMILY_CONTINUE_FAMILY,
	PROC_FAMILY_KILL_FAMILY,
	PROC_FAMILY_GET_USAGE,
	PROC_FAMILY_UNREGISTER_FAMILY,
	PROC_FAMILY_TAKE_SNAPSHOT,
	PROC_FAMILY_DUMP,
	PROC_FAMILY_QUIT
};

// return codes for ProcD operations
//
// IMPORTANT: if you ever add a value to this enum, you MUST also add
// an entry to the proc_family_error_strings array (defined in
// proc_family_io.C)
//
enum proc_family_error_t {
	PROC_FAMILY_ERROR_SUCCESS,
	PROC_FAMILY_ERROR_BAD_ROOT_PID,
	PROC_FAMILY_ERROR_BAD_WATCHER_PID,
	PROC_FAMILY_ERROR_BAD_SNAPSHOT_INTERVAL,
	PROC_FAMILY_ERROR_ALREADY_REGISTERED,
	PROC_FAMILY_ERROR_FAMILY_NOT_FOUND,
	PROC_FAMILY_ERROR_PROCESS_NOT_FOUND,
	PROC_FAMILY_ERROR_PROCESS_NOT_FAMILY,
	PROC_FAMILY_ERROR_UNREGISTER_ROOT,
	PROC_FAMILY_ERROR_BAD_ENVIRONMENT_INFO,
	PROC_FAMILY_ERROR_BAD_LOGIN_INFO,
	PROC_FAMILY_ERROR_NO_GROUP_ID_AVAILABLE
};

// an array of readable string representations of the above
// return codes
//
extern const char* proc_family_error_strings[];

// structure for retrieving process family usage data from
// the ProcD
//
struct ProcFamilyUsage {
	long          user_cpu_time;
	long          sys_cpu_time;
	double        percent_cpu;
	unsigned long max_image_size;
	unsigned long total_image_size;
	int           num_procs;
};

#endif
