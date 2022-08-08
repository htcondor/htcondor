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

#ifndef _PROC_FAMILY_MONITOR_H
#define _PROC_FAMILY_MONITOR_H

#include "condor_common.h"
#include "../condor_procapi/procapi.h"
#include "condor_pidenvid.h"
#include "tree.h"
#include "proc_family.h"
#include "proc_family_io.h"
#include "procd_common.h"

class PIDTracker;
#if defined(LINUX)
class GroupTracker;
#endif
#if defined(HAVE_EXT_LIBCGROUP)
class CGroupTracker;
#endif
class LoginTracker;
class EnvironmentTracker;
class ParentTracker;

class ProcFamilyMonitor {

public:	
	// construct a monitor using the process with the given pid and
	// birthday as the "root" process of the "root" family. The boolean
	// specifies if we'd like to EXCEPT of the "root" pid dies.
	//
	ProcFamilyMonitor(pid_t, birthday_t, int, bool);

	// clean up everything
	//
	~ProcFamilyMonitor();

#if defined(LINUX)
	// enable tracking based on group IDs by specifying a range that
	// can be used for this purpose
	//
	void enable_group_tracking(gid_t min_tracking_gid, 
			gid_t max_tracking_gid, bool allocating);
#endif

	// create a "subfamily", which can then be signalled and accounted
	// for as a unit
	//
	proc_family_error_t register_subfamily(pid_t,
	                                       pid_t,
	                                       int);

	// start tracking a family using envionment variables
	//
	proc_family_error_t track_family_via_environment(pid_t, PidEnvID*);

	// start tracking a family using login
	//
	proc_family_error_t track_family_via_login(pid_t, char*);

#if defined(LINUX)
	// start tracking a family using a supplementary group ID
	//
	proc_family_error_t track_family_via_supplementary_group(pid_t, gid_t&);
#endif

	// remove a subfamily
	//
	proc_family_error_t unregister_subfamily(pid_t);

#if defined(HAVE_EXT_LIBCGROUP)
	// Turn on the ability to track via a cgroup
	void enable_cgroup_tracking();

	// Associate a cgroup with a family
	proc_family_error_t track_family_via_cgroup(pid_t, const char*);
#endif

	// for sending signals to a single process
	//
	proc_family_error_t signal_process(pid_t, int);

	// send a signal to all processes in the family with the given
	// root pid and all subfamilies
	//
	proc_family_error_t signal_family(pid_t, int);
	
	// get resource usage information about a family (and all
	// subfamilies)
	//
	proc_family_error_t get_family_usage(pid_t, ProcFamilyUsage*);

	// helper for looking up families by PID. a 2nd argument of true
	// means that a PID value of zero will cause this method to return
	// the root of our tree (otherwise a PID of 0 causes the lookup to
	// fail)
	//
	Tree<ProcFamily*>* lookup_family(pid_t pid, bool zero_means_root = false);

	// return the time that the procd should ewait in between taking
	// snapshots. this is the minimum of all the "maximum snapshot
	// intervals" requested when subfamilies are created
	//
	int get_snapshot_interval();

	// use a snapshot of all processes on the system (from ProcAPI)
	// to update the families we are tracking
	//
	void snapshot(pid_t BOLOpid = 0);

	// used to access the pid_t to ProcFamilyMember hash table
	// (these need to be public since they are called from the
	//  various tracker classes)
	//
	void add_member(ProcFamilyMember*);
	void remove_member(ProcFamilyMember*);
	ProcFamilyMember* lookup_member(pid_t pid);

	// used by tracker classes to make associations between
	// processes and families we're monitoring
	//
	bool add_member_to_family(ProcFamily*, procInfo*, const char*);

private:
	// the tree structure for the families we're tracking
	//
	Tree<ProcFamily*> *m_tree;

	// a ProcFamily object in which to put all processes that are not
	// in the family that we're tracking
	//
	ProcFamily* m_everybody_else;

	// keep a hash of all families we are tracking, keyed by the "root"
	// process's pid
	//
	HashTable<pid_t, Tree<ProcFamily*>*> m_family_table;

	// we keep a hash table over all processes that are in families
	// we are monitoring
	//
	HashTable<pid_t, ProcFamilyMember*> m_member_table;

	// Upon consutruction of this object, the first pid we monitor is either
	// going to be the condor_master or an arbitrary pid on the machine.
	// In the former case, if that pid dies, then we also want to die.
	// Otherwise we don't.
	bool m_except_if_pid_dies;

	// the "tracker" objects we use for finding processes that belong
	// to the families we're tracking
	//
	PIDTracker*         m_pid_tracker;
#if defined(LINUX)
	GroupTracker*       m_group_tracker;
#endif
#if defined(HAVE_EXT_LIBCGROUP)
	CGroupTracker*      m_cgroup_tracker;
#endif
	LoginTracker*       m_login_tracker;
	EnvironmentTracker* m_environment_tracker;
	ParentTracker*      m_parent_tracker;

	// find the minimum of all the ProcFamilys' requested "maximum
	// snapshot intervals"
	//
	int get_snapshot_interval(Tree<ProcFamily*>*);

	// since we maintain the tree of process families in this class,
	// each ProcFamily object needs our help in maintaining the maximum
	// image size seen for its family of processes (in order to take
	// into account processes that are in child families)
	//
	unsigned long update_max_image_sizes(Tree<ProcFamily*>*);
	
	// aggregate usage information from the given family and all
	// subfamilies
	//
	void get_family_usage(Tree<ProcFamily*>*, ProcFamilyUsage*);

	// send a signal to all processes in the given family and in any
	// subfamilies
	//
	void signal_family(Tree<ProcFamily*>*, int);

	// call the remove_exited_processes on all ProcFamily objects we're
	// managing
	//
	void remove_exited_processes(Tree<ProcFamily*>*);

	// delete any families that no longer have any processes (that we
	// know about) on the system
	//
	void delete_unwatched_families(Tree<ProcFamily*>*);

	// common functionality for unregistering a subfamily (used by both
	// the public unregister_subfamily method and delete_unwatched_families)
	//
	void unregister_subfamily(Tree<ProcFamily*>*);

	// delete all family objects in the given tree (called by destructor)
	//
	void delete_all_families(Tree<ProcFamily*>*);

public:
	// output all our families
	//
	proc_family_error_t dump(pid_t, std::vector<ProcFamilyDump>& vec);

private:
	// helper for dump
	//
	void dump(Tree<ProcFamily*>* tree, std::vector<ProcFamilyDump>& vec);
};

#endif
