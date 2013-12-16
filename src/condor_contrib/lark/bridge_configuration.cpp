
#include "condor_common.h"

#include <string>
#include <sstream>
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
#include "bridge_routing.h"
#include "popen_util.h"

// We had to redefine this function because linux/if.h and net/if.h
// conflict with each other on RHEL6 :(
extern "C" {
extern unsigned int if_nametoindex (__const char *__ifname);
}
extern void MacAddressBinToHex(const unsigned char *, char *);
extern bool MacAddressHexToBin(const char *, unsigned char*);

using namespace lark;

int
BridgeConfiguration::Setup() {

	std::string bridge_name;
	if (!m_ad->EvaluateAttrString(ATTR_BRIDGE_NAME, bridge_name) &&
			!param(bridge_name, CONFIG_BRIDGE_NAME)) {
		dprintf(D_FULLDEBUG, "Could not determine the bridge name from the configuration; using the default " LARK_BRIDGE_NAME ".\n");
		bridge_name = LARK_BRIDGE_NAME;
        m_ad->InsertAttr(ATTR_BRIDGE_NAME, bridge_name);
	}

	std::string bridge_device;
	if (!m_ad->EvaluateAttrString(ATTR_BRIDGE_DEVICE, bridge_device)) {
		if (!param(bridge_device, CONFIG_BRIDGE_DEVICE)) {
			dprintf(D_ALWAYS, "Could not determine which device to use for the external bridge.\n");
			return 1;
		} else {
			m_ad->InsertAttr(ATTR_BRIDGE_DEVICE, bridge_device);
		}
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
    
    int result = 0;
    std::string configuration_type;
    m_ad->EvaluateAttrString(ATTR_NETWORK_TYPE, configuration_type);
    if(configuration_type == "bridge")
    {
        if ((result = create_bridge(bridge_name.c_str())) && (result != EEXIST))
        {
            dprintf(D_ALWAYS, "Unable to create linux bridge (%s); error=%d.\n", bridge_name.c_str(), result);
            return result;
        }
    }
    else if (configuration_type == "ovs_bridge")
    {
        if((result = ovs_create_bridge(bridge_name.c_str())) && (result != EEXIST))
        {
            dprintf(D_ALWAYS, "Unable to create ovs bridge (%s); error=%d.\n", bridge_name.c_str(), result);
            return result;
        }
    }

	// Manipulate the routing and state of the link.  Done internally; no callouts.
	NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
	int fd = manager.GetNetlinkSocket();

	// We are responsible for creating the bridge.
	if (result != EEXIST)
	{
		if (set_status(fd, bridge_name.c_str(), IFF_UP))
		{
			if(configuration_type == "bridge")
            {
                delete_bridge(bridge_name.c_str());
			    return 1;
            }
            else if (configuration_type == "ovs_bridge")
            {
                ovs_delete_bridge(bridge_name.c_str());
                return 1;
            }
		}
        // For ovs bridge, currently we just simply disable stp to make sure
        // the bridge can go to forwarding state after it is brought up
        if(configuration_type == "bridge")
        {
            if ((result = set_bridge_fd(bridge_name.c_str(), 0)))
            {
                dprintf(D_ALWAYS, "Unable to set bridge %s forwarding delay to 0.\n", bridge_name.c_str());
                delete_bridge(bridge_name.c_str());
                return result;
            }
        }
        else if(configuration_type == "ovs_bridge")
        {
            if((result = ovs_disable_stp(bridge_name.c_str())))
            {
                dprintf(D_ALWAYS, "Unable to disable ovs bridge %s stp.\n", bridge_name.c_str());
                ovs_delete_bridge(bridge_name.c_str());
                return result;
            }
        }
        
        if(configuration_type == "bridge")
        {
		    if ((result = add_interface_to_bridge(bridge_name.c_str(), bridge_device.c_str())))
            {
			    dprintf(D_ALWAYS, "Unable to add device %s to bridge %s\n", bridge_device.c_str(), bridge_name.c_str());
			    set_status(fd, bridge_name.c_str(), 0);
                delete_bridge(bridge_name.c_str());
                return result;
            }
        }
        else if(configuration_type == "ovs_bridge")
        {
		    if ((result = ovs_add_interface_to_bridge(bridge_name.c_str(), bridge_device.c_str())))
            {
			    dprintf(D_ALWAYS, "Unable to add device %s to openvswitch bridge %s\n", bridge_device.c_str(), bridge_name.c_str());
			    set_status(fd, bridge_name.c_str(), 0);
                ovs_delete_bridge(bridge_name.c_str());
                return result;
            }
        }

		// If either of these fail, we likely knock the system off the network.
		if ((result = move_routes_to_bridge(fd, bridge_device.c_str(), bridge_name.c_str())))
        {
			dprintf(D_ALWAYS, "Failed to move routes from %s to bridge %s\n", bridge_device.c_str(), bridge_name.c_str());
			set_status(fd, bridge_name.c_str(), 0);
            if(configuration_type == "bridge"){
                delete_bridge(bridge_name.c_str());
            }
            else if(configuration_type == "ovs_bridge"){
			    ovs_delete_bridge(bridge_name.c_str());
            }
			return result;
		}
		if ((result = move_addresses_to_bridge(fd, bridge_device.c_str(), bridge_name.c_str())))
		{
			dprintf(D_ALWAYS, "Failed to move addresses from %s to bridge %s.\n", bridge_device.c_str(), bridge_name.c_str());
			// Bridge not deleted - we might be able to survive without moving the address.
			//delete_bridge(bridge_name.c_str());
			return result;
		}
	}
    
    if(configuration_type == "bridge")
    {
	    if ((result = add_interface_to_bridge(bridge_name.c_str(), external_device.c_str())) && (result != EEXIST))
        {
		    dprintf(D_ALWAYS, "Unable to add device %s to bridge %s\n", external_device.c_str(), bridge_name.c_str());
		    return result;
	    }
    }
    else if(configuration_type == "ovs_bridge")
    {
        ovs_delete_interface_from_bridge(bridge_name.c_str(), external_device.c_str());
	    if ((result = ovs_add_interface_to_bridge(bridge_name.c_str(), external_device.c_str())) && (result != EEXIST))
        {
            dprintf(D_ALWAYS, "Unable to add device %s to openvswitch bridge %s\n", external_device.c_str(), bridge_name.c_str());
            return result;
        }
    }

	if ((m_has_default_route = interface_has_default_route(fd, bridge_name.c_str())) < 0) {
		dprintf(D_ALWAYS, "Unable to determine if bridge %s has a default route.\n", bridge_name.c_str());
		return m_has_default_route;
	}

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
	if (address.Setup())
	{
		dprintf(D_ALWAYS, "Failed to get IP address setup.\n");
		return 1;
	}

	int fd2 = open("/proc/sys/net/bridge/bridge-nf-call-iptables", O_RDWR);
	if (fd2 != -1)
	{
		const char one[2] = "1";
		if (full_write(fd2, one, 1) != 1)
		{
			dprintf(D_ALWAYS, "Failed to enable IPTables filtering for bridge.\n");
		}
	}
	std::string internal_addr;
	if (!m_ad->EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, internal_addr))
	{
		dprintf(D_ALWAYS, "No IPV4 address found.\n");
		return 1;
	}
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-I");
	args.AppendArg("FORWARD");
	args.AppendArg("-m");
	args.AppendArg("physdev");
	args.AppendArg("--physdev-is-bridged");
	args.AppendArg("-s");
	args.AppendArg(internal_addr);
	args.AppendArg("-g");
	args.AppendArg(chain_name);
	RUN_ARGS_AND_LOG(BridgeConfiguration::Setup, iptables_established_connections)
	}
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-I");
	args.AppendArg("FORWARD");
	args.AppendArg("-m");
	args.AppendArg("physdev");
	args.AppendArg("--physdev-is-bridged");
	args.AppendArg("-d");
	args.AppendArg(internal_addr);
	args.AppendArg("-g");
	args.AppendArg(chain_name);
	RUN_ARGS_AND_LOG(BridgeConfiguration::Setup, iptables_established_connections)
	}

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
	
	// We handle bridge status differently with linux bridge and ovs bridge
	// For linux bridge, stp is enabled we wait for the bridge to come up.
	// For ovs bridge, by default stp is disabled, we can assume the ports on
	// ovs bridge gets to forwarding state immediately, thus we don't check the 
	// state of ports connected to ovs bridge.
	
	int err;
	std::string configuration_type;
	m_ad->EvaluateAttrString(ATTR_NETWORK_TYPE, configuration_type);
	if(configuration_type == "bridge")
	{
		NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
        	int fd = manager.GetNetlinkSocket();
		if ((err = wait_for_bridge_status(fd, external_device.c_str()))) {
			return err;
		}
	}
	else if (configuration_type == "ovs_bridge")
	{
		// do nothing here
	}
	char go = 1;
	while (((err = write(m_p2c[1], &go, 1)) < 0) && (errno == EINTR)) {}
	if (err < 0) {
		dprintf(D_ALWAYS, "Error writing to child for bridge sync (errno=%d, %s).\n", errno, strerror(errno));
		return errno;
	}
    // At this point, the bridge can forward packets to the external_device,
    // If ovs bridge is used, we add the ovs QoS configuration according
    // to the submitted job request.
    std::string bandwidth_attr("Bandwidth");
    int bandwidth = 0;
    if(!m_ad->EvaluateAttrInt(bandwidth_attr, bandwidth) || bandwidth == 0){
        dprintf(D_ALWAYS, "Submitted job does not request for bandwidth resource. Bandwith rate limiting is skipped.\n");
    }
    else {
        // we utilize openvswitch QoS rate limiting, thus we need to make sure configuration type is "ovs_bridge"
        if (configuration_type == "ovs_bridge") {
            // The unit of "bandwidth is in Mbps, need to convert it to bps"
            dprintf(D_FULLDEBUG, "Requested bandwidth is %d Mbps.\n", bandwidth);
            int request_rate = bandwidth * 1000 * 1000;
            std::stringstream request_rate_value;
            request_rate_value << request_rate;
            std::string request_min_rate_str = std::string("other-config:min-rate=") + request_rate_value.str();
            std::string request_max_rate_str = std::string("other-config:max-rate=") + request_rate_value.str();
            {
            ArgList args;
            args.AppendArg("ovs-vsctl");
            args.AppendArg("set");
            args.AppendArg("port");
            args.AppendArg(external_device);
            args.AppendArg("qos=@newqos");
            args.AppendArg("--");
            args.AppendArg("--id=@newqos");
            args.AppendArg("create");
            args.AppendArg("qos");
            args.AppendArg("type=linux-htb");
            args.AppendArg(request_max_rate_str);
            args.AppendArg("queues:0=@newqueue");
            args.AppendArg("--");
            args.AppendArg("--id=@newqueue");
            args.AppendArg("create");
            args.AppendArg("queue");
            args.AppendArg(request_min_rate_str);
            args.AppendArg(request_max_rate_str);
            RUN_ARGS_AND_LOG(BridgeConfiguration::SetupPostForkParent, set_ovs_qos)
            }
        }
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

	// Manipulate the routing and state of the link.  Done internally; no callouts.
	NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
	int fd = manager.GetNetlinkSocket();

	std::string gw = "0.0.0.0";
	std::string dev = internal_device;
	if (!m_has_default_route) {
		if (!m_ad->EvaluateAttrString(ATTR_GATEWAY, gw)) {
			dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_GATEWAY " is missing.\n");
			return 1;
		}
		dev = "";
	}
	if (add_default_route(fd, gw.c_str(), dev.c_str())) {
		dprintf(D_ALWAYS, "Failed to add default route.\n");
		return 1;
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

	GratuitousArp(internal_device);

	return 0;
}

