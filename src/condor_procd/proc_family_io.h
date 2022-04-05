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
#include "../condor_procapi/procapi.h"

#include <vector>

// command identifiers for communication with the ProcD
//
enum proc_family_command_t {
	PROC_FAMILY_REGISTER_SUBFAMILY,
	PROC_FAMILY_TRACK_FAMILY_VIA_ENVIRONMENT,
	PROC_FAMILY_TRACK_FAMILY_VIA_LOGIN,
	PROC_FAMILY_TRACK_FAMILY_VIA_ALLOCATED_SUPPLEMENTARY_GROUP,
	PROC_FAMILY_TRACK_FAMILY_VIA_ASSOCIATED_SUPPLEMENTARY_GROUP,
	PROC_FAMILY_USE_GLEXEC_FOR_FAMILY,
	PROC_FAMILY_SIGNAL_PROCESS,
	PROC_FAMILY_SUSPEND_FAMILY,
	PROC_FAMILY_CONTINUE_FAMILY,
	PROC_FAMILY_KILL_FAMILY,
	PROC_FAMILY_GET_USAGE,
	PROC_FAMILY_UNREGISTER_FAMILY,
	PROC_FAMILY_TAKE_SNAPSHOT,
	PROC_FAMILY_DUMP,
	PROC_FAMILY_QUIT,
	PROC_FAMILY_TRACK_FAMILY_VIA_CGROUP
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
	PROC_FAMILY_ERROR_BAD_GLEXEC_INFO,
	PROC_FAMILY_ERROR_NO_GROUP_ID_AVAILABLE,
	PROC_FAMILY_ERROR_NO_GLEXEC,
	PROC_FAMILY_ERROR_NO_CGROUP_ID_AVAILABLE,
	PROC_FAMILY_ERROR_MAX
};

// returns readable string representations for the above error codes
//
const char* proc_family_error_lookup(proc_family_error_t);

// structure for retrieving process family usage data from
// the ProcD
//
struct ProcFamilyUsage;
struct ProcFamilyUsage {
	long          user_cpu_time;
	long          sys_cpu_time;
	double        percent_cpu;
	unsigned long max_image_size;
	unsigned long total_image_size;
	unsigned long total_resident_set_size;
#if HAVE_PSS
	unsigned long total_proportional_set_size;
	bool total_proportional_set_size_available;
	// These exist to make valgrind quit whining.
	bool pad1;
	bool pad2;
	bool pad3;
#endif
	int           num_procs;
	// These are signed so a negative number indicates uninitialized
	int64_t          block_reads;
	int64_t          block_writes;
	int64_t          block_read_bytes;
	int64_t          block_write_bytes;
	int64_t          m_instructions;
	double           io_wait;;

	ProcFamilyUsage() :
		user_cpu_time(0),
		sys_cpu_time(0),
		percent_cpu(0),
		max_image_size(0),
		total_image_size(0),
		total_resident_set_size(0),
#if HAVE_PSS
		total_proportional_set_size(0),
		total_proportional_set_size_available(0),
		pad1(0), pad2(0), pad3(0),
#endif
		num_procs(0),
		block_reads(0),
		block_writes(0),
		block_read_bytes(0),
		block_write_bytes(0),
		m_instructions(0),
		io_wait(0.0)
	{ }

	struct ProcFamilyUsage & operator += ( const struct ProcFamilyUsage & other ) {
		// These are cumulative.  percent_cpu is the normalized total.
		user_cpu_time += other.user_cpu_time;
		sys_cpu_time += other.sys_cpu_time;
		percent_cpu += other.percent_cpu;

		block_reads += other.block_reads;
		block_writes += other.block_writes;
		block_read_bytes += other.block_read_bytes;
		block_write_bytes += other.block_write_bytes;
		io_wait += other.io_wait;

		// These are current.
		num_procs = other.num_procs;
		m_instructions = other.m_instructions;

		// These are job maxima.  (Although we only document max_image_size,
		// total_resident_set_size, and total_proportional_set_size_available
		// as such, that's because those are the only ones we report.)
		max_image_size = MAX( max_image_size, other.max_image_size );
		total_image_size = MAX( total_image_size, other.total_image_size) ;
		total_resident_set_size = MAX( total_resident_set_size, other.total_resident_set_size );

#if HAVE_PSS
		total_proportional_set_size = MAX( total_proportional_set_size, other.total_proportional_set_size );
		total_proportional_set_size_available = MAX( total_proportional_set_size_available, other.total_proportional_set_size_available );
#endif

		return *this;
	}

};

// structures for retrieving a state dump from the ProcD
//
struct ProcFamilyProcessDump {
	pid_t      pid;
	pid_t      ppid;
	birthday_t birthday;
	long       user_time;
	long       sys_time;
};
struct ProcFamilyDump {
	pid_t                              parent_root;
	pid_t                              root_pid;
	pid_t                              watcher_pid;
	std::vector<ProcFamilyProcessDump> procs;
};

#endif
