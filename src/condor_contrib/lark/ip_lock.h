
#include <string>
#include <netinet/in.h>
#include "condor_sockaddr.h"

class IPLock
{

public:

	IPLock(const std::string & network);
	~IPLock();

	bool Lock(condor_sockaddr &);
	void Unlock();

private:

	bool ParseIPv4(const std::string &);
	bool ParseIPv6(const std::string &);

	enum LockStatus {
		Locked, // We have successfully taken the lock.
		Unavailable, // The lock is taken by someone else.
		Stale, // The lock exists on disk, but no process owns it.
		Error // Error when trying to lock
	};

	LockStatus LockIP(const std::string & network, bool force);

	std::string m_network;
	std::string m_netmask;
	// To be converted to Condor structs when we link in Condor
	in_addr m_network_addr;
	in_addr m_prefix_addr;
	int m_fd;
	std::string m_lockfile;
	bool m_valid;
};

class IPIterator
{

public:

	IPIterator(in_addr network, in_addr prefix)
	{
		n_addr = ntohl(network.s_addr);
		p_addr = ntohl(prefix.s_addr);
		limit = 0;
		for (in_addr_t i = 0; i<31; i++)
		{
			if ((p_addr & (1 << i)) == 0)
				limit = i+1;
		}
		current = 0;
	}

	in_addr_t Next()
	{
		while (((current % 256) == 0) || ((current % 256) == 255))
		{
			current++;
		}
		if (current > (in_addr_t)(1 << limit))
			return 0;
		in_addr_t result = n_addr & p_addr;
		result |= current & (~p_addr);
		current++;
		return htonl(result);
	}

private:
	in_addr_t n_addr;
	in_addr_t p_addr;
	in_addr_t limit;
	in_addr_t current;

};

