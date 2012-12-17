
#include "condor_common.h"

#include <net/if.h>
#include <string>

#include "condor_debug.h"
#include "condor_config.h"
#include <classad/classad.h>

#include "lark_attributes.h"
#include "network_manipulation.h"
#include "network_namespaces.h"

#include "address_selection.h"
#include "bridge_configuration.h"

using namespace lark;

int
BridgeConfiguration::Setup() {

	std::string bridge_device;
	if (!m_ad->EvaluateAttrString(ATTR_BRIDGE_DEVICE, bridge_device) &&
			!param(bridge_device, CONFIG_BRIDGE_DEVICE)) {
		dprintf(D_ALWAYS, "Could not determine which device to use for the external bridge.\n");
		return 1;
	}
	std::string external_device;
	if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_INTERFACE, external_device)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_EXTERNAL_INTERFACE " is missing.\n");
		return 1;
	}

	int result;
	if ((result = create_bridge(LARK_BRIDGE_NAME))) {
		dprintf(D_ALWAYS, "Unable to create a bridge (" LARK_BRIDGE_NAME "); error=%d.\n", result);
		return result;
	}

	if ((result = add_interface_to_bridge(LARK_BRIDGE_NAME, bridge_device.c_str()))) {
		dprintf(D_ALWAYS, "Unable to add device %s to bridge " LARK_BRIDGE_NAME "\n", bridge_device.c_str());
		return result;
	}

	if ((result = add_interface_to_bridge(LARK_BRIDGE_NAME, external_device.c_str()))) {
		dprintf(D_ALWAYS, "Unable to add device %s to bridge " LARK_BRIDGE_NAME "\n", external_device.c_str());
		return result;
	}

	// Manipulate the routing and state of the link.  Done internally; no callouts.
	NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
	int fd = manager.GetNetlinkSocket();

	// Enable the device
	// ip link set dev $DEV up
	if (set_status(fd, external_device.c_str(), IFF_UP)) {
		return 1;
	}

	if (pipe2(m_p2c, O_CLOEXEC)) {
		dprintf(D_ALWAYS, "Failed to create bridge synchronization pipe (errno=%d, %s).\n", errno, strerror(errno));
		return 1;
	}

	AddressSelection & address = GetAddressSelection();
	address.Setup();

	classad::PrettyPrint pp;
	std::string ad_str;
	pp.Unparse(ad_str, m_ad.get());
	dprintf(D_FULLDEBUG, "After bridge setup, machine classad: \n%s\n", ad_str.c_str());

	return 0;
}

int
BridgeConfiguration::SetupPostForkParent()
{
	close(m_p2c[0]); m_p2c[0] = -1;

	std::string external_device;
	if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_INTERFACE, external_device)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_EXTERNAL_INTERFACE " is missing.\n");
		return 1;
	}

	// Wait for the bridge to come up.
	int err;
	NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
        int fd = manager.GetNetlinkSocket();
	if ((err = wait_for_bridge_status(fd, external_device.c_str()))) {
		return err;
	}

	char go = 1;
	while (((err = write(m_p2c[1], &go, 1)) < 0) && (errno == EINTR)) {}
	if (err < 0) {
		dprintf(D_ALWAYS, "Error writing to child for bridge sync (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
	return 0;
}

int
BridgeConfiguration::SetupPostForkChild()
{
	close(m_p2c[1]);
	int err;
	char go;
	while (((err = read(m_p2c[0], &go, 1)) < 0) && (errno == EINTR)) {}
	if (err < 0) {
		dprintf(D_ALWAYS, "Error when waiting on parent (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}

	AddressSelection & address = GetAddressSelection();
	address.SetupPostFork();

	return 0;
}

int
BridgeConfiguration::Cleanup() {

	if (m_p2c[0] >= 0) close(m_p2c[0]);
	if (m_p2c[1] >= 0) close(m_p2c[1]);

	dprintf(D_FULLDEBUG, "Cleaning up network bridge configuration.\n");
	AddressSelection & address = GetAddressSelection();
	address.Cleanup();

	return 0;
}
