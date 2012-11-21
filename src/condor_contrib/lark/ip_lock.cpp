
#include "condor_common.h"
#include "condor_config.h"
#include "condor_uid.h"
#include "condor_attributes.h"
#include "directory.h"

#include "ip_lock.h"

#include <arpa/inet.h>
#include "condor_sockaddr.h"

#include <boost/lexical_cast.hpp>

IPLock::IPLock(const std::string & network_spec) :
	m_network(""),
	m_fd(-1),
	m_lockfile(""),
	m_valid(false)
{
	if (ParseIPv4(network_spec) || ParseIPv6(network_spec))
	{
		m_valid = true;
	}
}

IPLock::~IPLock()
{
	if (m_valid)
		Unlock();
}

bool IPLock::ParseIPv4(const std::string & network_spec)
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
	else
	{
		m_netmask = network_spec.substr(split+1);
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
			if (i < static_cast<in_addr_t>(netmask_val))
			{
				netmask_tmp |= (1 << i);
			}
		}
		m_prefix_addr.s_addr = netmask_tmp;
	}
	else if (1 != inet_pton(AF_INET, m_netmask.c_str(), &m_prefix_addr))
	{
		return false;
	}
	m_valid = true;
	return true;
}

bool IPLock::ParseIPv6(const std::string &)
{
	return false;
}

bool IPLock::Lock(condor_sockaddr &output_addr)
{
	if (!m_valid)
		return false;
	IPIterator it(m_network_addr, m_prefix_addr);
	in_addr_t tmp_address;
	condor_sockaddr test_addr_tmp(m_prefix_addr);
	int iteration = 0;
	while (iteration < 2)
	{
		while ((tmp_address = it.Next()))
		{
			in_addr tmp_in_addr;
			tmp_in_addr.s_addr = tmp_address;
			condor_sockaddr test_addr(tmp_in_addr);
			LockStatus status = LockIP(test_addr.to_ip_string(), false);
			switch (status)
			{
			case Locked:
				output_addr = test_addr;
				return true;
			case Stale:
				// Stale locks are acceptable on second iteration.
				if (iteration == 1 && (LockIP(test_addr.to_ip_string(), true) == Locked))
				{
					output_addr = test_addr;
					return true;
				}
				break;
			case Error:
				return false;
			default:
				break;
			}
		}
		iteration++;
	}
	return false;
}

IPLock::LockStatus IPLock::LockIP(const std::string & addr, bool force)
{
	std::string lock;
	if (!param(lock, "Lock"))
	{
		dprintf(D_ALWAYS, "LOCK parameter not set.\n");
		return Error;
	}
	std::string lark_lock_dir = lock + "/lark/network_addresses";
	if (!mkdir_and_parents_if_needed(lark_lock_dir.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH, PRIV_ROOT))
	{
		dprintf(D_ALWAYS, "Error creating Lark network lock directory %s. (errno=%d, %s)\n", lark_lock_dir.c_str(), errno, strerror(errno));
		return Error;
	}
	std::string lark_lock = lark_lock_dir + "/" + addr;
	int fd = open(lark_lock.c_str(), O_RDWR|O_CREAT|O_EXCL, S_IWUSR|S_IRUSR);
	if ((fd < 0) && (errno != EEXIST))
	{
		dprintf(D_ALWAYS, "Error opening lock file %s. (errno=%d, %s)\n", lark_lock.c_str(), errno, strerror(errno));
		return Error;
	}
	else if ((fd < 0) && (errno == EEXIST))
	{
		if (force)
		{
			fd = open(lark_lock.c_str(), O_RDWR|O_CREAT, S_IWUSR|S_IRUSR);
			if (fd < 0)
			{
				dprintf(D_ALWAYS, "Error opening stale lock file %s. (errno=%d, %s)\n", lark_lock.c_str(), errno, strerror(errno));
				return Error;
			}
		}
		else
		{
			return Stale;
		}
	}
	struct flock lock_info;
	lock_info.l_type = F_WRLCK;
	lock_info.l_whence = SEEK_SET;
	lock_info.l_start = 0;
	lock_info.l_len = 0;
	lock_info.l_pid = 0;
	if (fcntl(fd, F_SETLK, &lock_info) == -1)
	{
		if ((errno == EACCES) || (errno == EAGAIN))
		{
			return Unavailable;
		}
		else
		{
			dprintf(D_ALWAYS, "Error locking file %s. (errno=%d, %s)\n", lark_lock.c_str(), errno, strerror(errno));
			return Error;
		}
	}
	m_fd = fd;
	m_lockfile = lark_lock;
	return Locked;
}

void IPLock::Unlock()
{
	if (m_fd < 0)
	{
		return;
	}
	close(m_fd);
	m_fd = -1;
	if (m_valid)
	{
		unlink(m_lockfile.c_str());
	}
}

