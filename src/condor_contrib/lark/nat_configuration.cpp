
#include "condor_common.h"
#include "condor_debug.h"

#include <string>

#include "fcntl.h"
#include "errno.h"

#include "nat_configuration.h"

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
	std::string external_interface;
	if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_INTERFACE, external_interface)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_EXTERNAL_INTERFACE "\n");
		return 1;
	}
	std::string chain_name;
	if(!m_ad->EvaluateAttrString(ATTR_IPTABLE_NAME, chain_name)) {
		dprintf(D_ALWAYS, "Missing required ClassAd attribute " ATTR_IPTABLE_NAME "\n");
	}

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
	return 0;
}

int
NATConfiguration::Cleanup()
{
	return 0;
}

