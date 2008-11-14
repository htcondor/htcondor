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
#include "proc_family_io.h"

// IMPORTANT: these string constants must match the proc_family_error_t
// enumeration (in proc_family_io.h) in both number and order
//
static const char* proc_family_error_strings[] = {
	"SUCCESS",
	"ERROR: Invalid root PID",
	"ERROR: Invalid watcher PID",
	"ERROR: Invalid snapshot interval",
	"ERROR: A family with the given root PID is already registered",
	"ERROR: No family with the given PID is registered",
	"ERROR: The given PID is not found on the system",
	"ERROR: The given PID is not part of the family tree",
	"ERROR: Attempt to unregister root family",
	"ERROR: Bad environment tracking information",
	"ERROR: Bad login tracking information",
	"ERROR: Bad information for using GLExec",
	"ERROR: No group ID available for tracking",
	"ERROR: This ProcD is not able to use GLExec"
};

// helper for looking up error strings
//
const char*
proc_family_error_lookup(proc_family_error_t error_code)
{
	if ((error_code < 0) || (error_code >= PROC_FAMILY_ERROR_MAX)) {
		return NULL;
	}
	return proc_family_error_strings[error_code];
}
