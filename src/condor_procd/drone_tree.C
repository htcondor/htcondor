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
#include "condor_pidenvid.h"
#include "drone_tree.h"
#include "drone.h"
#include "network_util.h"
#include "process_util.h"

static int
int_hash(const int& i, int num_buckets)
{
	return i % num_buckets;
}

DroneTree::DroneTree(char* procd_addr, char* drone_path) :
	m_process_table(31, int_hash),
	m_family_client(procd_addr)
{
	// "init" the network (WSAStartup on Windows)
	//
	network_init();

	// setup the "command sock"
	//
	m_sock = get_bound_socket();
	disable_socket_inheritance(m_sock);
	unsigned short port = get_socket_port(m_sock);

	// launch the first drone
	//
	if (drone_path == NULL) {
		drone_path = "./procd_test_drone";
	}
	char port_str[10];
	snprintf(port_str, 10, "%hu", ntohs(port));
	char pid_str[10];
	snprintf(pid_str, 10, "%u", get_process_id());
	char* drone_argv[] = {drone_path, port_str, pid_str, NULL};
	create_detached_child(drone_argv);

	// wait for it to report back
	//
	pid_t root_pid = wait_for_new_drone(1);

	// register a subfamily for the drone tree with the procd
	//
	bool ok = m_family_client.register_subfamily(root_pid,
	                                             getpid(),
	                                             -1);
	ASSERT(ok);

	// create a reference to check correctness against
	//
	m_reference_tree = new ProcReferenceTree(root_pid);
}

DroneTree::~DroneTree()
{
	// free up dynamically allocated stuff
	//
	delete m_reference_tree;

	// kill off the tree of drones
	//
	DroneEntry* de;
	m_process_table.startIterations();
	while (m_process_table.iterate(de)) {
		send_command(de->port, COMMAND_DIE);
		delete de;
	}

	// "cleanup" the network (WSACleanup on Windows)
	//
	network_cleanup();
}

void
DroneTree::send_spin(int node_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(node_id, de);
	ASSERT(ret != -1);

	send_command(de->port, COMMAND_SPIN);

	pid_t ids[2];
	get_drone_response(ids);
	ASSERT((ids[0] == de->pid) && (ids[1] == de->ppid));
}

void
DroneTree::send_spawn(int parent_id, int child_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(parent_id, de);
	ASSERT(ret != -1);

	send_command(de->port, COMMAND_SPAWN);
	pid_t child_pid = wait_for_new_drone(child_id);

	m_reference_tree->process_created(de->pid, child_pid);
}

void
DroneTree::send_die(int node_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(node_id, de);
	assert(ret != -1);

	send_command(de->port, COMMAND_DIE);
	printf("killed drone(%d): pid = %d\n", node_id, de->pid);

	ret = m_process_table.remove(node_id);
	assert(ret != -1);

	m_reference_tree->process_exited(de->pid);

	delete de;
}

void
DroneTree::send_command(unsigned short port, drone_command_t command)
{
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(port);
	ssize_t bytes = sendto(m_sock,
	                       (char*)&command,
	                       sizeof(drone_command_t),
	                       0,
	                       (struct sockaddr*)&sin,
	                       sizeof(struct sockaddr_in));
	if (bytes == -1) {
		socket_error("sendto");
		exit(1);
	}
	assert(bytes == sizeof(drone_command_t));
}

pid_t
DroneTree::wait_for_new_drone(int child_id)
{
	pid_t ids[2];
	unsigned short port;
	get_drone_response(ids, &port);

	DroneEntry* de = new DroneEntry(ids[0], ids[1], port);
	int rv = m_process_table.insert(child_id, de);
	assert(rv != -1);

	printf("new drone (%d): pid = %d, ppid = %d, port = %hu\n",
	        child_id,
	        ids[0],
	        ids[1],
	        port);

	return ids[0];
}

