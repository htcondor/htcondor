#include "condor_common.h"
#include "condor_ipaddress.h"

int condor_connect(int sockfd, const ipaddr* addr)
{
	if ( addr->is_ipv4() )
	{
		sockaddr_in sin = addr->to_sin();
		return connect(sockfd, (const sockaddr*)&sin, sizeof(sin));
	}
	else
		return connect(sockfd, (const sockaddr*)&addr->to_sin6(), 
				sizeof(sockaddr_in6));
}

int condor_accept(int sockfd, ipaddr* addr)
{
	sockaddr_storage st;
	socklen_t len;
	int ret = accept(sockfd, (sockaddr*)&st, &len);
	if (st.ss_family == AF_INET) {
		sockaddr_in* sin = (sockaddr_in*)&st;		*addr = ipaddr(sin);
	}	
	else if (st.ss_family == AF_INET6) {
		sockaddr_in6* sin6 = (sockaddr_in6*)&st;
		*addr = ipaddr(sin6);
	}
	else
	{
		// should output error message
	}
	return ret;

}

int condor_bind(int sockfd, const ipaddr* addr)
{
	if ( addr->is_ipv4() )
	{
		sockaddr_in sin = addr->to_sin();
		return bind(sockfd, (const sockaddr*)&sin, sizeof(sin));
	}
	else
		return bind(sockfd, (const sockaddr*)&addr->to_sin6(), 
				sizeof(sockaddr_in6));

}

int condor_inet_pton(const char* src, ipaddr* dest)
{
	int ret;
	const char* colon = strchr(src, ':');
	if (!colon) {
		in_addr inaddr;
		ret = inet_pton(AF_INET, src, (void*)&inaddr);
		//printf("inet_pton ipv4 path, ret=%d sin=%08x\n", ret, inaddr.s_addr);
		if (ret)
			*dest = ipaddr(inaddr);
	}
	else
	{
		in6_addr in6addr;
		ret = inet_pton(AF_INET6, src, (void*)&in6addr);
		if (ret)
			*dest = ipaddr(in6addr);
	}
	return ret;
}
