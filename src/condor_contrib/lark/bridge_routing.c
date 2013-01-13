
#include "network_manipulation.h"

/**
 * Detect if an interface has a default route.
 * Returns 1 if it does; 0 if it does not
 * Returns a negative value on error.
 */
struct default_route_check_s {
	int iface_id,
	int has_default,
};

static int
check_default_route(struct nlmsghdr nlmsghdr, struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1], void *user_arg) {
	struct default_route_check_s *arg = (struct default_route_check_s*)user_arg;
	if (rtmsg->rtm_dst_len == 0 && attr_table[RTA_OIF] && 
			(arg->iface_id == *(int*)RTA_DATA(attr_table[RTA_OIF]))) {
		arg->has_default = 1;
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
	struct rtattr **attr_tables[RTA_MAX+1];
	size_t route_len;
	size_t route_alloc;
	int eth_dev;
	int bridge_dev;
} route_info_t;

static int
populate_route_info(route_info_t *r) {
	if (!r)
		return 1;
	r->routes = malloc(5*sizeof(struct *rtmsg));
	r->route_len = 0;
	r->route_alloc = 0;
	r->attr_tables = malloc(5*sizeof(struct *rtattr[RTA_MAX+1]));
	if (!r->routes || !r->attr_tables) {
		return 1;
	}
	r->route_alloc = 5;
	return 0;
}

int
add_route_info(route_info_t *route_info, struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1]) {
	route_info->route_len++;
	if (route_info->route_len > route_info->route_alloc) {
		route_info->routes = realloc((route_info->route_alloc+5)*sizeof(struct *rtmsg));
		route_info->attr_tables = realloc((route_info->route_alloc+5)*sizeof(struct *rtattr[RTA_MAX+1]));
		if (!route_info->routes || !route_info->attr_tables)
			return 1;
		memset(route_info->routes+route_info->route_alloc, 0, sizeof(struct *rtmsg)*5);
		memset(route_info->attr_tables+route_info->route_alloc, 0, sizeof(struct *rtattr[RTA_MAX+1])*5);
		for (unsigned i=0; i<5; i++) {
			route_info->attr_tables[i+route_info->route_alloc] = calloc((RTA_MAX+1), sizeof(struct *rtattr));
			if (!route_info->attr_tables[i+route_info->route_alloc])
				return 1;
		}
		route_info->route_alloc += 5;
	}
	memcpy(route_info->routes[route_info->route_len], rtmsg, sizeof(rtmsg));
	for (int idx=0; idx<RTA_MAX; idx++) {
		struct rtattr* attr = route_info->attr_table[idx];
		if (!attr) continue;
		struct rtattr* new_attr = malloc(attr->rta_len);
		if (!new_attr)
			return 1;
		memcpy(new_attr, attr, attr->rta_len);
		route_info->attr_table[route_info->route_len][idx] = new_attr;
	}
	return 0;
}

int
filter_routes(struct nlmsghdr, struct rtmsg rtmsg, struct rtattr *attr_table[RTA_MAX+1], void *user_arg) {
	route_info_t *arg = (route_info_t *)user_arg;

	if (!attr_table[RTA_OIF])
		return 0;

	int * oif = RTA_DATA(attr_table[RTA_OIF]);
	if (*oif == arg->eth_dev) {
		add_route_info(*arg, rtmsg, attr_table);
	}
	return 0;
}

void
free_route_info(route_info_t route_info) {
	if (route_info->route) {
		for (size_t idx=0; i<route_info->route_alloc; i++)
			if (route_info->routes[idx])
				free(route_info->routes[idx]);
		free(route_info->routes);
	}
	if (route_info->attr_tables) {
		for (size_t idx=0; i<route_info->route_alloc; i++) {
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
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK
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
		iov[2+attr_count].iov_base = attr_table[idx];
		iov[2+attr_count].iov_len = attr_table[idx]->rta_len;
		nlmsghdr.nlmsg_len += attr_table[idx]->rta_len;
		attr_count++;
	}

	return send_and_ack(iov, 2+attr_count);
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
		dprintf(D_ALWAYS, "Unable to determine index of bridge %s.\n", bridege);
		return -1;
	}

	route_info_t route_info;
	if (!populate_route_info(&route_info)) {
		dprintf(D_ALWAYS, "Failed to allocate route info structures.\n");
		return -1;
	}

	// Build up a list of routes to copy over.
	if (!get_routes(sock, filter_routes, &route_info)) {
		dprintf(D_ALWAYS, "Failed to retrieve list of routes from kernel.\n");
		free_route_info(route_info);
		return -1;
	}

	// Remove then add the list of routes.
	for (int idx=0; idx<route_info.route_len; idx++) {
		struct rtmsg *rtmsg = route_info.routes + idx;
		struct rtattr **attr_table = route_info.attr_tables[idx];

		if (!rtattr[RTA_OIF]) continue;
		// Remove old route
		if (!route_action(sock, RTM_DELROUTE, rtmsg, attr_table)) {
			dprintf(D_ALWAYS, "Failed to delete routes.\n");
			free_route_info(route_info);
			return -1;
		}

		// Make route point at new bridge iface
		int *oif = RTA_DATA(attr_table[RTA_OIF]);
		*oif = arg->bridge_dev;

		// Create new route.
		if (!route_action(sock, RTM_NEWROUTE, rtmsg, attr_table)) {
			dprintf(D_ALWAYS, "FATAL: Deleted old route but couldn't create new one.\n");
			free_route_info(route_info);
			return -1;
		}
	}
	free_route_info(route_info);
	return 0;
}

