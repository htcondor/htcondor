
#include "condor_common.h"

#include <classad/classad.h>
#include "condor_debug.h"

#include "lark_attributes.h"
#include "dhcp_address.h"
#include "dhcp_management.h"

using namespace lark;

int
DHCPAddressSelection::SelectAddresses()
{
	m_ad->InsertAttr(ATTR_INTERNAL_ADDRESS_IPV4, "0.0.0.0");
	m_ad->InsertAttr(ATTR_EXTERNAL_ADDRESS_IPV4, "0.0.0.0");
	return 0;
}

int
DHCPAddressSelection::Setup()
{
	if (dhcp_query(*m_ad)) {
		return 1;
	}
	return 0;
}

int
DHCPAddressSelection::SetupPostFork()
{
	if (dhcp_commit(*m_ad)) {
		return 1;
	}
	return 0;
}

int
DHCPAddressSelection::Cleanup()
{
	dprintf(D_FULLDEBUG, "Releasing DHCP address.\n");
	if (dhcp_release(*m_ad)) {
		return 1;
	}
	return 0;
}

