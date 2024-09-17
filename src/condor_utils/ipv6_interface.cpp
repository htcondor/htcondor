#include "condor_common.h"
#include "ipv6_interface.h"
#include "condor_config.h"
#include "condor_sockaddr.h"
#include "my_hostname.h"

static bool scope_id_inited = false;
static uint32_t scope_id = 0;

uint32_t ipv6_get_scope_id() {
	if (!scope_id_inited) {
		std::string network_interface;
		condor_sockaddr ipv4;
		condor_sockaddr ipv6;
		condor_sockaddr ipbest;
		if (param(network_interface, "NETWORK_INTERFACE") &&
		    network_interface_to_sockaddr("NETWORK_INTERFACE",
		                                  network_interface.c_str(),
		                                  ipv4, ipv6, ipbest) &&
		    ipv6.is_valid() && ipv6.is_link_local())
		{
			scope_id = ipv6.to_sin6().sin6_scope_id;
		}
		else if (network_interface_to_sockaddr("Ipv6LinkLocal", "fe80:*",
		                                       ipv4, ipv6, ipbest) &&
		         ipv6.is_valid() && ipv6.is_link_local())
		{
			scope_id = ipv6.to_sin6().sin6_scope_id;
		}
		scope_id_inited = true;
	}

	return scope_id;
}

