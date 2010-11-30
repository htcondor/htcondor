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
#include "group_tracker.linux.h"
#include "proc_family_monitor.h"

GroupTracker::GroupTracker(ProcFamilyMonitor* pfm,
                           gid_t min_gid,
                           gid_t max_gid) :
	ProcFamilyTracker(pfm),
	m_gid_pool(min_gid, max_gid)
{
}

bool
GroupTracker::add_mapping(ProcFamily* family, gid_t& gid)
{
	return m_gid_pool.allocate(family, gid);
}

bool
GroupTracker::remove_mapping(ProcFamily* family)
{
	return m_gid_pool.free(family);
}

bool
GroupTracker::check_process(procInfo* pi)
{
	static const char GROUPS_PREFIX[] = "Groups:\t";
	static const int GROUPS_PREFIX_SIZE = sizeof(GROUPS_PREFIX) - 1;

	// prepare the pathname to open: /proc/<pid>/status
	//
	char path[32];
	int ret = snprintf(path, 32, "/proc/%u/status", pi->pid);
	if (ret < 0) {
		dprintf(D_ALWAYS,
		        "GroupTracker (pid = %u): snprintf error: %s (%d)\n",
		        pi->pid,
		        strerror(errno),
		        errno);
		return false;
	}
	if (ret >= 32) {
		dprintf(D_ALWAYS,
		        "GroupTracker (pid = %u): error: path buffer too small\n",
		        pi->pid);
		return false;
	}

	// do the fopen
	//
	FILE* fp = safe_fopen_wrapper(path, "r");
	if (fp == NULL) {
		dprintf(D_ALWAYS,
		        "GroupTracker (pid = %u): fopen error: %s (%d)\n",
		        pi->pid,
		        strerror(errno),
		        errno);
		return false;
	}

	// look for the line starting with "Groups:"
	//
	char buffer[1024];
	bool found_groups = false;
	while (fgets(buffer, 1024, fp)) {
		if (strncmp(buffer, GROUPS_PREFIX, GROUPS_PREFIX_SIZE) == 0) {
			found_groups = true;
			break;
		}
	}
	if (!found_groups) {
		if (feof(fp)) {
			dprintf(D_ALWAYS,
			        "GroupTracker (pid = %u): "
			            "groups not found in status file\n",
			        pi->pid);
		}
		else {
			dprintf(D_ALWAYS,
			        "GroupTracker (pid = %u): "
			            "error reading from status file: %s (%d)\n",
			        pi->pid,
			        strerror(errno),
			        errno);
		}
		fclose(fp);
		return false;
	}
	fclose(fp);

	// make sure we didn't get a partial line
	//
	int line_len = strlen(buffer);
	if (buffer[line_len - 1] != '\n') {
		dprintf(D_ALWAYS,
		        "GroupTracker (pid = %u): "
		            "read partial line from status file\n",
		        pi->pid);
		return false;
	}
	buffer[line_len - 1] = '\0';

	// start scanning the "Groups:" line for any IDs that we may be tracking
	//
	char* buffer_ptr = buffer + GROUPS_PREFIX_SIZE;
	char* strtok_ptr;
	char* token = strtok_r(buffer_ptr, " ", &strtok_ptr);
	while (token != NULL) {
		ProcFamily* family = m_gid_pool.get_family((gid_t)atoi(token));
		if (family != NULL) {
			m_monitor->add_member_to_family(family, pi, "GROUP");
			return true;
		}
		token = strtok_r(NULL, " ", &strtok_ptr);
	}

	// didn't find any of our tracking groups for this process
	//
	return false;
}
