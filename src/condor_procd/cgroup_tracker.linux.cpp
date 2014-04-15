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


#include "condor_common.h"
#include "cgroup_tracker.linux.h"
#include "proc_family_monitor.h"

#if defined(HAVE_EXT_LIBCGROUP)
CGroupTracker::CGroupTracker(ProcFamilyMonitor* pfm) :
	ProcFamilyTracker(pfm)
{
}

bool
CGroupTracker::add_mapping(ProcFamily* family, const char * cgroup)
{
	int err = family->set_cgroup(cgroup);
	if (!err) {
		m_cgroup_pool[cgroup] = family;
		return true;
	}
	return false;
}

bool
CGroupTracker::remove_mapping(ProcFamily* family)
{
	// O(n) iteration through the map
	std::map<std::string, ProcFamily*>::const_iterator end = m_cgroup_pool.end();
	for (std::map<std::string, ProcFamily*>::const_iterator it = m_cgroup_pool.begin(); it != end; ++it) {
		if (it->second == family) {
			m_cgroup_pool.erase(it->first);
			return true;
		}
	}
	return false;
}

bool
CGroupTracker::check_process(procInfo* pi)
{
	// Really, the cgroup name should be part of the procInfo
	// However, it is a string, and would lead to lots of allocation
	// issues in the current code.
	// The majority of this is code from the GroupTracker.

	// prepare the pathname to open: /proc/<pid>/cgroup
	//
	char path[32];
	int ret = snprintf(path, 32, "/proc/%u/cgroup", pi->pid);
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
			"GroupTracker (pid = %u): fopen error: Failed to open file '%s'. Error %s (%d)\n",
			pi->pid,
			path,
			strerror(errno),
			errno);
		return false;
	}

	char buffer[1024];
	bool found_cgroup = false;
	std::map<std::string, ProcFamily*>::const_iterator end = m_cgroup_pool.end();
	while (fgets(buffer, 1024, fp)) {
		// Iterate through all our keys
		for (std::map<std::string, ProcFamily*>::const_iterator it = m_cgroup_pool.begin(); it != end; ++it) {
			char *pos = 0;
			if ((pos = strstr(buffer, it->first.c_str()))) {
				pos++;
				// this cgroup name is at least a substring of the cgroup, 
				// make sure it is a full match, not a substring
				if (strlen(pos) == strlen(it->first.c_str())) {
					m_monitor->add_member_to_family(it->second, pi, "CGROUP");
					found_cgroup = true;
				}
			}
		}
	}
	if (!found_cgroup) {
		if (!feof(fp)) {
			dprintf(D_ALWAYS,
				"CGroupTracker (pid = %u): "
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
	return true;
}

#endif
