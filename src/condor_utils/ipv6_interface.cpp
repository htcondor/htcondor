#include "condor_common.h"
#include "ipv6_interface.h"
#include "condor_config.h"
#include "condor_sockaddr.h"
#include "my_hostname.h"

#if HAVE_GETIFADDRS
#include <ifaddrs.h>

static bool scope_id_inited = false;
static uint32_t scope_id = 0;

uint32_t find_scope_id(const condor_sockaddr& addr) {
	if (!addr.is_ipv6())
		return 0;
	ifaddrs* ifaddr = 0;
	ifaddrs* ifa;

	if (getifaddrs(&ifaddr))
		return 0;

	uint32_t result = (uint32_t)-1;
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		// There's no point looking for scope IDs on anything other than
		// an IPv6 address.  Also, this check prevents condor_sockaddr()
		// from EXCEPT()ing because of an invalid family (e.g., raw socket).
		// We should probably add a from_sockaddr() function that can fail
		// for this use case (called by and EXCEPT()d on by the constructor).
		if( ifa->ifa_addr->sa_family == AF_INET6 ) {
			condor_sockaddr addr2(ifa->ifa_addr);
			if (addr.compare_address(addr2)) {
				result = addr2.to_sin6().sin6_scope_id;
				break;
			}
		}
	}
	freeifaddrs(ifaddr);

	return result;
}

uint32_t ipv6_get_scope_id() {
	if (!scope_id_inited) {
		std::string network_interface;
		std::string ipv4_str;
		std::string ipv6_str;
		std::string ipbest_str;
		condor_sockaddr addr;
		if (param(network_interface, "NETWORK_INTERFACE") &&
		    network_interface_to_ip("NETWORK_INTERFACE",
		                            network_interface.c_str(),
		                            ipv4_str, ipv6_str, ipbest_str) &&
		    addr.from_ip_string(ipv6_str.c_str()) && addr.is_link_local())
		{
			scope_id = find_scope_id(addr);
		}
		else if (network_interface_to_ip("Ipv6LinkLocal", "fe80:*",
		                                 ipv4_str, ipv6_str, ipbest_str) &&
		         addr.from_ip_string(ipv6_str.c_str()) &&
		         addr.is_link_local())
		{
			scope_id = find_scope_id(addr);
		}
		scope_id_inited = true;
	}

	return scope_id;
}

#else
	// Win32
uint32_t ipv6_get_scope_id() {
	// TODO
	EXCEPT("ipv6_get_scope_id is not implemented on this platform.  In practice this means IPv6 does not work on Windows.");
	return 0;
}

#endif
