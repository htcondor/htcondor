
/*
 * This header contains definitions of various helper functions for manipulating
 * network devices on Linux.  It is not meant to be used directly except by the
 * network namespace manager.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __NETWORK_MANIPULATION_H_
#define __NETWORK_MANIPULATION_H_

#include <sys/types.h>

// Forward decls
struct ipt_getinfo;
struct ipt_get_entries;

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
 * - prefix_length: Length of netmask
 * - eth:  name of network device
 *
 * If the prefix_length is not equal to the address size (32 for IPv4, 128 for IPv6),
 * the kernel will automatically setup a local route for the address and device.
 *
 * Returns 0 on success and non-zero on failure.
 */
int add_address(int sock, const char * addr, unsigned prefix_length, const char * eth);

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

/*
 * Remove a chain from the filter iptable from the kernel.  Removes the contents
 * of the chain, along with any references to that chain in other chains.
 *
 * - chain: Name of chain to remove
 *
 * Returns 0 on success and non-zero on failure.
 * On failure, the state of the firewall is undefined.
 */
int cleanup_chain(const char *chain);

/*
 * Retrieve the firewall data from the kernel; gets the "filter" table from iptables.
 * 
 * - info: On input, must be a valid pointer.  On successful finish, the contents
 *   are overwritten with the basic information about the firewall rules.
 * - result_entries: On input, should be an uninitialized pointer to a
 *   (struct ipt_get_entries *).  On successful finish, the function will have
 *   allocated memory and copied in the firewall content.
 *   The caller is responsible for calling free() on *result_entries once they are
 *   done with the data.
 * 
 * Returns 0 on success and non-zero on failure.
 * On failure, the contents of info and result_entries are undefined.  The caller does
 * not need to call free() on *result_entries on failure.
 * 
 */
int get_firewall_data(struct ipt_getinfo *info, struct ipt_get_entries **result_entries);

/*
 * Bridge helper functions.
 */

/*
 * Open socket.
 *
 * Open a socket usable for talking to the kernel to create bridges.
 *
 * Returns the fd when the open was successful (it logs failures) and -1 on
 * failure.
 *
 * The user is responsible for closing the socket when done.
 */
int open_bridge_socket();

/*
 * Create a bridge device.
 *
 * - bridge_name: String containing the bridge name; must be less than IFNAMSIZ
 *   characters in length.
 *
 * Return 0 on success and non-zero on failure.
 */
int create_bridge(const char * bridge_name);

/*
 * Delete a bridge device.
 *
 * - bridge_name: String containing the bridge name; must be less than IFNAMSIZ
 *   characters in length.
 * 
 * Return 0 on success and non-zero on failure.
 */
int delete_bridge(const char * bridge_name);

/*
 * Add a network interface to the bridge.
 *
 * - bridge_name: Name of the bridge device.
 * - dev: Name of the device to add.
 *
 * Return 0 on success and non-zero on failure.
 */
int add_interface_to_bridge(const char *bridge_name, const char *dev);

/*
 * Delete a network interface from the bridge.
 *
 * - bridge_name: Name of the bridge device.
 * - dev: Name of the device to delete.
 *
 * Return 0 on success and non-zero on failure.
 */
int delete_interface_from_bridge(const char *bridge_name, const char *dev);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif
