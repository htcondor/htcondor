
#include "condor_common.h"

#include "network_manipulation.h"
#include "bridge_routing.h"

#include "condor_debug.h"

int main(int argc, const char *argv[])
{
	if (argc != 3)
	{
		dprintf(D_ALWAYS, "Usage: %s <bridge_name> <ethernet_device>\n", argv[0]);
	}
	const char *bridge_name = argv[1];
	const char *bridge_device = argv[2];

	int result;
	if ((result = create_bridge(bridge_name)) && (result != EEXIST))
	{
		dprintf(D_ALWAYS, "Unable to create a bridge (%s); error=%d.\n", bridge_name, result);
		return result;
	}

	// Manipulate the routing and state of the link.  Done internally; no callouts.
	int fd = create_socket();
	if (fd == -1)
	{
		delete_bridge(bridge_name);
		dprintf(D_ALWAYS, "Unable to create netlink socket.\n");
		return 1;
	}

	// We are responsible for creating the bridge.
	if (result != EEXIST)
	{
		if ((result = add_interface_to_bridge(bridge_name, bridge_device)))
		{
			dprintf(D_ALWAYS, "Unable to add device %s to bridge %s\n", bridge_name, bridge_device);
			delete_bridge(bridge_name);
			return result;
		}

		if ((result = move_routes_to_bridge(fd, bridge_name, bridge_device)))
		{
			dprintf(D_ALWAYS, "Failed to move routes from %s to bridge %s\n", bridge_device, bridge_name);
			delete_bridge(bridge_name);
			return result;
		}
	}

	delete_bridge(bridge_name);
	return 0;
}

