#include "condor_common.h"
#include "condor_sockfunc.h"
#include "ipv6_hostname.h"
#include "ipv6_interface.h"
#include "condor_debug.h"

int condor_connect(int sockfd, const condor_sockaddr& addr)
{
	if (addr.is_ipv6() && addr.is_link_local()) {
		condor_sockaddr connect_addr = addr;
		connect_addr.set_scope_id(ipv6_get_scope_id());
		return connect(sockfd, connect_addr.to_sockaddr(), connect_addr.get_socklen());
	}
	return connect(sockfd, addr.to_sockaddr(), addr.get_socklen());
}

int condor_accept(int sockfd, condor_sockaddr& addr)
{
	sockaddr_storage st;
	socklen_t len = sizeof(st);
	int ret = accept(sockfd, (sockaddr*)&st, &len);
	
	if (ret >= 0) {
		addr = condor_sockaddr((sockaddr*)&st);
	}
	return ret;

}

int condor_bind(int sockfd, const condor_sockaddr& addr)
{
	if (addr.is_ipv6() && addr.is_link_local()) {
		condor_sockaddr bind_addr = addr;
		bind_addr.set_scope_id(ipv6_get_scope_id());
		return bind(sockfd, bind_addr.to_sockaddr(), bind_addr.get_socklen());
	}
	return bind(sockfd, addr.to_sockaddr(), addr.get_socklen());
}

int condor_inet_pton(const char* src, condor_sockaddr& dest)
{
	int ret;
	const char* colon = strchr(src, ':');
	if (!colon) {
		in_addr inaddr;
		ret = inet_pton(AF_INET, src, (void*)&inaddr);
		//printf("inet_pton ipv4 path, ret=%d sin=%08x\n", ret, inaddr.s_addr);
		if (ret)
			dest = condor_sockaddr(inaddr);
	}
	else
	{
		in6_addr in6addr;
		ret = inet_pton(AF_INET6, src, (void*)&in6addr);
		if (ret)
			dest = condor_sockaddr(in6addr);
	}
	return ret;
}

int condor_sendto(int sockfd, const void* buf, size_t len, int flags,
				  const condor_sockaddr& addr)
{
	int ret;
	if (addr.is_ipv6() && addr.is_link_local()) {
		condor_sockaddr send_addr = addr;
		send_addr.set_scope_id(ipv6_get_scope_id());
		ret = sendto(sockfd, (const char*)buf, len, flags, send_addr.to_sockaddr(),
				 send_addr.get_socklen());
	} else {
		ret = sendto(sockfd, (const char*)buf, len, flags, addr.to_sockaddr(),
					 addr.get_socklen());
	}
	return ret;
}


int condor_recvfrom(int sockfd, void* buf, size_t buf_size, int flags,
		        condor_sockaddr& addr)
{
		// we can further optimize it by passing addr into recvfrom() directly
	sockaddr_storage ss;
	int ret;
	socklen_t fromlen = sizeof(ss);
	memset(&ss, 0, sizeof(ss));

	ret = recvfrom(sockfd, (char*)buf, buf_size, flags, (sockaddr*)&ss, 
		&fromlen);
	if (ret>=0) {
		addr = condor_sockaddr( (sockaddr*)&ss );
	}
	return ret;
}

static double getnameinfo_slow_limit = 2.0;
int condor_getnameinfo (const condor_sockaddr& addr,
		                char * __host, socklen_t __hostlen,
		                char * __serv, socklen_t __servlen,
		                unsigned int __flags)
{
	const sockaddr* sa = addr.to_sockaddr();
	socklen_t len = addr.get_socklen();
	int ret;

	double begin = _condor_debug_get_time_double();
	ret = getnameinfo( sa, len, __host, __hostlen, __serv, __servlen, __flags);
	double timediff = _condor_debug_get_time_double() - begin;
	if (timediff > getnameinfo_slow_limit) {
		dprintf(D_ALWAYS, "WARNING: Saw slow DNS query, which may impact entire system: getnameinfo(%s) took %f seconds.\n", addr.to_ip_string().c_str(), timediff);
	}
	return ret;
}

int condor_getsockname_ex(int sockfd, condor_sockaddr& addr)
{
	int ret;
	ret = condor_getsockname(sockfd, addr);
	if (ret == 0 && addr.is_addr_any()) {
		unsigned short portno = addr.get_port();
		addr = get_local_ipaddr(addr.get_protocol());
		addr.set_port(portno);
	}

	return ret;
}

bool addr_is_local(const condor_sockaddr& addr)
{
	bool result = false;
	condor_sockaddr tmp_addr = addr;
		// ... but use any old ephemeral port.
	tmp_addr.set_port(0);
	int sock = socket(tmp_addr.get_aftype(), SOCK_DGRAM, IPPROTO_UDP);

	if (sock >= 0) { // unclear how this could fail, but best to check

			// invoke OS bind, not cedar bind - cedar bind does not allow us
			// to specify the local address.
		if (condor_bind(sock, tmp_addr) < 0) {
			// failed to bind.  assume we failed  because the peer address is
			// not local.
			result = false;
		} else {
			// bind worked, assume address has a local interface.
			result = true;
		}
		// must not forget to close the socket we just created!
#if defined(WIN32)
		closesocket(sock);
#else
		close(sock);
#endif
	}
	return result;
}
