
#include "condor_common.h"

#include "network_manipulation.h"

#include "condor_debug.h"
#include <net/if.h>
#include <netinet/in.h>
#include <linux/rtnetlink.h>

extern int seq;

/**
 * Detect if an interface has a default route.
 * Returns 1 if it does; 0 if it does not
 * Returns a negative value on error.
 */
struct default_route_check_s {
	int iface_id;
	int has_default;
};

static int
check_default_route(struct nlmsghdr /*nlmsghdr*/, struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1], void *user_arg) {
	struct default_route_check_s *arg = (struct default_route_check_s*)user_arg;
	if (rtmsg.rtm_dst_len == 0 && attr_table[RTA_OIF] && 
			(arg->iface_id == *(int*)RTA_DATA(attr_table[RTA_OIF]))) {
		arg->has_default = 1;
	}
	return 0;
}

void
print_route_info(struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1])
{
	dprintf(D_ALWAYS, "Route information\n");
	switch(rtmsg.rtm_family) {
	case AF_INET:
		dprintf(D_ALWAYS, "- IPv4 route.\n");
		break;
	case AF_INET6:
		dprintf(D_ALWAYS, "- IPv6 route.\n");
		break;
	default:
		dprintf(D_ALWAYS, "- Unknown family: %d.\n", rtmsg.rtm_family);
	}
	dprintf(D_ALWAYS, "- Source length: %d.\n", rtmsg.rtm_src_len);
	dprintf(D_ALWAYS, "- Destination length: %d.\n", rtmsg.rtm_dst_len);
	switch(rtmsg.rtm_scope) {
	case RT_SCOPE_UNIVERSE:
		dprintf(D_ALWAYS, "- Scope: Universe\n");
		break;
	case RT_SCOPE_NOWHERE:
		dprintf(D_ALWAYS, "- Scope: nowhere\n");
		break;
	case RT_SCOPE_LINK:
		dprintf(D_ALWAYS, "- Scope: link\n");
		break;
	case RT_SCOPE_HOST:
		dprintf(D_ALWAYS, "- Scope: host\n");
		break;
	default:
		dprintf(D_ALWAYS, "Unknown scope: %d.\n", rtmsg.rtm_scope);
	}

	char addr[256];
	struct in_addr ipv4;
	struct in6_addr ipv6;
	void * ipaddr = rtmsg.rtm_family == AF_INET ? static_cast<void*>(&ipv4) : static_cast<void*>(&ipv6);
	if ((attr_table[RTA_SRC]))
	{
		if (rtmsg.rtm_family == AF_INET)
			memcpy(&ipv4.s_addr, RTA_DATA(attr_table[RTA_SRC]), sizeof(ipv4.s_addr));
		else if (rtmsg.rtm_family == AF_INET6)
			memcpy(&ipv6.s6_addr, RTA_DATA(attr_table[RTA_SRC]), sizeof(ipv6.s6_addr));
		if (inet_ntop(rtmsg.rtm_family, ipaddr, addr, 256))
			dprintf(D_ALWAYS, "- Source address: %s.\n", addr);
		else
			dprintf(D_ALWAYS, "- Source address: PARSE FAILED");
	}
	if ((attr_table[RTA_DST]))
	{
		if (rtmsg.rtm_family == AF_INET)
			memcpy(&ipv4.s_addr, RTA_DATA(attr_table[RTA_DST]), sizeof(ipv4.s_addr));
		else if (rtmsg.rtm_family == AF_INET6)
			memcpy(&ipv6.s6_addr, RTA_DATA(attr_table[RTA_DST]), sizeof(ipv6.s6_addr));
		if (inet_ntop(rtmsg.rtm_family, ipaddr, addr, 256))
			dprintf(D_ALWAYS, "- Dest address: %s.\n", addr);
		else
			dprintf(D_ALWAYS, "- Dest address: PARSE FAILED");
	}
	if ((attr_table[RTA_GATEWAY]))
	{
		if (rtmsg.rtm_family == AF_INET)
			memcpy(&ipv4.s_addr, RTA_DATA(attr_table[RTA_GATEWAY]), sizeof(ipv4.s_addr));
		else if (rtmsg.rtm_family == AF_INET6)
			memcpy(&ipv6.s6_addr, RTA_DATA(attr_table[RTA_GATEWAY]), sizeof(ipv6.s6_addr));
		if (inet_ntop(rtmsg.rtm_family, ipaddr, addr, 256))
			dprintf(D_ALWAYS, "- Gateway address: %s.\n", addr);
		else
			dprintf(D_ALWAYS, "- Gateway address: PARSE FAILED");
	}
}

