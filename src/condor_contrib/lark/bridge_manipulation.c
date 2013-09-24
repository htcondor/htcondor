#include "condor_common.h"
#include "condor_config.h"

#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <linux/if_bridge.h>
#include <linux/rtnetlink.h>

#include "condor_debug.h"

#include "network_manipulation.h"

extern int seq;

// Cannot find this definition on RHEL6e
#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

int
create_bridge(const char * bridge_name)
{
	errno = 0;
	char brname[IFNAMSIZ];
	strncpy(brname, bridge_name, IFNAMSIZ);

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		dprintf(D_ALWAYS, "Unable to create socket for bridge manipulation (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	int ret = ioctl(fd, SIOCBRADDBR, brname);
	if (ret < 0 && errno != EEXIST) {
		dprintf(D_ALWAYS, "Error when creating bridge %s (errno=%d, %s).\n", bridge_name, errno, strerror(errno));
		close(fd);
		return errno;
	}
	close(fd);
	return errno;
}

int
delete_bridge(const char * bridge_name)
{
	errno = 0;
	char brname[IFNAMSIZ];
	strncpy(brname, bridge_name, IFNAMSIZ);

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		dprintf(D_ALWAYS, "Unable to create socket for bridge deletion (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	int ret = ioctl(fd, SIOCBRDELBR, brname);
	if (ret < 0 && errno != EEXIST) {
		dprintf(D_ALWAYS, "Error when deleting bridge %s (errno=%d, %s).\n", bridge_name, errno, strerror(errno));
		close(fd);
		return errno;
	}
	close(fd);
	return errno;
}

int
add_interface_to_bridge(const char * bridge_name, const char * dev)
{
	errno = 0;
	struct ifreq ifr;
	strncpy(ifr.ifr_name, bridge_name, IFNAMSIZ);

	int ifindex = if_nametoindex(dev);
	if (ifindex == 0) {
		return ENODEV;
	}

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		dprintf(D_FULLDEBUG, "Unable to create socket for bridge manipulation (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	ifr.ifr_ifindex = ifindex;
	int ret = ioctl(fd, SIOCBRADDIF, &ifr);
	if (ret < 0 && errno != EBUSY) {
		dprintf(D_FULLDEBUG, "Error when adding interface %s to bridge %s (errno=%d, %s).\n", dev, bridge_name, errno, strerror(errno));
		close(fd);
		return errno;
	}
	close(fd);
	return errno;
}

int
set_bridge_fd(const char * bridge_name, unsigned delay)
{
	struct ifreq ifr;
	strncpy(ifr.ifr_name, bridge_name, IFNAMSIZ);
	int ifindex = if_nametoindex(bridge_name);
	if (ifindex == 0) {
		return ENODEV;
	}

	int fd;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		dprintf(D_FULLDEBUG, "Unable to create socket for bridge manipulation (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	unsigned long args[4] = { BRCTL_SET_BRIDGE_FORWARD_DELAY, delay, 0, 0 };
	ifr.ifr_data = (char *) &args;

	int ret = ioctl(fd, SIOCDEVPRIVATE, &ifr);
	close(fd);
	if (ret < 0) {
		dprintf(D_ALWAYS, "Failed to set bridge %s forwarding delay to %u (errno=%d, %s).\n", bridge_name, delay, errno, strerror(errno));
	}
	return ret < 0 ? errno : 0;
}

static int
query_device(int fd, const char *link)
{
	struct iovec iov[4];

	struct ifinfomsg query; memset(&query, 0, sizeof(query));
	query.ifi_index = if_nametoindex(link);
	if (query.ifi_index < 0) {
		dprintf(D_FULLDEBUG, "Unknown device: %s\n", link);
		return 1;
	}

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(query)) + RTA_LENGTH(IFNAMSIZ);
	nlmsghdr.nlmsg_type = RTM_GETLINK;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST;
	nlmsghdr.nlmsg_pid = 0;
	nlmsghdr.nlmsg_seq = ++seq;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	iov[1].iov_base = &query;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(query));

	char dev[IFNAMSIZ+1]; dev[IFNAMSIZ] = '\0';
	strncpy(dev, link, IFNAMSIZ);

	struct rtattr rta; memset(&rta, 0, sizeof(rta));
	rta.rta_type = IFLA_IFNAME;
	rta.rta_len = RTA_LENGTH(0) + RTA_LENGTH(strlen(dev));
	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	iov[3].iov_base = dev;
	iov[3].iov_len = RTA_ALIGN(strlen(dev));

	int err;
	if ((err = send_to_kernel(fd, iov, 4))) {
		dprintf(D_ALWAYS, "Failed to query kernel for device %s information.\n", link);
		return err;
	}
	dprintf(D_FULLDEBUG, "Sent query to kernel for device %s.\n", link);
	return 0;
}

static int
parse_del_status(struct nlmsghdr *nlmsg, int dev)
{
	struct ifinfomsg *ifi = NLMSG_DATA(nlmsg);
	if (dev != ifi->ifi_index) {
		return -1;
	}
	return 1;
}

static int
parse_status(struct nlmsghdr *nlmsg, const char *link)
{
	struct ifinfomsg *ifi = NLMSG_DATA(nlmsg);
	int nllen = nlmsg->nlmsg_len;

	char b1[IFNAMSIZ+1]; b1[IFNAMSIZ] = '\0';
	int af_family = ifi->ifi_family;
	if (af_family != PF_BRIDGE) return -1;

	char * name = if_indextoname(ifi->ifi_index, b1);
	if (!name) return -1;

	if (strncmp(name, link, IFNAMSIZ) != 0) {
		//dprintf(D_FULLDEBUG, "Ignoring message about %s.\n", name);
		return -1;
	}
	//dprintf(D_FULLDEBUG, "Status from interface %s.\n", name);

	struct rtattr *attr;
	int protinfo;
	for (attr = IFLA_RTA(ifi); RTA_OK(attr, nllen); attr = RTA_NEXT(attr, nllen))
	{
		switch (attr->rta_type) {
			case IFLA_PROTINFO:
				protinfo = (int)*(char *)RTA_DATA(attr);
				//dprintf(D_FULLDEBUG, "Link state %d.\n", protinfo);
				return protinfo;
			default: // Ignore
				break;
		}
	}
	return -1;
}

static const char *port_states[] = {
	[BR_STATE_DISABLED] = "disabled",
	[BR_STATE_LISTENING] = "listening",
	[BR_STATE_LEARNING] = "learning",
	[BR_STATE_FORWARDING] = "forwarding",
	[BR_STATE_BLOCKING] = "blocking",
};

static int
wait_for_status(int fd, const char *link)
{
	struct msghdr msghdr;
	struct sockaddr_nl addr;
	struct iovec iov[1];
	char buf[getpagesize()];
	ssize_t len;

	int dev_idx = if_nametoindex(link);
	if (dev_idx == 0) {
		dprintf(D_ALWAYS, "Waiting for status of unknown device %s.\n", link);
		return -1;
	}

	iov[0].iov_base = buf;
	iov[0].iov_len = getpagesize();

	msghdr.msg_name = &addr;
	msghdr.msg_namelen = sizeof addr;
	msghdr.msg_iov = iov;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = NULL;
	msghdr.msg_controllen = 0;
	msghdr.msg_flags = 0;
	struct nlmsghdr *nlmsg;
	struct nlmsgerr *err;
	while ((len = recvmsg(fd, &msghdr, 0)) != -1) {
		nlmsg = (struct nlmsghdr *)buf;
		//dprintf(D_FULLDEBUG, "Got netlink message of size %ld.\n", len);
		for (nlmsg = (struct nlmsghdr *)buf;
				NLMSG_OK(nlmsg, len);
				nlmsg = NLMSG_NEXT(nlmsg, len))
		{
			int status;
			//dprintf(D_FULLDEBUG, "Parsing netlink message of type %d.\n", nlmsg->nlmsg_type);
			switch (nlmsg->nlmsg_type) {
			case NLMSG_NOOP:
				dprintf(D_FULLDEBUG, "Netlink no-op message from kernel.\n");
				break;
			case NLMSG_ERROR:
				err = (struct nlmsgerr*)NLMSG_DATA(nlmsg);
				if (err->error) {
					dprintf(D_ALWAYS, "Error message back from netlink "
						"(errno=%d, %s).\n", -err->error,
						strerror(-err->error));
					return err->error;
				}
				return -1;
			case NLMSG_DONE:
				dprintf(D_FULLDEBUG, "Recieved an unexpected netlink done message.\n");
				break;
			case RTM_DELLINK:
				if (parse_del_status(nlmsg, dev_idx) > 0) {
					return -2;
				}
				break;
			case RTM_NEWLINK:
				// Because there may be multiple packets in this message, we only
				// return if it is in the state we are looking for.
				if (((status = parse_status(nlmsg, link)) > -1) && (status == BR_STATE_FORWARDING)) {
					dprintf(D_FULLDEBUG, "Link %s has entered the forwarding state.\n", link);
					return status;
				} else if (status == -1) {
					// Could parse packet, but it didn't match.
					break;
				}
				dprintf(D_FULLDEBUG, "Link %s is in status %s (%d).\n", link, port_states[status], status);
				break;
			default:
				dprintf(D_FULLDEBUG, "Unknown message type: %d\n", nlmsg->nlmsg_type);
			};
		}
		//dprintf(D_FULLDEBUG, "Finished parsing netlink message from kernel.\n");
	}
	if (errno != EAGAIN)
		dprintf(D_ALWAYS, "Error from netlink socket (errno=%d, %s).\n", errno, strerror(errno));
	return -errno;
}

int
wait_for_bridge_status(int fd, const char *link)
{
	// Subscribe our socket to link-layer messages.
	int group = RTNLGRP_LINK;
	if (setsockopt(fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group, sizeof(group))) {
		dprintf(D_ALWAYS, "Failed to subscribe to the link-level notification group"
			" (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	// Query for the state of the link
	int retval = 0;
	if ((retval = query_device(fd, link))) {
		goto cleanup;
	}

	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	if (-1 == setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
		dprintf(D_ALWAYS, "Unable to set socket receive timeout.\n");
		goto cleanup;
	}
	// TODO: get forwarding delay from kernel and only wait for 2x that value

	// Recieve messages from the kernel
	if ((retval >= 0) && (retval < 5))
		dprintf(D_FULLDEBUG, "Link %s current status is %s.\n", link, port_states[retval]);
	else
		dprintf(D_FULLDEBUG, "Link %s current status is %d (unknown enum).\n", link, retval);
	while (retval != BR_STATE_FORWARDING) {
		if (((retval = wait_for_status(fd, link)) < 0) && (retval != -EAGAIN)) {
			dprintf(D_ALWAYS, "Failed to get status from kernel.\n");
			goto cleanup;
		}
		if ((retval >= 0) && (retval < 5))
			dprintf(D_FULLDEBUG, "Link %s current status is %s.\n", link, port_states[retval]);
		else if (retval != -EAGAIN)
			dprintf(D_FULLDEBUG, "Link %s current status is %d (unknown enum).\n", link, retval);
		else
			dprintf(D_FULLDEBUG, "Querying kernel again for device %s status.\n", link);
	}
	retval = 0;

cleanup:
	if (setsockopt(fd, SOL_NETLINK, NETLINK_DROP_MEMBERSHIP, &group, sizeof(group))) {
		dprintf(D_ALWAYS, "Failed to unsubscribe from the link-level notification group"
			" (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	return retval;
}

