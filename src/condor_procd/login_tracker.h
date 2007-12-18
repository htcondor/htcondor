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
		LoginTag() : m_login(NULL)
		{
#if !defined(WIN32)
			m_uid = 0;
#endif
		}
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
