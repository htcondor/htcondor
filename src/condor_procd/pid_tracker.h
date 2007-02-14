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

#ifndef _PID_TRACKER_H
#define _PID_TRACKER_H

#include "proc_family_tracker.h"
#include "tracker_helper_list.h"

class ProcFamily;

class PIDTracker : public ProcFamilyTracker {

public:

	PIDTracker(ProcFamilyMonitor* pfm) : ProcFamilyTracker(pfm) { }

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
			family->add_member(pi);
			return true;
		}
		return false;
	}

private:
	
	class PIDTag : public ProcInfoMatcher {

	public:
		PIDTag() { }
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
