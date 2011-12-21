#include "condor_common.h"
#include "ipv6_interface.h"
#include "condor_config.h"
#include "condor_sockaddr.h"

#if HAVE_GETIFADDRS
#include <ifaddrs.h>

static bool scope_id_inited = false;
static uint32_t scope_id = 0;

uint32_t find_scope_id(const condor_sockaddr& addr) {
	if (!addr.is_ipv6())
		return 0;
	ifaddrs* ifaddr;
	ifaddrs* ifa;

	if (getifaddrs(&ifaddr))
		return 0;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		condor_sockaddr addr2(ifa->ifa_addr);
		if (addr.compare_address(addr2)) {
			return addr2.to_sin6().sin6_scope_id;
		}
	}

	return (uint32_t)-1;
}

uint32_t ipv6_get_scope_id() {
	if (!scope_id_inited) {
		MyString network_interface;
		if (param(network_interface, "NETWORK_INTERFACE")) {
			condor_sockaddr addr;
			if (addr.from_ip_string(network_interface)) {
				scope_id = find_scope_id(addr);
			}
		}
	}

	return scope_id;
}

#else
	// Win32, Solaris
uint32_t ipv6_get_scope_id() {
	// TODO
	EXCEPT("ipv6_get_scope_id is not implemented on this platform.  In practice this means IPv6 does not work on Windows.");
	return 0;
}

#endif
