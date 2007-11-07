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
