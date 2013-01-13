
#include "condor_common.h"
#include "condor_debug.h"

#include <string>

#include <net/if.h>
#include <fcntl.h>
#include <errno.h>

#include "lark_attributes.h"
#include "nat_configuration.h"
#include "network_manipulation.h"
#include "network_namespaces.h"

#include "popen_util.h"
#include "condor_blkng_full_disk_io.h"

using namespace lark;

#define IP_FORWARD_FILENAME "/proc/sys/net/ipv4/ip_forward"

int
NATConfiguration::Setup()
{
	// Pull out some necessary attributes
	std::string internal_ip;
	if (!m_ad->EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, internal_ip)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_INTERNAL_ADDRESS_IPV4 "\n");
		return 1;
	}
	std::string external_ip;
	if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_ADDRESS_IPV4, external_ip)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_EXTERNAL_ADDRESS_IPV4 "\n");
		return 1;
	}
	std::string external_interface;
	if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_INTERFACE, external_interface)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_EXTERNAL_INTERFACE "\n");
		return 1;
	}
	std::string chain_name;
	if(!m_ad->EvaluateAttrString(ATTR_IPTABLE_NAME, chain_name)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_IPTABLE_NAME "\n");
		return 1;
	}

	// Record the external IP as the gateway to use for the internal device.
	m_ad->InsertAttr(ATTR_GATEWAY, external_ip);

	// Enable IP forwarding between devices.
	int fd = open(IP_FORWARD_FILENAME, O_WRONLY);
	if (fd == -1) {
		dprintf(D_ALWAYS, "Unable to open " IP_FORWARD_FILENAME " for write!  Cannot enable NAT. (errno=%d, %s)\n", errno, strerror(errno));
	}
	char one[] = "1";
	if (full_write(fd, one, 1) != 1) {
		dprintf(D_ALWAYS, "Error writing to " IP_FORWARD_FILENAME ".  (errno=%d, %s)\n", errno, strerror(errno));
		close(fd);
		return 1;
	}
	close(fd);

	// Enable IP masquerading:
	// iptables -t nat -A POSTROUTING --src $JOB_INNER_IP ! --dst $JOB_INNER_IP -j MASQUERADE
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-t");
	args.AppendArg("nat");
	args.AppendArg("-A");
	args.AppendArg("POSTROUTING");
	args.AppendArg("--src");
	args.AppendArg(internal_ip.c_str());
	args.AppendArg("!");
	args.AppendArg("--dst");
	args.AppendArg(internal_ip);
	args.AppendArg("-j");
	args.AppendArg("MASQUERADE");
	RUN_ARGS_AND_LOG(NATConfiguration::Setup, iptables_masquerade)
	}

	// Forward packets from the starter to the appropriate chain.
	// iptables -I FORWARD -i $DEV ! -o $DEV -g $JOBID
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-I");
	args.AppendArg("FORWARD");
	args.AppendArg("-i");
	args.AppendArg(external_interface.c_str());
	args.AppendArg("-g");
	args.AppendArg(chain_name.c_str());
	RUN_ARGS_AND_LOG(NATConfiguration::Setup, iptables_forward)
	}

	// Forward established connections back to the starter.
	// iptables -I FORWARD ! -i $DEV -o $DEV -m state --state RELATED,ESTABLISHED -g $JOBID
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-I");
	args.AppendArg("FORWARD");
	args.AppendArg("!");
	args.AppendArg("-i");
	args.AppendArg(external_interface.c_str());
	args.AppendArg("-o");
	args.AppendArg(external_interface.c_str());
	args.AppendArg("-m");
	args.AppendArg("state");
	args.AppendArg("--state");
	args.AppendArg("RELATED,ESTABLISHED");
	args.AppendArg("-g");
	args.AppendArg(chain_name.c_str());
	RUN_ARGS_AND_LOG(NATConfiguration::Setup, iptables_established_connections)
	}

	// Manipulate the routing and state of the link.  Done internally; no callouts.
	NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
	fd = manager.GetNetlinkSocket();

	// Add the address to the device
	// ip addr add $JOB_OUTER_IP/255.255.255.255 dev $DEV
	if (add_address(fd, external_ip.c_str(), 32, external_interface.c_str())) {
		dprintf(D_ALWAYS, "Failed to add external address.\n");
		return 1;
	}
	// Enable the device
	// ip link set dev $DEV up
	if (set_status(fd, external_interface.c_str(), IFF_UP)) {
		dprintf(D_ALWAYS, "Failed to set external device status.\n");
		return 1;
	}
	// Add a default route
	// route add $JOB_INNER_IP/32 gw $JOB_OUTER_IP
	{
	ArgList args;
	args.AppendArg("route");
	args.AppendArg("add");
	args.AppendArg((internal_ip + "/32").c_str());
	args.AppendArg("gw");
	args.AppendArg(external_ip.c_str());
	RUN_ARGS_AND_LOG(NATConfiguration::Setup, external_iface_route)
	}

	return 0;
}

int
NATConfiguration::SetupPostForkChild()
{
	std::string external_gw;
	if (!m_ad->EvaluateAttrString(ATTR_GATEWAY, external_gw)) {
		dprintf(D_FULLDEBUG, "Missing required ClassAd attribute " ATTR_GATEWAY "\n");
		return 1;
	}
	int err = 0;

	NetworkNamespaceManager & manager = NetworkNamespaceManager::GetManager();
	int sock = manager.GetNetlinkSocket();
	if ((err = add_default_route(sock, external_gw.c_str(), ""))) {
		dprintf(D_ALWAYS, "Unable to add default route via %s; %d\n", external_gw.c_str(), err);
		return err;
	}
	return err;
}

int
NATConfiguration::Cleanup()
{
	// Pull out attributes needed for cleanup.
	std::string internal_ip;
	if (!m_ad->EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, internal_ip)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_INTERNAL_ADDRESS_IPV4 "\n");
		return 1;
	}
	std::string chain_name;
	if(!m_ad->EvaluateAttrString(ATTR_IPTABLE_NAME, chain_name)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_IPTABLE_NAME "\n");
	}

	// Remove address masquerade.  Equivalent to:
	// iptables -t nat -D POSTROUTING --src $JOB_INNER_IP ! --dst $JOB_INNER_IP -j MASQUERADE
	{
	ArgList args;
	args.AppendArg("iptables");
	args.AppendArg("-t");
	args.AppendArg("nat");
	args.AppendArg("-D");
	args.AppendArg("POSTROUTING");
	args.AppendArg("--src");
	args.AppendArg(internal_ip.c_str());
	args.AppendArg("!");
	args.AppendArg("--dst");
	args.AppendArg(internal_ip.c_str());
	args.AppendArg("-j");
	args.AppendArg("MASQUERADE");
	RUN_ARGS_AND_LOG(NATConfiguration::Cleanup, iptables_masquerade)
	}

	return 0;
}

