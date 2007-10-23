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
#ifndef _PROC_FAMILY_MONITOR_H
#define _PROC_FAMILY_MONITOR_H

#include "condor_common.h"
#include "../condor_procapi/procapi.h"
#include "condor_pidenvid.h"
#include "tree.h"
#include "proc_family.h"
#include "proc_family_io.h"
#include "procd_common.h"

#if defined(PROCD_DEBUG)
#include "local_server.h"
#endif

class PIDTracker;
#if defined(LINUX)
class GroupTracker;
#endif
class LoginTracker;
class EnvironmentTracker;
class ParentTracker;

class ProcFamilyMonitor {

public:	
	// construct a monitor using the process with the given pid and
	// birthday as the "root" process of the "root" family
	//
	ProcFamilyMonitor(pid_t, birthday_t, int);

	// clean up everything
	//
	~ProcFamilyMonitor();

#if defined(LINUX)
	// enable tracking based on group IDs by specifying a range that
	// can be used for this purpose
	//
	void enable_group_tracking(gid_t min_tracking_gid, gid_t max_tracking_gid);
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

	// return the time that the procd should ewait in between taking
	// snapshots. this is the minimum of all the "maximum snapshot
	// intervals" requested when subfamilies are created
	//
	int get_snapshot_interval();

	// use a snapshot of all processes on the system (from ProcAPI)
	// to update the families we are tracking
	//
	void snapshot();

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
	bool add_member_to_family(ProcFamily*, procInfo*, char*);

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

	// the "tracker" objects we use for finding processes that belong
	// to the families we're tracking
	//
	PIDTracker*         m_pid_tracker;
#if defined(LINUX)
	GroupTracker*       m_group_tracker;
#endif
	LoginTracker*       m_login_tracker;
	EnvironmentTracker* m_environment_tracker;
	ParentTracker*      m_parent_tracker;

	// find the minimum of all the ProcFamilys' requested "maximum
	// snapshot intervals"
	//
	int get_snapshot_interval(Tree<ProcFamily*>*);
	
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

#if defined(PROCD_DEBUG)

public:
	// output all our families
	//
	void output(LocalServer&, pid_t);

private:
	// helper for output
	//
	void output(LocalServer&, pid_t, Tree<ProcFamily*>*);

#endif
};

#endif
