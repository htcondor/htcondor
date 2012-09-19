
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <linux/sched.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/veth.h>
#include <arpa/inet.h>

#include "condor_sys_linux.h"
#include "condor_debug.h"

// Below is the raw nasty netlink stuff
// I find it much easier to do this in straight-up C.

/**
 *  Create a socket to talk to the kernel via netlink
 *  Returns the socket fd upon success, or -errno upon failure
 */

#ifdef __cplusplus
extern "C" {
#endif

int seq = time(NULL);

int create_socket() {
	int sock;
	sock = socket (AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (sock == -1) {
		dprintf(D_ALWAYS, "Unable to create a netlink socket: %s\n", strerror(errno));
		return -errno;
	}

	struct sockaddr_nl addr;
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid();
	addr.nl_groups = 0;
    
	int result = bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_nl));
	if (result == -1) {
		dprintf(D_ALWAYS, "Unable to bind netlink socket to kernel: %s\n", strerror(errno));
		return -errno;
	}
    
		return sock;
}

static int send_to_kernel(int sock, struct iovec* iov, size_t ioveclen) {

	if (sock < 0) {
		dprintf(D_ALWAYS, "Invalid socket: %d.\n", sock);
		return 1;
	}

	struct sockaddr_nl nladdr; memset(&nladdr, 0, sizeof(struct sockaddr_nl));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = 0;
	nladdr.nl_groups = 0;

	struct msghdr msg; memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_name = &nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = iov;
	msg.msg_iovlen = ioveclen;


	if (sendmsg(sock, &msg, 0) < 0) {
		dprintf(D_ALWAYS, "Unable to send create_veth message to kernel: %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

// Forward decl
int recv_message(int sock);

static int send_and_ack(int sock, struct iovec* iov, size_t ioveclen) {

	int rc;
	if ((rc = send_to_kernel(sock, iov, ioveclen))) {
		dprintf(D_ALWAYS, "Send to kernel failed: %d\n", rc);
		return rc;
	} 
	if ((rc = recv_message(sock))) {
		dprintf(D_ALWAYS, "Message not successfully ACK'd: %d.\n", rc);
		return rc;
	}
	return 0;

}


#define VETH "veth"
#define VETH_LEN strlen(VETH)
int create_veth(int sock, const char * veth0, const char * veth1) {

	struct iovec iov[12];

	size_t veth0_len = strlen(veth0);
	size_t veth1_len = strlen(veth1);
	if (veth0_len >= IFNAMSIZ) {
		dprintf(D_ALWAYS, "Name too long for network device: %s (size %lu, max %u).\n", veth0, veth0_len, IFNAMSIZ);
		return 1;
	}
	if (veth1_len >= IFNAMSIZ) {
		dprintf(D_ALWAYS, "Name too long for network device: %s (size %lu, max %u).\n", veth1, veth1_len, IFNAMSIZ);
		return 1;
	}

	// Create the header of the netlink message
	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)) + RTA_LENGTH(0) + RTA_LENGTH(VETH_LEN) + 
			RTA_LENGTH(0) + RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + 
			RTA_LENGTH(0) + RTA_ALIGN(veth1_len) + RTA_LENGTH(0) + RTA_ALIGN(veth0_len);
	nlmsghdr.nlmsg_type = RTM_NEWLINK;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH (0);

	// Request the link
	struct ifinfomsg info_msg; memset(&info_msg, 0, sizeof(struct ifinfomsg));
	info_msg.ifi_family = AF_UNSPEC;

	iov[1].iov_base = &info_msg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	struct rtattr rta; memset(&rta, 0, sizeof(struct rtattr));
	rta.rta_type = IFLA_LINKINFO;
	rta.rta_len = RTA_LENGTH(0) + RTA_LENGTH(VETH_LEN) + RTA_LENGTH(0) + RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + RTA_LENGTH(0) + RTA_ALIGN(veth1_len);;
	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	struct rtattr rta2; memset(&rta2, 0, sizeof(struct rtattr));
	rta2.rta_type = IFLA_INFO_KIND;
	rta2.rta_len = RTA_LENGTH(VETH_LEN);
	iov[3].iov_base = &rta2;
	iov[3].iov_len = RTA_LENGTH(0);

	char type[VETH_LEN];
	memcpy(type, VETH, VETH_LEN);
	iov[4].iov_base = type;
	iov[4].iov_len = RTA_ALIGN(VETH_LEN);

	struct rtattr rta3; memset(&rta3, 0, sizeof(struct rtattr));
	rta3.rta_type = IFLA_INFO_DATA;
	rta3.rta_len = RTA_LENGTH(0) + RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + RTA_LENGTH(0) + RTA_ALIGN(veth1_len);

	iov[5].iov_base = &rta3;
	iov[5].iov_len = RTA_LENGTH(0);

	struct rtattr rta4; memset(&rta4, 0, sizeof(struct rtattr));
	rta4.rta_type =  VETH_INFO_PEER;
	rta4.rta_len = RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + RTA_LENGTH(0) + RTA_ALIGN(veth1_len);

	iov[6].iov_base = &rta4;
	iov[6].iov_len = RTA_LENGTH(0);

	// Add hole of size of size ifinfomsg
	struct ifinfomsg info_msg2; memset(&info_msg2, 0, sizeof(struct ifinfomsg));
	iov[7].iov_base = &info_msg2;
	iov[7].iov_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));
	

	struct rtattr rta5; memset(&rta5, 0, sizeof(struct rtattr));
	rta5.rta_type = IFLA_IFNAME;
	rta5.rta_len = RTA_LENGTH(veth1_len);

	iov[8].iov_base = &rta5;
	iov[8].iov_len = RTA_LENGTH(0);

	char veth1_copy[IFNAMSIZ];
	memcpy(veth1_copy, veth1, veth1_len);
	iov[9].iov_base = veth1_copy;
	iov[9].iov_len = RTA_ALIGN(veth1_len);

	struct rtattr rta6; memset(&rta6, 0, sizeof(struct rtattr));
	rta6.rta_type = IFLA_IFNAME;
	rta6.rta_len = RTA_LENGTH(veth0_len);

	iov[10].iov_base = &rta6;
	iov[10].iov_len = RTA_LENGTH(0);

	char veth0_copy[IFNAMSIZ];
	memcpy(veth0_copy, veth0, veth0_len);
	iov[11].iov_base = veth0_copy;
	iov[11].iov_len = RTA_ALIGN(veth0_len);

	return send_and_ack(sock, iov, 12);
}

