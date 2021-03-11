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


#ifndef _PID_TRACKER_H
#define _PID_TRACKER_H

#include "proc_family_tracker.h"
#include "tracker_helper_list.h"
#include "proc_family_monitor.h"

class ProcFamily;

class PIDTracker : public ProcFamilyTracker {

public:

	PIDTracker(ProcFamilyMonitor* pfm) : ProcFamilyTracker(pfm) { }
	virtual ~PIDTracker() { m_list.clear();}

	void add_mapping(ProcFamily* family, pid_t pid, birthday_t birthday)
	{
		m_list.add_mapping(PIDTag(pid, birthday), family);
	}

	void remove_mapping(ProcFamily* family)
	{
		m_list.remove_mapping(family);
	}

	bool check_process(procInfo* pi)
	{
		ProcFamily* family = m_list.find_family(pi);
		if (family != NULL) {
			m_monitor->add_member_to_family(family, pi, "PID");
			return true;
		}
		return false;
	}

private:
	
	class PIDTag : public ProcInfoMatcher {

	public:
		PIDTag() : m_pid(0), m_birthday(0) { }
		PIDTag(pid_t p, birthday_t b) : m_pid(p), m_birthday(b) { }

		bool test(procInfo* pi) {
			return ((pi->pid == m_pid) && (pi->birthday == m_birthday));
		}

	private:
		pid_t m_pid;
		birthday_t m_birthday;
	};

	TrackerHelperList<PIDTag> m_list;
};

#endif
