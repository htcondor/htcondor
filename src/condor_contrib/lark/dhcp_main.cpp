
/*
 * A test utility for the DHCP client code.
 */

#include "condor_common.h"

#include <net/if.h>

#include <classad/classad.h>
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"

#include "lark_attributes.h"
#include "popen_util.h"
#include "network_manipulation.h"
#include "dhcp_management.h"
#include "bridge_routing.h"

#define EXTERNAL_IFACE "e_tester"
#define INTERNAL_IFACE "i_tester"

#define BRIDGE_NAME "lark"
#define BRIDGE_IFACE "eth0"

int
main(int argc, char * argv[])
{
	if (argc || argv) {}

	int result = 0;
	int verbose_flag = 1; // TODO: add getopt_long support to make this a CLI flag.
	std::string client_ip;

	classad::ClassAd machine_ad;
	machine_ad.InsertAttr(ATTR_INTERNAL_INTERFACE, INTERNAL_IFACE);

	config();
	if (verbose_flag) {
		param_insert("TOOL_DEBUG", "D_FULLDEBUG");
	}
	dprintf_set_tool_debug("TOOL", 0);
	dprintf(D_FULLDEBUG, "Running network namespace tester in debug mode.\n");

	TemporaryPrivSentry sentry(PRIV_ROOT);

	int fd;
	if ((fd = create_socket()) == -1) {
		dprintf(D_ALWAYS, "Failed to create a netlink socket for kernel communication.\n");
		return 1;
	}

	// Create and enable the veth devices.
	if (create_veth(fd, EXTERNAL_IFACE, INTERNAL_IFACE, NULL, NULL)) {
		dprintf(D_ALWAYS, "Failed to create veth devices.\n");
		result=1;
		goto failed_create_veth;
	}
	if (set_status(fd, EXTERNAL_IFACE, IFF_UP)) {
		dprintf(D_ALWAYS, "Failed to set " EXTERNAL_IFACE " status to up.\n");
		result = 1;
		goto failed_set_status;
	}
	if (set_status(fd, INTERNAL_IFACE, IFF_UP)) {
		dprintf(D_ALWAYS, "Failed to set " INTERNAL_IFACE " status to up.\n");
		result = 1;
		goto failed_set_status;
	}

	// Create and setup the bridge.
	int retval;
	if ((retval = create_bridge(BRIDGE_NAME)) && (retval != EEXIST)) {
		dprintf(D_ALWAYS, "Failed to create bridge %s (errno=%d, %s)\n", BRIDGE_NAME, retval, strerror(retval));
		result = 1;
		goto failed_bridge;
	}
	if (retval != EEXIST)
	{
		if (set_status(fd, BRIDGE_NAME, IFF_UP))
		{
			delete_bridge(BRIDGE_NAME);
			result = 1;
			goto failed_bridge;
		}
		if ((result = set_bridge_fd(BRIDGE_NAME, 0)))
		{
			dprintf(D_ALWAYS, "Unable to set bridge " BRIDGE_NAME " forwarding delay to 0.\n");
			delete_bridge(BRIDGE_NAME);
			result = 1;
			goto failed_bridge;
		}

		if ((result = add_interface_to_bridge(BRIDGE_NAME, BRIDGE_IFACE))) {
			dprintf(D_ALWAYS, "Unable to add device %s to bridge %s\n", BRIDGE_IFACE, BRIDGE_NAME);
			set_status(fd, BRIDGE_NAME, 0);
			delete_bridge(BRIDGE_NAME);
			result = 1;
			goto failed_bridge;
		}
		// If either of these fail, we likely knock the system off the network.
		if ((result = move_routes_to_bridge(fd, BRIDGE_IFACE, BRIDGE_NAME))) {
			dprintf(D_ALWAYS, "Failed to move routes from %s to bridge %s\n", BRIDGE_IFACE, BRIDGE_NAME);
			set_status(fd, BRIDGE_NAME, 0);
			delete_bridge(BRIDGE_NAME);
			result = 1;
			goto failed_bridge;
		}
		if ((result = move_addresses_to_bridge(fd, BRIDGE_IFACE, BRIDGE_NAME)))
		{
			dprintf(D_ALWAYS, "Failed to move addresses from %s to bridge %s.\n", BRIDGE_IFACE, BRIDGE_NAME);
			result = 1;
			goto failed_bridge;
		}
	}
	if ((retval = add_interface_to_bridge(BRIDGE_NAME, EXTERNAL_IFACE))) {
		dprintf(D_ALWAYS, "Failed to add " EXTERNAL_IFACE " to bridge " BRIDGE_NAME " (errno=%d, %s)\n", retval, strerror(retval));
		result = 1;
		goto failed_bridge;
	}

	// Run the DHCP clieant.
	dprintf(D_ALWAYS, "Starting DHCP query.\n");
	if (dhcp_query(machine_ad)) {
		dprintf(D_ALWAYS, "DHCP client query failed.\n");
		result = 1;
		goto failed_dhcp_client;
	}
	dprintf(D_ALWAYS, "Finished DHCP query.\n");

	if (!machine_ad.EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, client_ip)) {
		dprintf(D_ALWAYS, "DHCP client did not return an IP address for " INTERNAL_IFACE ".\n");
		result = 1;
		goto failed_dhcp_client;
	}

	// Committing requires i_internal to actually be in a separate namespace.
	// If you want to test that aspect, use network_netlink_main.cpp
	// We just assume that the ACK comes back
/*
	// Commit to the address
	if (dhcp_commit(machine_ad)) {
		dprintf(D_ALWAYS, "Failed to DHCP ACK address %s to interface " INTERNAL_IFACE ".\n", client_ip.c_str());
		result = 1;
		goto failed_dhcp_commit;
	}
*/

	sleep(1);

	if (dhcp_release(machine_ad)) {
		dprintf(D_ALWAYS, "DHCP release failed.\n");
		result = 1;
		goto failed_dhcp_release;
	}

failed_dhcp_release:
failed_dhcp_client:
failed_bridge:
failed_set_status:
	delete_veth(fd, INTERNAL_IFACE);
failed_create_veth:
	close(fd);

	classad::PrettyPrint pp;
	std::string ad_str;
	pp.Unparse(ad_str, &machine_ad);
	dprintf(D_ALWAYS, "ClassAd contents: %s\n", ad_str.c_str());
	return result;
}