int delete_veth(int sock, const char * eth) {

	struct iovec iov[2];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsghdr.nlmsg_type = RTM_DELLINK;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct ifinfomsg info_msg; memset(&info_msg, 0, sizeof(struct ifinfomsg));
	info_msg.ifi_family = AF_UNSPEC;
	info_msg.ifi_index = if_nametoindex(eth);

	iov[1].iov_base = &info_msg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	return send_and_ack(sock, iov, 2);

}

int set_status(int sock, const char * eth, int status) {

	struct iovec iov[2];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsghdr.nlmsg_type = RTM_NEWLINK;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct ifinfomsg info_msg; memset(&info_msg, 0, sizeof(struct ifinfomsg));
	info_msg.ifi_family = AF_UNSPEC;
	info_msg.ifi_index = if_nametoindex(eth);
	info_msg.ifi_change = IFF_UP;

	info_msg.ifi_flags = (status == IFF_UP) ? IFF_UP : 0;
	iov[1].iov_base = &info_msg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	return send_and_ack(sock, iov, 2);

}

#define INET_LEN 4
#define INET_PREFIX_LEN 32
int add_address(int sock, const char * addr, const char * eth) {

	struct iovec iov[4];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg)) + RTA_LENGTH(INET_LEN);
	nlmsghdr.nlmsg_type = RTM_NEWADDR;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	// TODO: ipv6 support
	unsigned char ipv4_addr[4];
	if (inet_pton(AF_INET, addr, (void *)&ipv4_addr) != 1) {
		dprintf(D_ALWAYS, "Invalid IP address: %s\n", addr);
		return 1;
	}

	unsigned eth_dev;
	if (!(eth_dev = if_nametoindex(eth))) {
		dprintf(D_ALWAYS, "Unable to determine index of %s.\n", eth);
		return 1;
	}

	struct ifaddrmsg info_msg; memset(&info_msg, 0, sizeof(struct ifaddrmsg));
	info_msg.ifa_family = AF_INET;
	info_msg.ifa_prefixlen = INET_PREFIX_LEN;
	info_msg.ifa_index = if_nametoindex(eth);

	iov[1].iov_base = &info_msg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifaddrmsg));

	struct rtattr rta;
	rta.rta_type = IFA_LOCAL;
	rta.rta_len = RTA_LENGTH(INET_LEN);
	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	iov[3].iov_base = ipv4_addr;
	iov[3].iov_len = RTA_ALIGN(INET_LEN);
        
	return send_and_ack(sock, iov, 4);

}