typedef struct addr_info_s {
	struct ifaddrmsg * addrs;
	struct rtattr **attr_tables[IFA_MAX+1];
	size_t len;
	size_t alloc;
	int eth_dev;
} addr_info_t;

static int
populate_addr_info(addr_info_t *a) {
	if (!a)
		return 1;
	a->len = 0;
	a->alloc = 0;
	a->addrs = malloc(5*sizeof(struct *ifaddrmsg));
	a->attr_tables = malloc(5*sizeof(struct *rtattr[IFA_MAX+1]));
	if (!a->addrs || !a->attr_tables) {
		return 1;
	}
	a->alloc = 5;
	return 0;
}

int
add_addr_info(addr_info_t *addr_info, struct ifaddrmsg addrmsg, struct rtattr *attr_table[IFA_MAX+1]) {
	addr_info->len++;
	if (addr_info->len > addr_info->alloc) {
		addr_info->addrs = realloc((addr_info->alloc+5)*sizeof(struct *ifaddrmsg));
		addr_info->attr_tables = realloc((addr_info->alloc+5)*sizeof(struct *rtattr[IFA_MAX+1]));
		if (!addr_info->routes || !addr_info->attr_tables)
			return 1;
		memset(addr_info->addrs+addr_info->alloc, 0, sizeof(struct *ifaddrmsg)*5);
		memset(addr_info->attr_tables+addr_info->alloc, 0, sizeof(struct *rtattr[IFA_MAX+1])*5);
		for (unsigned i=0; i<5; i++) {
			addr_info->attr_tables[i+addr_info->alloc] = calloc((IFA_MAX+1), sizeof(struct *rtattr));
			if (!addr_info->attr_tables[i+addr_info->alloc])
				return 1;
		}
		addr_info->alloc += 5;
	}
	memcpy(addr_info->routes[addr_info->len], ifaddrmsg, sizeof(ifaddrmsg));
	for (int idx=0; idx<IFA_MAX; idx++) {
		struct rtattr* attr = addr_info->attr_table[idx];
		if (!attr) continue;
		struct rtattr* new_attr = malloc(attr->rta_len);
		if (!new_attr)
			return 1;
		memcpy(new_attr, attr, attr->rta_len);
		addr_info->attr_table[addr_info->len][idx] = new_attr;
	}
	return 0;
}

int
filter_addresses(struct nlmsghdr, struct ifaddrmsg addrmsg, struct rtattr *attr_table[IFA_MAX+1], void *user_arg) {
	addr_info_t *arg = (addr_info_t *)user_arg;

	if (ifa_index !=)
		return 0;

	add_addr_info(*arg, ifaddrmsg, attr_table);
	return 0;
}

void
free_addr_info(addr_info_t addr_info) {
	if (addr_info->addrs) {
		for (size_t idx=0; i<addr_info->alloc; i++)
			if (addr_info->addrs[idx])
				free(addr_info->addrs[idx]);
		free(addr_info->addrs);
	}
	if (addr_info->attr_tables) {
		for (size_t idx=0; i<addr_info->alloc; i++) {
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
	nlmsghdr.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK
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
		iov[2+attr_count].iov_base = attr_table[idx];
		iov[2+attr_count].iov_len = attr_table[idx]->rta_len;
		nlmsghdr.nlmsg_len += attr_table[idx]->rta_len;
		attr_count++;
	}

	return send_and_ack(iov, 2+attr_count);
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
		dprintf(D_ALWAYS, "Unable to determine index of bridge %s.\n", bridege);
		return -1;
	}

	addr_info_t addr_info;
	if (!populate_addr_info(&addr_info)) {
		dprintf(D_ALWAYS, "Failed to allocate address info structures.\n");
		return -1;
	}

	// Build up a list of addresses to copy over.
	if (!get_routes(sock, filter_addresses, &addr_info)) {
		dprintf(D_ALWAYS, "Failed to retrieve list of addresses from kernel.\n");
		free_addr_info(addr_info);
		return -1;
	}

	// Remove then add the list of addresses.
	for (int idx=0; idx<addr_info.len; idx++) {
		struct ifaddrmsg *addrmsg = addr_info.addresses + idx;
		struct rtattr **attr_table = addr_info.attr_tables[idx];

		// Remove old route
		if (!addr_action(sock, RTM_DELADDR, addrmsg, attr_table)) {
			dprintf(D_ALWAYS, "Failed to delete routes.\n");
			free_addr_info(addr_info);
			return -1;
		}

		// Make address point at new bridge iface
		addrmsg->ifa_index = bridge_dev;

		// Create new address.
		if (!addr_action(sock, RTM_NEWADDR, addrmsg, attr_table)) {
			dprintf(D_ALWAYS, "FATAL: Deleted old address but couldn't create new one.\n");
			free_addr_info(addr_info);
			return -1;
		}
	}
	free_addr_info(addr_info);
	return 0;
}
