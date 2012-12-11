
#ifndef __bridge_configuration_h_
#define __bridge_configuration_h_

#include "network_configuration.h"

namespace lark {

class BridgeConfiguration : public NetworkConfiguration {

public:
	BridgeConfiguration(classad_shared_ptr<classad::ClassAd> machine_ad)
		: NetworkConfiguration(machine_ad),
		  m_ad(machine_ad)
		{}

	virtual int Setup();
	virtual int SetupPostFork();
	virtual int Cleanup();

private:
	classad_shared_ptr<classad::ClassAd> m_ad;
};

}

#endif

