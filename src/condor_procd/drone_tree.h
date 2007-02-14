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

#ifndef _DRONE_TREE_H
#define _DRONE_TREE_H

#include "drone.h"
#include "network_util.h"
#include "HashTable.h"
#include "proc_family_monitor.h"
#include "proc_reference_tree.h"
#include "proc_family_client.h"

#include <set>
using namespace std;

class DroneTree {

public:
	DroneTree(char*, char*);
	~DroneTree();

	void send_spin(int);
	void send_spawn(int, int);
	void send_die(int);

	void snapshot();
	void register_subfamily(int, int);
	void get_family_usage(int);
	void signal_process(int);
	void kill_family(int);

	void dump();

private:

	// our "command sock"
	//
	SOCKET m_sock;

	// map from id to (pid, port) tuple
	//
	struct DroneEntry {
		pid_t pid;
		pid_t ppid;
		unsigned short port;
		DroneEntry(pid_t id, pid_t parent_id, unsigned short pt) :
			pid(id), ppid(parent_id), port(pt) { }
	};
	HashTable<int, DroneEntry*> m_process_table;

	// the "correct" answer for the process family (and subfamilies)
	//
	ProcReferenceTree* m_reference_tree;

	// connection to the UUT
	//
	ProcFamilyClient m_family_client;

	void send_command(unsigned short, drone_command_t);
	
	pid_t wait_for_new_drone(int);

	void get_drone_response(pid_t[2], unsigned short* = NULL);

	void dump_usage(ProcFamilyUsage&);
};

#endif
