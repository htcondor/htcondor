
/*
 * A test utility for the DHCP client code.
 */

#include "condor_common.h"

#include <net/if.h>

#include <classad/classad.h>
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"

#include "popen_util.h"
#include "network_configuration.h"
#include "network_manipulation.h"
#include "dhcp_management.h"
#include "address_selection.h"

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
	if (create_veth(fd, EXTERNAL_IFACE, INTERNAL_IFACE)) {
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
	if (create_bridge(BRIDGE_NAME)) {
		dprintf(D_ALWAYS, "Failed to create bridge " BRIDGE_NAME "\n");
		result = 1;
		goto failed_bridge;
	}
	if (add_interface_to_bridge(BRIDGE_NAME, BRIDGE_IFACE)) {
		dprintf(D_ALWAYS, "Failed to add " BRIDGE_IFACE " to bridge " BRIDGE_NAME "\n");
		result = 1;
		goto failed_bridge;
	}
	if (add_interface_to_bridge(BRIDGE_NAME, EXTERNAL_IFACE)) {
		dprintf(D_ALWAYS, "Failed to add " EXTERNAL_IFACE " to bridge " BRIDGE_NAME "\n");
		result = 1;
		goto failed_bridge;
	}

	// Run the DHCP clieant.
	if (dhcp_query(machine_ad)) {
		dprintf(D_ALWAYS, "DHCP client query failed.\n");
		result = 1;
		goto failed_dhcp_client;
	}

	if (!machine_ad.EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, client_ip)) {
		dprintf(D_ALWAYS, "DHCP client did not return an IP address for " INTERNAL_IFACE ".\n");
		result = 1;
		goto failed_dhcp_client;
	}

	// Committing requires i_internal to actually be in a separate interface.
	// If you want to test that aspect, use network_netlink_main.cpp
/*
	// Assign address to internal interface.
	// TODO: calculate correct subnet mask.
	if (add_address(fd, client_ip.c_str(), 32, INTERNAL_IFACE)) {
		dprintf(D_ALWAYS, "Failed to add address %s to interface " INTERNAL_IFACE ".\n", client_ip.c_str());
		result = 1;
		goto failed_assign_address;
	}
	// Commit to the address
	if (dhcp_commit(machine_ad)) {
		dprintf(D_ALWAYS, "Failed to DHCP ACK address %s to interface " INTERNAL_IFACE ".\n", client_ip.c_str());
		result = 1;
		goto failed_dhcp_commit;
	}

failed_dhcp_commit:
failed_assign_address:
*/
failed_dhcp_client:
failed_bridge:
failed_set_status:
//	delete_veth(fd, INTERNAL_IFACE);
failed_create_veth:
	close(fd);

	classad::PrettyPrint pp;
	std::string ad_str;
	pp.Unparse(ad_str, &machine_ad);
	dprintf(D_ALWAYS, "ClassAd contents: %s\n", ad_str.c_str());
	return result;
}
