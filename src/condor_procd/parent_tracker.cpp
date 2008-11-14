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
#include "condor_debug.h"
#include "parent_tracker.h"
#include "proc_family_monitor.h"

void
ParentTracker::find_processes(procInfo*& pi_list)
{
	m_keep_checking = 1;
	while( m_keep_checking ) {
		m_keep_checking = false;
		ProcFamilyTracker::find_processes(pi_list);
	}
}

bool
ParentTracker::check_process(procInfo* pi)
{
	// first, the parent pid needs to exist in one of
	// our monitored families
	//
	ProcFamilyMember* pm = m_monitor->lookup_member(pi->ppid);
	if (pm == NULL) {
		return false;
	}

	// HACK: if the family has a root pid of 0, it means its
	// the "not in any of the ProcD's families" family
	//
	if (pm->get_proc_family()->get_root_pid() == 0) {
		return false;
	}

	// if the (supposed) parent's birthday is after our
	// own, its definitely not a valid link
	//
	if (pm->get_proc_info()->birthday > pi->birthday) {
		return false;
	}

	// looks good; now try to update the family (if no change
	// is made, add_member_to_family will return false)
	//
	if (m_monitor->add_member_to_family(pm->get_proc_family(),
	                                    pi,
	                                    "PARENT")) {
		m_keep_checking = true;
		return true;
	}

	return false;
}
