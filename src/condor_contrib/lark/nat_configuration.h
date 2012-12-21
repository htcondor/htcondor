
#ifndef __nat_configuration_h_
#define __nat_configuration_h_

#include "network_configuration.h"

#include "address_selection.h"

namespace lark {

class NATConfiguration : public NetworkConfiguration {

public:
	NATConfiguration(classad_shared_ptr<classad::ClassAd> machine_ad)
		: NetworkConfiguration(machine_ad),
		  m_ad(machine_ad)
		{}

	virtual int Setup();
	// Post-fork setup is trivial for NAT.
	virtual int SetupPostForkChild();
	virtual int SetupPostForkParent() {return 0;}
	virtual int Cleanup();

private:
	classad_shared_ptr<classad::ClassAd> m_ad;

};

}

#endif