int
interface_has_default_route(int sock, const char * eth) {
	if (!eth || !*eth) {
		dprintf(D_ALWAYS, "Looking for default route on invalid interface");
		return -1;
	}
	unsigned eth_dev;
	if (!(eth_dev = if_nametoindex(eth))) {
		dprintf(D_ALWAYS, "Unable to determine index of %s.\n", eth);
		return -1;
	}

	struct default_route_check_s arg;
	arg.iface_id = eth_dev;
	arg.has_default = 0;
	if (get_routes(sock, check_default_route, &arg)) {
		dprintf(D_ALWAYS, "Error when searching for default route.\n");
		return -1;
	}
	return arg.has_default;
}

typedef struct route_info_s {
	struct rtmsg * routes;
	struct rtattr ***attr_tables; // An array of tables of pointers.  C sucks.
	size_t route_len;
	size_t route_alloc;
	int eth_dev;
	int bridge_dev;
} route_info_t;

static int
populate_route_info(route_info_t *r) {
	if (!r)
		return 1;
	r->routes = (struct rtmsg*)malloc(5*sizeof(struct rtmsg));
	if (!r->routes)
	{
		dprintf(D_ALWAYS, "Failed to allocate routes list.\n");
		return 2;
	}
	r->route_len = 0;
	r->route_alloc = 0;
	r->attr_tables = (struct rtattr***)malloc(5*sizeof(struct rtattr **));
	if (!r->attr_tables) {
		dprintf(D_ALWAYS, "Failed to allocate route attribute list.\n");
		return 3;
	}
	for (unsigned i=0; i<5; i++) {
		r->attr_tables[i] = (struct rtattr**)calloc((RTA_MAX+1), sizeof(struct rtattr*));
		if (!r->attr_tables[i])
			return 1;
	}
	r->route_alloc = 5;
	return 0;
}

int
add_route_info(route_info_t *route_info, struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1]) {
	if (route_info->route_len >= route_info->route_alloc) {
		route_info->routes = (struct rtmsg*)realloc(route_info->routes, (route_info->route_alloc+5)*sizeof(struct rtmsg));
		route_info->attr_tables = (struct rtattr ***)realloc(route_info->attr_tables, (route_info->route_alloc+5)*sizeof(struct rtattr**));
		if (!route_info->routes || !route_info->attr_tables)
			return 1;
		memset(route_info->routes+route_info->route_alloc, 0, sizeof(struct rtmsg*)*5);
		memset(route_info->attr_tables+route_info->route_alloc, 0, sizeof(struct rtattr**)*5);
		for (unsigned i=0; i<5; i++) {
			route_info->attr_tables[i+route_info->route_alloc] = (struct rtattr**)calloc((RTA_MAX+1), sizeof(struct rtattr*));
			if (!route_info->attr_tables[i+route_info->route_alloc])
				return 1;
		}
		route_info->route_alloc += 5;
	}
	memcpy((void*)(route_info->routes+route_info->route_len), (void*)(&rtmsg), sizeof(rtmsg));
	for (int idx=0; idx<RTA_MAX; idx++) {
		struct rtattr* attr = attr_table[idx];
		if (!attr) continue;
		struct rtattr* new_attr = (struct rtattr*)malloc(attr->rta_len);
		if (!new_attr)
			return 1;
		memcpy(new_attr, attr, attr->rta_len);
		route_info->attr_tables[route_info->route_len][idx] = new_attr;
	}
	route_info->route_len++;
	return 0;
}

