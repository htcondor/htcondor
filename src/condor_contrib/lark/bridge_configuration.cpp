
#include "condor_common.h"

#include <string>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include "condor_debug.h"
#include "condor_config.h"
#include <classad/classad.h>

#include "lark_attributes.h"
#include "network_manipulation.h"
#include "network_namespaces.h"

#include "address_selection.h"
#include "bridge_configuration.h"
#include "popen_util.h"

// We had to redefine this function because linux/if.h and net/if.h
// conflict with each other on RHEL6 :(
extern "C" {
extern unsigned int if_nametoindex (__const char *__ifname);
}

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
	std::string chain_name;
	if(!m_ad->EvaluateAttrString(ATTR_IPTABLE_NAME, chain_name)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_IPTABLE_NAME "\n");
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
	std::string internal_device;
	if (!m_ad->EvaluateAttrString(ATTR_INTERNAL_INTERFACE, internal_device)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_INTERNAL_INTERFACE " is missing.\n");
		return 1;
	}

	{
		ArgList args;
		args.AppendArg("ip");
		args.AppendArg("route");
		args.AppendArg("add");
		args.AppendArg("default");
		args.AppendArg("dev");
		args.AppendArg(internal_device.c_str());
		RUN_ARGS_AND_LOG(NetworkNamespaceManager::Cleanup, NETWORK_NAMESPACE_DELETE_SCRIPT)
	}

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

	std::string mac_addr;
	if (!m_ad->EvaluateAttrString(ATTR_DHCP_MAC, mac_addr) || mac_addr.length() != IFHWADDRLEN) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_DHCP_MAC " is missing.\n");
		return 1;
	}
	std::string bridge_device;
	if (!m_ad->EvaluateAttrString(ATTR_BRIDGE_DEVICE, bridge_device)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_BRIDGE_DEVICE " is missing.\n");
		return 1;
	}
	int dev_idx = if_nametoindex(bridge_device.c_str());	
	if (dev_idx == 0) {
		dprintf(D_ALWAYS, "Unknown device %s for sending ARP packets.\n", bridge_device.c_str());
		return 1;
	}

	// Spoof ARP packet saying we are giving up the packet.
	struct iovec iov[5];
	short hwtype = htons(1); // Hardware type - Ethernet
	iov[0].iov_base = &hwtype;
	iov[0].iov_len = 2;
	short pttype = htons(0x0800); // Protocol type - IPv4
	iov[1].iov_base = &pttype;
	iov[1].iov_len = 2;
	char addrlen[2];
	addrlen[0] = 6; // Hardware address length
	addrlen[1] = 4; // Protocol address length
	iov[2].iov_base = &addrlen;
	iov[2].iov_len = 2;
	short optype = htons(2); // Reply operation
	iov[3].iov_base = &optype;
	iov[3].iov_len = 2;
	char arp[20];
	memcpy(arp, mac_addr.c_str(), IFHWADDRLEN); // Source HW addr.
	memset(arp+6, 0, 4); // 0.0.0.0 IP address
	memset(arp+10, 0xff, 6); // Dest HW addr - ff:ff:ff:ff:ff:ff
	//memset(arp+16, 0xff, 4); // Dest protocol address - 255.255.255.255
	arp[16] = 129; arp[17] = 93; arp[18] = 244; arp[19] = 255;
	iov[4].iov_base = &arp;
	iov[4].iov_len = 20;
	int fd = socket(AF_PACKET, SOCK_DGRAM, 0);
	if (fd == -1) {
		dprintf(D_ALWAYS, "Unable to create socket for ARP message (errno=%d, %s).\n", errno, strerror(errno));
		return -1;
	}
	struct sockaddr_ll addr; bzero(&addr, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = ETH_P_ARP;
	addr.sll_ifindex = dev_idx;
	addr.sll_hatype = ARPOP_REPLY;
	addr.sll_pkttype = PACKET_BROADCAST;
	addr.sll_halen = IFHWADDRLEN;
	char hwaddr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	memcpy(addr.sll_addr, hwaddr, IFHWADDRLEN);
	union sockaddr_storage {struct sockaddr_ll *laddr; struct sockaddr *addr;} storage;
	storage.laddr = &addr;
	struct msghdr msg; bzero(&msg, sizeof(msg));
	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = iov;
	msg.msg_iovlen = 5;
	if (sendmsg(fd, &msg, 0) == -1) {
		dprintf(D_ALWAYS, "Failed to send ARP message (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}
