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

#include "condor_common.h"
#include "proc_family_member.h"
#include "proc_family.h"

void
ProcFamilyMember::still_alive(procInfo* pi)
{
	// update our procInfo
	//
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

	// ad add ourself to its list (we should be the only one)
	ASSERT(m_family->m_member_list == NULL);
	m_prev = m_next = NULL;
	m_family->m_member_list = this;
}