int
filter_routes(struct nlmsghdr, struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1], void *user_arg) {

	route_info_t *arg = (route_info_t *)user_arg;

	if (!attr_table[RTA_OIF])
		return 0;

	int * oif = (int*)RTA_DATA(attr_table[RTA_OIF]);
	if (*oif == arg->eth_dev) {
		//print_route_info(rtmsg, attr_table);
		add_route_info(arg, rtmsg, attr_table);
	}
	return 0;
}

void
free_route_info(route_info_t *route_info) {
	if (route_info->routes) {
		free(route_info->routes);
	}
	if (route_info->attr_tables) {
		for (size_t idx=0; idx<route_info->route_alloc; idx++) {
			if (route_info->attr_tables[idx]) {
				for (size_t j=0; j<RTA_MAX+1; j++)
					if (route_info->attr_tables[idx][j])
						free(route_info->attr_tables[idx][j]);
				free(route_info->attr_tables[idx]);
			}
		}
		free(route_info->attr_tables);
	}
}

static int
route_action(int sock, int action, struct rtmsg *rtmsg, struct rtattr *attr_table[RTA_MAX+1]) {

	struct iovec iov[2+RTA_MAX];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	nlmsghdr.nlmsg_type = action;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	if (action == RTM_NEWROUTE)
		nlmsghdr.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
	nlmsghdr.nlmsg_pid = 0;
	nlmsghdr.nlmsg_seq = seq++;
	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	iov[1].iov_base = rtmsg;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct rtmsg));

	unsigned attr_count = 0;
	for (unsigned idx=0; idx<RTA_MAX; idx++) {
		if (!attr_table[idx])
			continue;
		if (idx == RTA_UNSPEC) continue;
		iov[2+attr_count].iov_base = attr_table[idx];
		iov[2+attr_count].iov_len = attr_table[idx]->rta_len;
		nlmsghdr.nlmsg_len += attr_table[idx]->rta_len;
		attr_count++;
	}

	return send_and_ack(sock, iov, 2+attr_count);
}


int
move_routes_to_bridge(int sock, const char * eth, const char * bridge)
{
	if (!eth || !bridge) {
		dprintf(D_ALWAYS, "No default bridge or interface specified.\n");
		return 1;
	}

	unsigned eth_dev;
	if (!(eth_dev = if_nametoindex(eth))) {
		dprintf(D_ALWAYS, "Unable to determine index of interface %s.\n", eth);
		return -1;
	}
	unsigned bridge_dev;
	if (!(bridge_dev = if_nametoindex(bridge))) {
		dprintf(D_ALWAYS, "Unable to determine index of bridge %s.\n", bridge);
		return -1;
	}

	route_info_t route_info;
	if (populate_route_info(&route_info)) {
		dprintf(D_ALWAYS, "Failed to allocate route info structures.\n");
		return -1;
	}
	route_info.eth_dev = eth_dev;

	// Build up a list of routes to copy over.
	if (get_routes(sock, filter_routes, &route_info)) {
		dprintf(D_ALWAYS, "Failed to retrieve list of routes from kernel.\n");
		free_route_info(&route_info);
		return -1;
	}

	// Remove then add the list of routes.
	dprintf(D_FULLDEBUG, "Count of routes on bridge device: %lu.\n", route_info.route_len);
	for (unsigned idx=0; idx<route_info.route_len; idx++) {
		struct rtmsg *rtmsg = route_info.routes + idx;
		struct rtattr **attr_table = route_info.attr_tables[idx];

		if (!attr_table[RTA_OIF]) continue;

		int action_result = 0;
		// Remove old route
		// Ignore ESRCH - that indicates the deleted route was not found.
		if ((action_result = route_action(sock, RTM_DELROUTE, rtmsg, attr_table)) && (action_result != ESRCH)) {
			dprintf(D_ALWAYS, "Failed to delete routes (%d).\n", action_result);
			free_route_info(&route_info);
			return -1;
		}
		if (action_result == ESRCH)
		{
			continue;
		}

		// Make route point at new bridge iface
		int *oif = (int*)RTA_DATA(attr_table[RTA_OIF]);
		*oif = bridge_dev;

		// Create new route.
		// EEXIST indicates the route already exists - for example, this may be true if the kernel won the
		// race to install the IPv6 link-local / host-local routes.
		if ((action_result = route_action(sock, RTM_NEWROUTE, rtmsg, attr_table)) && (action_result != EEXIST)) {
			dprintf(D_ALWAYS, "FATAL: Deleted old route but couldn't create new one.\n");
			free_route_info(&route_info);
			return -1;
		}
	}
	free_route_info(&route_info);
	return 0;
}

