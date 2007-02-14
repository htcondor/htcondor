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
#include "network_util.h"

#if defined(WIN32)

void
socket_error(char* fn)
{
	fprintf(stderr, "%s error: %u\n", fn, WSAGetLastError());
}

#else

void
socket_error(char* fn)
{
	perror(fn);
}

#endif

#if defined(WIN32)

void
network_init()
{
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 0), &wsa_data) != 0) {
		socket_error("WSAStartup");
		exit(1);
	}
	if ((LOBYTE(wsa_data.wVersion) != 2) || (HIBYTE(wsa_data.wVersion) != 0)) {
		fprintf(stderr, "error: Winsock 2 not supported\n");
		exit(1);
	}
}

void
network_cleanup()
{
	if (WSACleanup() != 0) {
		socket_error("WSACleanup");
		exit(1);
	}
}

#else

void
network_init()
{
}

void
network_cleanup()
{
}

#endif

SOCKET
get_bound_socket()
{
	// create the socket
	SOCKET sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd == INVALID_SOCKET) {
		perror("socket");
		exit(1);
	}

	// bind it to the loopback interface
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(sock_fd,
	         (struct sockaddr*)&sin,
	         sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		socket_error("bind");
		exit(1);
	}

	return sock_fd;
}

unsigned short
get_socket_port(SOCKET sock_fd)
{
	// determine our port number
	struct sockaddr_in sin;
	socklen_t len = sizeof(struct sockaddr_in);
	if (getsockname(sock_fd,
	                (struct sockaddr*)&sin,
	                &len) == SOCKET_ERROR)
	{
		socket_error("getsockname");
		exit(1);
	}

	return sin.sin_port;
}

#if defined(WIN32)

void
disable_socket_inheritance(SOCKET sock_fd)
{
	if (SetHandleInformation((HANDLE)sock_fd, HANDLE_FLAG_INHERIT, 0) == FALSE) {
		socket_error("SetHandleInformation");
		exit(1);
	}
}

#else

void
disable_socket_inheritance(SOCKET sock_fd)
{
	int flags = fcntl(sock_fd, F_GETFL);
	if (flags == -1) {
		socket_error("fcntl");
		exit(1);
	}
	if (fcntl(sock_fd, F_SETFL, flags | FD_CLOEXEC) == -1) {
		socket_error("fcntl");
		exit(1);
	}
}

#endif
