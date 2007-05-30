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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <fcntl.h>
#include "privsep_open.h"

static int
send_fd(int client_fd, int fd)
{
	struct iovec iov[1];
	char buf[1];
#if !defined(PRIVSEP_OPEN_USE_ACCRIGHTS)
	char cmsg_buf[CMSG_SPACE(sizeof(int))];
	struct cmsghdr* cmsg;
#endif

	// set up a msghdr struct for our call to sendmsg
	//
	struct msghdr msg;

	// no need to give an address since the UDS is connected
	//
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	// send a single null byte of in-band data
	//
	buf[0] = '\0';
	iov[0].iov_base = buf;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	// NOTE: this code currently causes a bus error on HPUX.
	// I've been able to make the bus error go away by forcing
	// the cmsg_buf buffer to be aligned, but even then the sendmsg
	// call fails; for now I'm just going to document PrivSep on
	// HPUX as not yet working
	//
#if !defined(PRIVSEP_OPEN_USE_ACCRIGHTS)
	// the control message does the actual FD passing
	//
	msg.msg_control = cmsg_buf;
	msg.msg_controllen = CMSG_SPACE(sizeof(int));
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
#else
	// setup the file descriptor to pass using the
	// msg_accrights member
	//
	msg.msg_accrights = (caddr_t)&fd;
	msg.msg_accrightslen = sizeof(int);
#endif

	// send it!
	//
	if (sendmsg(client_fd, &msg, 0) == -1) {
		fprintf(stderr, "sendmsg error: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int
privsep_open_server(char* path, int flags, mode_t mode)
{
	int fd = open(path, flags, mode);
	if (fd == -1) {
		return 191;
	}

	if (send_fd(0, fd) == -1) {
		return 192;
	}

	return 0;
}