typedef struct addr_info_s {
	struct ifaddrmsg * addrs;
	struct rtattr ***attr_tables;
	size_t len;
	size_t alloc;
	unsigned eth_dev;
} addr_info_t;

static int
populate_addr_info(addr_info_t *a) {
	if (!a)
		return 1;
	a->len = 0;
	a->alloc = 0;
	a->addrs = (struct ifaddrmsg *)malloc(5*sizeof(struct ifaddrmsg));
	a->attr_tables = (struct rtattr ***)malloc(5*sizeof(struct rtattr**));
	if (!a->addrs || !a->attr_tables) {
		return 1;
	}
	for (unsigned i=0; i<5; i++) {
		a->attr_tables[i] = (struct rtattr**)calloc((IFA_MAX+1), sizeof(struct rtattr*));
		if (!a->attr_tables[i+a->alloc])
			return 1;
	}
	a->alloc = 5;
	return 0;
}

int
add_addr_info(addr_info_t *addr_info, struct ifaddrmsg addrmsg, struct rtattr *attr_table[IFA_MAX+1]) {
	if (addr_info->len >= addr_info->alloc) {
		addr_info->addrs = (struct ifaddrmsg*)realloc(addr_info->addrs, (addr_info->alloc+5)*sizeof(struct ifaddrmsg));
		addr_info->attr_tables = (struct rtattr ***)realloc(addr_info->attr_tables, (addr_info->alloc+5)*sizeof(struct rtattr**));
		if (!addr_info->addrs || !addr_info->attr_tables)
			return 1;
		memset(addr_info->addrs+addr_info->alloc, 0, sizeof(struct ifaddrmsg*)*5);
		memset(addr_info->attr_tables+addr_info->alloc, 0, sizeof(struct rtattr**)*5);
		for (unsigned i=0; i<5; i++) {
			addr_info->attr_tables[i+addr_info->alloc] = (struct rtattr**)calloc((IFA_MAX+1), sizeof(struct rtattr*));
			if (!addr_info->attr_tables[i+addr_info->alloc])
				return 1;
		}
		addr_info->alloc += 5;
	}
	memcpy(addr_info->addrs+addr_info->len, &addrmsg, sizeof(ifaddrmsg));
	for (int idx=0; idx<IFA_MAX; idx++) {
		struct rtattr* attr = attr_table[idx];
		if (!attr) continue;
		struct rtattr* new_attr = (struct rtattr*)malloc(attr->rta_len);
		if (!new_attr)
			return 1;
		memcpy(new_attr, attr, attr->rta_len);
		addr_info->attr_tables[addr_info->len][idx] = new_attr;
	}
	addr_info->len++;
	return 0;
}

int
filter_addresses(struct nlmsghdr, struct ifaddrmsg addrmsg, struct rtattr *attr_table[IFA_MAX+1], void *user_arg) {
	addr_info_t *arg = (addr_info_t *)user_arg;
	if (addrmsg.ifa_index != arg->eth_dev)
		return 0;

	add_addr_info(arg, addrmsg, attr_table);
	return 0;
}

