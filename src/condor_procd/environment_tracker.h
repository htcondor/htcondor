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


#ifndef _ENVIRONMENT_TRACKER_H
#define _ENVIRONMENT_TRACKER_H

#include "proc_family_tracker.h"
#include "tracker_helper_list.h"
#include "condor_pidenvid.h"
#include "proc_family.h"
#include "proc_family_monitor.h"

class EnvironmentTracker : public ProcFamilyTracker {

public:

	EnvironmentTracker(ProcFamilyMonitor* pfm) : ProcFamilyTracker(pfm) { }
	virtual ~EnvironmentTracker()  {m_list.clear();}

	void add_mapping(ProcFamily* family, PidEnvID* penvid)
	{
		m_list.add_mapping(EnvironmentTag(penvid), family);
	}

	void remove_mapping(ProcFamily* family)
	{
		EnvironmentTag tmp;
		if (m_list.remove_mapping(family, &tmp)) {
			delete tmp.get_penvid();
		}
	}

	bool check_process(procInfo* pi)
	{
		ProcFamily* family = m_list.find_family(pi);
		if (family != NULL) {
			m_monitor->add_member_to_family(family, pi, "ENV");
			return true;
		}
		return false;
	}

private:
	
	class EnvironmentTag : public ProcInfoMatcher {

	public:
		EnvironmentTag() : m_penvid(NULL) { }
		EnvironmentTag(PidEnvID* penvid) : m_penvid(penvid) { }
		PidEnvID* get_penvid() { return m_penvid; }

		bool test(procInfo* pi)
		{
			return (pidenvid_match(m_penvid, &pi->penvid) == PIDENVID_MATCH);
		}

	private:
		PidEnvID* m_penvid;
	};

	TrackerHelperList<EnvironmentTag> m_list;
};

#endif
