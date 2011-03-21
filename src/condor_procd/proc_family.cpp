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
#include "proc_family.h"
#include "proc_family_monitor.h"
#include "procd_common.h"

#if !defined(WIN32)
#include "glexec_kill.unix.h"
#endif

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
	m_max_image_size(0),
	m_member_list(NULL)
{
#if !defined(WIN32)
	m_proxy = NULL;
#endif
}

ProcFamily::~ProcFamily()
{
	// delete our member list
	//
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		ProcFamilyMember* next_member = member->m_next;
		//dprintf(D_ALWAYS,
		//        "PROCINFO DEALLOCATION: %p for pid %u\n",
		//        member->m_proc_info,
		//        member->m_proc_info->pid);
		delete member->m_proc_info;
		delete member;
		member = next_member;
	}

#if !defined(WIN32)
	// delete the proxy if we've been given one
	//
	if (m_proxy != NULL) {
		free(m_proxy);
	}
#endif
}

unsigned long
ProcFamily::update_max_image_size(unsigned long children_imgsize)
{
	// add image sizes from our processes to the total image size from
	// our child families
	//
	unsigned long imgsize = children_imgsize;
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
#if defined(WIN32)
		// comment copied from the older process tracking logic
		// (before the ProcD):
		//    On Win32, the imgsize from ProcInfo returns exactly
		//    what it says.... this means we get all the bytes mapped
		//    into the process image, incl all the DLLs. This means
		//    a little program returns at least 15+ megs. The ProcInfo
		//    rssize is much closer to what the TaskManager reports,
		//    which makes more sense for now.
		imgsize += member->m_proc_info->rssize;
#else
		imgsize += member->m_proc_info->imgsize;
#endif
		member = member->m_next;
	}

	// update m_max_image_size if we have a new max
	//
	if (imgsize > m_max_image_size) {
		m_max_image_size = imgsize;
	}

	// finally, return our _current_ total image size
	//
	return imgsize;
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

		// current total image size
		//
		usage->total_image_size += member->m_proc_info->imgsize;
		usage->total_resident_set_size += member->m_proc_info->rssize;

		// number of alive processes
		//
		usage->num_procs++;

		member = member->m_next;
	}

	// factor in CPU usage from processes that have exited
	//
	usage->user_cpu_time += m_exited_user_cpu_time;
	usage->sys_cpu_time += m_exited_sys_cpu_time;
}

void
ProcFamily::signal_root(int sig)
{
#if !defined(WIN32)
	if (m_proxy != NULL) {
		glexec_kill(m_proxy, m_root_pid, sig);
		return;
	}
#endif
	send_signal(m_root_pid, sig);
}

void
ProcFamily::spree(int sig)
{
	ProcFamilyMember* member;
	for (member = m_member_list; member != NULL; member = member->m_next) {
#if !defined(WIN32)
		if (m_proxy != NULL) {
			glexec_kill(m_proxy,
			            member->m_proc_info->pid,
			            sig);
			continue;
		}
#endif
		send_signal(member->m_proc_info->pid, sig);
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

			// HACK for logging: if our root pid is 0, we
			// know this to mean that we are actually the
			// family that holds all processes that aren't
			// in the monitored family; this hack should go
			// away when we pull out a separate ProcGroup
			// class
			//
			if (m_root_pid != 0) {
				dprintf(D_ALWAYS,
				        "process %u (of family %u) has exited\n",
				        member->m_proc_info->pid,
				        m_root_pid);
			}
			else {
				dprintf(D_ALWAYS,
				        "process %u (not in monitored family) has exited\n",
				        member->m_proc_info->pid);
			}

			// save CPU usage from this process
			//
			m_exited_user_cpu_time +=
				member->m_proc_info->user_time;
			m_exited_sys_cpu_time +=
				member->m_proc_info->sys_time;

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
			//dprintf(D_ALWAYS,
			//        "PROCINFO DEALLOCATION: %p for pid %u\n",
			//        member->m_proc_info,
			//        member->m_proc_info->pid);
			delete member->m_proc_info;
			delete member;
		}
		else {

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
ProcFamily::fold_into_parent(ProcFamily* parent)
{
	// fold in CPU usage info from our dead processes
	//
	parent->m_exited_user_cpu_time += m_exited_user_cpu_time;
	parent->m_exited_sys_cpu_time += m_exited_sys_cpu_time;

	// nothing left to do if our member list is empty
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
	member->m_family = parent;

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

#if !defined(WIN32)
void
ProcFamily::set_proxy(char* proxy)
{
	if (m_proxy != NULL) {
		free(m_proxy);
	}
	m_proxy = strdup(proxy);
	ASSERT(m_proxy != NULL);
}
#endif

void
ProcFamily::dump(ProcFamilyDump& fam)
{
	ProcFamilyMember* member = m_member_list;
	while (member != NULL) {
		ProcFamilyProcessDump proc;
		proc.pid = member->m_proc_info->pid;
		proc.ppid = member->m_proc_info->ppid;
		proc.birthday = member->m_proc_info->birthday;
		proc.user_time = member->m_proc_info->user_time;
		proc.sys_time = member->m_proc_info->sys_time;
		fam.procs.push_back(proc);
		member = member->m_next;
	}
}
