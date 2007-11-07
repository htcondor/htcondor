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
