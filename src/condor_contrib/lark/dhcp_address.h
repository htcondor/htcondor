
#ifndef __dhcp_address_h_
#define __dhcp_address_h_

#include "address_selection.h"

namespace lark {

/*
 * Invoke an DHCP client.
 *
 * As a side-effect, may populate other attributes about the network
 * that can be used later
 */
class DHCPAddressSelection : public AddressSelection {

public:
	DHCPAddressSelection(classad_shared_ptr<classad::ClassAd> machine_ad)
		: m_ad(machine_ad),
		  m_use_address_from_cache(false)
		{}

	virtual int SelectAddresses();

	virtual int Setup();
	virtual int SetupPostFork();

	virtual int Cleanup();

	static bool LeaseHasRemaining(classad::ClassAd &ad, unsigned remaining);

private:
	classad_shared_ptr<classad::ClassAd> m_ad;
	bool m_use_address_from_cache;
};

}

#endif

