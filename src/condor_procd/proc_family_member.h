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


#ifndef _PROC_FAMILY_MEMBER_H
#define _PROC_FAMILY_MEMBER_H

#include "../condor_procapi/procapi.h"

class ProcFamily;

class ProcFamilyMember {

	friend class ProcFamily;

public:
	ProcFamily* get_proc_family() { return m_family; }

	procInfo* get_proc_info() { return m_proc_info; }

	// this is called by ProcFamilyMonitor::takesnapshot()
	// to update the procInfo struct and to indicate that
	// this process has not yet exited
	//
	void still_alive(procInfo*);

	// this is called from ProcFamilyMonitor::register_subfamily
	// to move a process into the newly-registered subfamily
	// (of which it will be the "root" process)
	//
	void move_to_subfamily(ProcFamily*);

private:
	ProcFamily*       m_family;
	procInfo*         m_proc_info;
	ProcFamilyMember* m_prev;
	ProcFamilyMember* m_next;
	bool              m_still_alive;
};

#endif
