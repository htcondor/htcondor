#include "condor_common.h"
#include "condor_sockfunc.h"
#include "ipv6_hostname.h"

int condor_getsockname_ex(int sockfd, condor_sockaddr& addr)
{
	int ret;
	ret = condor_getsockname(sockfd, addr);
	if (ret == 0 && addr.is_addr_any()) {
		unsigned short portno = addr.get_port();
		addr = get_local_ipaddr();
		addr.set_port(portno);
	}

	return ret;
}

