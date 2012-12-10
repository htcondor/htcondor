
#include "condor_common.h"

#include <string>

#include "condor_debug.h"
#include "condor_config.h"
#include <classad/classad.h>

#include "network_configuration.h"
#include "network_manipulation.h"

#include "bridge_configuration.h"

#define LARK_BRIDGE_NAME "lark"

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

	return 1;
}

int
BridgeConfiguration::Cleanup() {
	// TODO: release DHCP address.
	return 1;
}
