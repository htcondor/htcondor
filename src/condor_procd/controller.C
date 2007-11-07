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
#include "drone_tree.h"

#include <iostream>
#include <string>
using namespace std;

int
main(int argc, char* argv[])
{
	//kill(getpid(), SIGSTOP);

	if (argc < 2) {
		fprintf(stderr,
	            "usage: procd_test_controller <procd_addr> [drone_binary]\n");
		exit(1);
	}

	DroneTree drone_tree(argv[1], (argc > 2) ? argv[2] : NULL);

	while (true) {

		drone_tree.dump();

		string cmd;
		cin >> cmd;
		if (!cin) {
			break;
		}
		if (cmd == "SPIN") {
			int node_id;
			cin >> node_id;
			if (!cin) {
				break;
			}
			printf("SPIN %d\n", node_id);
			drone_tree.send_spin(node_id);
		}
		else if (cmd == "SPAWN") {
			int parent_id, child_id;
			cin >> parent_id >> child_id;
			if (!cin) {
				break;
			}
			printf("SPAWN %d %d\n", parent_id, child_id);
			drone_tree.send_spawn(parent_id, child_id);
		}
		else if (cmd == "DIE") {
			int node_id;
			cin >> node_id;
			if (!cin) {
				break;
			}
			printf("DIE %d\n", node_id);
			drone_tree.send_die(node_id);
		}
		else if (cmd == "SNAPSHOT") {
			printf("SNAPSHOT\n");
			drone_tree.snapshot();
		}
		else if (cmd == "REGISTER_SUBFAMILY") {
			int node_id, watcher_node_id;
			cin >> node_id >> watcher_node_id;
			if (!cin) {
				break;
			}
			printf("REGISTER_SUBFAMILY %d %d\n",
			       node_id,
			       watcher_node_id);
			drone_tree.register_subfamily(node_id,
			                              watcher_node_id);
		}
		else if (cmd == "SIGNAL_PROCESS") {
			int node_id;
			cin >> node_id;
			if (!cin) {
				break;
			}
			printf("SIGNAL_PROCESS %d\n", node_id);
			drone_tree.signal_process(node_id);
		}
		else if (cmd == "KILL_FAMILY") {
			int node_id;
			cin >> node_id;
			if (!cin) {
				break;
			}
			printf("KILL_FAMILY %d\n", node_id);
			drone_tree.kill_family(node_id);
		}
		else if (cmd == "GET_FAMILY_USAGE") {
			int node_id;
			cin >> node_id;
			if (!cin) {
				break;
			}
			printf("GET_FAMILY_USAGE %d\n", node_id);
			drone_tree.get_family_usage(node_id);
		}
		else {
			printf("unknown command: %s\n", cmd.c_str());
			exit(1);
		}
	}
	if (!cin.eof()) {
		printf("error in input stream\n");
		exit(1);
	}

	return 0;
}
