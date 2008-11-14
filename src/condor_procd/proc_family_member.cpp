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
#include "proc_family_member.h"
#include "proc_family.h"

void
ProcFamilyMember::still_alive(procInfo* pi)
{
	// update our procInfo
	//
	//dprintf(D_ALWAYS,
	//        "PROCINFO DEALLOCATION: %p for pid %u\n",
	//        m_proc_info,
	//        m_proc_info->pid);
	delete m_proc_info;
	m_proc_info = pi;

	// mark ourselves as still alive so we don't get
	// deleted when remove_exited_processes is called
	//
	m_still_alive = true;
}

void
ProcFamilyMember::move_to_subfamily(ProcFamily* subfamily)
{
	// first remove ourselves from our current parent's list
	//
	if (m_prev) {
		m_prev->m_next = m_next;
	}
	else {
		m_family->m_member_list = m_next;
	}
	if (m_next) {
		m_next->m_prev = m_prev;
	}

	// change ourself to the new family
	m_family = subfamily;

	// and add ourself to its list
	m_prev = NULL;
	m_next = m_family->m_member_list;
	if (m_family->m_member_list != NULL) {
		m_family->m_member_list->m_prev = this;
	}
	m_family->m_member_list = this;
}
