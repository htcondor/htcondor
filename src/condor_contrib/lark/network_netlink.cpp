
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

/**
 * This module exports a few interfaces for manipulating the kernel's
 * network configuration.
 *
 * It can create ethernet devices, assign IP addresses, setup routes, etc.
 *
 * For example, to create an ethernet device and assign an IP address
 * (a real example should check the return codes):
 *
 * fd = create_socket();
 * create_veth(fd, "e_pipe", "i_pipe");
 * add_address(fd, "192.168.0.1", "i_pipe");
 * set_status(fd, "i_pipe", IFF_UP);
 * close(fd);
 *
 * Netlink is one of the less-documented parts of the kernel.  To help, we
 * have exhaustively documented the add_address function below.  If you want
 * to understand the code, start there.
 */

#ifdef __cplusplus
extern "C" {
#endif

int seq = time(NULL);


/**
 *  Create a socket to talk to the kernel via netlink
 *  Returns the socket fd upon success, or -errno upon failure
 */

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

/**
 * Send a netlink message to the kernel.
 * Appends the header for you - just input the raw data.
 *
 * Returns 0 on success, errno on failure.
 */
int send_to_kernel(int sock, struct iovec* iov, size_t ioveclen) {

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
		dprintf(D_ALWAYS, "Unable to send message to kernel: %d %s\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

// Forward decl
static int recv_message(int sock);

/**
 * Sends a message to the kernel; block until an ACK is recieved.
 *
 * Returns 0 on success, errno on failure.
 */
int send_and_ack(int sock, struct iovec* iov, size_t ioveclen) {

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
int create_veth(int sock, const char * veth0, const char * veth1, const char *veth0_mac, const char *veth1_mac) {

	struct iovec iov[16];

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
	nlmsghdr.nlmsg_len = NLMSG_SPACE(sizeof(struct ifinfomsg)) + RTA_LENGTH(0) + RTA_LENGTH(0) +
            RTA_ALIGN(VETH_LEN) + RTA_LENGTH(0) + RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + 
			RTA_LENGTH(0) + RTA_ALIGN(veth1_len) + (veth1_mac ? (RTA_LENGTH(0)+RTA_ALIGN(8)) : 0) + 
			RTA_LENGTH(0) + RTA_ALIGN(veth0_len) + (veth0_mac ? (RTA_LENGTH(0)+RTA_ALIGN(8)) : 0);
	nlmsghdr.nlmsg_type = RTM_NEWLINK;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH (0);

	// Request the link
	// Structure of message:
	// netlink message:
	//   - ifinfomsg
	//   - rtattr IFLA_LINKINFO (0)
	//     - rtattr IFLA_INFO_KIND (VETH_LEN)
	//     - rtattr IFLA_INFO_DATA (0)
	//       - rtattr VETH_INFO_PEER (0)
	//         - ifinfomsg
	//         - rtattr IFLA_IFNAME (veth1_len)
	//         - rtattr IFLA_ADDRESS (optional, 8)
	//   - rtattr IFLA_IFNAME (veth0_len)
	//   - rtattr IFLA_ADDRESS (optional, 8)
	struct ifinfomsg info_msg; memset(&info_msg, 0, sizeof(struct ifinfomsg));
	info_msg.ifi_family = AF_UNSPEC;

	iov[1].iov_base = &info_msg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

	struct rtattr rta; memset(&rta, 0, sizeof(struct rtattr));
	rta.rta_type = IFLA_LINKINFO;
	rta.rta_len = RTA_LENGTH(0) + RTA_LENGTH(VETH_LEN) + RTA_LENGTH(0) + RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + RTA_LENGTH(veth1_len) + (veth1_mac ? RTA_LENGTH(8) : 0);
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
	rta3.rta_len = RTA_LENGTH(0) + RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + RTA_LENGTH(veth1_len) + (veth1_mac ? RTA_LENGTH(8) : 0);

	iov[5].iov_base = &rta3;
	iov[5].iov_len = RTA_LENGTH(0);

	struct rtattr rta4; memset(&rta4, 0, sizeof(struct rtattr));
	rta4.rta_type =  VETH_INFO_PEER;
	rta4.rta_len = RTA_LENGTH(0) + NLMSG_ALIGN(sizeof(struct ifinfomsg)) + RTA_LENGTH(veth1_len) + (veth1_mac ? RTA_LENGTH(8) : 0);

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

	unsigned extra_iov = 0;
	char veth1_mac_copy[8]; memset(veth1_mac_copy, 0, 8);
	struct rtattr rta6; memset(&rta6, 0, sizeof(struct rtattr));
	if (veth1_mac)
	{
		rta6.rta_type = IFLA_ADDRESS;
		rta6.rta_len = RTA_LENGTH(6);
		iov[10].iov_base = &rta6;
		iov[10].iov_len = RTA_LENGTH(0);
		
		memcpy(veth1_mac_copy, veth1_mac, 6);
		iov[11].iov_base = veth1_mac_copy;
		iov[11].iov_len = RTA_ALIGN(8);
		extra_iov += 2;
	}

	struct rtattr rta7; memset(&rta7, 0, sizeof(struct rtattr));
	rta7.rta_type = IFLA_IFNAME;
	rta7.rta_len = RTA_LENGTH(veth0_len);

	iov[10+extra_iov].iov_base = &rta7;
	iov[10+extra_iov].iov_len = RTA_LENGTH(0);

	char veth0_copy[IFNAMSIZ];
	memcpy(veth0_copy, veth0, veth0_len);
	iov[11+extra_iov].iov_base = veth0_copy;
	iov[11+extra_iov].iov_len = RTA_ALIGN(veth0_len);

	char veth0_mac_copy[8]; memset(veth0_mac_copy, 0, 8);
	struct rtattr rta8; memset(&rta8, 0, sizeof(struct rtattr));
	if (veth0_mac)
	{
		rta8.rta_type = IFLA_ADDRESS;
		rta8.rta_len = RTA_LENGTH(6);
		iov[12+extra_iov].iov_base = &rta8;
		iov[12+extra_iov].iov_len = RTA_LENGTH(0);

		memcpy(veth0_mac_copy, veth0_mac, 6);
		iov[13+extra_iov].iov_base = veth0_mac_copy;
		iov[13+extra_iov].iov_len = RTA_ALIGN(8);
		extra_iov += 2;
	}

	return send_and_ack(sock, iov, 12+extra_iov);
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

/**
 * Add a given IP address (Support both IPv4 and IPv6) to an ethernet device.
 */
#define INET_LEN 4
#define INET6_LEN 16
int add_address(int sock, const char * addr, unsigned prefix_length, const char * eth) {

	/**
	 *  The message we send to the kernel will have four parts.  The netlink
	 *  packet has a header and body.  The body consists of an ifaddrmsg and
	 *  an RTA packet.  The RTA packet has a header and a body.
	 *   (1) The netlink header.  Contains the length, the message type,
	 *       packet sequence number, and the flags.  Type is struct nlmsghdr.
	 *   (2) A structure about which ethernet device to add the address too.
	 *       Specifies the address type, the prefix length, and the ethernet
	 *       device number.  Type is struct ifaddrmsg
	 *   (3) A RTA packet header.  Specifies the RTA packet type (IFA_LOCAL)
	 *       and total RTA length.
	 *   (4) RTA body.  Specifies the IPv4 address to use.
	 *
	 *   We construct the message piecemeal with an io-vector.  This results
	 *   (I think) in code a bit more readable than manually mucking about with
	 *   offsets.
	 *
	 */
	struct iovec iov[4];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
	// Compute the size of the packet (as it is fixed-size).  NLMSG_LENGTH is the
	// aligned size of the nlmsghdr plus a body of one (struct ifaddrmsg).  We add
	// a RTA packet too - RTA_LENGTH is the aligned size of the rtattr header and
	// an IPv4 address or an IPv6 address. The IP address length will be added after
	// the IP address type is determined.
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlmsghdr.nlmsg_type = RTM_NEWADDR; // This message is requesting to add a new address.
	// Flags are documented in /usr/include/linux/netlink.h, but copied here for completeness:
	//   NLM_F_REQUEST - It is request message
	//   NLM_F_CREATE  - Create, if it does not exist
	//   NLM_F_EXCL    - Do not touch, if it exists
	//   NLM_F_ACK     - Reply with ack, with zero or error code
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq; // The sequence number of the packet.  The kernel must
                                    // see an ordered sequence of netlink messages.
	nlmsghdr.nlmsg_pid = 0; // The destination of the message - PID 0, the kernel.


	// Add ipv6 support
	unsigned char ipv4_addr[4];
	unsigned char ipv6_addr[16];

	// Check whether the input string is a valid IPv4 or IPv6 address
	// Convert it into binary representation accordingly use inet_pton
	// First try IPv4, then IPv6, set ipaddr_type accordingly.
	
	int ipaddr_type;
	int return_value1 = inet_pton(AF_INET, addr, (void *)&ipv4_addr);
	if(return_value1 == 1){
		ipaddr_type = AF_INET;
	}
	else{
		int return_value2 = inet_pton(AF_INET6, addr, (void *)&ipv6_addr);
		if(return_value2 == 1){
			ipaddr_type = AF_INET6;
		}
		else{
			dprintf(D_ALWAYS, "Either invalid IPv4 or IPv6 address: %s\n", addr);
			return 1;
		}
	}
	

	// check the validness for prefix for either IPv4 or IPv6
	if(ipaddr_type == AF_INET){
		if ((prefix_length == 0) || (prefix_length > 32)) {
			dprintf(D_ALWAYS, "Invalid IPv4 prefix: %u\n", prefix_length);
			return 1;
		}
	}
	if(ipaddr_type == AF_INET6){
		if ((prefix_length == 0) || (prefix_length > 128)) {
			dprintf(D_ALWAYS, "Invalid IPv6 prefix: %u\n", prefix_length);
			return 1;
		}
	}

	// Update the length of the nlmsg according to ipaddr_type
	if(ipaddr_type == AF_INET){
		nlmsghdr.nlmsg_len += RTA_LENGTH(INET_LEN);
	}
	if(ipaddr_type == AF_INET6){
		nlmsghdr.nlmsg_len += RTA_LENGTH(INET6_LEN);
	}
		
	// The first element of the packet will be the header created above.
	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);



	unsigned eth_dev;
	// Each ethernet device is represented internally by an unsigned int.  Convert
	// the string name into this index number, as the kernel will want to operate
	// only on the offsets
	if (!(eth_dev = if_nametoindex(eth))) {
		dprintf(D_ALWAYS, "Unable to determine index of %s.\n", eth);
		return 1;
	}

	// Specify the kind of address we will be adding, the netmask, and the 
	// ethernet device to change.
	struct ifaddrmsg info_msg; memset(&info_msg, 0, sizeof(struct ifaddrmsg));
	if(ipaddr_type == AF_INET){
		info_msg.ifa_family = AF_INET;
	}
	if(ipaddr_type == AF_INET6){
		info_msg.ifa_family = AF_INET6;
	}
	info_msg.ifa_prefixlen = prefix_length;
	info_msg.ifa_index = if_nametoindex(eth);

	iov[1].iov_base = &info_msg;
	// NLMSG_ALIGN specifies the size of this part of the packet; unlike the NLMSG_LENGTH, it does
	// not include any header bits.
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifaddrmsg));

	// Finally, we create our RTA packet.  The RTA packet, embedded in the netlink packet, has
	// a header and a fairly simple body.
	struct rtattr rta;
	rta.rta_type = IFA_LOCAL;
	if(ipaddr_type == AF_INET){
		rta.rta_len = RTA_LENGTH(INET_LEN);
	}
	if(ipaddr_type == AF_INET6){
		rta.rta_len = RTA_LENGTH(INET6_LEN);
	}	
	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	// ipv4_addr and ipv6_addr are the binary encoding of the IPv4 address and IPv6 address done above respectively.
	if(ipaddr_type == AF_INET){
		iov[3].iov_base = ipv4_addr;
		iov[3].iov_len = RTA_ALIGN(INET_LEN);
	}
	if(ipaddr_type == AF_INET6){
		iov[3].iov_base = ipv6_addr;
		iov[3].iov_len = RTA_ALIGN(INET6_LEN);
	}

	// Finally, send the constructed packet to the kernel and wait for a response. 
	return send_and_ack(sock, iov, 4);

}

int add_local_route(int sock, const char * gw, const char * eth, int dst_len) {

	// Equivalent to:
	// ip route add 10.10.10.1/24 via veth1
	struct iovec iov[6];

	// Add IPv6 Support
	unsigned char ipv4_addr[4];
	unsigned char ipv6_addr[16];

	int ipaddr_type;
	int return_value1 = inet_pton(AF_INET, gw, (void *)&ipv4_addr);
	if(return_value1 == 1){
		ipaddr_type = AF_INET;
	}
	else{
		int return_value2 = inet_pton(AF_INET6, gw, (void *)&ipv6_addr);
		if(return_value2 == 1){
			ipaddr_type = AF_INET6;
		}
		else{
			dprintf(D_ALWAYS, "Either invalid IPv4 or IPv6 address: %s\n", gw);
			return 1;
		}
	}

	// Don't know how to deal with this part
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
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)) + RTA_LENGTH((ipaddr_type == AF_INET) ? INET_LEN : INET6_LEN) + RTA_LENGTH(sizeof(unsigned));
	nlmsghdr.nlmsg_type = RTM_NEWROUTE;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct rtmsg rtmsg; memset(&rtmsg, 0, sizeof(struct rtmsg));
	rtmsg.rtm_family = (ipaddr_type == AF_INET) ? AF_INET : AF_INET6;
	rtmsg.rtm_dst_len = dst_len;
	rtmsg.rtm_table = RT_TABLE_MAIN;
	rtmsg.rtm_protocol = RTPROT_KERNEL;
	rtmsg.rtm_scope = RT_SCOPE_LINK;
	rtmsg.rtm_type = RTN_UNICAST;

	iov[1].iov_base = &rtmsg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg)); // Note: not sure if there's a better alignment here

	struct rtattr rta; memset(&rta, 0, sizeof(struct rtattr));
	rta.rta_type = RTA_DST;
	rta.rta_len = RTA_LENGTH((ipaddr_type == AF_INET) ? INET_LEN : INET6_LEN);

	iov[2].iov_base = &rta;
	iov[2].iov_len = RTA_LENGTH(0);

	iov[3].iov_base = (ipaddr_type == AF_INET) ? ipv4_addr : ipv6_addr;	
	iov[3].iov_len = RTA_ALIGN((ipaddr_type == AF_INET) ? INET_LEN : INET6_LEN);

	struct rtattr rta2; memset(&rta2, 0, sizeof(struct rtattr));
	rta2.rta_type = RTA_OIF;
	rta2.rta_len = RTA_LENGTH(sizeof(unsigned));

	iov[4].iov_base = &rta2;
	iov[4].iov_len = RTA_LENGTH(0);

	iov[5].iov_base = &eth_dev;
	iov[5].iov_len = RTA_ALIGN(sizeof(unsigned));

	return send_and_ack(sock, iov, 6);
}

