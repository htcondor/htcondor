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

#ifndef _ENVIRONMENT_TRACKER_H
#define _ENVIRONMENT_TRACKER_H

#include "condor_pidenvid.h"
#include "proc_family_tracker.h"
#include "proc_family.h"

class EnvironmentTracker : public ProcFamilyTracker {

public:

	EnvironmentTracker(ProcFamilyMonitor* pfm) : ProcFamilyTracker(pfm) { }

	void add_mapping(ProcFamily* family, PidEnvID* penvid)
	{
		PidEnvID* tmp = new PidEnvID;
		ASSERT(tmp != NULL);
		pidenvid_copy(tmp, penvid);
		m_list.add_mapping(EnvironmentTag(tmp), family);
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
			family->add_member(pi);
			return true;
		}
		return false;
	}

private:
	
	class EnvironmentTag : public ProcInfoMatcher {

	public:
		EnvironmentTag() { }
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
