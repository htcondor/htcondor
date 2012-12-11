
#ifndef __address_selection_h_
#define __address_selection_h_

#include <classad/classad_stl.h>

namespace classad {
	class ClassAd;
}

namespace lark {

class AddressSelection {

public:
	virtual ~AddressSelection() {}

	/*
	 * See notes in network_configuration.h for how this function should work.
	 */
	virtual int SelectAddresses() = 0;
};

/*
 * Utilize a static IP address found in the classad
 * (possibly specified by the user).
 */
class StaticAddressSelection : public AddressSelection {

public:
	StaticAddressSelection(classad_shared_ptr<classad::ClassAd> machine_ad)
		: m_ad(machine_ad)
		{}

	virtual int SelectAddresses();

private:
	classad_shared_ptr<classad::ClassAd> m_ad;
};

}

#endif