int add_default_route(int sock, const char * gw, const char * dev) {

	// Equivalent to:
	// ip route add default via 10.10.10.1 dev eth0
	// internally, default = 0/0
	struct iovec iov[6];

	// Setup the dest address/prefix
	size_t dst_len = 0;

	int has_gw = 0;
	if (gw && strcmp(gw, "0.0.0.0"))
		has_gw = 1;

	int has_dev = 0;
	int eth_dev;
	if (dev && strlen(dev)) {
		has_dev = 1;
		if (!(eth_dev = if_nametoindex(dev))) {
			dprintf(D_ALWAYS, "Unable to determine index of %s.\n", dev);
			return 1;
	        }
	}

	if (!has_dev && !has_gw) {
		dprintf(D_ALWAYS, "Must specify a gateway or device for default address.\n");
		return 1;
	}

	unsigned char ipv4_addr[4];
	unsigned char ipv6_addr[16];
	int ipaddr_type = AF_INET;
	if (has_gw) {
		dprintf(D_FULLDEBUG, "Adding IP address %s as default gateway.\n", gw);

		int return_value1 = inet_pton(AF_INET, gw, (void *)&ipv4_addr);
		if(return_value1 == 1) {
			ipaddr_type = AF_INET;
		} else {
			int return_value2 = inet_pton(AF_INET6, gw, (void *)&ipv6_addr);
			if(return_value2 == 1) {
				ipaddr_type = AF_INET6;
			}
			else {
				dprintf(D_ALWAYS, "Either invalid IPv4 or IPv6 address: %s\n", gw);
				return 1;
			}
		}
	}

	struct nlmsghdr nlmsghdr;
	memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	if (has_gw)
		nlmsghdr.nlmsg_len += RTA_LENGTH((ipaddr_type==AF_INET) ? INET_LEN : INET6_LEN);
	if (has_dev)
		nlmsghdr.nlmsg_len += RTA_LENGTH(sizeof(unsigned));
	nlmsghdr.nlmsg_type = RTM_NEWROUTE;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsghdr.nlmsg_seq = ++seq;
	nlmsghdr.nlmsg_pid = 0;

	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct rtmsg rtmsg; memset(&rtmsg, 0, sizeof(struct rtmsg));
	rtmsg.rtm_family = (ipaddr_type == AF_INET) ? AF_INET : AF_INET6;
	rtmsg.rtm_dst_len = dst_len;
	rtmsg.rtm_table = RT_TABLE_MAIN;
	rtmsg.rtm_protocol = RTPROT_BOOT;
	rtmsg.rtm_scope = RT_SCOPE_UNIVERSE;
	rtmsg.rtm_type = RTN_UNICAST;

	iov[1].iov_base = &rtmsg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg)); // Note: not sure if there's a better alignment here

	int rta_ctr = 2;

	struct rtattr rta2; 
	if (has_gw) {
		memset(&rta2, 0, sizeof(struct rtattr));
		rta2.rta_type = RTA_GATEWAY;
		rta2.rta_len = RTA_LENGTH((ipaddr_type == AF_INET) ? INET_LEN : INET6_LEN);

		iov[2].iov_base = &rta2;
		iov[2].iov_len = RTA_LENGTH(0);

		iov[3].iov_base = (ipaddr_type == AF_INET) ? ipv4_addr : ipv6_addr;
		iov[3].iov_len = RTA_ALIGN((ipaddr_type == AF_INET) ? INET_LEN : INET6_LEN);
		rta_ctr += 2;
	}
	struct rtattr rta3;
	if (has_dev) {
		memset(&rta3, 0, sizeof(struct rtattr));
		rta3.rta_type = RTA_OIF;
		rta3.rta_len = RTA_LENGTH(sizeof(unsigned));

		iov[rta_ctr].iov_base = &rta3;
		iov[rta_ctr].iov_len = RTA_LENGTH(0);
		rta_ctr++;

		iov[rta_ctr].iov_base = &eth_dev;
		iov[rta_ctr].iov_len = RTA_ALIGN(sizeof(unsigned));
		rta_ctr++;
	}
	return send_and_ack(sock, iov, rta_ctr);
}