void
free_addr_info(addr_info_t *addr_info) {
	if (addr_info->addrs) {
		free(addr_info->addrs);
	}
	if (addr_info->attr_tables) {
		for (size_t idx=0; idx<addr_info->alloc; idx++) {
			if (addr_info->attr_tables[idx]) {
				for (size_t j=0; j<IFA_MAX+1; j++)
					if (addr_info->attr_tables[idx][j])
						free(addr_info->attr_tables[idx][j]);
				free(addr_info->attr_tables[idx]);
			}
		}
		free(addr_info->attr_tables);
	}
}

static int
iface_action(int sock, int action, struct ifaddrmsg *iface, struct rtattr *attr_table[IFA_MAX+1]) {

	struct iovec iov[2+IFA_MAX];

	struct nlmsghdr nlmsghdr; memset(&nlmsghdr, 0, sizeof(nlmsghdr));
	nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlmsghdr.nlmsg_type = action;
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	if (action == RTM_NEWADDR)
		nlmsghdr.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
	nlmsghdr.nlmsg_pid = 0;
	nlmsghdr.nlmsg_seq = seq++;
	iov[0].iov_base = &nlmsghdr;
	iov[0].iov_len = NLMSG_LENGTH(0);

	iov[1].iov_base = iface;
	iov[1].iov_len = NLMSG_ALIGN(sizeof(struct ifaddrmsg));

	unsigned attr_count = 0;
	for (unsigned idx=0; idx<IFA_MAX; idx++) {
		if (!attr_table[idx])
			continue;
		if (idx == IFA_UNSPEC) continue;
		iov[2+attr_count].iov_base = attr_table[idx];
		iov[2+attr_count].iov_len = attr_table[idx]->rta_len;
		nlmsghdr.nlmsg_len += attr_table[idx]->rta_len;
		attr_count++;
	}

	return send_and_ack(sock, iov, 2+attr_count);
}

int
move_addresses_to_bridge(int sock, const char *eth, const char *bridge) {

	if (!eth || !bridge) {
		dprintf(D_ALWAYS, "No default bridge or interface specified.\n");
		return 1;
	}

	unsigned eth_dev;
	if (!(eth_dev = if_nametoindex(eth))) {
		dprintf(D_ALWAYS, "Unable to determine index of interface %s.\n", eth);
		return -1;
	}
	unsigned bridge_dev;
	if (!(bridge_dev = if_nametoindex(bridge))) {
		dprintf(D_ALWAYS, "Unable to determine index of bridge %s.\n", bridge);
		return -1;
	}

	addr_info_t addr_info;
	if (populate_addr_info(&addr_info)) {
		dprintf(D_ALWAYS, "Failed to allocate address info structures.\n");
		return -1;
	}
	addr_info.eth_dev = eth_dev;

	// Build up a list of addresses to copy over.
	if (get_addresses(sock, filter_addresses, &addr_info)) {
		dprintf(D_ALWAYS, "Failed to retrieve list of addresses from kernel.\n");
		free_addr_info(&addr_info);
		return -1;
	}

	// Remove then add the list of addresses.
	for (unsigned idx=0; idx<addr_info.len; idx++) {
		struct ifaddrmsg *addrmsg = addr_info.addrs + idx;
		struct rtattr **attr_table = addr_info.attr_tables[idx];

		int action_result = 0;
		// Remove old address
		if ((action_result = iface_action(sock, RTM_DELADDR, addrmsg, attr_table))) {
			dprintf(D_ALWAYS, "Failed to delete address (%d).\n", action_result);
			free_addr_info(&addr_info);
			return -1;
		}

		// Make address point at new bridge iface
		addrmsg->ifa_index = bridge_dev;

		// Create new address.
		if (iface_action(sock, RTM_NEWADDR, addrmsg, attr_table)) {
			dprintf(D_ALWAYS, "FATAL: Deleted old address but couldn't create new one.\n");
			free_addr_info(&addr_info);
			return -1;
		}
	}
	free_addr_info(&addr_info);
	return 0;
}
