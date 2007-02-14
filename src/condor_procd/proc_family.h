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

#ifndef _PROC_FAMILY_H
#define _PROC_FAMILY_H

#include "../condor_procapi/procapi.h"
#include "proc_family_member.h"
#include "proc_family_io.h"

#if defined(PROCD_DEBUG)
#include "local_server.h"
#endif

class ProcFamilyMonitor;

class ProcFamily {

friend class ProcFamilyMember;

public:
	// create a ProcFamily that's rooted at the process which has
	// the given ppid/birthday, has been registered by the given
	// ppid, requires the given maximum interval in between snapshots,
	// and that we'll optionally track via environment variables or
	// user login
	//
	ProcFamily(ProcFamilyMonitor* monitor,
	           pid_t              root_pid,
	           birthday_t         root_birthday,
	           pid_t              watcher_pid,
	           int                max_snapshot_interval);

	// cleanup time!
	//
	~ProcFamily();

	// accessor for the "root" process PID
	//
	pid_t get_root_pid() { return m_root_pid; };

	// accessor for the "root" process birthday
	//
	birthday_t get_root_birthday() { return m_root_birthday; }

	// accessor for the "watcher" process PID
	//
	pid_t get_watcher_pid() { return m_watcher_pid; };

	// accessor for the requested maximum snapshot interval
	//
	int get_max_snapshot_interval() { return m_max_snapshot_interval; }

	// fill in usage information about this family
	//
	void aggregate_usage(ProcFamilyUsage*);

	// send a signal to all processes in the family
	//
	void spree(int);

	// add a new family member
	//
	void add_member(procInfo*);

	// deal with processes that are no longer on the system: account for
	// their resource usage, remove them from our bookkeeping data, and
	// free up their data structures
	//
	void remove_exited_processes();

	// our monitor is about to delete us, so we need to offload any
	// members we have in our list to our parent (passed in)
	//
	void give_away_members(ProcFamily*);

#if defined(PROCD_DEBUG)
	// output the PIDs of all processes in this family
	//
	void output(LocalServer&);
#endif

private:
	// we need a pointer to the monitor that's tracking us since we
	// help maintain its hash table
	//
	ProcFamilyMonitor* m_monitor;

	// information about our "root" process
	//
	pid_t      m_root_pid;
	birthday_t m_root_birthday;

	// the pid of the process that has registered us (should be the parent
	// of our root process)
	//
	pid_t m_watcher_pid;

	// the maximum time in between snapshots. this will be honored by
	// higher layers in the procd.
	//
	int m_max_snapshot_interval;

	// usage from exited processes
	//
	long          m_exited_user_cpu_time;
	long          m_exited_sys_cpu_time;
	unsigned long m_exited_max_image_size;

	// lists of member processes; at the beginning of a snapshot (i.e.
	// when takesnapshot() in our containing ProcFamilyMonitor begins
	// executing, all out family members will be cotained in
	// m_member_list. the snapshot algorithm will call the still_alive()
	// method on any Member whose process is still alive on the system,
	// which will move the Member object over to m_alive_member_list.
	// finally, when remove_exited_processes() is called at the end of
	// takesnapshot(), any processes still in m_member_list will be
	// consdered to have exited
	//
	ProcFamilyMember* m_member_list;
};

#endif