/**
 * Move a ethernet device into a specified network namespace.
 *
 * Takes the ethernet device named $eth and moves it from the system
 * namespace into the network namespace inhabited by $pid.
 *
 * Returns 0 on sucecss, errno on failure.
 */
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

/**
 * Idea for this function is taken from the iproute2 source.  Iterate through all
 * the attributes and place them into a lookup table, making it more convenient
 * to manipulate the attributes than iterating through a huge switch statement.
 */
static
int parse_rtattr(struct rtattr *attr_table[], struct rtattr * rta, size_t len, int max)
{
	memset(attr_table, 0, sizeof(struct rtattr*) * (max + 1));
	while (RTA_OK(rta, len)) {
		if ((rta->rta_type <= RTA_MAX) && (!attr_table[rta->rta_type]))
		{
			attr_table[rta->rta_type] = rta;
		}
		rta = RTA_NEXT(rta, len);
	}
	if (len) {
		return 1;
	}
	return 0;
}

/**
 * Query the kernel for all routes.
 * For each route returned, the buffer containing the route will be fed to the
 * filter function specified.
 *
 * Returns 0 if all routes were successfully processed.
 * Returns 1 otherwise.
 *  - If the filter returns non-zero on a route, all subsequent routes are ignored.
 */
int get_routes(int sock, int (*filter)(struct nlmsghdr, struct rtmsg, struct rtattr *[RTA_MAX+1], void *), void * user_arg) {

	struct iovec iov[2];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nlmsghdr.nlmsg_type = RTM_GETROUTE;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ROOT;
	nlmsghdr.nlmsg_pid = 0;
	nlmsghdr.nlmsg_seq = 0;
	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct rtmsg rtm; memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_flags = AF_INET;
	iov[1].iov_base = &rtm;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg));

	int result;
	if ((result = send_to_kernel(sock, iov, 2) < 0)) {
		dprintf(D_ALWAYS, "Failed to query kernel for routes.\n");
		return 1;
	}

	struct sockaddr_nl nladdr; memset(&nladdr, 0, sizeof(nladdr));
	struct msghdr msg; memset(&msg, 0, sizeof(msg));
	msg.msg_name = &nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	char msg_buffer[16*1024]; memset(msg_buffer, 0, 16*1024);
	iov[0].iov_base = msg_buffer;
	iov[0].iov_len = 16*1024;

	int done = 0, had_filter_error = 0, filter_result = 0;
	while (1)
	{
		if (done) break;

		result = recvmsg(sock, &msg, 0);
		if (result < 0) {
			if (errno == EINTR || errno == EAGAIN)
			{
				continue;
			}
			dprintf(D_ALWAYS, "Error recieved from kernel (errno=%d, %s)\n", errno, strerror(errno));
			return errno;
		}
		if (result == 0) {
			dprintf(D_ALWAYS, "Unexpected EOF from kernel\n");
			return 1;
		}

		struct nlmsghdr *nlmsghdr2;
		unsigned message_size = static_cast<unsigned>(result);
		for (nlmsghdr2 = (struct nlmsghdr *)iov[0].iov_base; NLMSG_OK(nlmsghdr2, message_size); nlmsghdr2 = NLMSG_NEXT(nlmsghdr2, message_size)) {

			if (nlmsghdr2->nlmsg_type == NLMSG_NOOP) {
				dprintf(D_ALWAYS, "Ignoring message due to no-op.\n");
				continue;
			}
			if (nlmsghdr2->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlmsghdr2);
				if (nlmsghdr2->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
					dprintf(D_ALWAYS, "Error message truncated.\n");
					return 1;
				} else if (err->error) {
					dprintf(D_ALWAYS, "Error message back from netlink: %d %s\n", -err->error, strerror(-err->error));
					errno = -err->error;
					return errno;
				}
				return 1;
			}
			if (nlmsghdr2->nlmsg_type == NLMSG_DONE) {
				done = 1;
				continue;
			}
			// We continue to recieve messages even if we failed in processing one of them.
			// This is so the next netlink user does not recieve unexpected messages.
			if (had_filter_error) {
				continue;
			}

			if (nlmsghdr2->nlmsg_type != RTM_NEWROUTE && nlmsghdr2->nlmsg_type != RTM_DELROUTE) {
				dprintf(D_ALWAYS, "Ignoring non-route message of type %d.\n", nlmsghdr2->nlmsg_type);
				continue;
			}
			if (nlmsghdr2->nlmsg_len < NLMSG_LENGTH(sizeof(struct rtmsg))) {
				dprintf(D_ALWAYS, "Ignoring truncated route message of length %d.\n", nlmsghdr2->nlmsg_len);
				continue;
			}
			struct rtmsg route_msg; memcpy(&route_msg, NLMSG_DATA(nlmsghdr2), sizeof(struct rtmsg));
			struct rtattr *attr_table[RTA_MAX+1];
			struct rtattr *rta = RTM_RTA(NLMSG_DATA(nlmsghdr2));
			size_t len = message_size - NLMSG_LENGTH(sizeof(struct rtmsg));
			if (parse_rtattr(attr_table, rta, len, RTA_MAX)) {
				had_filter_error = 1;
				continue;
			}

			if ((filter_result = (*filter)(*nlmsghdr2, route_msg, attr_table, user_arg))) {
				had_filter_error = 1;
			}
		}
	}
	return 0;
}


