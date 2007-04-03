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
#include "condor_debug.h"
#include "parent_tracker.h"
#include "proc_family_monitor.h"

void
ParentTracker::find_processes(procInfo*& pi_list)
{
	keep_checking = 1;
	while( keep_checking ) {
		keep_checking = false;
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
		keep_checking = true;
		return true;
	}

	return false;
}
