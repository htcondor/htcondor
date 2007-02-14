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

#include "../condor_procapi/procapi.h"
#include "condor_pidenvid.h"
#include "tree.h"
#include "proc_family.h"
#include "proc_family_io.h"
#include "pid_tracker.h"
#include "login_tracker.h"
#include "environment_tracker.h"
#include "parent_tracker.h"
#include "procd_common.h"

#if defined(PROCD_DEBUG)
#include "local_server.h"
#endif

class ProcFamilyMonitor {

public:	
	// construct a monitor using the process with the given pid and
	// birthday as the "root" process of the "root" family
	//
	ProcFamilyMonitor(pid_t, birthday_t, int);

	// clean up everything
	//
	~ProcFamilyMonitor();

	// create a "subfamily", which can then be signalled and accounted
	// for as a unit
	//
	bool register_subfamily(pid_t,
	                        pid_t,
	                        int,
	                        PidEnvID* = NULL,
	                        char* = NULL);

	// remove a subfamily
	//
	bool unregister_subfamily(pid_t);

	// for sending signals to a single process
	//
	bool signal_process(pid_t, int);

	// send a signal to all processes in the family with the given
	// root pid and all subfamilies
	//
	bool signal_family(pid_t, int);

	// get resource usage information about a family (and all
	// subfamilies)
	//
	bool get_family_usage(pid_t, ProcFamilyUsage*);

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

private:
	// the tree structure for the families we're tracking
	//
	Tree<ProcFamily*> *m_tree;

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
	PIDTracker         m_pid_tracker;
	LoginTracker       m_login_tracker;
	EnvironmentTracker m_environment_tracker;
	ParentTracker      m_parent_tracker;

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
	bool unregister_subfamily(Tree<ProcFamily*>*);

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