/**
 * Query the kernel for all addresses.
 * For each address returned, the buffer containing the address will be fed to the
 * filter function specified.
 *
 * Returns 0 if all routes were successfully processed.
 * Returns 1 otherwise.
 *  - If the filter returns non-zero on an address, all subsequent addresses are ignored.
 */
int get_addresses(int sock, int (*filter)(struct nlmsghdr, struct ifaddrmsg, struct rtattr *[IFA_MAX+1], void *), void * user_arg) {

	struct iovec iov[2];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nlmsghdr.nlmsg_type = RTM_GETADDR;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ROOT;
	nlmsghdr.nlmsg_pid = 0;
	nlmsghdr.nlmsg_seq = 0;
	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	struct rtmsg rtm; memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_flags = AF_INET; // |AF_INET6 -- once we're ready
	iov[1].iov_base = &rtm;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg));

	int result;
	if ((result = send_to_kernel(sock, iov, 2) < 0)) {
		dprintf(D_ALWAYS, "Failed to query kernel for routes.\n");
		return 1;
	}

	struct sockaddr_nl nladdr; memset(&nladdr, 0, sizeof(nladdr));
	struct msghdr msg; memset(&msg, 0, sizeof(msg));
	msg.msg_name = &nladdr;
	msg.msg_namelen = sizeof(nladdr);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	char msg_buffer[16*1024]; memset(msg_buffer, 0, 16*1024);
	iov[0].iov_base = msg_buffer;
	iov[0].iov_len = 16*1024;

	int done = 0, had_filter_error = 0, filter_result = 0;
	while (1)
	{
		if (done) break;

		result = recvmsg(sock, &msg, 0);
		if (result < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			dprintf(D_ALWAYS, "Error recieved from kernel (errno=%d, %s)\n", errno, strerror(errno));
			return errno;
		}
		if (result == 0) {
			dprintf(D_ALWAYS, "Unexpected EOF from kernel\n");
			return 1;
		}

		struct nlmsghdr *nlmsghdr2;
		unsigned message_size = static_cast<unsigned>(result);
		for (nlmsghdr2 = (struct nlmsghdr *)iov[0].iov_base; NLMSG_OK(nlmsghdr2, message_size); nlmsghdr2 = NLMSG_NEXT(nlmsghdr2, message_size))
		{
			if (nlmsghdr2->nlmsg_type == NLMSG_NOOP) {
				dprintf(D_ALWAYS, "Ignoring message due to no-op.\n");
				continue;
			}
			if (nlmsghdr2->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlmsghdr2);
				if (nlmsghdr2->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
					dprintf(D_ALWAYS, "Error message truncated.\n");
					return 1;
				} else if (err->error) {
					dprintf(D_ALWAYS, "Error message back from netlink: %d %s\n", -err->error, strerror(-err->error));
					errno = -err->error;
					return errno;
				}
				return 1;
			}
			if (nlmsghdr2->nlmsg_type == NLMSG_DONE) {
				done = 1;
				continue;
			}
			// We continue to recieve messages even if we failed in processing one of them.
			// This is so the next netlink user does not recieve unexpected messages.
			if (had_filter_error) {
				continue;
			}

			if (nlmsghdr2->nlmsg_type != RTM_NEWADDR && nlmsghdr2->nlmsg_type != RTM_DELADDR) {
				dprintf(D_ALWAYS, "Ignoring non-address message of type %d.\n", nlmsghdr2->nlmsg_type);
				continue;
			}
			if (nlmsghdr2->nlmsg_len < NLMSG_LENGTH(sizeof(struct ifaddrmsg))) {
				dprintf(D_ALWAYS, "Ignoring truncated route message of length %d.\n", nlmsghdr2->nlmsg_len);
				continue;
			}
			struct ifaddrmsg addr_msg; memcpy(&addr_msg, NLMSG_DATA(nlmsghdr2), sizeof(struct ifaddrmsg));
			struct rtattr *attr_table[IFA_MAX+1];
			struct rtattr *rta = IFA_RTA(NLMSG_DATA(nlmsghdr2));
			size_t len = message_size - NLMSG_LENGTH(sizeof(struct ifaddrmsg));
			if (parse_rtattr(attr_table, rta, len, IFA_MAX)) {
				had_filter_error = 1;
				continue;
			}

			if ((filter_result = (*filter)(*nlmsghdr2, addr_msg, attr_table, user_arg))) {
				had_filter_error = 1;
			}
		}
	}
	return 0;
}


/*
 * Receive a message from netlink.
 *
 * Returns 0 or negative on success.
 * Returns 1 on generic failure, or errno of the failed system call.
 * If return value is negative, we recieved an netlink message of type NLMSG_ERROR;
 * the -errno from that message is returned.
 */
static int
recv_message(int sock) {

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

	while (1) {
		len = recvmsg(sock, &msghdr, 0);
		if ((len < 0) && (errno == EINTR || errno == EAGAIN))
			continue;
		if (len < 0) {
			dprintf(D_ALWAYS, "Error from kernel when recieving message (errno=%d, %s)\n", errno, strerror(errno));
			return errno;
		}
		if (len == 0) {
			dprintf(D_ALWAYS, "Got EOF on netlink socket.\n");
			return 1;
		}
		if (addr.nl_pid == 0)
			break;
	}

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

		// Ignore these messages - may be leftover from a bridge query.
		if (nlmsghdr->nlmsg_type == RTM_NEWLINK || nlmsghdr->nlmsg_type == RTM_DELLINK) {
			continue;
		}
		dprintf(D_ALWAYS, "Unknown message type: %d\n", nlmsghdr->nlmsg_type);
		return 1;
	}

	return 1;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

