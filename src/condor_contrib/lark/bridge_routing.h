#ifndef __BRIDGE_ROUTING_H_
#define __BRIDGE_ROUTING_H_

/**
 * Check to see if a given interface has a default route.
 * 
 * Args:
 * - sock: Netlink socket to kernel.
 * - eth: Valid ethernet device name.
 * Returns 1 if it does; 0 if it does not.
 * Return a negative value on error.
 */
int
interface_has_default_route(int sock, const char * eth);

/**
 * Move all routes referencing an ethernet device to the bridge device.
 *
 * Args:
 * - sock: Netlink socket to kernel.
 * - eth: Name of ethernet device.
 * - bridge: Name of bridge.
 * Returns 0 on success, non-zero otherwise.
 */
int
move_routes_to_bridge(int sock, const char * eth, const char * bridge);

/**
 * Move all addresses for an ethernet device to the bridge device.
 *
 * Args:
 * - sock: Netlink socket to kernel.
 * - eth: Name of ethernet device.
 * - bridge: Name of bridge;
 * Returns 0 on success, non-zero otherwise.
 */
int
move_addresses_to_bridge(int sock, const char * eth, const char * bridge);

#endif

