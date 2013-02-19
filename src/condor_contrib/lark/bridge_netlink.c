
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_bridge.h>
#include <linux/if.h>
#include <linux/socket.h>

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "network_manipulation.h"

static const char *port_states[] = {
        [BR_STATE_DISABLED] = "disabled",
        [BR_STATE_LISTENING] = "listening",
        [BR_STATE_LEARNING] = "learning",
        [BR_STATE_FORWARDING] = "forwarding",
        [BR_STATE_BLOCKING] = "blocking",
};

#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

static const char *link_states[] = {
	[IF_OPER_UNKNOWN] = "unknown",
	[IF_OPER_NOTPRESENT] = "notpresent",
	[IF_OPER_DOWN] = "down",
	[IF_OPER_LOWERLAYERDOWN] = "lowerlayerdown",
	[IF_OPER_TESTING] = "testing",
	[IF_OPER_DORMANT] = "dormant",
	[IF_OPER_UP] = "up",
};

extern int seq;

static int send_to_kernel(int sock, struct iovec* iov, size_t ioveclen) {

        if (sock < 0) {
                printf("Invalid socket: %d.\n", sock);
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
                printf("Unable to send create_veth message to kernel: %d %s\n", errno, strerror(errno));
                return errno;
        }
        return 0;
}

void parse_message(struct nlmsghdr *nlmsg, ssize_t len)
{
   struct ifinfomsg *ifi = NLMSG_DATA(nlmsg);
   int nllen = nlmsg->nlmsg_len;

   char b1[IFNAMSIZ+1]; b1[IFNAMSIZ] = '\0';
   struct rtattr * tb[IFLA_MAX+1];
   int af_family = ifi->ifi_family;

   nllen -= NLMSG_LENGTH(sizeof(*ifi));

   char * name = if_indextoname(ifi->ifi_index, b1) ? b1 : "unknown";

   printf("IFI - family %d, device %d, name %s.\n", af_family, ifi->ifi_index, name);
   if (af_family == PF_BRIDGE) printf("Address family is bridge.\n");
   else printf("Address family is not bridge.\n");

   struct rtattr *attribute;
   int protinfo, state;
   for (attribute = IFLA_RTA(ifi); RTA_OK(attribute, len); attribute = RTA_NEXT(attribute, nllen))
   {
      switch (attribute->rta_type){
         case IFLA_IFNAME:
            printf("Interface %d : %s\n", ifi->ifi_index, (char *) RTA_DATA(attribute));
            break;
         case IFLA_MASTER:
            name = if_indextoname(*(uint32_t *)RTA_DATA(attribute), b1) ? b1 : "unknown";
            printf("Interface master: %s\n", name);
            break;
         case IFLA_MTU:
            printf("MTU %d\n", *(uint32_t *)RTA_DATA(attribute));
            break;
         case IFLA_ADDRESS:
            printf("Address RTA.\n");
            break;
         case IFLA_OPERSTATE:
            state = (int)*(char *)RTA_DATA(attribute);
            printf("Operational state: %s\n", link_states[state]);
            break;
         case IFLA_PROTINFO:
            protinfo = (int)*(char *)RTA_DATA(attribute);
            printf("Bridge state: %s\n", port_states[protinfo]);
            break;
         default:
            printf("Unknown RTA: %d\n", attribute->rta_type);
      }
   }
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
			printf("Ignoring message due to error.\n");
			continue;
		}
	
		if (nlmsghdr->nlmsg_type == NLMSG_ERROR) {
			struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(nlmsghdr);
			if (nlmsghdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
				printf("Error message truncated.\n");
				return 1;
			} else if (err->error) {
				printf("Error message back from netlink: %d %s\n", -err->error, strerror(-err->error));
				errno = -err->error;
				return errno;
			} else {
				return 0;
			}
			return 1;
		}

		if (nlmsghdr->nlmsg_type == NLMSG_DONE) {
			printf("Netlink done.\n");
			return 0;
		}

		parse_message(nlmsghdr, len);

		printf("Unknown message type: %d\n", nlmsghdr->nlmsg_type);
		//return 1;
	}

	return 1;
}

static int send_and_ack(int sock, struct iovec* iov, size_t ioveclen) {

	int rc;
	if ((rc = send_to_kernel(sock, iov, ioveclen))) {
		printf("Send to kernel failed: %d\n", rc);
		return rc;
	} 
	if ((rc = recv_message(sock))) {
		printf("Message not successfully ACK'd: %d.\n", rc);
		return rc;
	}
	return 0;

}

int wait_for_forwarding(int fd, const char *link)
{
   int fd = create_socket(RTMGRP_LINK);

   // Subscribe to the link-layer multicast group.
   int group = RTNLGRP_LINK;
   setsockopt(fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group, sizeof(group));

   struct iovec iov[3];

   struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(struct nlmsghdr));
   nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg));
   nlmsghdr.nlmsg_type = RTM_GETLINK;
   nlmsghdr.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
   nlmsghdr.nlmsg_pid = 0;
   nlmsghdr.nlmsg_seq = ++seq;

   iov[0].iov_base = &nlmsghdr;
   iov[0].iov_len = NLMSG_LENGTH(0);

   struct rtgenmsg gen;
   gen.rtgen_family = PF_BRIDGE;

   iov[1].iov_base = &gen;
   iov[1].iov_len = sizeof(gen);

   send_and_ack(fd, iov, 2);
   while(1)
   recv_message(fd);
}

