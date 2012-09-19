
/*
 * This header contains definitions of various helper functions for manipulating
 * network devices on Linux.  It is not meant to be used directly except by the
 * network namespace manager.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a socket to talk to the kernel via netlink
 * Returns the socket fd upon success, or -errno upon failure
 */
int create_socket();

/*
 * Create a pair of virtual ethernet devices that act as pipes.  Equivalent to:
 *  ip link add type veth
 *
 * - sock: Netlink socket to the kernel.
 * - veth0: name of external network device to create.
 * - veth1: name of internal network device.
 */
int create_veth(int sock, const char * veth0, const char * veth1);

/*
 * Delete an ethernet device.  Equivalent to:
 *  ip link delete eth
 *
 * - sock: Netlink socket to the kernel.
 * - eth: name of device to delete
 */
int delete_veth(int sock, const char * eth);

/*
 * Set the status of an ethernet device.  Equivalent to:
 *  ip link set dev $eth [up/down] 
 *
 * - sock: Netlink socket to the kernel
 * - eth: Name of a network device
 * - status: Either IFF_UP or IFF_DOWN
 */
int set_status(int sock, const char * eth, int status);

/*
 * Add an INET4 address to a network device.  Equivalent to:
 *  ip addr add $addr dev $eth
 *
 * - sock: Netlink socket to the kernel
 * - addr: IPv4 address to add.
 * - eth:  name of network device
 */
int add_address(int sock, const char * addr, const char * eth);

/*
 * Add a local route to a network device. Equivalent to:
 *  ip route add $dest/$dst_len dev $eth
 * 
 * - sock: Netlink socket to the kernel
 * - gw: IPv4 address of a gate
 * - eth: name of network device
 * - dst_len: Bits in the netmask.  Only 24 is supported right now.
 */
int add_local_route(int sock, const char * gw, const char * eth, int dst_len);

/*
 * Add a default route to a network device.  Equivalent to:
 *  ip route add default via $gw
 *
 * - sock: Netlink socket to the kernel
 * - gw: IPv4 address of a gate
 * - eth: name of network device
 */
int add_default_route(int sock, const char * gw);

/*
 * Send an ethernet device from the current network namespace to the one
 * occupied by `pid`.  Equivalent to:
 *  ip link set $eth netns $pid
 *
 * - sock: Netlink socket to the kernel.
 * - eth: Name of network device.
 * - pid: Target pid to receive the network device.
 */
int set_netns(int sock, const char * eth, pid_t pid);

/*
 * Read out the iptables from the kernel; each time there's a match, call the match_fcn.
 * - rule_name: Value of comment field associated with the rule.
 * - match_fcn: Function that is invoked for each rule in the chain that 
 *   performs accounting.  Invoked with the rule name and the number of bytes 
 *   that have passed the filter, and the provided callback data
 * - callback_data: Opaque data provided to the callback
 */
int perform_accounting(const char * rule_name, int (*match_fcn)(const unsigned char *, long long, void *), void * callback_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