void
DroneTree::get_drone_response(pid_t pid[2], unsigned short* port)
{
	ASSERT(pid != NULL);

	struct sockaddr_in sin;
	socklen_t len = sizeof(struct sockaddr_in);
	ssize_t bytes = recvfrom(m_sock,
	                         (char*)pid,
	                         2 * sizeof(pid_t),
	                         0,
	                         (struct sockaddr*)&sin,
	                         &len);
	if (bytes == SOCKET_ERROR) {
		socket_error("recvfrom");
		exit(1);
	}
	assert(bytes == 2 * sizeof(pid_t));

	if (port != NULL) {
		*port = ntohs(sin.sin_port);
	}
}

void
DroneTree::snapshot()
{
	// HACK: we currently do not receive notification when a DIE command
	// has resulted in the given process exiting and being reaped; we thus
	// sleep a second before taking a snapshot and assume that is ample
	// time for this to happen for any processes we told to go away
	//
	// TODO: fix this by using daemon core to start drones?
	//       (this would also allow us to easily start testing the PidEnvID
	//        stuff)
	//
	sleep(1);

	// tell the procd to take a snapshot
	//
	bool ok = m_family_client.snapshot();
	ASSERT(ok);

	// now ask it for its state so we can compare it against reality
	//
	LocalClient* client =
		m_family_client.dump(m_reference_tree->get_root_pid());
	ASSERT(client != NULL);
	ProcReferenceTree procd_tree(*client);
	m_reference_tree->test(procd_tree);
}

void
DroneTree::register_subfamily(int node_id, int watcher_node_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(node_id, de);
	assert(ret != -1);

	bool ok = m_family_client.register_subfamily(de->pid, de->ppid, -1);
	ASSERT(ok);

	DroneEntry* watcher_de;
	ret = m_process_table.lookup(watcher_node_id, watcher_de);
	ASSERT(ret != -1);
	m_reference_tree->subfamily_registered(de->pid, watcher_de->pid);
}

void
DroneTree::get_family_usage(int node_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(node_id, de);
	assert(ret != -1);

	ProcFamilyUsage usage;
	m_family_client.get_usage(de->pid, usage);

	dump_usage(usage);
}

void
DroneTree::signal_process(int node_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(node_id, de);
	assert(ret != -1);

	bool ok = m_family_client.signal_process(de->pid, SIGUSR1);
	ASSERT(ok);

	pid_t pid;
	struct sockaddr_in sin;
	socklen_t len = sizeof(struct sockaddr_in);
	ssize_t bytes = recvfrom(m_sock,
	                         (char*)&pid,
	                         sizeof(pid_t),
	                         0,
	                         (struct sockaddr*)&sin,
	                         &len);
	if (bytes == -1) {
		socket_error("recv");
		exit(1);
	}
	assert(bytes == sizeof(pid_t));
	
	ASSERT(pid == de->pid);
}

void
DroneTree::kill_family(int node_id)
{
	DroneEntry* de;
	int ret = m_process_table.lookup(node_id, de);
	assert(ret != -1);

	bool ok = m_family_client.kill_family(de->pid);
	ASSERT(ok);

	m_reference_tree->family_killed(de->pid);
}

void
DroneTree::dump()
{
	printf("----------------------------------------------\n");
	int node_id;
	DroneEntry* de;
	m_process_table.startIterations();
	while (m_process_table.iterate(node_id, de)) {
		printf("%d: pid = %d, port = %hu\n",
		       node_id,
		       de->pid,
		       de->port);
	}
	printf("----------------------------------------------\n");
	m_reference_tree->output(stdout);
	printf("----------------------------------------------\n");
}

void
DroneTree::dump_usage(ProcFamilyUsage& usage)
{
	printf("  Number of processes: %d\n", usage.num_procs);
	printf("  User CPU: %ld\n", usage.user_cpu_time);
	printf("  System CPU: %ld\n", usage.sys_cpu_time);
	printf("  Percent CPU: %f%%\n", usage.percent_cpu);
	printf("  Max Image Size: %lu\n", usage.max_image_size);
	printf("  Total Image Size: %lu\n", usage.total_image_size);
}
