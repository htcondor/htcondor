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
#include "condor_debug.h"
#include "fdpass.h"

int
fdpass_send(int uds_fd, int fd)
{
	struct msghdr msg;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	struct iovec iov;
	char nil = '\0';
	iov.iov_base = &nil;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	char buf[CMSG_SPACE(sizeof(int))];
	msg.msg_control = buf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	*(int*)CMSG_DATA(cmsg) = fd;

	ssize_t bytes = sendmsg(uds_fd, &msg, 0);
	if (bytes == -1) {
		dprintf(D_ALWAYS,
		        "fdpass: sendmsg error: %s\n",
		        strerror(errno));
		return -1;
	}
	if (bytes != 1) {
		dprintf(D_ALWAYS,
		        "fdpass: unexpected return from sendmsg: %d\n",
		        (int)bytes);
		return -1;
	}

	return 0;
}

int
fdpass_recv(int uds_fd)
{
	struct msghdr msg;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	struct iovec iov;
	char nil = 'X';
	iov.iov_base = &nil;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	char buf[CMSG_SPACE(sizeof(int))];
	msg.msg_control = buf;
	msg.msg_controllen = CMSG_LEN(sizeof(int));

	ssize_t bytes = recvmsg(uds_fd, &msg, 0);
	if (bytes == -1) {
		dprintf(D_ALWAYS,
		        "fdpass: recvmsg error: %s\n",
		        strerror(errno));
		return -1;
	}
	if (bytes != 1) {
		dprintf(D_ALWAYS,
		        "fdpass: unexpected return from recvmsg: %d\n",
		        (int)bytes);
		return -1;
	}

	if (nil != '\0') {
		dprintf(D_ALWAYS,
		        "fdpass: unexpected value received from recvmsg: %d\n",
		        nil);
		return -1;
	}

	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
	int fd = *(int*)CMSG_DATA(cmsg);

	return fd;
}
