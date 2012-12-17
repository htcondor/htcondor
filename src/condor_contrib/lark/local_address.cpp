
#include <condor_common.h>
#include <condor_config.h>

#include <string>

#include "lark_attributes.h"
#include "local_address.h"

using namespace lark;

int
HostLocalAddressSelection::SelectAddresses() {
	condor_sockaddr internal_address, external_address;

	std::string network_spec;
	if (!param(network_spec, CONFIG_LOCAL_NETWORK, "192.168.181.0/255.255.255.0"))
	{
		dprintf(D_FULLDEBUG, "Parameter " CONFIG_LOCAL_NETWORK " is not specified; using default %s.\n", network_spec.c_str());
	}
	m_iplock_external.reset(new IPLock(network_spec));
	m_iplock_internal.reset(new IPLock(network_spec));
	if (!m_iplock_external->Lock(external_address))
	{
		dprintf(D_ALWAYS, "Unable to lock an external IP address to use.\n");
		return 1;
	}
	if (!m_iplock_internal->Lock(internal_address))
	{
		dprintf(D_ALWAYS, "Unable to lock an internal IP address for use.\n");
		return 1;
	}
	if (!m_ad->InsertAttr(ATTR_INTERNAL_ADDRESS_IPV4, internal_address.to_ip_string().Value())) {
		dprintf(D_ALWAYS, "Failure to set " ATTR_INTERNAL_ADDRESS_IPV4 " to %s\n", internal_address.to_ip_string().Value());
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Using address %s for internal network.\n", internal_address.to_ip_string().Value());
	}
	if (!m_ad->InsertAttr(ATTR_EXTERNAL_ADDRESS_IPV4, external_address.to_ip_string().Value())) {
		dprintf(D_ALWAYS, "Failure to set " ATTR_EXTERNAL_ADDRESS_IPV4 " to %s\n", external_address.to_ip_string().Value());
		return 1;
	} else {
		dprintf(D_FULLDEBUG, "Using address %s for external network.\n", external_address.to_ip_string().Value());
	}
	return 0;
}

