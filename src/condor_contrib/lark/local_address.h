
#ifndef __local_address_h_
#define __local_address_h_

#include "address_selection.h"
#include "ip_lock.h"

namespace lark {
/*
 * Provides an IPv4 address guaranteed to be local to this host.
 * Uses POSIX file locking to implement this guarantee.
 */
class HostLocalAddressSelection : public AddressSelection {

public:
	HostLocalAddressSelection(classad_shared_ptr<classad::ClassAd> machine_ad)
		: m_ad(machine_ad),
		  m_iplock_external(NULL),
		  m_iplock_internal(NULL)
		{}

	virtual ~HostLocalAddressSelection() {}

	virtual int SelectAddresses();

private:
	classad_shared_ptr<classad::ClassAd> m_ad;
	std::auto_ptr<IPLock> m_iplock_external;
	std::auto_ptr<IPLock> m_iplock_internal;
};

}

#endif