int
BridgeConfiguration::Cleanup() {

	if (m_p2c[0] >= 0) close(m_p2c[0]);
	if (m_p2c[1] >= 0) close(m_p2c[1]);

	dprintf(D_FULLDEBUG, "Cleaning up network bridge configuration.\n");
	AddressSelection & address = GetAddressSelection();
	address.Cleanup();

	std::string bridge_device;
	if (!m_ad->EvaluateAttrString(ATTR_BRIDGE_NAME, bridge_device)) {
		dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_BRIDGE_NAME " is missing.\n");
		return 1;
	}

	DIR *dirp = opendir(("/sys/class/net/" + bridge_device + "/brif").c_str());
	struct dirent *dp;
	while (dirp)
	{
		errno = 0;
		if ((dp = readdir(dirp)) != NULL)
		{
			if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			{
				continue;
			}
			GratuitousArp(dp->d_name);
		} else {
			closedir(dirp);
			dirp = NULL;
		}
	}
	GratuitousArp(bridge_device);
	// For ovs bridge, we also need to delete the external deivce connected to the bridge for clean up
	std::string bridge_name;
	m_ad->EvaluateAttrString(ATTR_BRIDGE_NAME, bridge_name);
	std::string configuration_type;
    	m_ad->EvaluateAttrString(ATTR_NETWORK_TYPE, configuration_type);
	if(configuration_type == "ovs_bridge") {
		std::string external_device;
		if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_INTERFACE, external_device)) {
			dprintf(D_ALWAYS, "Required ClassAd attribute " ATTR_EXTERNAL_INTERFACE " is missing.\n");
			return 1;
		}
		ovs_delete_interface_from_bridge(bridge_name.c_str(), external_device.c_str());
        // destroy all qos records in ovsdb, note that the option "--all"
        // requires openvswitch with version > 1.8
        {
        ArgList args;
        args.AppendArg("ovs-vsctl");
        args.AppendArg("--");
        args.AppendArg("--all");
        args.AppendArg("destroy");
        args.AppendArg("Queue");
        RUN_ARGS_AND_LOG(BridgeConfiguration::Cleanup, destroy_qos_record)
        }
    }

	return 0;
}

