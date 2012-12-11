
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
		: m_ad(machine_ad)
		{}

	virtual int SelectAddresses();

	virtual int Setup();
	virtual int SetupPostFork();

	virtual int Cleanup();

private:
	classad_shared_ptr<classad::ClassAd> m_ad;
};

}

#endif

