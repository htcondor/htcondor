
#include "condor_common.h"
#include "condor_debug.h"

#include "lark_attributes.h"
#include "address_selection.h"

#include <classad/classad.h>

#include <string>

using namespace lark;

int
StaticAddressSelection::SelectAddresses()
{
	std::string address_str;
	if (!m_ad->EvaluateAttrString(ATTR_INTERNAL_ADDRESS_IPV4, address_str)) {
		dprintf(D_ALWAYS, "The required classad attribute " ATTR_INTERNAL_ADDRESS_IPV4 " is missing; cannot assign internal static IPv4 address.\n");
		return 1;
	}
	if (!m_ad->EvaluateAttrString(ATTR_EXTERNAL_ADDRESS_IPV4, address_str)) {
		dprintf(D_ALWAYS, "The required classad attribute " ATTR_EXTERNAL_ADDRESS_IPV4 " is missing; cannot assign external static IPv4 address.\n");
		return 1;
	}
	return 0;
}

