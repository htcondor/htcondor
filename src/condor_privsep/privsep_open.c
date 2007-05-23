#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>

static int
send_fd(int client_fd, int fd)
{
	// set up a msghdr struct for our call to sendmsg
	//
	struct msghdr msg;

	// no need to give an address since the UDS is connected
	//
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	// send a single null byte of in-band data
	//
	struct iovec iov[1];
	char buf[1];
	buf[0] = '\0';
	iov[0].iov_base = buf;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	// the control message does the actual FD passing
	//
	char cmsg_buf[CMSG_LEN(sizeof(int))];
	struct cmsghdr* cmsg = (struct cmsghdr*)cmsg_buf;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	*(int*)CMSG_DATA(cmsg) = fd;
	msg.msg_control = cmsg;
	msg.msg_controllen = CMSG_LEN(sizeof(int));

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
