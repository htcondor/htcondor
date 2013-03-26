
#ifndef __bridge_configuration_h_
#define __bridge_configuration_h_

#include "network_configuration.h"

namespace lark {

class BridgeConfiguration : public NetworkConfiguration {

public:
	BridgeConfiguration(classad_shared_ptr<classad::ClassAd> machine_ad)
		: NetworkConfiguration(machine_ad),
		  m_ad(machine_ad),
		  m_has_default_route(0)
		{m_p2c[0] = -1; m_p2c[1] = -1;}

	virtual int Setup();
	virtual int SetupPostForkChild();
	virtual int SetupPostForkParent();
	virtual int Cleanup();

	static int GratuitousArp(const std::string &device);


private:
	static int GratuitousArpAddressCallback(struct nlmsghdr hdr, struct ifaddrmsg addr, struct rtattr ** attr, void * user_arg);

	classad_shared_ptr<classad::ClassAd> m_ad;
	int m_p2c[2];
	int m_has_default_route;

	struct GratuitousArpInfo
	{
		GratuitousArpInfo(unsigned idx) : m_callback_eth(idx) {}
		std::vector<in_addr_t> m_callback_addresses;
		unsigned m_callback_eth;
	};
};

}

#endif

