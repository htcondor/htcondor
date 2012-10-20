
#include "ip_lock.h"

#include <boost/lexical_cast.hpp>

// To be replaced by the condor locking system once this links with Condor
#define ATTR_LOCK_DIR "/tmp"

IPLock::IPLock() :
	m_valid(false)
{
}

bool IPLock::Parse_IPv4(const std::string & network_spec)
{
	// TODO: IPv4 only; convert to IPv6
	size_t split = network_spec.find("/");
	m_network = network_spec.substr(0, split);

	if (1 != inet_pton(AF_INET, m_network.c_str(), &m_network_addr))
	{
		return false;
	}
	if (split == std::string::npos)
	{
		m_netmask = "32";
	}
	int netmask_val;
	try
	{
		netmask_val = boost::lexical_cast<int>(m_netmask);
	}
	catch(const boost::bad_lexical_cast&)
	{
		netmask_val = -1;
	}

	if (netmask_val >= 0)
	{
		in_addr_t netmask_tmp = 0;
		for (in_addr_t i = 0; i < 32; i++)
		{
			if (i < netmask_val)
			{
				netmask_tmp |= (1 << 1);
			}
		}
		m_netmask_addr.s_addr = htonl(netmask_tmp);
	}
	else if (1 != inet_pton(AF_INET, m_network.c_str(), &m_network_addr))
	{
		return false;
	}
	m_valid = true;
	return true;
}

sockaddr IPLock::SelectIP(const std::string &)
{
}

