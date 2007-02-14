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
#include "proc_family.h"
#include "proc_family_monitor.h"
#include "procd_common.h"

ProcFamily::ProcFamily(ProcFamilyMonitor* monitor,
                       pid_t              root_pid,
                       birthday_t         root_birthday,
                       pid_t              watcher_pid,
                       int                max_snapshot_interval) :
	m_monitor(monitor),
	m_root_pid(root_pid),
	m_root_birthday(root_birthday),
	m_watcher_pid(watcher_pid),
	m_max_snapshot_interval(max_snapshot_interval),
	m_exited_user_cpu_time(0),
	m_exited_sys_cpu_time(0),
	m_exited_max_image_size(0),
	m_member_list(NULL)
{
}

ProcFamily::~ProcFamily()
{
	// delete our member list
	//
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		ProcFamilyMember* next_member = member->m_next;
		delete member->m_proc_info;
		delete member;
		member = next_member;
	}
}

void
ProcFamily::aggregate_usage(ProcFamilyUsage* usage)
{
	ASSERT(usage != NULL);

	// factor in usage from processes that are still alive
	//
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {

		// cpu
		//
		usage->user_cpu_time += member->m_proc_info->user_time;
		usage->sys_cpu_time += member->m_proc_info->sys_time;
		usage->percent_cpu += member->m_proc_info->cpuusage;

		// mage size
		//
		unsigned long image_size = get_image_size(member->m_proc_info);
		if (image_size > usage->max_image_size) {
			usage->max_image_size = image_size;
		}
		usage->total_image_size += member->m_proc_info->imgsize;

		// number of alive processes
		//
		usage->num_procs++;

		member = member->m_next;
	}

	// factor in usage from processes that have exited
	//
	usage->user_cpu_time += m_exited_user_cpu_time;
	usage->sys_cpu_time += m_exited_sys_cpu_time;
	if (m_exited_max_image_size > usage->max_image_size) {
		usage->max_image_size = m_exited_max_image_size;
	}
}

void
ProcFamily::spree(int sig)
{
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		send_signal(member->m_proc_info, sig);
		member = member->m_next;
	}
}

void
ProcFamily::add_member(procInfo* pi)
{
	ProcFamilyMember* member = new ProcFamilyMember;
	member->m_family = this;
	member->m_proc_info = pi;
	member->m_still_alive = false;

	// add it to our list
	//
	member->m_prev = NULL;
	member->m_next = m_member_list;
	if (m_member_list != NULL) {
		m_member_list->m_prev = member;
	}
	m_member_list = member;

	// keep our monitor's hash table up to date!
	m_monitor->add_member(member);
}

void
ProcFamily::remove_exited_processes()
{
	// our monitor will have marked the m_still_alive field of all
	// processes that are still alive on the system, so all remaining
	// processes have exited and need to be removed
	//
	ProcFamilyMember* member = m_member_list;
	ProcFamilyMember* prev = NULL;
	ProcFamilyMember** prev_ptr = &m_member_list;
	while (member != NULL) {
	
		ProcFamilyMember* next_member = member->m_next;

		if (!member->m_still_alive) {
		
			dprintf(D_ALWAYS, "%d is dead\n", member->m_proc_info->pid);

			// account for usage from this process
			//
			m_exited_user_cpu_time +=
				member->m_proc_info->user_time;
			m_exited_sys_cpu_time +=
				member->m_proc_info->sys_time;
			unsigned long image_size =
				get_image_size(member->m_proc_info);
			if (image_size > m_exited_max_image_size) {
				m_exited_max_image_size = image_size;
			}

			// keep our monitor's hash table up to date!
			//
			m_monitor->remove_member(member);

			// remove this member from our list and free up our data
			// structures
			//
			*prev_ptr = next_member;
			if (next_member != NULL) {
				next_member->m_prev = prev;
			}
			delete member->m_proc_info;
			delete member;
		}
		else {

			dprintf(D_ALWAYS, "clearing alive bit on %u\n", member->m_proc_info->pid);
		
			// clear still_alive bit for next time around
			//
			member->m_still_alive = false;

			// update our "previous" list data to point to
			// the current member
			//
			prev = member;
			prev_ptr = &member->m_next;
		}	

		// advance to the next member in the list
		//
		member = next_member;
	}
}

void
ProcFamily::give_away_members(ProcFamily* parent)
{
	// nothing to do if our member list is empty
	//
	if (m_member_list == NULL) {
		return;
	}

	// traverse our list, pointing all members at their
	// new family
	//
	ProcFamilyMember* member = m_member_list;
	while (member->m_next != NULL) {
		member->m_family = parent;
		member = member->m_next;
	}

	// attach the end of our list to the beginning of
	// the parent's
	//
	member->m_next = parent->m_member_list;
	if (member->m_next != NULL) {
		member->m_next->m_prev = member;
	}

	// make our beginning the beginning of the parent's
	// list, and then make our list empty
	//
	parent->m_member_list = m_member_list;
	m_member_list = NULL;
}

#if defined(PROCD_DEBUG)

void
ProcFamily::output(LocalServer& server)
{
	// we'll send the following back to the client:
	//   - our root pid
	//   - each pid in the list
	//   - zero to terminate the list
	//

	server.write_data(&m_root_pid, sizeof(pid_t));

	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		server.write_data(&member->m_proc_info->pid, sizeof(pid_t));
		member = member->m_next;
	}

	int zero = 0;
	server.write_data(&zero, sizeof(int));
}

#endif
