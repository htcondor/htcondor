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
#include "drone.h"
#include "network_util.h"
#include "process_util.h"

static char* drone_path = NULL;
static pid_t parent_pid = -1;
static int controller_port = -1;
static SOCKET sock_fd = INVALID_SOCKET;

void
handle_signal(int)
{
	assert(controller_port != -1);
	assert(sock_fd != INVALID_SOCKET);
	pid_t pid = get_process_id();
	if (send(sock_fd,
	         (char*)&pid,
	         sizeof(pid_t),
	         0) == SOCKET_ERROR)
	{
		socket_error("send");
		exit(1);
	}
}

void
init_controller_socket()
{
	sock_fd = get_bound_socket();
	disable_socket_inheritance(sock_fd);
	
	// make the socket's default address point to the controller
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = htons(controller_port);
	if (connect(sock_fd,
	            (struct sockaddr*)&sin,
	            sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		socket_error("connect");
		exit(1);
	}

	// send our pid and ppid up to the controller
	pid_t ids[2] = {get_process_id(), parent_pid};
	if (send(sock_fd,
	         (char*)&ids,
	         sizeof(ids),
	         0) == SOCKET_ERROR)
	{
		socket_error("send");
		exit(1);
	}
}

static void
spin()
{
	pid_t pid;
	for (int i = 0; i < 150000000; i++){ 
		pid = get_process_id();
	}
	if (send(sock_fd, (char*)&pid, sizeof(pid), 0) == SOCKET_ERROR) {
		socket_error("send");
		exit(1);
	}
}

static void
spawn(unsigned short port)
{
	char port_str[10];
	snprintf(port_str, 10, "%hu", port);
	char pid_str[10];
	snprintf(pid_str, 10, "%u", get_process_id());
	char* argv[] = {drone_path, port_str, pid_str, NULL};
	create_detached_child(argv);
}

void
process_command()
{
	char* buffer = new char[MAX_COMMAND_SIZE];
	assert(buffer != NULL);
	while (true) {
		ssize_t bytes = recv(sock_fd,
		                     buffer,
		                     MAX_COMMAND_SIZE,
		                     0);
		if (bytes == -1) {
#if !defined(WIN32)
			if (errno == EINTR) {
				continue;
			}
#endif
			socket_error("recv");
			exit(1);
		}
		break;
	}

	drone_command_t command = *(drone_command_t*)buffer;
	switch (command) {
		case COMMAND_SPIN:
			spin();
			break;
		case COMMAND_SPAWN:
			spawn(controller_port);
			break;
		case COMMAND_DIE:
			network_cleanup();
			exit(0);
		default:
			fprintf(stderr, "invalid command\n");
			exit(1);
	}

	delete[] buffer;
}

int
main(int argc, char* argv[])
{
	// make sure we are passed in a controller port
	if (argc != 3) {
		fprintf(stderr, "usage: %s <contoller_port> <ppid>\n", argv[0]);
		exit(1);
	}
	drone_path = argv[0];
	controller_port = atoi(argv[1]);
	assert(controller_port);
	parent_pid = atoi(argv[2]);
	assert(parent_pid);
	
	// set up our "signal handler"
	init_signal_handler(handle_signal);
	
	// set up our "command sock"
	network_init();
	init_controller_socket();

	while (true) {
		process_command();
	}
}
