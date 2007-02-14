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
