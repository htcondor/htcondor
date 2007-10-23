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

#ifndef _LOGIN_TRACKER_H
#define _LOGIN_TRACKER_H

#include "proc_family_tracker.h"
#include "tracker_helper_list.h"
#include "proc_family.h"
#include "proc_family_monitor.h"

class LoginTracker : public ProcFamilyTracker {

public:

	LoginTracker(ProcFamilyMonitor* pfm) : ProcFamilyTracker(pfm) { }

	void add_mapping(ProcFamily* family, char* login)
	{
		m_list.add_mapping(LoginTag(login), family);
	}

	void remove_mapping(ProcFamily* family)
	{
		LoginTag tmp;
		if (m_list.remove_mapping(family, &tmp)) {
			delete[] tmp.get_login();
		}
	}

	bool check_process(procInfo* pi)
	{
		ProcFamily* family = m_list.find_family(pi);
		if (family != NULL) {
			m_monitor->add_member_to_family(family, pi, "LOGIN");
			return true;
		}
		return false;
	}

private:

	class LoginTag : public ProcInfoMatcher {

	public:
		LoginTag() { }
		LoginTag(char* login);
		char* get_login() { return m_login; }
		bool test(procInfo*);

	private:
		char* m_login;
#if !defined(WIN32)
		uid_t m_uid;
#endif
	};

	TrackerHelperList<LoginTag> m_list;
};

#endif