int BridgeConfiguration::GratuitousArpAddressCallback(struct nlmsghdr /*hdr*/, struct ifaddrmsg addr, struct rtattr ** attr, void * user_arg)
{
	GratuitousArpInfo *info = static_cast<GratuitousArpInfo*>(user_arg);
	if (addr.ifa_index != info->m_callback_eth)
	{
		return 0;
	}

	if (addr.ifa_family == AF_INET && attr[IFLA_ADDRESS] && attr[IFLA_ADDRESS]->rta_len == RTA_LENGTH(sizeof(in_addr_t)))
	{
		in_addr_t saddr;
		memcpy(&saddr, static_cast<char*>(RTA_DATA(attr[IFLA_ADDRESS]))+sizeof(RTA_LENGTH(0)), sizeof(in_addr_t));
		info->m_callback_addresses.push_back(saddr);
	}
	return 0;
}

int BridgeConfiguration::GratuitousArp(const std::string &device)
{
	if (device.size() >= IFNAMSIZ)
	{
		dprintf(D_ALWAYS, "Device name too long for gratuitous ARP.\n");
		return 1;
	}

	int fd;
	if ((fd = socket(AF_PACKET, SOCK_DGRAM, 0)) == -1)
	{
		dprintf(D_ALWAYS, "Failed to create socket for retrieving hardware address (errno=%d, %s).\n", errno, strerror(errno));
		return 1;
	}

	int dev_idx = if_nametoindex(device.c_str());
	if (dev_idx == 0) {
		dprintf(D_ALWAYS, "Unknown device %s for sending ARP packets.\n", device.c_str());
		close(fd);
		return 1;
	}

	struct ifreq iface_req; memset(&iface_req, '\0', sizeof(iface_req));
	strncpy(iface_req.ifr_name, device.c_str(), IFNAMSIZ);
	if (-1 == ioctl(fd, SIOCGIFHWADDR, &iface_req))
	{
		dprintf(D_ALWAYS, "Error when retrieving hardware address (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return 1;
	}
	unsigned char mac_address[IFHWADDRLEN+1]; mac_address[IFHWADDRLEN] = '\0';
	memcpy(mac_address, iface_req.ifr_hwaddr.sa_data, IFHWADDRLEN);

        NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
        int sock= manager.GetNetlinkSocket();
	int retval;
	GratuitousArpInfo info(dev_idx);
	if ((retval = get_addresses(sock, &(BridgeConfiguration::GratuitousArpAddressCallback), &info)))
	{
		dprintf(D_ALWAYS, "Failed to get device %s addresses.\n", device.c_str());
		close(fd);
		return 1;
	}

	struct ifreq ifr; memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_ifindex = dev_idx;
	strcpy(ifr.ifr_name, device.c_str());
	ifr.ifr_flags = IFF_UP|IFF_BROADCAST|IFF_RUNNING|IFF_PROMISC|IFF_MULTICAST;

	if ((retval = ioctl(fd, SIOCGIFFLAGS, &ifr)) == -1)
	{
		dprintf(D_ALWAYS, "Unable to set IO flags for ARP socket (errno=%d, %s).\n", errno, strerror(errno));
		close(fd);
		return 1;
	}

	for (std::vector<in_addr_t>::const_iterator it = info.m_callback_addresses.begin(); it != info.m_callback_addresses.end(); it++)
	{
		char ipaddr[INET_ADDRSTRLEN]; ipaddr[0] = '\0';
		inet_ntop(AF_INET, &(*it), ipaddr, INET_ADDRSTRLEN);
		char mac_address_hex[18]; mac_address_hex[17] = '\0';
		MacAddressBinToHex(mac_address, mac_address_hex);
		dprintf(D_FULLDEBUG, "Sending out gratuitous ARP for device %s; hwaddr %s, IP address %s.\n", device.c_str(), mac_address_hex, ipaddr);
		// Gratuitous ARP packet to announce/re-affirm our presence.
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
		short optype = htons(1); // Request operation
		iov[3].iov_base = &optype;
		iov[3].iov_len = 2;
		char arp[20];
		memcpy(arp, mac_address, IFHWADDRLEN); // Source HW addr.
		memcpy(arp+6, &(*it), 4);
		memset(arp+10, 0xff, 6); // Dest HW addr - ff:ff:ff:ff:ff:ff
		memcpy(arp+16, &(*it), 4);
		iov[4].iov_base = &arp;
		iov[4].iov_len = 20;

		struct sockaddr_ll addr; bzero(&addr, sizeof(addr));
		addr.sll_family = AF_PACKET;
		addr.sll_protocol = htons(ETH_P_ARP);
		addr.sll_ifindex = dev_idx;
		addr.sll_hatype = ARPOP_REQUEST;
		addr.sll_pkttype = PACKET_HOST;
		addr.sll_halen = IFHWADDRLEN;
		char hwaddr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		memcpy(addr.sll_addr, hwaddr, IFHWADDRLEN);
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
	}
	close(fd);
	return 0;
}
