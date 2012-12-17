
#ifndef __bridge_configuration_h_
#define __bridge_configuration_h_

#include "network_configuration.h"

namespace lark {

class BridgeConfiguration : public NetworkConfiguration {

public:
	BridgeConfiguration(classad_shared_ptr<classad::ClassAd> machine_ad)
		: NetworkConfiguration(machine_ad),
		  m_ad(machine_ad)
		{m_p2c[0] = -1; m_p2c[1] = -1;}

	virtual int Setup();
	virtual int SetupPostForkChild();
	virtual int SetupPostForkParent();
	virtual int Cleanup();

private:
	classad_shared_ptr<classad::ClassAd> m_ad;
	int m_p2c[2];
};

}

#endif

