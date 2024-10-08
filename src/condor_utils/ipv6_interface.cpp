#include "condor_common.h"
#include "ipv6_interface.h"
#include "condor_config.h"
#include "condor_sockaddr.h"
#include "my_hostname.h"

static bool scope_id_inited = false;
static uint32_t scope_id = 0;

#if HAVE_GETIFADDRS
#include <ifaddrs.h>

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


#else
	// Win32

#include <iphlpapi.h>

uint32_t find_scope_id(const condor_sockaddr& addr) {
	if (!addr.is_ipv6())
		return 0;

	ULONG family = AF_INET6;
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX; // Set the flags to pass to GetAdaptersAddresses
	ULONG cbBuf = 16*1024; // start with 16Kb
	PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
	auto_free_ptr addrbuf((char*)malloc(cbBuf));
	int iterations = 0;

	char ipbuf[IP_STRING_BUF_SIZE+1];
	dprintf(D_ALWAYS, "Win32 find_scope_id(%s) id called\n", addr.to_ip_string(ipbuf, IP_STRING_BUF_SIZE, false));

	do {
		ASSERT(addrbuf.ptr())
		pAddresses = (PIP_ADAPTER_ADDRESSES)addrbuf.ptr();
		DWORD retval = GetAdaptersAddresses(family, flags, NULL, pAddresses, &cbBuf);
		if (retval == ERROR_BUFFER_OVERFLOW) {
			addrbuf.set((char*)malloc(cbBuf));
			continue;
		} else if (retval == NO_ERROR) {
			break;
		} else {
			dprintf(D_ERROR, "ipv6_get_scope_id() error %u from GetAdaptersAddresses\n", retval);
			return 0;
		}
	} while (++iterations < 3);


	uint32_t result = (uint32_t)-1;
	for (auto * pip = pAddresses; pip; pip->Next) {
		for (auto * fma = pip->FirstUnicastAddress; fma; fma = fma->Next) {
			if (fma->Address.lpSockaddr) {
				condor_sockaddr addr2(fma->Address.lpSockaddr);
				if (addr.compare_address(addr2)) {
					return addr2.to_sin6().sin6_scope_id;
				}
			}
		}
	}

	return 0;
}

#endif

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