int add_local_route(int sock, const char * gw, const char * eth, int dst_len) {

	// Equivalent to:
	// ip route add default via 10.10.10.1
	// internally, default = 0/0
	struct iovec iov[6];

	unsigned char ipv4_addr[4];
	if (inet_pton(AF_INET, gw, (void *)&ipv4_addr) != 1) {
		dprintf(D_ALWAYS, "Invalid IP address: %s\n", gw);
		return 1;
	}
	if (dst_len == 24) {
		ipv4_addr[3] = 0;
	} else {
		dprintf(D_ALWAYS, "For the time being, only /24 local routes are supported (dst_len=%d).\n", dst_len);
		return 1;
	}

	unsigned eth_dev;
	if (!(eth_dev = if_nametoindex(eth))) {
		dprintf(D_ALWAYS, "Unable to determine index of %s.\n", eth);
		return 1;
	}

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)) + RTA_LENGTH(INET_LEN) + RTA_LENGTH(sizeof(unsigned));
	nlmsghdr.nlmsg_type = RTM_NEWROUTE;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct rtmsg rtmsg; memset(&rtmsg, 0, sizeof(struct rtmsg));
	rtmsg.rtm_family = AF_INET;
	rtmsg.rtm_dst_len = dst_len;
	rtmsg.rtm_table = RT_TABLE_MAIN;
	rtmsg.rtm_protocol = RTPROT_KERNEL;
	rtmsg.rtm_scope = RT_SCOPE_LINK;
	rtmsg.rtm_type = RTN_UNICAST;

	iov[1].iov_base = &rtmsg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg)); // Note: not sure if there's a better alignment here

	struct rtattr rta; memset(&rta, 0, sizeof(struct rtattr));
	rta.rta_type = RTA_DST;
	rta.rta_len = RTA_LENGTH(INET_LEN);

	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	iov[3].iov_base = ipv4_addr;	
	iov[3].iov_len = RTA_ALIGN(INET_LEN);

	struct rtattr rta2; memset(&rta2, 0, sizeof(struct rtattr));
	rta2.rta_type = RTA_OIF;
	rta2.rta_len = RTA_LENGTH(sizeof(unsigned));

	iov[4].iov_base = &rta2;
	iov[4].iov_len = RTA_LENGTH(0);

	iov[5].iov_base = &eth_dev;
	iov[5].iov_len = RTA_ALIGN(sizeof(unsigned));

	return send_and_ack(sock, iov, 6);
}

int add_default_route(int sock, const char * gw) {

	// Equivalent to:
	// ip route add default via 10.10.10.1
	// internally, default = 0/0
	struct iovec iov[4];

	// Setup the dest address/prefix
	size_t dst_len = 0;

	// TODO: ipv6 support
	unsigned char ipv4_addr[4];
	if (inet_pton(AF_INET, gw, (void *)&ipv4_addr) != 1) {
		dprintf(D_ALWAYS, "Invalid IP address: %s\n", gw);
		return 1;
	}
	ipv4_addr[3] = 1;

	struct nlmsghdr nlmsghdr;
	memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)) + RTA_LENGTH(INET_LEN);
	nlmsghdr.nlmsg_type = RTM_NEWROUTE;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct rtmsg rtmsg; memset(&rtmsg, 0, sizeof(struct rtmsg));
	rtmsg.rtm_family = AF_INET;
	rtmsg.rtm_dst_len = dst_len;
	rtmsg.rtm_table = RT_TABLE_MAIN;
	rtmsg.rtm_protocol = RTPROT_BOOT;
	rtmsg.rtm_scope = RT_SCOPE_UNIVERSE;
	rtmsg.rtm_type = RTN_UNICAST;

	iov[1].iov_base = &rtmsg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg)); // Note: not sure if there's a better alignment here

	struct rtattr rta2; memset(&rta2, 0, sizeof(struct rtattr));
	rta2.rta_type = RTA_GATEWAY;
	rta2.rta_len = RTA_LENGTH(INET_LEN);

	iov[2].iov_base = &rta2;
	iov[2].iov_len = RTA_LENGTH(0);

	iov[3].iov_base = ipv4_addr;
	iov[3].iov_len = RTA_ALIGN(INET_LEN);
	return send_and_ack(sock, iov, 4);
}

#define PID_T_LEN sizeof(pid_t)
int set_netns(int sock, const char * eth, pid_t pid) {

	struct iovec iov[4];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)) + RTA_LENGTH(PID_T_LEN);
	nlmsghdr.nlmsg_type = RTM_NEWLINK;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct ifinfomsg info_msg; memset(&info_msg, 0, sizeof(struct ifinfomsg));
	info_msg.ifi_family = AF_UNSPEC;
	info_msg.ifi_index = if_nametoindex(eth);

	iov[1].iov_base = &info_msg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	struct rtattr rta; memset(&rta, 0, sizeof(struct rtattr));
	rta.rta_type = IFLA_NET_NS_PID;
	rta.rta_len = RTA_LENGTH(PID_T_LEN);

	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	iov[3].iov_base = &pid;
	iov[3].iov_len = RTA_ALIGN(PID_T_LEN);

	return send_and_ack(sock, iov, 4);
}

int recv_message(int sock) {

	struct msghdr msghdr;
	struct sockaddr_nl addr;
	struct iovec iov[1];
	char buf[getpagesize()];
	ssize_t len;

	msghdr.msg_name = &addr;
	msghdr.msg_namelen = sizeof addr;
	msghdr.msg_iov = iov;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = NULL;
	msghdr.msg_controllen = 0;
	msghdr.msg_flags = 0;

	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof buf;

	struct nlmsghdr *nlmsghdr;

	len = recvmsg (sock, &msghdr, 0);

	for (nlmsghdr = (struct nlmsghdr *)buf; NLMSG_OK (nlmsghdr, len); nlmsghdr = NLMSG_NEXT (nlmsghdr, len)) {

		if (nlmsghdr->nlmsg_type == NLMSG_NOOP) {
			dprintf(D_ALWAYS, "Ignoring message due to error.\n");
			continue;
		}
	
		if (nlmsghdr->nlmsg_type == NLMSG_ERROR) {
			struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlmsghdr);
			if (nlmsghdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
				dprintf(D_ALWAYS, "Error message truncated.\n");
				return 1;
			} else if (err->error) {
				dprintf(D_ALWAYS, "Error message back from netlink: %d %s\n", -err->error, strerror(-err->error));
				errno = -err->error;
				return errno;
			} else {
				return 0;
			}
			return 1;
		}

		dprintf(D_ALWAYS, "Unknown message type: %d\n", nlmsghdr->nlmsg_type);
		return 1;
	}

	return 1;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

